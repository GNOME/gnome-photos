/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2015 – 2017 Red Hat, Inc.
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
#include <jpeglib.h>

#include "photos-jpeg-count.h"
#include "photos-operation-jpg-guess-sizes.h"


struct _PhotosOperationJpgGuessSizes
{
  GeglOperationSink parent_instance;
  gboolean optimize;
  gboolean progressive;
  gboolean sampling;
  gint quality;
  gsize sizes[2];
};

enum
{
  PROP_0,
  PROP_OPTIMIZE,
  PROP_PROGRESSIVE,
  PROP_QUALITY,
  PROP_SAMPLING,
  PROP_SIZE,
  PROP_SIZE_1
};


G_DEFINE_TYPE (PhotosOperationJpgGuessSizes, photos_operation_jpg_guess_sizes, GEGL_TYPE_OPERATION_SINK);


static gsize
photos_operation_jpg_guess_sizes_count (GeglBuffer *buffer,
                                        gint quality,
                                        gint smoothing,
                                        gboolean optimize,
                                        gboolean progressive,
                                        gboolean sampling,
                                        gboolean grayscale,
                                        gdouble zoom,
                                        gint src_x,
                                        gint src_y,
                                        gint width,
                                        gint height)
{
  struct jpeg_compress_struct cinfo;
  struct jpeg_error_mgr jerr;
  JSAMPROW row_pointer[1];
  const Babl *format;
  gint bpp;
  gsize size;

  cinfo.err = jpeg_std_error (&jerr);
  jpeg_create_compress (&cinfo);

  photos_jpeg_count_dest (&cinfo, &size);

  cinfo.image_width = width;
  cinfo.image_height = height;

  if (!grayscale)
    {
      cinfo.input_components = 3;
      cinfo.in_color_space = JCS_RGB;
      format = babl_format ("R'G'B' u8");
    }
  else
    {
      cinfo.input_components = 1;
      cinfo.in_color_space = JCS_GRAYSCALE;
      format = babl_format ("Y' u8");
    }

  jpeg_set_defaults (&cinfo);
  jpeg_set_quality (&cinfo, quality, TRUE);
  cinfo.smoothing_factor = smoothing;
  cinfo.optimize_coding = optimize;
  if (progressive)
    jpeg_simple_progression (&cinfo);

  if (!sampling)
    {
      /* Use 1x1,1x1,1x1 MCUs and no subsampling */
      cinfo.comp_info[0].h_samp_factor = 1;
      cinfo.comp_info[0].v_samp_factor = 1;

      if (!grayscale)
        {
          cinfo.comp_info[1].h_samp_factor = 1;
          cinfo.comp_info[1].v_samp_factor = 1;
          cinfo.comp_info[2].h_samp_factor = 1;
          cinfo.comp_info[2].v_samp_factor = 1;
        }
    }

  /* No restart markers */
  cinfo.restart_interval = 0;
  cinfo.restart_in_rows = 0;

  jpeg_start_compress (&cinfo, TRUE);

  bpp = babl_format_get_bytes_per_pixel (format);
  row_pointer[0] = g_malloc (width * bpp);

  while (cinfo.next_scanline < cinfo.image_height)
    {
      GeglRectangle rect;

      rect.x = src_x;
      rect.y = src_y + cinfo.next_scanline;
      rect.width = width;
      rect.height = 1;
      gegl_buffer_get (buffer, &rect, zoom, format, row_pointer[0], GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_NONE);
      jpeg_write_scanlines (&cinfo, row_pointer, 1);
    }

  jpeg_finish_compress (&cinfo);
  jpeg_destroy_compress (&cinfo);
  g_free (row_pointer[0]);

  return size;
}


static gboolean
photos_operation_jpg_guess_sizes_process (GeglOperation *operation,
                                          GeglBuffer *input,
                                          const GeglRectangle *roi,
                                          gint level)
{
  PhotosOperationJpgGuessSizes *self = PHOTOS_OPERATION_JPG_GUESS_SIZES (operation);
  gsize i;

  for (i = 0; i < G_N_ELEMENTS (self->sizes); i++)
    {
      GeglRectangle roi_zoomed;
      gdouble zoom = 1.0 / (gdouble) (1 << i);

      roi_zoomed.height = (gint) (zoom * roi->height + 0.5);
      roi_zoomed.width = (gint) (zoom * roi->width + 0.5);
      roi_zoomed.x = (gint) (zoom * roi->x + 0.5);
      roi_zoomed.y = (gint) (zoom * roi->y + 0.5);

      self->sizes[i] = photos_operation_jpg_guess_sizes_count (input,
                                                               self->quality,
                                                               0,
                                                               self->optimize,
                                                               self->progressive,
                                                               self->sampling,
                                                               FALSE,
                                                               zoom,
                                                               roi_zoomed.x,
                                                               roi_zoomed.y,
                                                               roi_zoomed.width,
                                                               roi_zoomed.height);
    }

  return TRUE;
}


static void
photos_operation_jpg_guess_sizes_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
  PhotosOperationJpgGuessSizes *self = PHOTOS_OPERATION_JPG_GUESS_SIZES (object);

  switch (prop_id)
    {
    case PROP_OPTIMIZE:
      g_value_set_boolean (value, self->optimize);
      break;

    case PROP_PROGRESSIVE:
      g_value_set_boolean (value, self->progressive);
      break;

    case PROP_QUALITY:
      g_value_set_int (value, self->quality);
      break;

    case PROP_SAMPLING:
      g_value_set_boolean (value, self->sampling);
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
photos_operation_jpg_guess_sizes_set_property (GObject *object,
                                               guint prop_id,
                                               const GValue *value,
                                               GParamSpec *pspec)
{
  PhotosOperationJpgGuessSizes *self = PHOTOS_OPERATION_JPG_GUESS_SIZES (object);

  switch (prop_id)
    {
    case PROP_OPTIMIZE:
      self->optimize = g_value_get_boolean (value);
      break;

    case PROP_PROGRESSIVE:
      self->progressive = g_value_get_boolean (value);
      break;

    case PROP_QUALITY:
      self->quality = g_value_get_int (value);
      break;

    case PROP_SAMPLING:
      self->sampling = g_value_get_boolean (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
photos_operation_jpg_guess_sizes_init (PhotosOperationJpgGuessSizes *self)
{
}


static void
photos_operation_jpg_guess_sizes_class_init (PhotosOperationJpgGuessSizesClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GeglOperationClass *operation_class = GEGL_OPERATION_CLASS (class);
  GeglOperationSinkClass *sink_class = GEGL_OPERATION_SINK_CLASS (class);

  operation_class->opencl_support = FALSE;
  sink_class->needs_full = TRUE;

  object_class->get_property = photos_operation_jpg_guess_sizes_get_property;
  object_class->set_property = photos_operation_jpg_guess_sizes_set_property;
  sink_class->process = photos_operation_jpg_guess_sizes_process;

  g_object_class_install_property (object_class,
                                   PROP_OPTIMIZE,
                                   g_param_spec_boolean ("optimize",
                                                         "Optimize",
                                                         "Use optimized huffman tables",
                                                         TRUE,
                                                         G_PARAM_CONSTRUCT | G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_PROGRESSIVE,
                                   g_param_spec_boolean ("progressive",
                                                         "Progressive",
                                                         "Create progressive JPEG images",
                                                         TRUE,
                                                         G_PARAM_CONSTRUCT | G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_QUALITY,
                                   g_param_spec_int ("quality",
                                                     "Quality",
                                                     "JPEG compression quality (between 1 and 100)",
                                                     1,
                                                     100,
                                                     90,
                                                     G_PARAM_CONSTRUCT | G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_SAMPLING,
                                   g_param_spec_boolean ("sampling",
                                                         "Sampling",
                                                         "Use sub-sampling",
                                                         FALSE,
                                                         G_PARAM_CONSTRUCT | G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_SIZE,
                                   g_param_spec_uint64 ("size",
                                                        "Size (level=0)",
                                                        "Approximate size in bytes after applying JPEG compression"
                                                        "at zoom=1.0",
                                                        0,
                                                        G_MAXSIZE,
                                                        0,
                                                        G_PARAM_READABLE));

  g_object_class_install_property (object_class,
                                   PROP_SIZE_1,
                                   g_param_spec_uint64 ("size-1",
                                                        "Size (level=1)",
                                                        "Approximate size in bytes after applying JPEG compression"
                                                        "at zoom=0.5",
                                                        0,
                                                        G_MAXSIZE,
                                                        0,
                                                        G_PARAM_READABLE));

  gegl_operation_class_set_keys (operation_class,
                                 "name", "photos:jpg-guess-sizes",
                                 "title", "JPEG Guess Sizes",
                                 "description", "Guesses the size of a GeglBuffer after applying JPEG compression",
                                 NULL);
}
