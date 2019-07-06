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

/* Based on code from:
 *   + Darktable
 */


#include "config.h"

#include <cmath>
#include <cstring>
#include <memory>
#include <string>

#include <babl/babl.h>
#include <glib/gi18n.h>
#include <RawSpeed-API.h>

#include "photos-debug.h"
#include "photos-error.h"
#include "photos-rawspeed.h"


enum
{
  N_BAYER_INDICES = 4,
  N_COEFFICIENTS = 4,
  N_PROCESSED_MAXIMUMS = 4,
  N_XTRANS = 6
};

static std::shared_ptr<rawspeed::CameraMetaData> metadata;


static void
photos_rawspeed_destroy_image (rawspeed::RawImage *image)
{
  delete image;
}


static gint
photos_rawspeed_get_2x2_bayer_index (gint crop_x, gint crop_y, gint x, gint y)
{
  const gint index = ((crop_x + x) & 1) + (((crop_y + y) & 1) << 1);

  g_return_val_if_fail (0 <= index && index < N_BAYER_INDICES, 0);
  return index;
}


static const Babl *
photos_rawspeed_get_format (rawspeed::RawImageType image_type, gsize components)
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


static GeglBuffer *
photos_rawspeed_buffer_new_from_image (rawspeed::RawImage image, gsize components)
{
  GeglBuffer *ret_val = NULL;
  rawspeed::RawImageData *image_data = image.get ();

  const guchar *data_uncropped = image_data->getDataUncropped (0, 0);

  const rawspeed::RawImageType image_type = image_data->getDataType ();
  const Babl *format = photos_rawspeed_get_format (image_type, components);

  const rawspeed::iPoint2D dimensions_uncropped = image_data->getUncroppedDim();

  GeglRectangle bbox_uncropped;
  gegl_rectangle_set (&bbox_uncropped,
                      0,
                      0,
                      static_cast<guint> (dimensions_uncropped.x),
                      static_cast<guint> (dimensions_uncropped.y));

  const gint stride = static_cast<gint> (image_data->pitch);
  rawspeed::RawImage *image_copy = new rawspeed::RawImage (image);

  g_autoptr (GeglBuffer) buffer_uncropped
    = gegl_buffer_linear_new_from_data ((const gpointer) data_uncropped,
                                        format,
                                        &bbox_uncropped,
                                        stride,
                                        reinterpret_cast<GDestroyNotify> (photos_rawspeed_destroy_image),
                                        image_copy);

  const rawspeed::iPoint2D dimensions_cropped = image_data->dim;
  const rawspeed::iPoint2D crop_offset = image_data->getCropOffset ();

  GeglRectangle bbox_cropped;
  gegl_rectangle_set (&bbox_cropped,
                      crop_offset.x,
                      crop_offset.y,
                      static_cast<guint> (dimensions_cropped.x),
                      static_cast<guint> (dimensions_cropped.y));

  g_autoptr (GeglBuffer) buffer_cropped = gegl_buffer_create_sub_buffer (buffer_uncropped, &bbox_cropped);

  g_autoptr (GeglBuffer) buffer_shifted = static_cast<GeglBuffer *> (g_object_new (GEGL_TYPE_BUFFER,
                                                                                   "abyss-width", -1,
                                                                                   "shift-x", crop_offset.x,
                                                                                   "shift-y", crop_offset.y,
                                                                                   "source", buffer_cropped,
                                                                                   NULL));

  {
    const GeglRectangle bbox = *gegl_buffer_get_extent (buffer_shifted);
    g_return_val_if_fail (bbox.height == dimensions_cropped.y, NULL);
    g_return_val_if_fail (bbox.width == dimensions_cropped.x, NULL);
    g_return_val_if_fail (bbox.x == 0, NULL);
    g_return_val_if_fail (bbox.y == 0, NULL);
  }

  ret_val = static_cast<GeglBuffer *> (g_object_ref (buffer_shifted));

  return ret_val;
}


static gint
photos_rawspeed_calculate_pattern_color_bayer (const gsize row, const gsize column, const guint32 filters)
{
  const gint ret_val = filters >> (((row << 1 & 14) + (column & 1)) << 1) & 3;
  g_return_val_if_fail (0 <= ret_val && ret_val < N_COEFFICIENTS, 0);

  return ret_val;
}


static gint
photos_rawspeed_calculate_pattern_color_xtrans (gsize column,
                                                gsize row,
                                                const GeglRectangle *bbox,
                                                const guint8 (*const xtrans)[6])
{
  gint ret_val = 0;

  g_return_val_if_fail (xtrans != 0, 0);

  column += 600;
  row += 600;
  g_return_val_if_fail (column >=0 && row >= 0, 0);

  if (bbox != NULL)
    {
      column += bbox->x;
      row += bbox->y;
    }

  ret_val = xtrans[row % N_XTRANS][column % N_XTRANS];
  g_return_val_if_fail (0 <= ret_val && ret_val < N_COEFFICIENTS, 0);

  return ret_val;
}


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


static bool
photos_rawspeed_is_4bayer (guint32 filters)
{
  bool is_4bayer = false;

  /* cyan, yellow, green, magenta */
  if (filters == 0xb4b4b4b4 || filters == 0x4b4b4b4b || filters == 0x1e1e1e1e || filters == 0xe1e1e1e1)
    is_4bayer = true;
  /* red, green, blue, emerald */
  else if (filters == 0x63636363 || filters == 0x36363636 || filters == 0x9c9c9c9c || filters == 0xc9c9c9c9)
    is_4bayer = true;

  return is_4bayer;
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


static GeglBuffer *
photos_rawspeed_op_apply_white_balance (GeglBuffer *buffer_input,
                                        rawspeed::RawImageData *image_data,
                                        gfloat processed_maximums[4],
                                        gsize components,
                                        guint32 filters)
{
  GeglBuffer *ret_val = NULL;

  const GeglRectangle bbox = *gegl_buffer_get_extent (buffer_input);
  g_return_val_if_fail (bbox.x == 0, NULL);
  g_return_val_if_fail (bbox.y == 0, NULL);

  g_return_val_if_fail (filters == 0 || (filters != 0 && components == 1), NULL);

  bool are_coefficients_valid = true;
  gfloat coefficients[N_COEFFICIENTS];

  const bool is_4bayer = photos_rawspeed_is_4bayer (filters);
  photos_debug (PHOTOS_DEBUG_GEGL, "Rawspeed: Is 4bayer: %s", is_4bayer ? "true" : "false");

  /* The fourth value is usually NAN for non-4Bayer images. */
  const guint n_coefficients_to_check = is_4bayer ? 4 : 3;
  for (guint i = 0; i < G_N_ELEMENTS (coefficients); i++)
    {
      if (i < n_coefficients_to_check
          && (!std::isnormal (image_data->metadata.wbCoeffs[i])
              || G_APPROX_VALUE (image_data->metadata.wbCoeffs[i], 0.0f, PHOTOS_EPSILON)))
        {
          are_coefficients_valid = false;
          break;
        }

      coefficients[i] = image_data->metadata.wbCoeffs[i];
    }

  photos_debug (PHOTOS_DEBUG_GEGL,
                "Rawspeed: Camera white balance coefficients valid: %s",
                are_coefficients_valid ? "true" : "false");

  if (!are_coefficients_valid)
    {
      static_assert(G_N_ELEMENTS (coefficients) == 4, "Value of N_COEFFICIENTS has changed");
      coefficients[0] = 2.0f;
      coefficients[1] = 1.0f;
      coefficients[2] = 1.5f;
      coefficients[3] = 1.0f;
    }

  static_assert(G_N_ELEMENTS (coefficients) == 4, "Value of N_COEFFICIENTS has changed");
  coefficients[0] = 1.0f / coefficients[1];
  coefficients[2] = 1.0f / coefficients[1];
  coefficients[3] = 1.0f / coefficients[1];
  coefficients[1] = 1.0f;

  const Babl *format = gegl_buffer_get_format (buffer_input);
  const GeglAbyssPolicy abyss_policy = static_cast<GeglAbyssPolicy> (GEGL_ABYSS_NONE | GEGL_BUFFER_FILTER_AUTO);
  GeglBufferIterator *it = gegl_buffer_iterator_new (buffer_input,
                                                     &bbox,
                                                     0,
                                                     format,
                                                     GEGL_ACCESS_READWRITE,
                                                     abyss_policy,
                                                     1);

  if (filters == 9)
    {
      guint8 xtrans[N_XTRANS][N_XTRANS];

      for (gint i = 0; i < N_XTRANS; i++)
        {
          for (gint j = 0; j < N_XTRANS; j++)
            xtrans[j][i] = image_data->cfa.getColorAt (i, j);
        }

      while (gegl_buffer_iterator_next (it))
        {
          GeglBufferIteratorItem *item = &it->items[0];
          const GeglRectangle &roi = item->roi;

          const gfloat *const in = static_cast<gfloat *> (item->data);
          gfloat *const out = static_cast<gfloat *> (item->data);

          for (gint j = 0, y = roi.y; j < roi.height; j++, y++)
            {
              for (gint i = 0, x = roi.x; i < roi.width; i++, x++)
                {
                  const gint fc = photos_rawspeed_calculate_pattern_color_xtrans (x, y, &bbox, xtrans);

                  const gsize offset = static_cast<gsize> (j) * static_cast<gsize> (roi.width)
                                       + static_cast<gsize> (i);

                  out[offset] = in[offset] * coefficients[fc];
                }
            }
        }
    }
  else if (filters != 0)
    {
      while (gegl_buffer_iterator_next (it))
        {
          GeglBufferIteratorItem *item = &it->items[0];
          const GeglRectangle &roi = item->roi;

          const gfloat *const in = static_cast<gfloat *> (item->data);
          gfloat *const out = static_cast<gfloat *> (item->data);

          for (gint j = 0, y = roi.y; j < roi.height; j++, y++)
            {
              for (gint i = 0, x = roi.x; i < roi.width; i++, x++)
                {
                  const gint fc = photos_rawspeed_calculate_pattern_color_bayer (x, y, filters);

                  const gsize offset = static_cast<gsize> (j) * static_cast<gsize> (roi.width)
                                       + static_cast<gsize> (i);

                  out[offset] = in[offset] * coefficients[fc];
                }
            }
        }
    }
  else
    {
      while (gegl_buffer_iterator_next (it))
        {
          GeglBufferIteratorItem *item = &it->items[0];
          const GeglRectangle &roi = item->roi;

          const gfloat *const in = static_cast<gfloat *> (item->data);
          gfloat *const out = static_cast<gfloat *> (item->data);

          for (gint j = 0, y = roi.y; j < roi.height; j++, y++)
            {
              for (gint i = 0, x = roi.x; i < roi.width; i++, x++)
                {
                  for (gsize c = 0; c < components; c++)
                    {
                      const gsize offset = components
                                           * (static_cast<gsize> (j) * static_cast<gsize> (roi.width)
                                              + static_cast<gsize> (i))
                                           + c;

                      out[offset] = in[offset] * coefficients[c];
                    }
                }
            }
        }
    }

  static_assert(G_N_ELEMENTS (coefficients) == N_PROCESSED_MAXIMUMS, "Size of the arrays have changed");
  for (guint i = 0; i < G_N_ELEMENTS (coefficients); i++)
    processed_maximums[i] *= coefficients[i];

  ret_val = static_cast<GeglBuffer *> (g_object_ref (buffer_input));
  return ret_val;
}


static GeglBuffer *
photos_rawspeed_op_correct_black_white_point (GeglBuffer *buffer_input,
                                              rawspeed::RawImageData *image_data,
                                              gfloat processed_maximums[4],
                                              gsize components,
                                              guint32 filters)
{
  GeglBuffer *ret_val = NULL;

  const GeglRectangle bbox = *gegl_buffer_get_extent (buffer_input);
  g_return_val_if_fail (bbox.x == 0, NULL);
  g_return_val_if_fail (bbox.y == 0, NULL);

  const rawspeed::RawImageType image_type = image_data->getDataType ();
  if (photos_rawspeed_is_normalized (image_type, filters, static_cast<guint32> (image_data->whitePoint)))
    {
      ret_val = static_cast<GeglBuffer *> (g_object_ref (buffer_input));
      return ret_val;
    }

  gfloat div[N_BAYER_INDICES];
  gfloat sub[N_BAYER_INDICES];
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

  const GeglAbyssPolicy abyss_policy = static_cast<GeglAbyssPolicy> (GEGL_ABYSS_NONE | GEGL_BUFFER_FILTER_AUTO);

  const Babl *format_input = gegl_buffer_get_format (buffer_input);
  GeglBufferIterator *it = gegl_buffer_iterator_new (buffer_input,
                                                     &bbox,
                                                     0,
                                                     format_input,
                                                     GEGL_ACCESS_READ,
                                                     abyss_policy,
                                                     2);
  const gint it_input_handle = 0;

  const Babl *format_output = photos_rawspeed_get_format (rawspeed::TYPE_FLOAT32, components);
  g_autoptr (GeglBuffer) buffer_output = gegl_buffer_new (&bbox, format_output);

  const gint it_output_handle = gegl_buffer_iterator_add (it,
                                                          buffer_output,
                                                          &bbox,
                                                          0,
                                                          format_output,
                                                          GEGL_ACCESS_WRITE,
                                                          abyss_policy);

  const rawspeed::iPoint2D crop_offset_top_left = image_data->getCropOffset ();

  if (filters != 0 && components == 1 && image_type == rawspeed::TYPE_USHORT16)
    {
      while (gegl_buffer_iterator_next (it))
        {
          GeglBufferIteratorItem *item_input = &it->items[it_input_handle];
          GeglBufferIteratorItem *item_output = &it->items[it_output_handle];
          g_assert_cmpint (item_input->roi.height, ==, item_output->roi.height);
          g_assert_cmpint (item_input->roi.width, ==, item_output->roi.width);
          g_assert_cmpint (item_input->roi.x, ==, item_output->roi.x);
          g_assert_cmpint (item_input->roi.y, ==, item_output->roi.y);

          const GeglRectangle roi = item_input->roi;

          const guint16 *const in = static_cast<guint16 *> (item_input->data);
          gfloat *const out = static_cast<gfloat *> (item_output->data);

          for (gint j = 0, y = roi.y; j < roi.height; j++, y++)
            {
              for (gint i = 0, x = roi.x; i < roi.width; i++, x++)
                {
                  const gint bayer_index = photos_rawspeed_get_2x2_bayer_index (crop_offset_top_left.x,
                                                                                crop_offset_top_left.y,
                                                                                x,
                                                                                y);

                  const gsize offset = static_cast<gsize> (j) * static_cast<gsize> (roi.width)
                                       + static_cast<gsize> (i);

                  out[offset] = (static_cast<gfloat> (in[offset]) - sub[bayer_index]) / div[bayer_index];
                }
            }
        }
    }
  else if (filters != 0 && components == 1 && image_type == rawspeed::TYPE_FLOAT32)
    {
      while (gegl_buffer_iterator_next (it))
        {
          GeglBufferIteratorItem *item_input = &it->items[it_input_handle];
          GeglBufferIteratorItem *item_output = &it->items[it_output_handle];
          g_assert_cmpint (item_input->roi.height, ==, item_output->roi.height);
          g_assert_cmpint (item_input->roi.width, ==, item_output->roi.width);
          g_assert_cmpint (item_input->roi.x, ==, item_output->roi.x);
          g_assert_cmpint (item_input->roi.y, ==, item_output->roi.y);

          const GeglRectangle roi = item_input->roi;

          const gfloat *const in = static_cast<gfloat *> (item_input->data);
          gfloat *const out = static_cast<gfloat *> (item_output->data);

          for (gint j = 0, y = roi.y; j < roi.height; j++, y++)
            {
              for (gint i = 0, x = roi.x; i < roi.width; i++, x++)
                {
                  const gint bayer_index = photos_rawspeed_get_2x2_bayer_index (crop_offset_top_left.x,
                                                                                crop_offset_top_left.y,
                                                                                x,
                                                                                y);

                  const gsize offset = static_cast<gsize> (j) * static_cast<gsize> (roi.width)
                                       + static_cast<gsize> (i);

                  out[offset] = (in[offset] - sub[bayer_index]) / div[bayer_index];
                }
            }
        }
    }
  else
    {
      while (gegl_buffer_iterator_next (it))
        {
          GeglBufferIteratorItem *item_input = &it->items[it_input_handle];
          GeglBufferIteratorItem *item_output = &it->items[it_output_handle];
          g_assert_cmpint (item_input->roi.height, ==, item_output->roi.height);
          g_assert_cmpint (item_input->roi.width, ==, item_output->roi.width);
          g_assert_cmpint (item_input->roi.x, ==, item_output->roi.x);
          g_assert_cmpint (item_input->roi.y, ==, item_output->roi.y);

          const GeglRectangle roi = item_input->roi;

          const gfloat *const in = static_cast<gfloat *> (item_input->data);
          gfloat *const out = static_cast<gfloat *> (item_output->data);

          for (gint j = 0, y = roi.y; j < roi.height; j++, y++)
            {
              for (gint i = 0, x = roi.x; i < roi.width; i++, x++)
                {
                  for (gsize c = 0; c < components; c++)
                    {
                      const gsize offset = components
                                           * (static_cast<gsize> (j) * static_cast<gsize> (roi.width)
                                              + static_cast<gsize> (i))
                                           + c;

                      out[offset] = (in[offset] - sub[0]) / div[0];
                    }
                }
            }
        }
    }

  ret_val = static_cast<GeglBuffer *> (g_object_ref (buffer_output));
  return ret_val;
}


static GeglBuffer *
photos_rawspeed_op_demosaic (GeglBuffer *buffer_input,
                             rawspeed::RawImageData *image_data,
                             gfloat processed_maximums[4],
                             gsize components,
                             guint32 filters)
{
  GeglBuffer *ret_val = NULL;

  const GeglRectangle bbox = *gegl_buffer_get_extent (buffer_input);
  g_return_val_if_fail (bbox.x == 0, NULL);
  g_return_val_if_fail (bbox.y == 0, NULL);

  if (filters == 0)
    {
      ret_val = static_cast<GeglBuffer *> (g_object_ref (buffer_input));
      return ret_val;
    }

  const GeglAbyssPolicy abyss_policy = static_cast<GeglAbyssPolicy> (GEGL_ABYSS_NONE | GEGL_BUFFER_FILTER_AUTO);

  const Babl *format_input = gegl_buffer_get_format (buffer_input);
  GeglBufferIterator *it = gegl_buffer_iterator_new (buffer_input,
                                                     &bbox,
                                                     0,
                                                     format_input,
                                                     GEGL_ACCESS_READ,
                                                     abyss_policy,
                                                     2);
  const gint it_input_handle = 0;

  const Babl *format_output = photos_rawspeed_get_format (rawspeed::TYPE_FLOAT32, 3);
  g_autoptr (GeglBuffer) buffer_output = gegl_buffer_new (&bbox, format_output);

  const gint it_output_handle = gegl_buffer_iterator_add (it,
                                                          buffer_output,
                                                          &bbox,
                                                          0,
                                                          format_output,
                                                          GEGL_ACCESS_WRITE,
                                                          abyss_policy);

  if (filters == 9)
    {
    }
  else
    {
      const gint offset_bottom_right_x = 3;
      const gint offset_bottom_right_y = 3;
      const gint offset_top_left_x = 3;
      const gint offset_top_left_y = 3;

      while (gegl_buffer_iterator_next (it))
        {
          GeglBufferIteratorItem *item_input = &it->items[it_input_handle];
          GeglBufferIteratorItem *item_output = &it->items[it_output_handle];
          g_assert_cmpint (item_input->roi.height, ==, item_output->roi.height);
          g_assert_cmpint (item_input->roi.width, ==, item_output->roi.width);
          g_assert_cmpint (item_input->roi.x, ==, item_output->roi.x);
          g_assert_cmpint (item_input->roi.y, ==, item_output->roi.y);

          const GeglRectangle roi = item_input->roi;

          const gfloat *const in = static_cast<gfloat *> (item_input->data);
          gfloat *const out = static_cast<gfloat *> (item_output->data);

          gfloat count[4];
          gfloat sum[4];

          for (gint j = 0, y = roi.y; j < roi.height; j++, y++)
            {
              for (gint i = 0, x = roi.x; i < roi.width; i++, x++)
                {
                  if (x >= offset_top_left_x && y >= offset_top_left_y && y < bbox.height - offset_bottom_right_y)
                    {
                      const gint x_incremented = bbox.width - offset_bottom_right_x;
                      const gint increment = x_incremented - x;

                      x += increment;
                      i += increment;

                      if (i >= roi.width)
                        break;
                    }

                  std::memset (count, 0, sizeof (gfloat) * G_N_ELEMENTS (count));
                  std::memset (sum, 0, sizeof (gfloat) * G_N_ELEMENTS (sum));

                  for (gint q = j - 1, y1 = y - 1; q < j + 2; q++, y1++)
                    {
                      for (gint p = i - 1, x1 = x - 1; p < i + 2; p++, x1++)
                        {
                          if (y1 >= 0 && x1 >= 0 && y1 < bbox.height && x < bbox.width)
                            {
                              const gint fc = photos_rawspeed_calculate_pattern_color_bayer (x1, y1, filters);
                              const gsize offset = static_cast<gsize> (q) * static_cast<gsize> (roi.width)
                                                   + static_cast<gsize> (p);

                              sum[fc] += in[offset];
                              count[fc]++;
                            }
                        }
                    }

                  const gint fc = photos_rawspeed_calculate_pattern_color_bayer (x, y, filters);

                  for (gsize c = 0; c < 3; c++)
                    {
                      const gsize offset_out = 3
                                               * (static_cast<gsize> (j) * static_cast<gsize> (roi.width)
                                                  + static_cast<gsize> (i))
                                               + c;

                      if (c != fc && count[c] > 0.0f)
                        out[offset_out] = sum[c] / count[c];
                      else
                        out[offset_out];
                    }
                }
            }
        }
    }

  ret_val = static_cast<GeglBuffer *> (g_object_ref (buffer_input));
  return ret_val;
}


static GeglBuffer *
photos_rawspeed_op_reconstruct_highlights (GeglBuffer *buffer_input,
                                           rawspeed::RawImageData *image_data,
                                           gfloat processed_maximums[4],
                                           gsize components,
                                           guint32 filters)
{
  GeglBuffer *ret_val = NULL;

  const GeglRectangle bbox = *gegl_buffer_get_extent (buffer_input);
  g_return_val_if_fail (bbox.x == 0, NULL);
  g_return_val_if_fail (bbox.y == 0, NULL);

  g_return_val_if_fail (filters == 0 || (filters != 0 && components == 1), NULL);

  static_assert(N_PROCESSED_MAXIMUMS == 4, "Value of N_PROCESSED_MAXIMUMS has changed");
  const gfloat clip = std::fminf (processed_maximums[0],
                                  std::fminf (processed_maximums[1], processed_maximums[2]));

  const GeglAbyssPolicy abyss_policy = static_cast<GeglAbyssPolicy> (GEGL_ABYSS_NONE | GEGL_BUFFER_FILTER_AUTO);

  const Babl *format_input = gegl_buffer_get_format (buffer_input);
  GeglBufferIterator *it = gegl_buffer_iterator_new (buffer_input,
                                                     &bbox,
                                                     0,
                                                     format_input,
                                                     GEGL_ACCESS_READ,
                                                     abyss_policy,
                                                     2);
  const gint it_input_handle = 0;

  const Babl *format_output = photos_rawspeed_get_format (rawspeed::TYPE_FLOAT32, components);
  g_autoptr (GeglBuffer) buffer_output = gegl_buffer_new (&bbox, format_output);

  const gint it_output_handle = gegl_buffer_iterator_add (it,
                                                          buffer_output,
                                                          &bbox,
                                                          0,
                                                          format_output,
                                                          GEGL_ACCESS_WRITE,
                                                          abyss_policy);

  if (filters == 0)
    {
      while (gegl_buffer_iterator_next (it))
        {
          GeglBufferIteratorItem *item_input = &it->items[it_input_handle];
          GeglBufferIteratorItem *item_output = &it->items[it_output_handle];
          g_assert_cmpint (item_input->roi.height, ==, item_output->roi.height);
          g_assert_cmpint (item_input->roi.width, ==, item_output->roi.width);
          g_assert_cmpint (item_input->roi.x, ==, item_output->roi.x);
          g_assert_cmpint (item_input->roi.y, ==, item_output->roi.y);

          const GeglRectangle roi = item_input->roi;

          const gfloat *const in = static_cast<gfloat *> (item_input->data);
          gfloat *const out = static_cast<gfloat *> (item_output->data);

          for (gint j = 0; j < roi.height; j++)
            {
              for (gint i = 0; i < roi.width; i++)
                {
                  for (gsize c = 0; c < components; c++)
                    {
                      const gsize offset = components
                                           * (static_cast<gsize> (j) * static_cast<gsize> (roi.width)
                                              + static_cast<gsize> (i))
                                           + c;

                      out[offset] = fminf (clip, in[offset]);
                    }
                }
            }
        }

      for (guint i = 0; i < N_PROCESSED_MAXIMUMS - 1; i++)
        processed_maximums[i] = clip;
    }
  else
    {
      while (gegl_buffer_iterator_next (it))
        {
          GeglBufferIteratorItem *item_input = &it->items[it_input_handle];
          GeglBufferIteratorItem *item_output = &it->items[it_output_handle];
          g_assert_cmpint (item_input->roi.height, ==, item_output->roi.height);
          g_assert_cmpint (item_input->roi.width, ==, item_output->roi.width);
          g_assert_cmpint (item_input->roi.x, ==, item_output->roi.x);
          g_assert_cmpint (item_input->roi.y, ==, item_output->roi.y);

          const GeglRectangle roi = item_input->roi;

          const gfloat *const in = static_cast<gfloat *> (item_input->data);
          gfloat *const out = static_cast<gfloat *> (item_output->data);

          for (gint j = 0; j < roi.height; j++)
            {
              for (gint i = 0; i < roi.width; i++)
                {
                  const gsize offset = static_cast<gsize> (j) * static_cast<gsize> (roi.width)
                                       + static_cast<gsize> (i);

                  out[offset] = fminf (clip, in[offset]);
                }
            }
        }

      const gfloat max = std::fmaxf (processed_maximums[0],
                                     std::fmaxf (processed_maximums[1], processed_maximums[2]));

      for (guint i = 0; i < N_PROCESSED_MAXIMUMS - 1; i++)
        processed_maximums[i] = max;
    }

  ret_val = static_cast<GeglBuffer *> (g_object_ref (buffer_output));
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

  std::string image_type_str;
  switch (image_type)
    {
    case rawspeed::TYPE_FLOAT32:
      image_type_str = "float32";
      break;

    case rawspeed::TYPE_USHORT16:
      image_type_str = "ushort16";
      break;

    default:
      image_type_str = "unknown";
      break;
    }

  photos_debug (PHOTOS_DEBUG_GEGL, "Rawspeed: Image data type: %s", image_type_str.c_str());

  if (image_type != rawspeed::TYPE_USHORT16 && image_type != rawspeed::TYPE_FLOAT32)
    {
      g_set_error (error, PHOTOS_ERROR, 0, _("Unsupported image type: %d"), image_type);
      return ret_val;
    }

  const guint32 bpp = image_data->getBpp ();
  photos_debug (PHOTOS_DEBUG_GEGL, "Rawspeed: Bytes per pixel: %" G_GUINT32_FORMAT, bpp);

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
  photos_debug (PHOTOS_DEBUG_GEGL, "Rawspeed: Components per pixel: %" G_GUINT32_FORMAT, cpp);

  if (cpp != 1)
    {
      g_set_error (error,
                   PHOTOS_ERROR,
                   0,
                   _("Unsupported number of components per pixel: %" G_GUINT16_FORMAT),
                   bpp);
      return ret_val;
    }

  const rawspeed::iPoint2D dimensions_uncropped = image_data->getUncroppedDim();
  photos_debug (PHOTOS_DEBUG_GEGL,
                "Rawspeed: Uncropped size: %d×%d",
                dimensions_uncropped.x,
                dimensions_uncropped.y);

  const rawspeed::iPoint2D crop_offset_top_left = image_data->getCropOffset ();
  photos_debug (PHOTOS_DEBUG_GEGL,
                "Rawspeed: Crop offset top-left: %d×%d",
                crop_offset_top_left.x,
                crop_offset_top_left.y);

  guint32 filters = image_data->cfa.getDcrawFilter ();
  if (filters != 0 && filters != 9)
    {
      filters = rawspeed::ColorFilterArray::shiftDcrawFilter (filters,
                                                              crop_offset_top_left.x,
                                                              crop_offset_top_left.y);
    }

  photos_debug (PHOTOS_DEBUG_GEGL, "Rawspeed: Filters: %" G_GUINT32_FORMAT, filters);

  gfloat processed_maximums[N_PROCESSED_MAXIMUMS] = { 1.0f, 1.0f, 1.0f, 1.0f };

  g_autoptr (GeglBuffer) buffer_original = photos_rawspeed_buffer_new_from_image (image, 1);

  g_autoptr (GeglBuffer) buffer_corrected = photos_rawspeed_op_correct_black_white_point (buffer_original,
                                                                                          image_data,
                                                                                          processed_maximums,
                                                                                          1,
                                                                                          filters);

  g_autoptr (GeglBuffer) buffer_white_balanced = photos_rawspeed_op_apply_white_balance (buffer_corrected,
                                                                                         image_data,
                                                                                         processed_maximums,
                                                                                         1,
                                                                                         filters);

  g_autoptr (GeglBuffer) buffer_reconstructed = photos_rawspeed_op_reconstruct_highlights (buffer_white_balanced,
                                                                                           image_data,
                                                                                           processed_maximums,
                                                                                           1,
                                                                                           filters);

  g_autoptr (GeglBuffer) buffer_demosaiced = photos_rawspeed_op_demosaic (buffer_reconstructed,
                                                                          image_data,
                                                                          processed_maximums,
                                                                          1,
                                                                          filters);

  ret_val = static_cast<GeglBuffer *> (g_object_ref (buffer_corrected));
  return ret_val;
}
