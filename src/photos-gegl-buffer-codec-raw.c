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

#include <gegl.h>
#include <glib.h>
#include <glib/gi18n.h>

#include "photos-error.h"
#include "photos-gegl.h"
#include "photos-gegl-buffer-codec-raw.h"
#include "photos-rawspeed.h"


struct _PhotosGeglBufferCodecRaw
{
  PhotosGeglBufferCodec parent_instance;
  GQueue bytes_queue;
  GeglBuffer *buffer;
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


static GeglBuffer *
photos_gegl_buffer_codec_raw_get_buffer (PhotosGeglBufferCodec *codec)
{
  PhotosGeglBufferCodecRaw *self = PHOTOS_GEGL_BUFFER_CODEC_RAW (codec);
  return self->buffer;
}


static gboolean
photos_gegl_buffer_codec_raw_load_begin (PhotosGeglBufferCodec *codec, GError **error)
{
  PhotosGeglBufferCodecRaw *self = PHOTOS_GEGL_BUFFER_CODEC_RAW (codec);

  g_return_val_if_fail (!self->decoding, FALSE);

  self->decoding = TRUE;
  return self->decoding;
}


static gboolean
photos_gegl_buffer_codec_raw_load_increment (PhotosGeglBufferCodec *codec, GBytes *bytes, GError **error)
{
  PhotosGeglBufferCodecRaw *self = PHOTOS_GEGL_BUFFER_CODEC_RAW (codec);
  gsize count;

  g_return_val_if_fail (self->decoding, FALSE);

  count = g_bytes_get_size (bytes);
  g_return_val_if_fail (count > 0, FALSE);

  g_queue_push_tail (&self->bytes_queue, g_bytes_ref (bytes));
  return TRUE;
}


static gboolean
photos_gegl_buffer_codec_raw_load_stop (PhotosGeglBufferCodec *codec, GError **error)
{
  PhotosGeglBufferCodecRaw *self = PHOTOS_GEGL_BUFFER_CODEC_RAW (codec);
  g_autoptr (GBytes) bytes_flattened = NULL;
  gboolean ret_val = FALSE;

  g_return_val_if_fail (self->decoding, FALSE);

  if (g_queue_is_empty (&self->bytes_queue))
    {
      g_set_error (error, PHOTOS_ERROR, 0, _("Premature end-of-file encountered"));
      goto out;
    }

  {
    GByteArray *byte_array = NULL;
    GList *l;

    {
      GBytes *bytes = NULL;

      bytes = (GBytes *) g_queue_pop_head (&self->bytes_queue);
      byte_array = g_bytes_unref_to_array (bytes);
    }

    for (l = self->bytes_queue.head; l != NULL; l = l->next)
      {
        GBytes *bytes = (GBytes *) l->data;
        gsize count;
        const guint8 *data;

        data = (const guint8 *) g_bytes_get_data (bytes, &count);
        g_byte_array_append (byte_array, data, (guint) count);
      }

    bytes_flattened = g_byte_array_free_to_bytes (byte_array);
  }

  self->buffer = photos_rawspeed_decode_bytes (bytes_flattened, error);
  if (self->buffer == NULL)
    goto out;

  ret_val = TRUE;

 out:
  self->decoding = FALSE;
  return ret_val;
}


static void
photos_gegl_buffer_codec_raw_dispose (GObject *object)
{
  PhotosGeglBufferCodecRaw *self = PHOTOS_GEGL_BUFFER_CODEC_RAW (object);

  g_clear_object (&self->buffer);

  G_OBJECT_CLASS (photos_gegl_buffer_codec_raw_parent_class)->dispose (object);
}


static void
photos_gegl_buffer_codec_raw_finalize (GObject *object)
{
  PhotosGeglBufferCodecRaw *self = PHOTOS_GEGL_BUFFER_CODEC_RAW (object);

  g_queue_foreach (&self->bytes_queue, (GFunc) g_bytes_unref, NULL);

  G_OBJECT_CLASS (photos_gegl_buffer_codec_raw_parent_class)->finalize (object);
}


static void
photos_gegl_buffer_codec_raw_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
  PhotosGeglBufferCodecRaw *self = PHOTOS_GEGL_BUFFER_CODEC_RAW (object);

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
photos_gegl_buffer_codec_raw_init (PhotosGeglBufferCodecRaw *self)
{
  g_queue_init (&self->bytes_queue);
}


static void
photos_gegl_buffer_codec_raw_class_init (PhotosGeglBufferCodecRawClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  PhotosGeglBufferCodecClass *buffer_codec_class = PHOTOS_GEGL_BUFFER_CODEC_CLASS (class);

  {
    GList *content_types = NULL;
    GList *l;
    GPtrArray *content_types_raw = NULL;

    content_types = g_content_types_get_registered ();
    content_types_raw = g_ptr_array_new ();
    for (l = content_types; l != NULL; l = l->next)
      {
        const gchar *content_type = (gchar *) l->data;

        if (!g_content_type_is_a (content_type, "image/x-dcraw"))
          continue;

        g_ptr_array_add (content_types_raw, (gpointer) content_type);
        l->data = NULL;
      }

    g_ptr_array_add (content_types_raw, NULL);
    buffer_codec_class->mime_types = (GStrv) g_ptr_array_free (content_types_raw, FALSE);
    g_list_free_full (content_types, g_free);
  }

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
