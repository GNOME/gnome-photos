/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 1999 The Free Software Foundation
 * Copyright © 1999 Michael Zucchi
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
#include <stdio.h>
#include <string.h>

#include <gegl.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <jpeglib.h>

#include "photos-debug.h"
#include "photos-error.h"
#include "photos-gegl.h"
#include "photos-gegl-buffer-codec-jpeg.h"


enum
{
  SOURCE_BUF_SIZE = 32768
};

typedef struct _PhotosJpegErrorMgr PhotosJpegErrorMgr;
typedef struct _PhotosJpegSourceMgr PhotosJpegSourceMgr;

struct _PhotosJpegErrorMgr
{
  struct jpeg_error_mgr pub;
  GError **error;
  sigjmp_buf setjmp_buffer;
};

struct _PhotosJpegSourceMgr
{
  struct jpeg_source_mgr pub;
  JOCTET buffer[SOURCE_BUF_SIZE];
  gsize skip_next;
};

struct _PhotosGeglBufferCodecJpeg
{
  PhotosGeglBufferCodec parent_instance;
  GeglBuffer *buffer;
  GeglBuffer *buffer_mipmap;
  GeglBuffer *buffer_zoom_from_mipmap;
  GeglMatrix2 inverse_jacobian;
  GeglSampler *sampler;
  JDIMENSION stride_mipmap;
  JSAMPARRAY lines;
  PhotosJpegErrorMgr jerr;
  PhotosJpegSourceMgr src;
  gboolean can_set_size;
  gboolean decoding;
  gboolean did_prescan;
  gboolean got_header;
  gboolean in_output;
  gboolean pending_zoom_from_mipmap;
  gboolean src_initialized;
  gdouble zoom_from_mipmap_x;
  gdouble zoom_from_mipmap_y;
  gint mipmap_height_needed_for_zoom;
  gint scanline;
  gint tile_height;
  gint tile_stride;
  gint tile_width;
  gpointer tile_memory;
  struct jpeg_decompress_struct cinfo;
};

enum
{
  PROP_0,
  PROP_BUFFER,
  PROP_CAN_SET_SIZE
};


G_DEFINE_TYPE_WITH_CODE (PhotosGeglBufferCodecJpeg, photos_gegl_buffer_codec_jpeg, PHOTOS_TYPE_GEGL_BUFFER_CODEC,
                         photos_gegl_ensure_extension_points ();
                         g_io_extension_point_implement (PHOTOS_GEGL_BUFFER_CODEC_EXTENSION_POINT_NAME,
                                                         g_define_type_id,
                                                         "jpeg",
                                                         0));


static const gchar *MIME_TYPES[] =
{
  "image/jpeg",
  NULL
};


static const gchar *
photos_gegl_buffer_codec_jpeg_get_colorspace_name (J_COLOR_SPACE jpeg_color_space)
{
  const gchar *ret_val = NULL;

  switch (jpeg_color_space)
    {
    case JCS_UNKNOWN:
      ret_val = "UNKNOWN";
      break;

    case JCS_CMYK:
      ret_val = "CMYK";
      break;

    case JCS_GRAYSCALE:
      ret_val = "GRAYSCALE";
      break;

    case JCS_RGB:
      ret_val = "RGB";
      break;

    case JCS_YCbCr:
      ret_val = "YCbCr";
      break;

    case JCS_YCCK:
      ret_val = "YCCK";
      break;

    case JCS_EXT_ARGB:
    case JCS_EXT_ABGR:
    case JCS_EXT_BGR:
    case JCS_EXT_BGRA:
    case JCS_EXT_BGRX:
    case JCS_EXT_RGB:
    case JCS_EXT_RGBA:
    case JCS_EXT_RGBX:
    case JCS_EXT_XBGR:
    case JCS_EXT_XRGB:
    case JCS_RGB565:
    default:
      ret_val = "invalid";
      break;
    }

  return ret_val;
}


static const Babl *
photos_gegl_buffer_codec_jpeg_get_format (J_COLOR_SPACE jpeg_color_space,
                                          JOCTET *icc_profile,
                                          guint icc_profile_len,
                                          GError **error)
{
  const Babl *format = NULL;
  const Babl *ret_val = NULL;
  const Babl *space = NULL;
  const Babl *space_sRGB;
  const Babl *type;
  const Babl *type_u8;

  space_sRGB = babl_space ("sRGB");
  type_u8 = babl_type ("u8");

  if (icc_profile == NULL)
    {
      space = space_sRGB;
    }
  else
    {
      const gchar *error_message = NULL;

      space = babl_space_from_icc ((gchar *) icc_profile,
                                   (gint) icc_profile_len,
                                   BABL_ICC_INTENT_RELATIVE_COLORIMETRIC,
                                   &error_message);
      if (space == NULL)
        {
          g_warning ("Unable to create Babl space from ICC profile: %s", error_message);
          space = space_sRGB;
        }
    }

  switch (jpeg_color_space)
    {
    case JCS_CMYK:
      format = babl_format_with_space ("CMYK u8", space);
      break;

    case JCS_GRAYSCALE:
      format = babl_format_with_space ("Y' u8", space);
      break;

    case JCS_RGB:
      format = babl_format_with_space ("R'G'B' u8", space);
      break;

    case JCS_YCbCr:
    case JCS_YCCK:
    case JCS_EXT_ARGB:
    case JCS_EXT_ABGR:
    case JCS_EXT_BGR:
    case JCS_EXT_BGRA:
    case JCS_EXT_BGRX:
    case JCS_EXT_RGB:
    case JCS_EXT_RGBA:
    case JCS_EXT_RGBX:
    case JCS_EXT_XBGR:
    case JCS_EXT_XRGB:
    case JCS_RGB565:
    case JCS_UNKNOWN:
    default:
      {
        const gchar *colorspace_name;

        colorspace_name = photos_gegl_buffer_codec_jpeg_get_colorspace_name (jpeg_color_space);
        g_set_error (error, PHOTOS_ERROR, 0, _("Unsupported JPEG color space: %s"), colorspace_name);
        goto out;
        break;
      }
    }

  type = babl_format_get_type (format, 0);
  g_return_val_if_fail (type == type_u8, NULL);

  ret_val = format;

 out:
  return ret_val;
}


static void
photos_gegl_buffer_codec_jpeg_zoom_from_mipmap (PhotosGeglBufferCodecJpeg *self)
{
  const Babl *format_mipmap;
  const Babl *format_zoom_from_mipmap;
  GeglRectangle bbox_tile;
  gint buffer_width;
  gint tile_row;

  g_return_if_fail (self->buffer != self->buffer_mipmap);
  g_return_if_fail (self->buffer_zoom_from_mipmap != self->buffer_mipmap);
  g_return_if_fail (self->pending_zoom_from_mipmap);
  g_return_if_fail (self->tile_memory != NULL);

  format_mipmap = gegl_buffer_get_format (self->buffer_mipmap);
  g_return_if_fail (format_mipmap != NULL);

  format_zoom_from_mipmap = gegl_buffer_get_format (self->buffer_zoom_from_mipmap);

  bbox_tile.height = self->tile_height;
  bbox_tile.width = self->tile_width;
  bbox_tile.x = 0;

  tile_row = (self->scanline - 1) / self->mipmap_height_needed_for_zoom;
  bbox_tile.y = tile_row * self->tile_height;

  buffer_width = gegl_buffer_get_width (self->buffer_zoom_from_mipmap);

  if (G_APPROX_VALUE (self->zoom_from_mipmap_x, self->zoom_from_mipmap_y, PHOTOS_EPSILON))
    {
      g_return_if_fail (format_mipmap == format_zoom_from_mipmap);

      for (bbox_tile.x = 0; bbox_tile.x < buffer_width; bbox_tile.x += self->tile_width)
        {
          gegl_buffer_get (self->buffer_mipmap,
                           &bbox_tile,
                           self->zoom_from_mipmap_x,
                           format_mipmap,
                           self->tile_memory,
                           self->tile_stride,
                           GEGL_ABYSS_NONE | GEGL_BUFFER_FILTER_AUTO);

          gegl_buffer_set (self->buffer_zoom_from_mipmap,
                           &bbox_tile,
                           0,
                           format_mipmap,
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
      format_components = babl_format_get_n_components (format_zoom_from_mipmap);

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

          gegl_buffer_set (self->buffer_zoom_from_mipmap,
                           &bbox_tile,
                           0,
                           format_zoom_from_mipmap,
                           self->tile_memory,
                           self->tile_stride);
        }
    }
}


static gboolean
photos_gegl_buffer_codec_jpeg_load_lines (PhotosGeglBufferCodecJpeg *self, GError **error)
{
  const Babl *cmyk_u8;
  const Babl *format_mipmap;
  GeglRectangle bbox_scanline;

  cmyk_u8 = babl_format ("CMYK u8");
  format_mipmap = gegl_buffer_get_format (self->buffer_mipmap);

  gegl_rectangle_set (&bbox_scanline, 0, self->scanline, (guint) self->cinfo.output_width, 1);

  while (self->cinfo.output_scanline < self->cinfo.output_height)
    {
      JDIMENSION i;
      JDIMENSION num_lines;

      num_lines = jpeg_read_scanlines (&self->cinfo, self->lines, self->cinfo.rec_outbuf_height);
      if (num_lines == 0)
        break;

      for (i = 0; i < num_lines; i++)
        {
          /* Assume that all CMYK JPEG files use inverted CMYK, as
           * Photoshop does. See:
           * https://bugzilla.gnome.org/show_bug.cgi?id=618096
           * https://bugzilla.mozilla.org/show_bug.cgi?id=674619
           */
          if (format_mipmap == cmyk_u8)
            {
              JDIMENSION j;

              for (j = 0; j < self->stride_mipmap; j++)
                self->lines[i][j] = 255 - self->lines[i][j];
            }

          gegl_buffer_set (self->buffer_mipmap,
                           &bbox_scanline,
                           0,
                           format_mipmap,
                           self->lines[i],
                           (gint) self->stride_mipmap);

          self->pending_zoom_from_mipmap = TRUE;

          self->scanline++;
          bbox_scanline.y = self->scanline;

          if (self->buffer != self->buffer_mipmap && self->scanline % self->mipmap_height_needed_for_zoom == 0)
            {
              photos_gegl_buffer_codec_jpeg_zoom_from_mipmap (self);
              self->pending_zoom_from_mipmap = FALSE;
            }
        }
    }

  if (self->buffer != self->buffer_mipmap
      && self->cinfo.output_scanline >= self->cinfo.output_height
      && self->pending_zoom_from_mipmap)
    {
      photos_gegl_buffer_codec_jpeg_zoom_from_mipmap (self);
      self->pending_zoom_from_mipmap = FALSE;
    }

  return TRUE;
}


static void
photos_gegl_buffer_codec_jpeg_parse_exif_app1 (PhotosGeglBufferCodecJpeg *self, jpeg_saved_marker_ptr marker)
{
}


static void
photos_gegl_buffer_codec_jpeg_parse_exif (PhotosGeglBufferCodecJpeg *self)
{
  jpeg_saved_marker_ptr marker;

  for (marker = self->cinfo.marker_list; marker != NULL; marker = marker->next)
    {
      if (marker->marker == JPEG_APP0 + 1)
        photos_gegl_buffer_codec_jpeg_parse_exif_app1 (self, marker);
    }
}


static void
photos_gegl_buffer_codec_jpeg_error_exit (j_common_ptr cinfo)
{
  PhotosJpegErrorMgr *jerr = (PhotosJpegErrorMgr *) cinfo->err;
  gchar buffer[JMSG_LENGTH_MAX];

  cinfo->err->format_message (cinfo, buffer);

  /* Check for *error == NULL for robustness against crappy JPEG
   * library.
   */
  if (jerr->error && *jerr->error == NULL)
    g_set_error (jerr->error, PHOTOS_ERROR, 0, _("Error reading JPEG image: %s"), buffer);

  siglongjmp (jerr->setjmp_buffer, 1);
  g_assert_not_reached ();
}


static gboolean
photos_gegl_buffer_codec_jpeg_fill_input_buffer (j_decompress_ptr cinfo)
{
  return FALSE;
}


static void
photos_gegl_buffer_codec_jpeg_init_source (j_decompress_ptr cinfo)
{
  PhotosJpegSourceMgr *src = (PhotosJpegSourceMgr *) cinfo->src;
  src->skip_next = 0;
}


static void
photos_gegl_buffer_codec_jpeg_output_message (j_common_ptr cinfo)
{
}


static void
photos_gegl_buffer_codec_jpeg_skip_input_data (j_decompress_ptr cinfo, glong num_bytes)
{
  PhotosJpegSourceMgr *src = (PhotosJpegSourceMgr *) cinfo->src;
  glong num_can_do;

  if (num_bytes > 0)
    {
      num_can_do = MIN (src->pub.bytes_in_buffer, (gsize) num_bytes);
      src->pub.next_input_byte += (gsize) num_can_do;
      src->pub.bytes_in_buffer -= (gsize) num_can_do;
      src->skip_next = (gsize) (num_bytes - num_can_do);
    }
}


static void
photos_gegl_buffer_codec_jpeg_term_source (j_decompress_ptr cinfo)
{
}


static GeglBuffer *
photos_gegl_buffer_codec_jpeg_get_buffer (PhotosGeglBufferCodec *codec)
{
  PhotosGeglBufferCodecJpeg *self = PHOTOS_GEGL_BUFFER_CODEC_JPEG (codec);
  return self->buffer;
}


static gboolean
photos_gegl_buffer_codec_jpeg_load_begin (PhotosGeglBufferCodec *codec, GError **error)
{
  PhotosGeglBufferCodecJpeg *self = PHOTOS_GEGL_BUFFER_CODEC_JPEG (codec);

  g_return_val_if_fail (!self->decoding, FALSE);

  self->cinfo.err = jpeg_std_error (&self->jerr.pub);
  self->jerr.pub.error_exit = photos_gegl_buffer_codec_jpeg_error_exit;
  self->jerr.pub.output_message = photos_gegl_buffer_codec_jpeg_output_message;

  self->jerr.error = error;
  if (sigsetjmp (self->jerr.setjmp_buffer, 1))
    {
      jpeg_destroy_decompress (&self->cinfo);
      self->decoding = FALSE;
      goto out;
    }

  jpeg_create_decompress (&self->cinfo);
  self->decoding = TRUE;

  self->cinfo.src = (struct jpeg_source_mgr *) &self->src;
  self->src.pub.init_source = photos_gegl_buffer_codec_jpeg_init_source;
  self->src.pub.fill_input_buffer = photos_gegl_buffer_codec_jpeg_fill_input_buffer;
  self->src.pub.skip_input_data = photos_gegl_buffer_codec_jpeg_skip_input_data;
  self->src.pub.resync_to_restart = jpeg_resync_to_restart;
  self->src.pub.term_source = photos_gegl_buffer_codec_jpeg_term_source;

  g_return_val_if_fail (self->decoding, FALSE);

 out:
  self->jerr.error = NULL;
  return self->decoding;
}


static gboolean
photos_gegl_buffer_codec_jpeg_load_increment (PhotosGeglBufferCodec *codec,
                                              const guchar *buf,
                                              gsize count,
                                              GError **error)
{
  PhotosGeglBufferCodecJpeg *self = PHOTOS_GEGL_BUFFER_CODEC_JPEG (codec);
  gboolean first;
  gboolean ret_val = FALSE;
  gsize last_bytes_left;
  gsize last_num_left;
  gsize num_left;
  guint spinguard;
  const guchar *bufhd;

  g_return_val_if_fail (self->decoding, FALSE);

  self->jerr.error = error;
  if (sigsetjmp (self->jerr.setjmp_buffer, 1))
    {
      ret_val = FALSE;
      goto out;
    }

  if (self->src_initialized && self->src.skip_next > 0)
    {
      if (self->src.skip_next > count)
        {
          self->src.skip_next -= count;
          ret_val = TRUE;
          goto out;
        }

      num_left = count - self->src.skip_next;
      bufhd = buf + self->src.skip_next;
      self->src.skip_next = 0;
    }
  else
    {
      num_left = count;
      bufhd = buf;
    }

  if (num_left == 0)
    {
      ret_val = TRUE;
      goto out;
    }

  last_num_left = num_left;
  last_bytes_left = 0;
  spinguard = 0;
  first = TRUE;

  while (TRUE)
    {
      if (num_left > 0)
        {
          gsize num_copy;

          if (self->src.pub.bytes_in_buffer > 0 && self->src.pub.next_input_byte != self->src.buffer)
            memmove (self->src.buffer, self->src.pub.next_input_byte, self->src.pub.bytes_in_buffer);

          num_copy = MIN (SOURCE_BUF_SIZE - self->src.pub.bytes_in_buffer, num_left);
          memcpy (self->src.buffer + self->src.pub.bytes_in_buffer, bufhd, num_copy);
          self->src.pub.next_input_byte = self->src.buffer;
          self->src.pub.bytes_in_buffer += num_copy;
          bufhd += num_copy;
          num_left -= num_copy;
        }

      if (first)
        {
          last_bytes_left = self->src.pub.bytes_in_buffer;
          first = FALSE;
        }
      else if (self->src.pub.bytes_in_buffer == last_bytes_left && num_left == last_num_left)
        {
          spinguard++;
        }
      else
        {
          last_bytes_left = self->src.pub.bytes_in_buffer;
          last_num_left = num_left;
        }

      if (spinguard > 2)
        {
          ret_val = TRUE;
          goto out;
        }

      if (!self->got_header)
        {
          const Babl *format_mipmap = NULL;
          GeglRectangle bbox;
          g_autofree JOCTET *icc_profile = NULL;
          const gchar *format_name;
          gdouble target_height;
          gdouble target_width;
          gint bpp_mipmap;
          gint format_components;
          gint rc;
          guint icc_profile_len;
          guint target_height_rounded;
          guint target_width_rounded;

          jpeg_save_markers (&self->cinfo, JPEG_APP0 + 1, 0xffff);
          jpeg_save_markers (&self->cinfo, JPEG_APP0 + 2, 0xffff);
          jpeg_save_markers (&self->cinfo, JPEG_COM, 0xffff);
          rc = jpeg_read_header (&self->cinfo, TRUE);
          self->src_initialized = TRUE;

          if (rc == JPEG_SUSPENDED)
            continue;

          self->got_header = TRUE;

          photos_gegl_buffer_codec_jpeg_parse_exif (self);

          photos_debug (PHOTOS_DEBUG_GEGL,
                        "GeglBufferCodecJpeg: Original size: %u×%u",
                        (guint) self->cinfo.image_width,
                        (guint) self->cinfo.image_height);

          if (self->cinfo.image_height == 0 || self->cinfo.image_width == 0)
            {
              g_set_error (error, PHOTOS_ERROR, 0, _("Original JPEG has zero width or height"));
              ret_val = FALSE;
              goto out;
            }

          self->can_set_size = TRUE;
          g_signal_emit_by_name (self,
                                 "size-prepared",
                                 (guint) self->cinfo.image_width,
                                 (guint) self->cinfo.image_height);
          self->can_set_size = FALSE;

          target_height = photos_gegl_buffer_codec_get_height (PHOTOS_GEGL_BUFFER_CODEC (self));
          target_width = photos_gegl_buffer_codec_get_width (PHOTOS_GEGL_BUFFER_CODEC (self));

          photos_debug (PHOTOS_DEBUG_GEGL,
                        "GeglBufferCodecJpeg: Target size: %f×%f",
                        target_width,
                        target_height);

          if (target_height <= 0 || target_width <= 0)
            {
              g_set_error (error, PHOTOS_ERROR, 0, _("Transformed JPEG has invalid width or height"));
              ret_val = FALSE;
              goto out;
            }

          target_height_rounded = (guint) (target_height + 0.5);
          target_width_rounded = (guint) (target_width + 0.5);

          photos_debug (PHOTOS_DEBUG_GEGL,
                        "GeglBufferCodecJpeg: Target size rounded: %u×%u",
                        target_width_rounded,
                        target_height_rounded);

          if (target_height_rounded == 0 || target_width_rounded == 0)
            {
              g_set_error (error, PHOTOS_ERROR, 0, _("Transformed JPEG has zero width or height"));
              ret_val = FALSE;
              goto out;
            }

          self->cinfo.scale_num = 1;
          for (self->cinfo.scale_denom = 2; self->cinfo.scale_denom <= 8; self->cinfo.scale_denom *= 2)
            {
              jpeg_calc_output_dimensions (&self->cinfo);
              if ((guint) self->cinfo.output_height < target_height_rounded
                  || (guint) self->cinfo.output_width < target_width_rounded)
                {
                  self->cinfo.scale_denom /= 2;
                  break;
                }
            }

          jpeg_calc_output_dimensions (&self->cinfo);
          photos_debug (PHOTOS_DEBUG_GEGL,
                        "GeglBufferCodecJpeg: Mipmap size: %u×%u",
                        (guint) self->cinfo.output_width,
                        (guint) self->cinfo.output_height);

          jpeg_read_icc_profile (&self->cinfo, &icc_profile, &icc_profile_len);
          format_mipmap = photos_gegl_buffer_codec_jpeg_get_format (self->cinfo.out_color_space,
                                                                    icc_profile,
                                                                    icc_profile_len,
                                                                    error);
          if (format_mipmap == NULL)
            goto out;

          format_name = babl_get_name (format_mipmap);
          photos_debug (PHOTOS_DEBUG_GEGL, "GeglBufferCodecJpeg: Original format: %s", format_name);

          format_components = babl_format_get_n_components (format_mipmap);
          g_assert_cmpint (format_components, ==, self->cinfo.output_components);

          bpp_mipmap = babl_format_get_bytes_per_pixel (format_mipmap);
          g_assert_cmpint (bpp_mipmap, ==, self->cinfo.output_components);

          g_assert_null (self->buffer);
          g_assert_null (self->buffer_mipmap);
          g_assert_null (self->buffer_zoom_from_mipmap);

          gegl_rectangle_set (&bbox, 0, 0, (guint) self->cinfo.output_width, (guint) self->cinfo.output_height);
          self->buffer_mipmap = gegl_buffer_new (&bbox, format_mipmap);

          if ((guint) self->cinfo.output_height == target_height_rounded
              && (guint) self->cinfo.output_width == target_width_rounded)
            {
              self->buffer_zoom_from_mipmap = g_object_ref (self->buffer_mipmap);
              self->buffer = g_object_ref (self->buffer_zoom_from_mipmap);
            }
          else
            {
              const Babl *format_zoom_from_mipmap = NULL;
              gdouble aspect_ratio = (gdouble) self->cinfo.image_width / (gdouble) self->cinfo.image_height;
              gdouble target_aspect_ratio = target_width / target_height;
              gint bpp_zoom_from_mipmap;

              self->zoom_from_mipmap_x = (gdouble) target_width_rounded / (gdouble) self->cinfo.output_width;
              self->zoom_from_mipmap_y = (gdouble) target_height_rounded / (gdouble) self->cinfo.output_height;

              if (G_APPROX_VALUE (aspect_ratio, target_aspect_ratio, PHOTOS_EPSILON))
                {
                  gint x;
                  gint y;
                  guint zoom_from_mipmap_height;
                  guint zoom_from_mipmap_width;

                  format_zoom_from_mipmap = format_mipmap;

                  self->zoom_from_mipmap_x = MAX (self->zoom_from_mipmap_x, self->zoom_from_mipmap_y);
                  self->zoom_from_mipmap_y = self->zoom_from_mipmap_x;

                  photos_debug (PHOTOS_DEBUG_GEGL,
                                "GeglBufferCodecJpeg: Keep aspect ratio: Zoom from mipmap: %f×%f",
                                self->zoom_from_mipmap_x,
                                self->zoom_from_mipmap_y);

                  zoom_from_mipmap_height = (guint) (self->zoom_from_mipmap_y * (gdouble) self->cinfo.output_height
                                                     + 0.5);
                  zoom_from_mipmap_width = (guint) (self->zoom_from_mipmap_x * (gdouble) self->cinfo.output_width
                                                    + 0.5);

                  photos_debug (PHOTOS_DEBUG_GEGL,
                                "GeglBufferCodecJpeg: Keep aspect ratio: Zoom from mipmap size: %u×%u",
                                zoom_from_mipmap_width,
                                zoom_from_mipmap_height);

                  g_assert_cmpuint (zoom_from_mipmap_height, >=, target_height_rounded);
                  g_assert_cmpuint (zoom_from_mipmap_width, >=, target_width_rounded);

                  gegl_rectangle_set (&bbox, 0, 0, zoom_from_mipmap_width, zoom_from_mipmap_height);
                  self->buffer_zoom_from_mipmap = gegl_buffer_new (&bbox, format_zoom_from_mipmap);

                  x = (zoom_from_mipmap_width - target_width_rounded) / 2;
                  y = (zoom_from_mipmap_height - target_height_rounded) / 2;
                  gegl_rectangle_set (&bbox, x, y, target_width_rounded, target_height_rounded);
                  self->buffer = gegl_buffer_create_sub_buffer (self->buffer_zoom_from_mipmap, &bbox);
                }
              else
                {
                  const Babl *space;

                  space = babl_format_get_space (format_mipmap);
                  format_zoom_from_mipmap = babl_format_with_space ("RaGaBaA float", space);

                  gegl_rectangle_set (&bbox, 0, 0, target_width_rounded, target_height_rounded);
                  self->buffer_zoom_from_mipmap = gegl_buffer_new (&bbox, format_zoom_from_mipmap);
                  self->buffer = g_object_ref (self->buffer_zoom_from_mipmap);

                  photos_debug (PHOTOS_DEBUG_GEGL,
                                "GeglBufferCodecJpeg: Don't keep aspect ratio: Zoom from mipmap: %f×%f",
                                self->zoom_from_mipmap_x,
                                self->zoom_from_mipmap_y);

                  photos_gegl_inverse_jacobian_zoom (&self->inverse_jacobian,
                                                     self->zoom_from_mipmap_x,
                                                     self->zoom_from_mipmap_y);

                  photos_debug (PHOTOS_DEBUG_GEGL,
                                "GeglBufferCodecJpeg: Don't keep aspect ratio: Inverse Jacobian: %f, %f",
                                self->inverse_jacobian.coeff[0][0],
                                self->inverse_jacobian.coeff[0][1]);
                  photos_debug (PHOTOS_DEBUG_GEGL,
                                "GeglBufferCodecJpeg: Don't keep aspect ratio: Inverse Jacobian: %f, %f",
                                self->inverse_jacobian.coeff[1][0],
                                self->inverse_jacobian.coeff[1][1]);

                  self->sampler = gegl_buffer_sampler_new (self->buffer_mipmap,
                                                           format_zoom_from_mipmap,
                                                           GEGL_SAMPLER_LINEAR);
                }

              g_assert_nonnull (format_zoom_from_mipmap);
              bpp_zoom_from_mipmap = babl_format_get_bytes_per_pixel (format_zoom_from_mipmap);

              g_object_get (self->buffer_zoom_from_mipmap,
                            "tile-height", &self->tile_height,
                            "tile-width", &self->tile_width,
                            NULL);

              self->tile_stride = bpp_zoom_from_mipmap * self->tile_width;
              self->tile_memory = g_malloc0_n (self->tile_height, self->tile_stride);

              self->mipmap_height_needed_for_zoom = (gint) ((gdouble) self->tile_height
                                                            * (gdouble) self->cinfo.output_height
                                                            / (gdouble) target_height_rounded
                                                            + 0.5);
            }

          g_object_notify (G_OBJECT (self), "buffer");

          g_assert_null (self->lines);

          /* Row strides are represented as signed integers (ie. gint)
           * in GEGL. Even though the output_width is an unsigned
           * integer (ie. guint or JDIMENSION), it must be ensured
           * that the row stride can fit into the expected signed
           * integer type (ie. gint).
           *
           * The comparison is made using the same unsigned integer
           * type to avoid triggering -Wsign-compare.
           *
           * Note that product of bpp and output_width should be less
           * than G_MAXINT because an odd-valued product gets
           * incremented by one, and G_MAXINT is odd.
           */
          if (bpp_mipmap > 0
              && self->cinfo.output_width > 0
              && (guint) self->cinfo.output_width >= G_MAXINT / (guint) bpp_mipmap)
            {
              g_set_error (error,
                           PHOTOS_ERROR,
                           0,
                           _("Overflow calculating row stride: %d×%u"),
                           bpp_mipmap,
                           (guint) self->cinfo.output_width);
              goto out;
            }

          self->stride_mipmap = (JDIMENSION) bpp_mipmap * self->cinfo.output_width;
          if (self->stride_mipmap % 2 == 1)
            self->stride_mipmap++;

          self->lines = self->cinfo.mem->alloc_sarray ((j_common_ptr) &self->cinfo,
                                                       JPOOL_IMAGE,
                                                       self->stride_mipmap,
                                                       (JDIMENSION) self->cinfo.rec_outbuf_height);

          self->scanline = 0;
        }
      else if (!self->did_prescan)
        {
          gint rc;

          self->cinfo.buffered_image = self->cinfo.progressive_mode;
          rc = jpeg_start_decompress (&self->cinfo);
          self->cinfo.do_fancy_upsampling = FALSE;
          self->cinfo.do_block_smoothing = FALSE;

          if (rc == JPEG_SUSPENDED)
            continue;

          self->did_prescan = TRUE;
        }
      else if (!self->cinfo.buffered_image)
        {
          if (!photos_gegl_buffer_codec_jpeg_load_lines (self, error))
            goto out;

          if (self->cinfo.output_scanline >= self->cinfo.output_height)
            {
              ret_val = TRUE;
              goto out;
            }
        }
      else
        {
          while (!jpeg_input_complete (&self->cinfo))
            {
              if (!self->in_output)
                {
                  if (jpeg_start_output (&self->cinfo, self->cinfo.input_scan_number))
                    {
                      self->in_output = TRUE;
                      self->scanline = 0;
                    }
                  else
                    {
                      break;
                    }
                }

              if (!photos_gegl_buffer_codec_jpeg_load_lines (self, error))
                goto out;

              if (self->cinfo.output_scanline >= self->cinfo.output_height && jpeg_finish_output (&self->cinfo))
                self->in_output = FALSE;
              else
                break;
            }

          if (jpeg_input_complete (&self->cinfo))
            {
              ret_val = TRUE;
              goto out;
            }
          else
            {
              continue;
            }
        }
    }

 out:
  self->jerr.error = NULL;
  return ret_val;
}


static gboolean
photos_gegl_buffer_codec_jpeg_load_stop (PhotosGeglBufferCodec *codec, GError **error)
{
  PhotosGeglBufferCodecJpeg *self = PHOTOS_GEGL_BUFFER_CODEC_JPEG (codec);
  gboolean ret_val = FALSE;

  g_return_val_if_fail (self->decoding, FALSE);

  self->jerr.error = error;
  if (!sigsetjmp (self->jerr.setjmp_buffer, 1))
    {
      if (self->buffer != NULL && self->cinfo.output_scanline < self->cinfo.output_height)
        {
          if (self->src.skip_next < sizeof (self->src.buffer) - 2)
            {
              self->src.buffer[self->src.skip_next] = (JOCTET) 0xFF;
              self->src.buffer[self->src.skip_next + 1] = (JOCTET) JPEG_EOI;
              self->src.pub.next_input_byte = &self->src.buffer[self->src.skip_next];
              self->src.pub.bytes_in_buffer = 2;

              photos_gegl_buffer_codec_jpeg_load_lines (self, NULL);
            }
        }
    }

  /* FIXME: This thing needs to report errors if we have unused image
   * data.
   */

  self->jerr.error = error;
  if (sigsetjmp (self->jerr.setjmp_buffer, 1))
    {
      ret_val = FALSE;
      goto out;
    }

  jpeg_finish_decompress (&self->cinfo);
  ret_val = TRUE;

 out:
  jpeg_destroy_decompress (&self->cinfo);
  self->decoding = FALSE;
  self->jerr.error = NULL;
  return ret_val;
}


static void
photos_gegl_buffer_codec_jpeg_dispose (GObject *object)
{
  PhotosGeglBufferCodecJpeg *self = PHOTOS_GEGL_BUFFER_CODEC_JPEG (object);

  g_clear_object (&self->buffer);
  g_clear_object (&self->buffer_mipmap);
  g_clear_object (&self->buffer_zoom_from_mipmap);
  g_clear_object (&self->sampler);

  G_OBJECT_CLASS (photos_gegl_buffer_codec_jpeg_parent_class)->dispose (object);
}


static void
photos_gegl_buffer_codec_jpeg_finalize (GObject *object)
{
  PhotosGeglBufferCodecJpeg *self = PHOTOS_GEGL_BUFFER_CODEC_JPEG (object);

  if (self->decoding)
    jpeg_destroy_decompress (&self->cinfo);

  g_free (self->tile_memory);

  G_OBJECT_CLASS (photos_gegl_buffer_codec_jpeg_parent_class)->finalize (object);
}


static void
photos_gegl_buffer_codec_jpeg_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
  PhotosGeglBufferCodecJpeg *self = PHOTOS_GEGL_BUFFER_CODEC_JPEG (object);

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
photos_gegl_buffer_codec_jpeg_init (PhotosGeglBufferCodecJpeg *self)
{
}


static void
photos_gegl_buffer_codec_jpeg_class_init (PhotosGeglBufferCodecJpegClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  PhotosGeglBufferCodecClass *buffer_codec_class = PHOTOS_GEGL_BUFFER_CODEC_CLASS (class);

  buffer_codec_class->mime_types = (GStrv) MIME_TYPES;

  object_class->dispose = photos_gegl_buffer_codec_jpeg_dispose;
  object_class->finalize = photos_gegl_buffer_codec_jpeg_finalize;
  object_class->get_property = photos_gegl_buffer_codec_jpeg_get_property;
  buffer_codec_class->get_buffer = photos_gegl_buffer_codec_jpeg_get_buffer;
  buffer_codec_class->load_begin = photos_gegl_buffer_codec_jpeg_load_begin;
  buffer_codec_class->load_increment = photos_gegl_buffer_codec_jpeg_load_increment;
  buffer_codec_class->load_stop = photos_gegl_buffer_codec_jpeg_load_stop;

  g_object_class_override_property (object_class, PROP_BUFFER, "buffer");
  g_object_class_override_property (object_class, PROP_CAN_SET_SIZE, "can-set-size");
}
