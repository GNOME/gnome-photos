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

#include <setjmp.h>

#include <gegl.h>
#include <glib.h>
#include <glib/gi18n.h>

#include "photos-debug.h"
#include "photos-error.h"
#include "photos-gegl.h"
#include "photos-gegl-buffer-codec-raw.h"


struct _PhotosGeglBufferCodecRaw
{
  PhotosGeglBufferCodec parent_instance;
  gboolean can_set_size;
  gboolean decoding;
};

enum
{
  PROP_0,
  PROP_BUFFER,
  PROP_CAN_SET_SIZE
};


G_DEFINE_TYPE_WITH_CODE (PhotosGeglBufferCodecRaw, photos_gegl_buffer_codec_raw, PHOTOS_TYPE_GEGL_BUFFER_CODEC,
                         photos_gegl_ensure_extension_points ();
                         g_io_extension_point_implement (PHOTOS_GEGL_BUFFER_CODEC_EXTENSION_POINT_NAME,
                                                         g_define_type_id,
                                                         "raw",
                                                         0));


static const gchar *MIME_TYPES[] =
{
  NULL
};


static GeglBuffer *
photos_gegl_buffer_codec_raw_get_buffer (PhotosGeglBufferCodec *codec)
{
  PhotosGeglBufferCodecRaw *self = PHOTOS_GEGL_BUFFER_CODEC_RAW (codec);
  return NULL;
}


static gboolean
photos_gegl_buffer_codec_raw_load_begin (PhotosGeglBufferCodec *codec, GError **error)
{
  PhotosGeglBufferCodecRaw *self = PHOTOS_GEGL_BUFFER_CODEC_RAW (codec);

  g_return_val_if_fail (!self->decoding, FALSE);

  self->decoding = TRUE;

 out:
  return self->decoding;
}


static gboolean
photos_gegl_buffer_codec_raw_load_increment (PhotosGeglBufferCodec *codec,
                                             const guchar *buf,
                                             gsize count,
                                             GError **error)
{
  PhotosGeglBufferCodecRaw *self = PHOTOS_GEGL_BUFFER_CODEC_RAW (codec);
  gboolean ret_val = FALSE;

  g_return_val_if_fail (self->decoding, FALSE);

  ret_val = TRUE;

 out:
  return ret_val;
}


static gboolean
photos_gegl_buffer_codec_raw_load_stop (PhotosGeglBufferCodec *codec, GError **error)
{
  PhotosGeglBufferCodecRaw *self = PHOTOS_GEGL_BUFFER_CODEC_RAW (codec);
  gboolean ret_val = FALSE;

  g_return_val_if_fail (self->decoding, FALSE);

  ret_val = TRUE;

 out:
  self->decoding = FALSE;
  return ret_val;
}


static void
photos_gegl_buffer_codec_raw_dispose (GObject *object)
{
  PhotosGeglBufferCodecRaw *self = PHOTOS_GEGL_BUFFER_CODEC_RAW (object);

  G_OBJECT_CLASS (photos_gegl_buffer_codec_raw_parent_class)->dispose (object);
}


static void
photos_gegl_buffer_codec_raw_finalize (GObject *object)
{
  PhotosGeglBufferCodecRaw *self = PHOTOS_GEGL_BUFFER_CODEC_RAW (object);

  G_OBJECT_CLASS (photos_gegl_buffer_codec_raw_parent_class)->finalize (object);
}


static void
photos_gegl_buffer_codec_raw_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
  PhotosGeglBufferCodecRaw *self = PHOTOS_GEGL_BUFFER_CODEC_RAW (object);

  switch (prop_id)
    {
    case PROP_BUFFER:
      g_value_set_object (value, NULL);
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
photos_gegl_buffer_codec_raw_init (PhotosGeglBufferCodecRaw *self)
{
}


static void
photos_gegl_buffer_codec_raw_class_init (PhotosGeglBufferCodecRawClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  PhotosGeglBufferCodecClass *buffer_codec_class = PHOTOS_GEGL_BUFFER_CODEC_CLASS (class);

  buffer_codec_class->mime_types = (GStrv) MIME_TYPES;

  object_class->dispose = photos_gegl_buffer_codec_raw_dispose;
  object_class->finalize = photos_gegl_buffer_codec_raw_finalize;
  object_class->get_property = photos_gegl_buffer_codec_raw_get_property;
  buffer_codec_class->get_buffer = photos_gegl_buffer_codec_raw_get_buffer;
  buffer_codec_class->load_begin = photos_gegl_buffer_codec_raw_load_begin;
  buffer_codec_class->load_increment = photos_gegl_buffer_codec_raw_load_increment;
  buffer_codec_class->load_stop = photos_gegl_buffer_codec_raw_load_stop;

  g_object_class_override_property (object_class, PROP_BUFFER, "buffer");
  g_object_class_override_property (object_class, PROP_CAN_SET_SIZE, "can-set-size");
}
