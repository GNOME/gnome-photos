/*
 * Photos - access, organize and share your photos on GNOME
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
 */


#include "config.h"

#include "photos-gegl-buffer-codec.h"
#include "photos-marshalers.h"


struct _PhotosGeglBufferCodecPrivate
{
  gboolean decoding;
  gboolean used;
  gdouble height;
  gdouble width;
};

enum
{
  PROP_0,
  PROP_BUFFER,
  PROP_CAN_SET_SIZE,
  PROP_HEIGHT,
  PROP_WIDTH
};

enum
{
  SIZE_PREPARED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };


G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (PhotosGeglBufferCodec, photos_gegl_buffer_codec, G_TYPE_OBJECT);


static void
photos_gegl_buffer_codec_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
  PhotosGeglBufferCodec *self = PHOTOS_GEGL_BUFFER_CODEC (object);
  PhotosGeglBufferCodecPrivate *priv;

  priv = photos_gegl_buffer_codec_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_HEIGHT:
      g_value_set_double (value, priv->height);
      break;

    case PROP_WIDTH:
      g_value_set_double (value, priv->width);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
photos_gegl_buffer_codec_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
  PhotosGeglBufferCodec *self = PHOTOS_GEGL_BUFFER_CODEC (object);

  switch (prop_id)
    {
    case PROP_HEIGHT:
      {
        gdouble height;

        height = g_value_get_double (value);
        photos_gegl_buffer_codec_set_height (self, height);
        break;
      }

    case PROP_WIDTH:
      {
        gdouble width;

        width = g_value_get_double (value);
        photos_gegl_buffer_codec_set_width (self, width);
        break;
      }

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
photos_gegl_buffer_codec_init (PhotosGeglBufferCodec *self)
{
}


static void
photos_gegl_buffer_codec_class_init (PhotosGeglBufferCodecClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->get_property = photos_gegl_buffer_codec_get_property;
  object_class->set_property = photos_gegl_buffer_codec_set_property;

  g_object_class_install_property (object_class,
                                   PROP_BUFFER,
                                   g_param_spec_object ("buffer",
                                                        "Buffer",
                                                        "The GeglBuffer being loaded",
                                                        GEGL_TYPE_BUFFER,
                                                        G_PARAM_READABLE
                                                        | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class,
                                   PROP_CAN_SET_SIZE,
                                   g_param_spec_boolean ("can-set-size",
                                                         "Can set size",
                                                         "Whether the desired size of the GeglBuffer can be "
                                                         "changed, or not",
                                                         FALSE,
                                                         G_PARAM_READABLE
                                                         | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class,
                                   PROP_HEIGHT,
                                   g_param_spec_double ("height",
                                                        "Height",
                                                        "The desired height of the GeglBuffer being loaded",
                                                        -1.0,
                                                        G_MAXDOUBLE,
                                                        -1.0,
                                                        G_PARAM_EXPLICIT_NOTIFY
                                                        | G_PARAM_READWRITE
                                                        | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class,
                                   PROP_WIDTH,
                                   g_param_spec_double ("width",
                                                        "Width",
                                                        "The desired width of the GeglBuffer being loaded",
                                                        -1.0,
                                                        G_MAXDOUBLE,
                                                        -1.0,
                                                        G_PARAM_EXPLICIT_NOTIFY
                                                        | G_PARAM_READWRITE
                                                        | G_PARAM_STATIC_STRINGS));

  signals[SIZE_PREPARED] = g_signal_new ("size-prepared",
                                         G_TYPE_FROM_CLASS (class),
                                         G_SIGNAL_RUN_LAST,
                                         G_STRUCT_OFFSET (PhotosGeglBufferCodecClass, size_prepared),
                                         NULL, /* accumulator */
                                         NULL, /* accu_data */
                                         _photos_marshal_VOID__UINT_UINT,
                                         G_TYPE_NONE,
                                         2,
                                         G_TYPE_UINT,
                                         G_TYPE_UINT);
}


GeglBuffer *
photos_gegl_buffer_codec_get_buffer (PhotosGeglBufferCodec *self)
{
  g_return_val_if_fail (PHOTOS_IS_GEGL_BUFFER_CODEC (self), NULL);
  return PHOTOS_GEGL_BUFFER_CODEC_GET_CLASS (self)->get_buffer (self);
}


gboolean
photos_gegl_buffer_codec_get_can_set_size (PhotosGeglBufferCodec *self)
{
  gboolean can_set_size;

  g_return_val_if_fail (PHOTOS_IS_GEGL_BUFFER_CODEC (self), FALSE);

  g_object_get (self, "can-set-size", &can_set_size, NULL);
  return can_set_size;
}


gdouble
photos_gegl_buffer_codec_get_height (PhotosGeglBufferCodec *self)
{
  PhotosGeglBufferCodecPrivate *priv;

  g_return_val_if_fail (PHOTOS_IS_GEGL_BUFFER_CODEC (self), 0.0);
  priv = photos_gegl_buffer_codec_get_instance_private (self);

  return priv->height;
}


gdouble
photos_gegl_buffer_codec_get_width (PhotosGeglBufferCodec *self)
{
  PhotosGeglBufferCodecPrivate *priv;

  g_return_val_if_fail (PHOTOS_IS_GEGL_BUFFER_CODEC (self), 0.0);
  priv = photos_gegl_buffer_codec_get_instance_private (self);

  return priv->width;
}


gboolean
photos_gegl_buffer_codec_load_begin (PhotosGeglBufferCodec *self, GError **error)
{
  PhotosGeglBufferCodecPrivate *priv;
  gboolean ret_val;

  g_return_val_if_fail (PHOTOS_IS_GEGL_BUFFER_CODEC (self), FALSE);
  priv = photos_gegl_buffer_codec_get_instance_private (self);

  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);
  g_return_val_if_fail (!priv->decoding, FALSE);
  g_return_val_if_fail (!priv->used, FALSE);

  ret_val = PHOTOS_GEGL_BUFFER_CODEC_GET_CLASS (self)->load_begin (self, error);

  if (ret_val)
    priv->decoding = TRUE;

  priv->used = TRUE;

  return ret_val;
}


gboolean
photos_gegl_buffer_codec_load_increment (PhotosGeglBufferCodec *self,
                                         const guchar *buf,
                                         gsize count,
                                         GError **error)
{
  PhotosGeglBufferCodecPrivate *priv;
  gboolean ret_val;

  g_return_val_if_fail (PHOTOS_IS_GEGL_BUFFER_CODEC (self), FALSE);
  priv = photos_gegl_buffer_codec_get_instance_private (self);

  g_return_val_if_fail (buf != NULL, FALSE);
  g_return_val_if_fail (count > 0, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);
  g_return_val_if_fail (priv->decoding, FALSE);

  ret_val = PHOTOS_GEGL_BUFFER_CODEC_GET_CLASS (self)->load_increment (self, buf, count, error);
  return ret_val;
}


gboolean
photos_gegl_buffer_codec_load_stop (PhotosGeglBufferCodec *self, GError **error)
{
  PhotosGeglBufferCodecPrivate *priv;
  gboolean ret_val;

  g_return_val_if_fail (PHOTOS_IS_GEGL_BUFFER_CODEC (self), FALSE);
  priv = photos_gegl_buffer_codec_get_instance_private (self);

  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);
  g_return_val_if_fail (priv->decoding, FALSE);

  ret_val = PHOTOS_GEGL_BUFFER_CODEC_GET_CLASS (self)->load_stop (self, error);

  priv->decoding = FALSE;

  return ret_val;
}


void
photos_gegl_buffer_codec_set_height (PhotosGeglBufferCodec *self, gdouble height)
{
  PhotosGeglBufferCodecPrivate *priv;

  g_return_if_fail (PHOTOS_IS_GEGL_BUFFER_CODEC (self));
  priv = photos_gegl_buffer_codec_get_instance_private (self);

  g_return_if_fail (photos_gegl_buffer_codec_get_can_set_size (self));

  if (priv->height != height)
    {
      priv->height = height;
      g_object_notify (G_OBJECT (self), "height");
    }
}


void
photos_gegl_buffer_codec_set_width (PhotosGeglBufferCodec *self, gdouble width)
{
  PhotosGeglBufferCodecPrivate *priv;

  g_return_if_fail (PHOTOS_IS_GEGL_BUFFER_CODEC (self));
  priv = photos_gegl_buffer_codec_get_instance_private (self);

  g_return_if_fail (photos_gegl_buffer_codec_get_can_set_size (self));

  if (priv->width != width)
    {
      priv->width = width;
      g_object_notify (G_OBJECT (self), "width");
    }
}
