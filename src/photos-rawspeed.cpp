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


static gint
photos_rawspeed_get_2x2_bayer_index (gint crop_x, gint crop_y, gint x, gint y)
{
  const gint index = ((crop_x + x) & 1) + (((crop_y + y) & 1) << 1);

  g_return_val_if_fail (0 <= index && index < 4, 0);
  return index;
}


static const Babl *
photos_rawspeed_get_format (rawspeed::RawImageType image_type, guint components)
{
  g_return_val_if_fail ((components == 1
                         && (image_type == rawspeed::TYPE_FLOAT32 || image_type == rawspeed::TYPE_USHORT16))
                        || (components == 3 && image_type == rawspeed::TYPE_FLOAT32),
                        NULL);

  std::string format_name;

  switch (components)
    {
    case 1:
      format_name = "Y";
      break;

    case 3:
      format_name = "RGB";
      break;

    default:
      g_assert_not_reached ();
      break;
    }

  format_name += " ";

  switch (image_type)
    {
    case rawspeed::TYPE_FLOAT32:
      format_name += "float";
      break;

    case rawspeed::TYPE_USHORT16:
      format_name += "u16";
      break;

    default:
      g_assert_not_reached ();
      break;
    }

  const Babl *format = babl_format (format_name.c_str ());
  return format;
}


static bool
photos_rawspeed_is_normalized (rawspeed::RawImageType image_type, guint32 filters, guint32 white_point)
{
  bool is_normalized;

  if (filters != 0 && image_type == rawspeed::TYPE_FLOAT32)
    {
      union
        {
          gfloat f;
          guint32 u;
        } normalized;

      normalized.f = 1.0f;
      is_normalized = white_point == normalized.u;
    }
  else
    {
      is_normalized = image_type == rawspeed::TYPE_FLOAT32;
    }

  return is_normalized;
}


static void
photos_rawspeed_destroy_image (rawspeed::RawImage *image)
{
  delete image;
}


static GeglBuffer *
photos_rawspeed_correct_black_and_white_point (rawspeed::RawImage image, gint components, guint32 filters)
{
  GeglBuffer *ret_val = NULL;

  rawspeed::RawImageData *image_data = image.get ();
  const rawspeed::RawImageType image_type = image_data->getDataType ();

  const guchar *data_uncropped = image_data->getDataUncropped (0, 0);
  const rawspeed::iPoint2D dimensions_uncropped = image_data->getUncroppedDim();

  if (photos_rawspeed_is_normalized (image_type, filters, static_cast<guint32> (image_data->whitePoint)))
    {
      const Babl *format_original = photos_rawspeed_get_format (image_type, components);

      GeglRectangle bbox_original;
      gegl_rectangle_set (&bbox_original,
                          0,
                          0,
                          static_cast<guint> (dimensions_uncropped.x),
                          static_cast<guint> (dimensions_uncropped.y));

      const gint stride_original = static_cast<gint> (image_data->pitch);
      rawspeed::RawImage *image_original = new rawspeed::RawImage (image);

      g_autoptr (GeglBuffer) buffer_original
        = gegl_buffer_linear_new_from_data ((const gpointer) data_uncropped,
                                            format_original,
                                            &bbox_original,
                                            stride_original,
                                            reinterpret_cast<GDestroyNotify> (photos_rawspeed_destroy_image),
                                            image_original);

      ret_val = static_cast<GeglBuffer *> (g_object_ref (buffer_original));
      return ret_val;
    }

  gfloat div[4];
  gfloat sub[4];
  const std::size_t black_level_separate_size = image_data->blackLevelSeparate.size ();

  if (filters == 0)
    {
      const gfloat white = static_cast<gfloat> (image_data->whitePoint) / static_cast<gfloat> (G_MAXUINT16);
      gfloat black = 0.0f;

      for (std::size_t i = 0; i < black_level_separate_size; i++)
        black += static_cast<gfloat> (image_data->blackLevelSeparate[i]) / static_cast<gfloat> (G_MAXUINT16);

      black /= static_cast<gfloat> (black_level_separate_size);

      for (guint i = 0; i < G_N_ELEMENTS (div); i++)
        {
          sub[i] = black;
          div[i] = white - sub[i];
        }
    }
  else
    {
      const gfloat white = static_cast<gfloat> (image_data->whitePoint);

      for (guint i = 0; i < G_N_ELEMENTS (div); i++)
        {
          sub[i] = static_cast<gfloat> (image_data->blackLevelSeparate[i]);
          div[i] = white - sub[i];
        }
    }

  const guint32 bpp = image_data->getBpp ();

  const Babl *format_output = photos_rawspeed_get_format (rawspeed::TYPE_FLOAT32, components);

  const rawspeed::iPoint2D dimensions_cropped = image_data->dim;
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
  g_message ("rawprepare: process: x, y, width, height: %d, %d, %d, %d",
             bbox_output.x,
             bbox_output.y,
             bbox_output.width,
             bbox_output.height);

  g_autoptr (GeglBuffer) buffer_output = gegl_buffer_new (&bbox_output, format_output);
  GeglBufferIterator *it = gegl_buffer_iterator_new (buffer_output,
                                                     &bbox_output,
                                                     0,
                                                     format_output,
                                                     GEGL_ACCESS_WRITE,
                                                     GEGL_ABYSS_NONE,
                                                     1);

  if (filters != 0 && components == 1 && image_type == rawspeed::TYPE_USHORT16)
    {
      while (gegl_buffer_iterator_next (it))
        {
          GeglBufferIteratorItem *item = &it->items[0];
          gfloat *const out = static_cast<gfloat *> (item->data);

          for (gint j = 0, y = item->roi.y; j < item->roi.height; j++, y++)
            {
              for (gint i = 0, x = item->roi.x; i < item->roi.width; i++, x++)
                {
                  const gsize in_offset = static_cast<gsize> (crop_y + y) * static_cast<gsize> (image_data->pitch)
                                          + static_cast<gsize> (crop_x + x) * static_cast<gsize> (bpp);
                  const guint16 in_value = *(reinterpret_cast<const guint16 *> (data_uncropped + in_offset));

                  const gsize out_offset = static_cast<gsize> (y) * static_cast<gsize> (width_output)
                                           + static_cast<gsize> (x);

                  const gint bayer_index = photos_rawspeed_get_2x2_bayer_index (crop_x, crop_y, x, y);

                  out[out_offset] = (static_cast<gfloat> (in_value) - sub[bayer_index]) / div[bayer_index];

                  if (x % 1000 == 0 && y % 1000 == 0)
                    g_message ("rawprepare: process: (%d, %d) %d -> %f", x, y, (gint) in_value, out[out_offset]);
                }
            }
        }
    }
  else if (filters != 0 && components == 1 && image_type == rawspeed::TYPE_FLOAT32)
    {
      while (gegl_buffer_iterator_next (it))
        {
          GeglBufferIteratorItem *item = &it->items[0];
          gfloat *const out = static_cast<gfloat *> (item->data);

          for (gint y = item->roi.y; y < item->roi.height; y++)
            {
              for (gint x = item->roi.x; x < item->roi.width; x++)
                {
                  const gsize in_offset = static_cast<gsize> (crop_y + y) * static_cast<gsize> (image_data->pitch)
                                          + static_cast<gsize> (crop_x + x) * static_cast<gsize> (bpp);
                  const gfloat in_value = *(reinterpret_cast<const gfloat *> (data_uncropped + in_offset));

                  const gsize out_offset = static_cast<gsize> (y) * static_cast<gsize> (width_output)
                                           + static_cast<gsize> (x);

                  const gint bayer_index = photos_rawspeed_get_2x2_bayer_index (crop_x, crop_y, x, y);

                  out[out_offset] = (in_value - sub[bayer_index]) / div[bayer_index];
                }
            }
        }
    }
  else
    {
      while (gegl_buffer_iterator_next (it))
        {
          GeglBufferIteratorItem *item = &it->items[0];
          gfloat *const out = static_cast<gfloat *> (item->data);

          for (gint y = item->roi.y; y < item->roi.height; y++)
            {
              for (gint x = item->roi.x; x < item->roi.width; x++)
                {
                  for (gint c = 0; c < components; c++)
                    {
                      const gsize in_offset = static_cast<gsize> (crop_y + y)
                                              * static_cast<gsize> (image_data->pitch)
                                              + static_cast<gsize> (crop_x + x) * static_cast<gsize> (bpp)
                                              + static_cast<gsize> (c) * sizeof (gfloat);
                      const gfloat in_value = *(reinterpret_cast<const gfloat *> (data_uncropped + in_offset));

                      const gsize out_offset = (static_cast<gsize> (y) * static_cast<gsize> (width_output)
                                                + static_cast<gsize> (x))
                                               * static_cast<gsize> (components)
                                               + static_cast<gsize> (c);

                      out[out_offset] = (in_value - sub[0]) / div[0];
                    }
                }
            }
        }
    }

  ret_val = static_cast<GeglBuffer *> (g_object_ref (buffer_output));
  return ret_val;
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

  g_autoptr (GeglBuffer) buffer_corrected = photos_rawspeed_correct_black_and_white_point (image, 1, filters);

  ret_val = static_cast<GeglBuffer *> (g_object_ref (buffer_corrected));
  return ret_val;
}
