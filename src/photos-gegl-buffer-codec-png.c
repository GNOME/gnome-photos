/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 1999 The Free Software Foundation
 * Copyright © 1999 Mark Crichton
 * Copyright © 2006 Øyvind Kolås
 * Copyright © 2018 Red Hat, Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/* Based on code from:
 *   + GdkPixbuf
 *   + GEGL
 */


#include "config.h"

#include <setjmp.h>

#include <gegl.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <png.h>

#include "photos-debug.h"
#include "photos-error.h"
#include "photos-gegl.h"
#include "photos-gegl-buffer-codec-png.h"


struct _PhotosGeglBufferCodecPng
{
  PhotosGeglBufferCodec parent_instance;
  GError **error;
  GeglBuffer *buffer;
  GeglBuffer *buffer_original;
  GeglBuffer *buffer_zoom_from_original;
  GeglMatrix2 inverse_jacobian;
  GeglSampler *sampler;
  gboolean can_set_size;
  gboolean decoding;
  gboolean fatal_error_occurred;
  gboolean pending_zoom_from_original;
  gdouble zoom_from_original_x;
  gdouble zoom_from_original_y;
  gint n_interlacing_passes;
  gint original_height_needed_for_zoom;
  gint scanline;
  gint tile_height;
  gint tile_stride;
  gint tile_width;
  gpointer line;
  gpointer tile_memory;
  guint stride_original;
  png_infop png_info_ptr;
  png_structp png_read_ptr;
};

enum
{
  PROP_0,
  PROP_BUFFER,
  PROP_CAN_SET_SIZE
};


G_DEFINE_TYPE_WITH_CODE (PhotosGeglBufferCodecPng, photos_gegl_buffer_codec_png, PHOTOS_TYPE_GEGL_BUFFER_CODEC,
                         photos_gegl_ensure_extension_points ();
                         g_io_extension_point_implement (PHOTOS_GEGL_BUFFER_CODEC_EXTENSION_POINT_NAME,
                                                         g_define_type_id,
                                                         "png",
                                                         0));


static const gchar *MIME_TYPES[] =
{
  "image/png",
  NULL
};


static const Babl *
photos_gegl_buffer_codec_png_get_format (const Babl *space, gint bit_depth, gint color_type, GError **error)
{
  const Babl *ret_val = NULL;
  g_autoptr (GString) format_name_str = NULL;

  g_return_val_if_fail (space != NULL, NULL);
  g_return_val_if_fail (bit_depth == 8 || bit_depth == 16, NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  format_name_str = g_string_new (NULL);

  switch (color_type)
    {
    case PNG_COLOR_TYPE_GRAY:
      g_string_append (format_name_str, "Y'");
      break;

    case PNG_COLOR_TYPE_GRAY_ALPHA:
      g_string_append (format_name_str, "Y'A");
      break;

    case PNG_COLOR_TYPE_PALETTE:
    case PNG_COLOR_TYPE_RGB:
      g_string_append (format_name_str, "R'G'B'");
      break;

    case PNG_COLOR_TYPE_PALETTE | PNG_COLOR_MASK_ALPHA:
    case PNG_COLOR_TYPE_RGB_ALPHA:
      g_string_append (format_name_str, "R'G'B'A");
      break;

    default:
      g_set_error (error, PHOTOS_ERROR, 0, _("Unsupported PNG color space: %d"), color_type);
      goto out;
      break;
    }

  g_string_append_c (format_name_str, ' ');

  switch (bit_depth)
    {
    case 8:
      g_string_append (format_name_str, "u8");
      break;

    case 16:
      g_string_append (format_name_str, "u16");
      break;

    default:
      g_assert_not_reached ();
      break;
    }

  ret_val = babl_format_with_space (format_name_str->str, space);

 out:
  return ret_val;
}


static const Babl *
photos_gegl_buffer_codec_png_get_space (png_structp png_read_ptr,
                                        png_infop png_info_ptr,
                                        gdouble *out_gamma,
                                        GError **error)
{
  const Babl *ret_val = NULL;
  const Babl *space = NULL;
  const Babl *space_sRGB;
  gdouble gamma = 0.45455;
  gint icc_profile_compression_type;
  png_bytep icc_profile = NULL;
  png_charp icc_profile_name;
  png_uint_32 has_gamma = 0;
  png_uint_32 icc_profile_len;

  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  space_sRGB = babl_space ("sRGB");

  has_gamma = png_get_valid (png_read_ptr, png_info_ptr, PNG_INFO_gAMA);
  if (has_gamma != 0)
    {
      png_get_gAMA (png_read_ptr, png_info_ptr, &gamma);
      if (gamma <= 0.0)
        {
          g_set_error (error, PHOTOS_ERROR, 0, _("Invalid gamma in PNG: %f"), gamma);
          goto out;
        }
    }

  png_get_iCCP (png_read_ptr,
                png_info_ptr,
                &icc_profile_name,
                &icc_profile_compression_type,
                &icc_profile,
                &icc_profile_len);

  if (icc_profile != NULL)
    {
      const gchar *error_message = NULL;

      space = babl_space_from_icc ((char *) icc_profile,
                                   (gint) icc_profile_len,
                                   BABL_ICC_INTENT_RELATIVE_COLORIMETRIC,
                                   &error_message);
      if (space == NULL)
        {
          g_warning ("Unable to create Babl space from ICC profile: %s", error_message);
          space = space_sRGB;
        }
    }

  if (space == NULL)
    {
      png_uint_32 has_sRGB;

      has_sRGB = png_get_valid (png_read_ptr, png_info_ptr, PNG_INFO_sRGB);
      if (has_sRGB != 0)
        space = space_sRGB;
    }

  if (space == NULL && has_gamma != 0)
    {
      const Babl *trc;
      gdouble blue[2] = {0.1500, 0.0600}; /* default to sRGB */
      gdouble green[2] = {0.3000, 0.6000}; /* default to sRGB */
      gdouble red[2] = {0.6400, 0.3300}; /* default to sRGB */
      gdouble white_point[2] = {0.3127, 0.3290}; /* default to sRGB */
      png_uint_32 has_chromaticities = 0;

      has_chromaticities = png_get_valid (png_read_ptr, png_info_ptr, PNG_INFO_cHRM);
      if (has_chromaticities != 0)
        {
          png_get_cHRM (png_read_ptr,
                        png_info_ptr,
                        &white_point[0],
                        &white_point[1],
                        &red[0],
                        &red[1],
                        &green[0],
                        &green[1],
                        &blue[0],
                        &blue[1]);
        }

      trc = babl_trc_gamma (1.0 / gamma);
      space = babl_space_from_chromaticities (NULL,
                                              white_point[0],
                                              white_point[1],
                                              red[0],
                                              red[1],
                                              green[0],
                                              green[1],
                                              blue[0],
                                              blue[1],
                                              trc,
                                              trc,
                                              trc,
                                              BABL_SPACE_FLAG_EQUALIZE);
    }

  if (space == NULL)
    space = space_sRGB;

  if (out_gamma != NULL)
    *out_gamma = gamma;

  g_return_val_if_fail (space != NULL, NULL);
  g_return_val_if_fail (gamma > 0.0, NULL);

  ret_val = space;

 out:
  return ret_val;
}


static void
photos_gegl_buffer_codec_png_zoom_from_original (PhotosGeglBufferCodecPng *self)
{
  const Babl *format_original;
  const Babl *format_zoom_from_original;
  GeglRectangle bbox_tile;
  gint buffer_width;
  gint tile_row;

  g_return_if_fail (self->buffer != self->buffer_original);
  g_return_if_fail (self->buffer_zoom_from_original != self->buffer_original);
  g_return_if_fail (self->pending_zoom_from_original);
  g_return_if_fail (self->tile_memory != NULL);

  format_original = gegl_buffer_get_format (self->buffer_original);
  g_return_if_fail (format_original != NULL);

  format_zoom_from_original = gegl_buffer_get_format (self->buffer_zoom_from_original);

  bbox_tile.height = self->tile_height;
  bbox_tile.width = self->tile_width;
  bbox_tile.x = 0;

  tile_row = (self->scanline - 1) / self->original_height_needed_for_zoom;
  bbox_tile.y = tile_row * self->tile_height;

  buffer_width = gegl_buffer_get_width (self->buffer_zoom_from_original);

  if (G_APPROX_VALUE (self->zoom_from_original_x, self->zoom_from_original_y, PHOTOS_EPSILON))
    {
      g_return_if_fail (format_original == format_zoom_from_original);

      for (bbox_tile.x = 0; bbox_tile.x < buffer_width; bbox_tile.x += self->tile_width)
        {
          gegl_buffer_get (self->buffer_original,
                           &bbox_tile,
                           self->zoom_from_original_x,
                           format_original,
                           self->tile_memory,
                           self->tile_stride,
                           GEGL_ABYSS_NONE | GEGL_BUFFER_FILTER_AUTO);

          gegl_buffer_set (self->buffer_zoom_from_original,
                           &bbox_tile,
                           0,
                           format_original,
                           self->tile_memory,
                           self->tile_stride);
        }
    }
  else
    {
      GeglSamplerGetFun sampler_get_fun;
      const gdouble u_base = self->inverse_jacobian.coeff[0][0] * 0.5;
      const gdouble v_base = self->inverse_jacobian.coeff[1][1] * 0.5;
      const gdouble v_tile = v_base + self->inverse_jacobian.coeff[1][1] * bbox_tile.y;
      gint format_components;
      gint j;
      gint k;

      sampler_get_fun = gegl_sampler_get_fun (self->sampler);
      format_components = babl_format_get_n_components (format_zoom_from_original);

      for (bbox_tile.x = 0; bbox_tile.x < buffer_width; bbox_tile.x += self->tile_width)
        {
          gdouble u_tile = u_base + self->inverse_jacobian.coeff[0][0] * bbox_tile.x;
          gdouble v = v_tile;
          gfloat *pixel = (gfloat *) self->tile_memory;

          for (j = 0; j < self->tile_height; j++)
            {
              gdouble u = u_tile;

              for (k = 0; k < self->tile_width; k++)
                {
                  sampler_get_fun (self->sampler, u, v, &self->inverse_jacobian, pixel, GEGL_ABYSS_NONE);
                  pixel += format_components;
                  u += self->inverse_jacobian.coeff[0][0];
                }

              v += self->inverse_jacobian.coeff[1][1];
            }

          gegl_buffer_set (self->buffer_zoom_from_original,
                           &bbox_tile,
                           0,
                           format_zoom_from_original,
                           self->tile_memory,
                           self->tile_stride);
        }
    }
}


static void
photos_gegl_buffer_codec_png_end (png_structp png_read_ptr, png_infop png_info_ptr)
{
  PhotosGeglBufferCodecPng *self;

  self = PHOTOS_GEGL_BUFFER_CODEC_PNG (png_get_progressive_ptr (png_read_ptr));

  if (self->fatal_error_occurred)
    goto out;

  g_return_if_fail (GEGL_IS_BUFFER (self->buffer));
  g_return_if_fail (GEGL_IS_BUFFER (self->buffer_original));

  if (self->buffer != self->buffer_original && self->pending_zoom_from_original)
    {
      photos_gegl_buffer_codec_png_zoom_from_original (self);
      self->pending_zoom_from_original = FALSE;
    }

 out:
  return;
}


static void
photos_gegl_buffer_codec_png_error (png_structp png_read_ptr, png_const_charp error_message)
{
  PhotosGeglBufferCodecPng *self;

  self = PHOTOS_GEGL_BUFFER_CODEC_PNG (png_get_error_ptr (png_read_ptr));
  self->fatal_error_occurred = TRUE;

  /* Check for *error == NULL for robustness against crappy PNG
   * library.
   */
  if (self->error && *self->error == NULL)
    g_set_error (self->error, PHOTOS_ERROR, 0, _("Error reading PNG: %s"), error_message);

  longjmp (png_jmpbuf (png_read_ptr), 1);
  g_assert_not_reached ();
}


static void
photos_gegl_buffer_codec_png_info (png_structp png_read_ptr, png_infop png_info_ptr)
{
  PhotosGeglBufferCodecPng *self;
  const Babl *format_original = NULL;
  const Babl *space = NULL;
  const Babl *space_sRGB;
  GeglRectangle bbox;
  const gchar *format_name;
  gdouble gamma;
  gdouble target_height;
  gdouble target_width;
  gint bit_depth;
  gint bpp_original;
  gint color_type;
  gint format_components;
  gint interlace_type;
  guint target_height_rounded;
  guint target_width_rounded;
  png_byte channels;
  png_uint_32 has_transparency;
  png_uint_32 height;
  png_uint_32 width;

  self = PHOTOS_GEGL_BUFFER_CODEC_PNG (png_get_progressive_ptr (png_read_ptr));

  if (self->fatal_error_occurred)
    goto out;

  g_return_if_fail (self->buffer == NULL);
  g_return_if_fail (self->buffer_original == NULL);
  g_return_if_fail (self->line == NULL);

  space_sRGB = babl_space ("sRGB");

  png_get_IHDR (png_read_ptr, png_info_ptr, NULL, NULL, &bit_depth, &color_type, &interlace_type, NULL, NULL);

  if (bit_depth < 8)
    png_set_expand (png_read_ptr);

#if BYTE_ORDER == LITTLE_ENDIAN
  else if (bit_depth == 16)
    png_set_swap (png_read_ptr);
#endif

  if (color_type == PNG_COLOR_TYPE_PALETTE)
    png_set_expand (png_read_ptr);

  has_transparency = png_get_valid (png_read_ptr, png_info_ptr, PNG_INFO_tRNS);
  if (has_transparency != 0)
    png_set_expand (png_read_ptr);

  self->n_interlacing_passes = png_set_interlace_handling (png_read_ptr);
  g_return_if_fail ((interlace_type == PNG_INTERLACE_NONE && self->n_interlacing_passes == 1)
                    || (interlace_type != PNG_INTERLACE_NONE && self->n_interlacing_passes > 1));

  space = photos_gegl_buffer_codec_png_get_space (png_read_ptr, png_info_ptr, &gamma, self->error);
  if (space == NULL)
    {
      self->fatal_error_occurred = TRUE;
      goto out;
    }

  if (space == space_sRGB)
    png_set_gamma (png_read_ptr, 2.2, gamma);

  png_read_update_info (png_read_ptr, png_info_ptr);
  png_get_IHDR (png_read_ptr, png_info_ptr, &width, &height, &bit_depth, &color_type, NULL, NULL, NULL);

  g_return_if_fail (bit_depth == 8 || bit_depth == 16);

  photos_debug (PHOTOS_DEBUG_GEGL, "GeglBufferCodecPng: Original size: %u×%u", (guint) width, (guint) height);
  if (height == 0 || width == 0)
    {
      g_set_error (self->error, PHOTOS_ERROR, 0, _("Original PNG has zero width or height"));
      self->fatal_error_occurred = TRUE;
      goto out;
    }

  self->can_set_size = TRUE;
  g_signal_emit_by_name (self, "size-prepared", (guint) width, (guint) height);
  self->can_set_size = FALSE;

  target_height = photos_gegl_buffer_codec_get_height (PHOTOS_GEGL_BUFFER_CODEC (self));
  target_width = photos_gegl_buffer_codec_get_width (PHOTOS_GEGL_BUFFER_CODEC (self));

  photos_debug (PHOTOS_DEBUG_GEGL, "GeglBufferCodecPng: Target size: %f×%f", target_width, target_height);

  if (target_height <= 0 || target_width <= 0)
    {
      g_set_error (self->error, PHOTOS_ERROR, 0, _("Transformed PNG has invalid width or height"));
      self->fatal_error_occurred = TRUE;
      goto out;
    }

  target_height_rounded = (guint) (target_height + 0.5);
  target_width_rounded = (guint) (target_width + 0.5);

  photos_debug (PHOTOS_DEBUG_GEGL,
                "GeglBufferCodecPng: Target size rounded: %u×%u",
                target_width_rounded,
                target_height_rounded);

  format_original = photos_gegl_buffer_codec_png_get_format (space, bit_depth, color_type, self->error);
  if (format_original == NULL)
    {
      self->fatal_error_occurred = TRUE;
      goto out;
    }

  format_name = babl_get_name (format_original);
  photos_debug (PHOTOS_DEBUG_GEGL, "GeglBufferCodecPng: Original format: %s", format_name);

  format_components = babl_format_get_n_components (format_original);
  channels = png_get_channels (png_read_ptr, png_info_ptr);
  g_return_if_fail (format_components == (gint) channels);

  bpp_original = babl_format_get_bytes_per_pixel (format_original);
  g_return_if_fail (bpp_original == bit_depth * (gint) channels / 8);

  gegl_rectangle_set (&bbox, 0, 0, (guint) width, (guint) height);
  self->buffer_original = gegl_buffer_new (&bbox, format_original);

  if ((guint) height == target_height_rounded && (guint) width == target_width_rounded)
    {
      self->buffer_zoom_from_original = g_object_ref (self->buffer_original);
      self->buffer = g_object_ref (self->buffer_zoom_from_original);
    }
  else
    {
      const Babl *format_zoom_from_original = NULL;
      gdouble aspect_ratio = (gdouble) width / (gdouble) height;
      gdouble target_aspect_ratio = target_width / target_height;
      gint bpp_zoom_from_original;

      self->zoom_from_original_x = (gdouble) target_width_rounded / (gdouble) width;
      self->zoom_from_original_y = (gdouble) target_height_rounded / (gdouble) height;

      if (G_APPROX_VALUE (aspect_ratio, target_aspect_ratio, PHOTOS_EPSILON))
        {
          gint x;
          gint y;
          guint zoom_from_original_height;
          guint zoom_from_original_width;

          format_zoom_from_original = format_original;

          self->zoom_from_original_x = MAX (self->zoom_from_original_x, self->zoom_from_original_y);
          self->zoom_from_original_y = self->zoom_from_original_x;

          photos_debug (PHOTOS_DEBUG_GEGL,
                        "GeglBufferCodecPng: Keep aspect ratio: Zoom from original: %f×%f",
                        self->zoom_from_original_x,
                        self->zoom_from_original_y);

          zoom_from_original_height = (guint) (self->zoom_from_original_y * (gdouble) height + 0.5);
          zoom_from_original_width = (guint) (self->zoom_from_original_x * (gdouble) width + 0.5);

          photos_debug (PHOTOS_DEBUG_GEGL,
                        "GeglBufferCodecPng: Keep aspect ratio: Zoom from original size: %u×%u",
                        zoom_from_original_width,
                        zoom_from_original_height);

          g_assert_cmpuint (zoom_from_original_height, >=, target_height_rounded);
          g_assert_cmpuint (zoom_from_original_width, >=, target_width_rounded);

          gegl_rectangle_set (&bbox, 0, 0, zoom_from_original_width, zoom_from_original_height);
          self->buffer_zoom_from_original = gegl_buffer_new (&bbox, format_zoom_from_original);

          x = (zoom_from_original_width - target_width_rounded) / 2;
          y = (zoom_from_original_height - target_height_rounded) / 2;
          gegl_rectangle_set (&bbox, x, y, target_width_rounded, target_height_rounded);
          self->buffer = gegl_buffer_create_sub_buffer (self->buffer_zoom_from_original, &bbox);
        }
      else
        {
          format_zoom_from_original = babl_format_with_space ("RaGaBaA float", space);

          gegl_rectangle_set (&bbox, 0, 0, target_width_rounded, target_height_rounded);
          self->buffer_zoom_from_original = gegl_buffer_new (&bbox, format_zoom_from_original);
          self->buffer = g_object_ref (self->buffer_zoom_from_original);

          photos_debug (PHOTOS_DEBUG_GEGL,
                        "GeglBufferCodecPng: Don't keep aspect ratio: Zoom from mipmap: %f×%f",
                        self->zoom_from_original_x,
                        self->zoom_from_original_y);

          photos_gegl_inverse_jacobian_zoom (&self->inverse_jacobian,
                                             self->zoom_from_original_x,
                                             self->zoom_from_original_y);

          photos_debug (PHOTOS_DEBUG_GEGL,
                        "GeglBufferCodecPng: Don't keep aspect ratio: Inverse Jacobian: %f, %f",
                        self->inverse_jacobian.coeff[0][0],
                        self->inverse_jacobian.coeff[0][1]);
          photos_debug (PHOTOS_DEBUG_GEGL,
                        "GeglBufferCodecPng: Don't keep aspect ratio: Inverse Jacobian: %f, %f",
                        self->inverse_jacobian.coeff[1][0],
                        self->inverse_jacobian.coeff[1][1]);

          self->sampler = gegl_buffer_sampler_new (self->buffer_original,
                                                   format_zoom_from_original,
                                                   GEGL_SAMPLER_LINEAR);
        }

      g_return_if_fail (format_zoom_from_original != NULL);
      bpp_zoom_from_original = babl_format_get_bytes_per_pixel (format_zoom_from_original);

      g_object_get (self->buffer_zoom_from_original,
                    "tile-height", &self->tile_height,
                    "tile-width", &self->tile_width,
                    NULL);

      self->tile_stride = bpp_zoom_from_original * self->tile_width;
      self->tile_memory = g_malloc0_n (self->tile_height, self->tile_stride);

      self->original_height_needed_for_zoom = (gint) ((gdouble) self->tile_height
                                                      * (gdouble) height
                                                      / (gdouble) target_height_rounded
                                                      + 0.5);
    }

  g_object_notify (G_OBJECT (self), "buffer");

  /* Row strides are represented as signed integers (ie. gint)
   * in GEGL. Even though the output_width is an unsigned
   * integer (ie. guint or JDIMENSION), it must be ensured
   * that the row stride can fit into the expected signed
   * integer type (ie. gint).
   *
   * The comparison is made using the same unsigned integer
   * type to avoid triggering -Wsign-compare.
   */
  if (bpp_original > 0 && width > 0 && (guint) width > G_MAXINT / (guint) bpp_original)
    {
      g_set_error (self->error,
                   PHOTOS_ERROR,
                   0,
                   _("Overflow calculating row stride: %d×%u"),
                   bpp_original,
                   (guint) width);
      goto out;
    }

  self->stride_original = (guint) bpp_original * (guint) width;
  self->line = g_malloc_n (width, bpp_original);

 out:
  return;
}


static void
photos_gegl_buffer_codec_png_row (png_structp png_read_ptr,
                                  png_bytep new_row,
                                  png_uint_32 row_num,
                                  gint pass_num)
{
  PhotosGeglBufferCodecPng *self;
  const Babl *format_original;
  GeglRectangle bbox_scanline;
  gint buffer_height;
  gint buffer_width;

  self = PHOTOS_GEGL_BUFFER_CODEC_PNG (png_get_progressive_ptr (png_read_ptr));

  if (self->fatal_error_occurred)
    goto out;

  g_return_if_fail (GEGL_IS_BUFFER (self->buffer));
  g_return_if_fail (GEGL_IS_BUFFER (self->buffer_original));
  g_return_if_fail (pass_num >= 0);
  g_return_if_fail (pass_num < self->n_interlacing_passes);

  format_original = gegl_buffer_get_format (self->buffer_original);
  g_return_if_fail (format_original != NULL);

  buffer_height = gegl_buffer_get_height (self->buffer_original);
  g_return_if_fail (buffer_height > 0);

  buffer_width = gegl_buffer_get_width (self->buffer_original);
  g_return_if_fail (buffer_width > 0);

  self->scanline = (gint) row_num;
  if (self->scanline >= buffer_height)
    {
      g_set_error (self->error, PHOTOS_ERROR, 0, _("PNG has too many rows"));
      self->fatal_error_occurred = TRUE;
      goto out;
    }

  gegl_rectangle_set (&bbox_scanline, 0, self->scanline, (guint) buffer_width, 1);

  if (pass_num > 0)
    {
      gegl_buffer_get (self->buffer_original,
                       &bbox_scanline,
                       1.0,
                       format_original,
                       self->line,
                       (gint) self->stride_original,
                       GEGL_ABYSS_NONE);
    }

  png_progressive_combine_row (png_read_ptr, self->line, new_row);
  gegl_buffer_set (self->buffer_original,
                   &bbox_scanline,
                   0,
                   format_original,
                   self->line,
                   (gint) self->stride_original);

  self->pending_zoom_from_original = TRUE;

  self->scanline++;
  if (self->buffer != self->buffer_original && self->scanline % self->original_height_needed_for_zoom == 0)
    {
      photos_gegl_buffer_codec_png_zoom_from_original (self);
      self->pending_zoom_from_original = FALSE;
    }

 out:
  return;
}


static void
photos_gegl_buffer_codec_png_warning (png_structp png_read_ptr, png_const_charp warning_msg)
{
}


static GeglBuffer *
photos_gegl_buffer_codec_png_get_buffer (PhotosGeglBufferCodec *codec)
{
  PhotosGeglBufferCodecPng *self = PHOTOS_GEGL_BUFFER_CODEC_PNG (codec);
  return self->buffer;
}


static gboolean
photos_gegl_buffer_codec_png_load_begin (PhotosGeglBufferCodec *codec, GError **error)
{
  PhotosGeglBufferCodecPng *self = PHOTOS_GEGL_BUFFER_CODEC_PNG (codec);

  g_return_val_if_fail (!self->decoding, FALSE);

  self->error = error;

  self->png_read_ptr = png_create_read_struct (PNG_LIBPNG_VER_STRING,
                                               self,
                                               photos_gegl_buffer_codec_png_error,
                                               photos_gegl_buffer_codec_png_warning);
  if (self->png_read_ptr == NULL)
    {
      /* A failure here isn't supposed to call the error callback, but
       * it doesn't hurt to be careful.
       */
      if (self->error && *self->error == NULL)
        g_set_error (self->error, PHOTOS_ERROR, 0, _("Couldn’t allocate memory for loading PNG"));

      self->decoding = FALSE;
      goto out;
    }

  self->png_info_ptr = png_create_info_struct (self->png_read_ptr);
  if (self->png_info_ptr == NULL)
    {
      /* A failure here isn't supposed to call the error callback, but
       * it doesn't hurt to be careful.
       */
      if (self->error && *self->error == NULL)
        g_set_error (self->error, PHOTOS_ERROR, 0, _("Couldn’t allocate memory for loading PNG"));

      self->decoding = FALSE;
      goto out;
    }

  /* See:
   * https://bugzilla.gnome.org/show_bug.cgi?id=721135
   * https://bugzilla.gnome.org/show_bug.cgi?id=765850
   */
  png_set_benign_errors (self->png_read_ptr, TRUE);
  png_set_option (self->png_read_ptr, PNG_SKIP_sRGB_CHECK_PROFILE, PNG_OPTION_ON);

  if (setjmp (png_jmpbuf (self->png_read_ptr)))
    {
      self->decoding = FALSE;
      goto out;
    }

  png_set_progressive_read_fn (self->png_read_ptr,
                               self,
                               photos_gegl_buffer_codec_png_info,
                               photos_gegl_buffer_codec_png_row,
                               photos_gegl_buffer_codec_png_end);

  self->decoding = TRUE;

 out:
  self->error = NULL;
  return self->decoding;
}


static gboolean
photos_gegl_buffer_codec_png_load_increment (PhotosGeglBufferCodec *codec,
                                             const guchar *buf,
                                             gsize count,
                                             GError **error)
{
  PhotosGeglBufferCodecPng *self = PHOTOS_GEGL_BUFFER_CODEC_PNG (codec);
  gboolean ret_val = FALSE;

  g_return_val_if_fail (self->decoding, FALSE);

  self->error = error;
  if (setjmp (png_jmpbuf (self->png_read_ptr)))
    {
      ret_val = FALSE;
      goto out;
    }

  png_process_data (self->png_read_ptr, self->png_info_ptr, (guchar *) buf, count);
  if (self->fatal_error_occurred)
    {
      ret_val = FALSE;
      goto out;
    }

  ret_val = TRUE;

 out:
  self->error = NULL;
  return ret_val;
}


static gboolean
photos_gegl_buffer_codec_png_load_stop (PhotosGeglBufferCodec *codec, GError **error)
{
  PhotosGeglBufferCodecPng *self = PHOTOS_GEGL_BUFFER_CODEC_PNG (codec);
  gboolean ret_val = FALSE;

  g_return_val_if_fail (self->decoding, FALSE);

  /* FIXME: This thing needs to report errors if we have unused image
   * data.
   */

  if (self->buffer == NULL)
    {
      g_set_error (error, PHOTOS_ERROR, 0, _("Premature end-of-file encountered"));
      goto out;
    }

  ret_val = TRUE;

 out:
  self->decoding = FALSE;
  return ret_val;
}


static void
photos_gegl_buffer_codec_png_dispose (GObject *object)
{
  PhotosGeglBufferCodecPng *self = PHOTOS_GEGL_BUFFER_CODEC_PNG (object);

  g_clear_object (&self->buffer);
  g_clear_object (&self->buffer_original);
  g_clear_object (&self->buffer_zoom_from_original);
  g_clear_object (&self->sampler);

  G_OBJECT_CLASS (photos_gegl_buffer_codec_png_parent_class)->dispose (object);
}


static void
photos_gegl_buffer_codec_png_finalize (GObject *object)
{
  PhotosGeglBufferCodecPng *self = PHOTOS_GEGL_BUFFER_CODEC_PNG (object);

  g_free (self->line);
  g_free (self->tile_memory);
  png_destroy_read_struct (&self->png_read_ptr, &self->png_info_ptr, NULL);

  G_OBJECT_CLASS (photos_gegl_buffer_codec_png_parent_class)->finalize (object);
}


static void
photos_gegl_buffer_codec_png_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
  PhotosGeglBufferCodecPng *self = PHOTOS_GEGL_BUFFER_CODEC_PNG (object);

  switch (prop_id)
    {
    case PROP_BUFFER:
      g_value_set_object (value, self->buffer);
      break;

    case PROP_CAN_SET_SIZE:
      g_value_set_boolean (value, self->can_set_size);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
photos_gegl_buffer_codec_png_init (PhotosGeglBufferCodecPng *self)
{
}


static void
photos_gegl_buffer_codec_png_class_init (PhotosGeglBufferCodecPngClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  PhotosGeglBufferCodecClass *buffer_codec_class = PHOTOS_GEGL_BUFFER_CODEC_CLASS (class);

  buffer_codec_class->mime_types = (GStrv) MIME_TYPES;

  object_class->dispose = photos_gegl_buffer_codec_png_dispose;
  object_class->finalize = photos_gegl_buffer_codec_png_finalize;
  object_class->get_property = photos_gegl_buffer_codec_png_get_property;
  buffer_codec_class->get_buffer = photos_gegl_buffer_codec_png_get_buffer;
  buffer_codec_class->load_begin = photos_gegl_buffer_codec_png_load_begin;
  buffer_codec_class->load_increment = photos_gegl_buffer_codec_png_load_increment;
  buffer_codec_class->load_stop = photos_gegl_buffer_codec_png_load_stop;

  g_object_class_override_property (object_class, PROP_BUFFER, "buffer");
  g_object_class_override_property (object_class, PROP_CAN_SET_SIZE, "can-set-size");
}
