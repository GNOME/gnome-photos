/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2019 Red Hat, Inc.
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


#include "config.h"

#include <cmath>
#include <memory>
#include <string>

#include <babl/babl.h>
#include <glib/gi18n.h>
#include <RawSpeed-API.h>

#include "photos-error.h"
#include "photos-rawspeed.h"


static std::shared_ptr<rawspeed::CameraMetaData> metadata;


static gboolean
photos_rawspeed_ensure_metadata (GError **error)
{
  static GError *local_error = NULL;
  static gsize once_init_value = 0;
  gboolean ret_val = FALSE;

  if (g_once_init_enter (&once_init_value))
    {
      const std::string path = PACKAGE_DATA_DIR G_DIR_SEPARATOR_S "cameras.xml";

      try
        {
          metadata.reset (new rawspeed::CameraMetaData (path.c_str()));
        }
      catch (rawspeed::RawspeedException &e)
        {
          g_set_error (&local_error, PHOTOS_ERROR, 0, e.what());
        }

      g_once_init_leave (&once_init_value, 1);
    }

  if (local_error != NULL)
    {
      g_propagate_error (error, g_error_copy (local_error));
      return ret_val;
    }

  ret_val = TRUE;
  return ret_val;
}


static GeglBuffer *
photos_rawspeed_correct_black_and_white_point (GeglBuffer *buffer_input,
                                               rawspeed::RawImage image,
                                               const guint32 filters)
{
  rawspeed::RawImageData *image_data = image.get ();
  gfloat div[4];
  gfloat sub[4];

  if (filters == 0)
    {
      const gfloat white = static_cast<gfloat> (image_data->whitePoint) / static_cast<gfloat> (G_MAXUINT16);
      gfloat black = 0.0f;

      for (gint i = 0; i < 4; i++)
        black += static_cast<gfloat> (image_data->blackLevelSeparate[i]) / static_cast<gfloat> (G_MAXUINT16);

      black /= 4.0f;

      for (gint i = 0; i < 4; i++)
        {
          sub[i] = black;
          div[i] = white - black;
        }
    }
  else
    {
      const gfloat white = static_cast<gfloat> (image_data->whitePoint);

      for (gint i = 0; i < 4; i++)
        {
          sub[i] = image_data->blackLevelSeparate[i];
          div[i] = white - sub[i];
        }
    }

  float black = 0.0f;

  for (gint i = 0; i < 4; i++)
    black += static_cast<gfloat> (image_data->blackLevelSeparate[i]);

  const guint16 black_level = static_cast<guint16> (black / 4.0f);
  const Babl *format_output = gegl_buffer_get_format (buffer_input);

  const rawspeed::iPoint2D dimensions_cropped = image_data->dim;

  const rawspeed::iPoint2D dimensions_uncropped = image_data->getUncroppedDim();
  const gint height_input = dimensions_uncropped.y;
  const gint width_input = dimensions_uncropped.x;

  const rawspeed::iPoint2D crop_top_left = image_data->getCropOffset ();
  const gint crop_x = crop_top_left.x;
  const gint crop_y = crop_top_left.y;

  const rawspeed::iPoint2D crop_bottom_right = dimensions_uncropped - dimensions_cropped - crop_top_left;
  const gint crop_height = crop_bottom_right.y;
  const gint crop_width = crop_bottom_right.x;

  const gint height_output = height_input - (crop_y + crop_height);
  const gint width_output = width_input - (crop_x + crop_width);

  GeglRectangle bbox_output;
  gegl_rectangle_set (&bbox_output, 0, 0, static_cast<guint> (width_output), static_cast<guint> (height_output));

  g_autoptr (GeglBuffer) buffer_output = gegl_buffer_new (&bbox_output, format_output);
  GeglBufferIterator *it = gegl_buffer_iterator_new (buffer_output,
                                                     &bbox_output,
                                                     0,
                                                     format_output,
                                                     GEGL_ACCESS_WRITE,
                                                     GEGL_ABYSS_NONE,
                                                     1);

  const rawspeed::RawImageType image_type = image_data->getDataType ();

  if (filters != 0 && image_type == rawspeed::TYPE_USHORT16)
    {
      while (gegl_buffer_iterator_next (it))
        {
          for (gint j = it->items[0].roi.y; j < it->items[0].roi.height; j++)
            {
              for (gint i = it->items[0].roi.x; i < it->items[0].roi.width; i++)
                {
                }
            }
        }
    }
  else if (filters != 0 && image_type == rawspeed::TYPE_FLOAT32)
    {
      while (gegl_buffer_iterator_next (it))
        {
          for (gint j = 0; j < height_output; j++)
            {
              for (gint i = 0; i < width_output; i++)
                {
                }
            }
        }
    }
  else
    {
      while (gegl_buffer_iterator_next (it))
        {
          for (gint j = 0; j < height_output; j++)
            {
              for (gint i = 0; i < width_output; i++)
                {
                }
            }
        }
    }

  return NULL;
}


static rawspeed::RawImage
photos_rawspeed_decode_bytes_to_image (GBytes *bytes, GError **error)
{
  rawspeed::RawImage ret_val = rawspeed::RawImage::create (rawspeed::TYPE_USHORT16);

  gsize count;
  const guchar *data = (const guchar *) g_bytes_get_data (bytes, &count);

  rawspeed::Buffer rs_buffer (data, count);
  rawspeed::RawParser parser (&rs_buffer);

  std::unique_ptr<rawspeed::RawDecoder> decoder;

  try
    {
      decoder = parser.getDecoder(metadata.get());
    }
  catch (rawspeed::RawspeedException &e)
    {
      g_set_error (error, PHOTOS_ERROR, 0, e.what());
      return ret_val;
    }

  decoder->failOnUnknown = true;

  try
    {
      decoder->checkSupport (metadata.get());
    }
  catch (rawspeed::RawspeedException &e)
    {
      g_set_error (error, PHOTOS_ERROR, 0, e.what());
      return ret_val;
    }

  try
    {
      decoder->decodeRaw ();
      decoder->decodeMetaData (metadata.get());
    }
  catch (rawspeed::RawspeedException &e)
    {
      g_set_error (error, PHOTOS_ERROR, 0, e.what());
      return ret_val;
    }

  ret_val = decoder->mRaw;
  return ret_val;
}


static const Babl *
photos_rawspeed_get_format (rawspeed::RawImageType image_type, guint components)
{
  const Babl *format = NULL;

  switch (image_type)
    {
    case rawspeed::TYPE_FLOAT32:
      format = babl_format ("Y float");
      break;

    case rawspeed::TYPE_USHORT16:
      format = babl_format ("Y u16");
      break;

    default:
      g_assert_not_reached ();
      break;
    }

  return format;
}


GeglBuffer *
photos_rawspeed_decode_bytes (GBytes *bytes, GError **error)
{
  GeglBuffer *ret_val = NULL;

  if (!photos_rawspeed_ensure_metadata (error))
    return ret_val;

  rawspeed::RawImage image = photos_rawspeed_decode_bytes_to_image (bytes, error);
  rawspeed::RawImageData *image_data = image.get ();
  if (!image_data->isAllocated ())
    return ret_val;

  if (image_data->blackLevelSeparate[0] == -1
      || image_data->blackLevelSeparate[1] == -1
      || image_data->blackLevelSeparate[2] == -1
      || image_data->blackLevelSeparate[3] == -1)
    {
      image_data->calculateBlackAreas ();
    }

  gint black_level = image_data->blackLevel;
  if (black_level == -1)
    {
      gfloat black = 0.0f;

      for (gint i = 0; i < 4; i++)
        black += static_cast<gfloat> (image_data->blackLevelSeparate[i]);

      black /= 4.0f;
      black_level = static_cast<gfloat> (CLAMP (black, 0, G_MAXUINT16));
    }

  const rawspeed::RawImageType image_type = image_data->getDataType ();
  if (image_type != rawspeed::TYPE_USHORT16 && image_type != rawspeed::TYPE_FLOAT32)
    {
      g_set_error (error, PHOTOS_ERROR, 0, _("Unsupported image type: %d"), image_type);
      return ret_val;
    }

  const guint32 bpp = image_data->getBpp ();
  if (bpp != sizeof (guint16) && bpp != sizeof (gfloat))
    {
      g_set_error (error, PHOTOS_ERROR, 0, _("Unsupported number of bytes per pixel: %" G_GUINT16_FORMAT), bpp);
      return ret_val;
    }

  if (image_type == rawspeed::TYPE_USHORT16 && bpp != sizeof (guint16))
    {
      g_set_error (error,
                   PHOTOS_ERROR,
                   0,
                   _("Unexpected number of bytes per pixel (%" G_GUINT16_FORMAT ") for a 16-bit image"),
                   bpp);
      return ret_val;
    }

  if (image_type == rawspeed::TYPE_FLOAT32 && bpp != sizeof (gfloat))
    {
      g_set_error (error,
                   PHOTOS_ERROR,
                   0,
                   _("Unexpected number of bytes per pixel (%" G_GUINT16_FORMAT ") for a floating point image"),
                   bpp);
      return ret_val;
    }

  const guint32 cpp = image_data->getCpp ();
  if (cpp != 1)
    {
      g_set_error (error,
                   PHOTOS_ERROR,
                   0,
                   _("Unsupported number of components per pixel: %" G_GUINT16_FORMAT),
                   bpp);
      return ret_val;
    }

  const rawspeed::iPoint2D crop_top_left = image_data->getCropOffset ();

  guint32 filters = image_data->cfa.getDcrawFilter ();
  if (filters != 0 && filters != 9)
    filters = rawspeed::ColorFilterArray::shiftDcrawFilter (filters, crop_top_left.x, crop_top_left.y);

  const Babl *format_original = photos_rawspeed_get_format (image_type, 1);

  const rawspeed::iPoint2D dimensions_uncropped = image_data->getUncroppedDim();
  GeglRectangle bbox_original;
  gegl_rectangle_set (&bbox_original,
                      0,
                      0,
                      static_cast<guint> (dimensions_uncropped.x),
                      static_cast<guint> (dimensions_uncropped.y));

  /* Row strides are represented as signed integers (ie. gint) in
   * GEGL, while the bytes per pixel is an unsigned integer
   * (ie. guint32). It must be ensured that the row stride can fit
   * into the expected signed integer type (ie. gint).
   *
   * The comparison is made using the same unsigned integer type to
   * avoid triggering -Wsign-compare.
   */
  if (bpp > 0 && dimensions_uncropped.x > 0 && (guint) dimensions_uncropped.x > G_MAXINT / bpp)
    {
      g_set_error (error,
                   PHOTOS_ERROR,
                   0,
                   _("Overflow calculating row stride: %u×%d"),
                   bpp,
                   dimensions_uncropped.x);
      return ret_val;
    }

  const gint stride_original = (gint) bpp * dimensions_uncropped.x;
  const guchar *data_uncropped = image_data->getDataUncropped (0, 0);

  g_autoptr (GeglBuffer) buffer_original = gegl_buffer_linear_new_from_data ((const gpointer) data_uncropped,
                                                                             format_original,
                                                                             &bbox_original,
                                                                             static_cast<gint> (image_data->pitch),
                                                                             NULL,
                                                                             NULL);

  bool is_normalized;

  if (filters != 0 && image_type == rawspeed::TYPE_FLOAT32)
    {
      union
        {
          gfloat f;
          guint32 u;
        } normalized;

      normalized.f = 1.0f;
      is_normalized = image_data->whitePoint == normalized.u;
    }
  else
    {
      is_normalized = image_type == rawspeed::TYPE_FLOAT32;
    }

  if (!is_normalized)
    photos_rawspeed_correct_black_and_white_point (buffer_original, image, filters);

  return ret_val;
}
