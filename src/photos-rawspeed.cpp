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

#include <memory>
#include <string>

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


rawspeed::RawImage
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

  return ret_val;
}
