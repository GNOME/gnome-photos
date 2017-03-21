/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2016 – 2017 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */


#include "config.h"

#include <stdio.h>

#include <babl/babl.h>
#include <gegl.h>
#include <png.h>

#include "photos-png-count.h"
#include "photos-operation-png-guess-sizes.h"


struct _PhotosOperationPngGuessSizes
{
  GeglOperationSink parent_instance;
  gboolean background;
  gint bitdepth;
  gint compression;
  gsize sizes[2];
};

enum
{
  PROP_0,
  PROP_BACKGROUND,
  PROP_BITDEPTH,
  PROP_COMPRESSION,
  PROP_SIZE,
  PROP_SIZE_1
};


G_DEFINE_TYPE (PhotosOperationPngGuessSizes, photos_operation_png_guess_sizes, GEGL_TYPE_OPERATION_SINK);


static gsize
photos_operation_png_guess_sizes_count (GeglBuffer *buffer,
                                        gint compression,
                                        gint bitdepth,
                                        gboolean background,
                                        gdouble zoom,
                                        gint src_x,
                                        gint src_y,
                                        gint width,
                                        gint height)
{
  gint bpp;
  gint i;
  gint png_color_type;
  gchar format_string[16];
  const Babl *format;
  const Babl *format_buffer;
  gsize ret_val = 0;
  gsize size;
  guchar *pixels = NULL;
  png_infop info_ptr = NULL;
  png_structp png_ptr = NULL;

  format_buffer = gegl_buffer_get_format (buffer);
  if (babl_format_has_alpha (format_buffer))
    {
      if (babl_format_get_n_components (format_buffer) != 2)
        {
          png_color_type = PNG_COLOR_TYPE_RGB_ALPHA;
          strcpy (format_string, "R'G'B'A ");
        }
      else
        {
          png_color_type = PNG_COLOR_TYPE_GRAY_ALPHA;
          strcpy (format_string, "Y'A ");
        }
    }
  else
    {
      if (babl_format_get_n_components (format_buffer) != 1)
        {
          png_color_type = PNG_COLOR_TYPE_RGB;
          strcpy (format_string, "R'G'B' ");
        }
      else
        {
          png_color_type = PNG_COLOR_TYPE_GRAY;
          strcpy (format_string, "Y' ");
        }
    }

  if (bitdepth == 16)
    strcat (format_string, "u16");
  else
    strcat (format_string, "u8");

  png_ptr = png_create_write_struct (PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  if (png_ptr == NULL)
    goto out;

  info_ptr = png_create_info_struct (png_ptr);
  if (info_ptr == NULL)
    goto out;

  if (setjmp (png_jmpbuf (png_ptr)))
    goto out;

  if (compression >= 0)
    png_set_compression_level (png_ptr, compression);

  photos_png_init_count (png_ptr, &size);

  png_set_IHDR (png_ptr,
                info_ptr,
                width,
                height,
                bitdepth,
                png_color_type,
                PNG_INTERLACE_NONE,
                PNG_COMPRESSION_TYPE_BASE,
                PNG_FILTER_TYPE_DEFAULT);

  if (background)
    {
      png_color_16 white;

      if (png_color_type == PNG_COLOR_TYPE_RGB || png_color_type == PNG_COLOR_TYPE_RGB_ALPHA)
        {
          white.red = 0xff;
          white.blue = 0xff;
          white.green = 0xff;
        }
      else
        {
          white.gray = 0xff;
        }

      png_set_bKGD (png_ptr, info_ptr, &white);
    }

  png_write_info (png_ptr, info_ptr);

#if BYTE_ORDER == LITTLE_ENDIAN
  if (bitdepth > 8)
    png_set_swap (png_ptr);
#endif

  format = babl_format (format_string);
  bpp = babl_format_get_bytes_per_pixel (format);
  pixels = g_malloc0 (width * bpp);

  for (i = 0; i < height; i++)
    {
      GeglRectangle rect;

      rect.x = src_x;
      rect.y = src_y + i;
      rect.width = width;
      rect.height = 1;
      gegl_buffer_get (buffer, &rect, zoom, format, pixels, GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_NONE);
      png_write_rows (png_ptr, &pixels, 1);
    }

  png_write_end (png_ptr, info_ptr);
  ret_val = size;

 out:
  g_free (pixels);
  png_destroy_write_struct (&png_ptr, &info_ptr);
  return ret_val;
}


static gboolean
photos_operation_png_guess_sizes_process (GeglOperation *operation,
                                          GeglBuffer *input,
                                          const GeglRectangle *roi,
                                          gint level)
{
  PhotosOperationPngGuessSizes *self = PHOTOS_OPERATION_PNG_GUESS_SIZES (operation);
  gsize i;

  for (i = 0; i < G_N_ELEMENTS (self->sizes); i++)
    {
      GeglRectangle roi_zoomed;
      gdouble zoom = 1.0 / (gdouble) (1 << i);

      roi_zoomed.height = (gint) (zoom * roi->height + 0.5);
      roi_zoomed.width = (gint) (zoom * roi->width + 0.5);
      roi_zoomed.x = (gint) (zoom * roi->x + 0.5);
      roi_zoomed.y = (gint) (zoom * roi->y + 0.5);

      self->sizes[i] = photos_operation_png_guess_sizes_count (input,
                                                               self->compression,
                                                               self->bitdepth,
                                                               self->background,
                                                               zoom,
                                                               roi_zoomed.x,
                                                               roi_zoomed.y,
                                                               roi_zoomed.width,
                                                               roi_zoomed.height);
    }

  return TRUE;
}


static void
photos_operation_png_guess_sizes_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
  PhotosOperationPngGuessSizes *self = PHOTOS_OPERATION_PNG_GUESS_SIZES (object);

  switch (prop_id)
    {
    case PROP_BACKGROUND:
      g_value_set_boolean (value, self->background);
      break;

    case PROP_BITDEPTH:
      g_value_set_int (value, self->bitdepth);
      break;

    case PROP_COMPRESSION:
      g_value_set_int (value, self->compression);
      break;

    case PROP_SIZE:
      g_value_set_uint64 (value, (guint64) self->sizes[0]);
      break;

    case PROP_SIZE_1:
      g_value_set_uint64 (value, (guint64) self->sizes[1]);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
photos_operation_png_guess_sizes_set_property (GObject *object,
                                               guint prop_id,
                                               const GValue *value,
                                               GParamSpec *pspec)
{
  PhotosOperationPngGuessSizes *self = PHOTOS_OPERATION_PNG_GUESS_SIZES (object);

  switch (prop_id)
    {
    case PROP_BACKGROUND:
      self->background = g_value_get_boolean (value);
      break;

    case PROP_BITDEPTH:
      self->bitdepth = g_value_get_int (value);
      break;

    case PROP_COMPRESSION:
      self->compression = g_value_get_int (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
photos_operation_png_guess_sizes_init (PhotosOperationPngGuessSizes *self)
{
}


static void
photos_operation_png_guess_sizes_class_init (PhotosOperationPngGuessSizesClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GeglOperationClass *operation_class = GEGL_OPERATION_CLASS (class);
  GeglOperationSinkClass *sink_class = GEGL_OPERATION_SINK_CLASS (class);

  operation_class->opencl_support = FALSE;
  sink_class->needs_full = TRUE;

  object_class->get_property = photos_operation_png_guess_sizes_get_property;
  object_class->set_property = photos_operation_png_guess_sizes_set_property;
  sink_class->process = photos_operation_png_guess_sizes_process;

  g_object_class_install_property (object_class,
                                   PROP_BACKGROUND,
                                   g_param_spec_boolean ("background",
                                                         "Background",
                                                         "Set bKGD chunk information",
                                                         TRUE,
                                                         G_PARAM_CONSTRUCT | G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_BITDEPTH,
                                   g_param_spec_int ("bitdepth",
                                                     "Bitdepth",
                                                     "Number of bits per channel — 8 and 16 are the currently "
                                                     "accepted values",
                                                     8,
                                                     16,
                                                     16,
                                                     G_PARAM_CONSTRUCT | G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_COMPRESSION,
                                   g_param_spec_int ("compression",
                                                     "Compression",
                                                     "PNG compression level (between -1 and 9)",
                                                     -1,
                                                     9,
                                                     3,
                                                     G_PARAM_CONSTRUCT | G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_SIZE,
                                   g_param_spec_uint64 ("size",
                                                        "Size (level=0)",
                                                        "Approximate size in bytes after applying PNG compression"
                                                        "at zoom=1.0",
                                                        0,
                                                        G_MAXSIZE,
                                                        0,
                                                        G_PARAM_READABLE));

  g_object_class_install_property (object_class,
                                   PROP_SIZE_1,
                                   g_param_spec_uint64 ("size-1",
                                                        "Size (level=1)",
                                                        "Approximate size in bytes after applying PNG compression"
                                                        "at zoom=0.5",
                                                        0,
                                                        G_MAXSIZE,
                                                        0,
                                                        G_PARAM_READABLE));

  gegl_operation_class_set_keys (operation_class,
                                 "name", "photos:png-guess-sizes",
                                 "title", "PNG Guess Sizes",
                                 "description", "Guesses the size of a GeglBuffer after applying PNG compression",
                                 NULL);
}
