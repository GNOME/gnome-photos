/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 1999 The Free Software Foundation
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

#include <string.h>

#include <glib/gi18n.h>

#include "photos-debug.h"
#include "photos-error.h"
#include "photos-gegl-buffer-codec.h"
#include "photos-gegl-buffer-loader.h"
#include "photos-marshalers.h"


enum
{
  SNIFF_BUFFER_SIZE = 4096
};

struct _PhotosGeglBufferLoader
{
  GObject parent_instance;
  GFile *file;
  PhotosGeglBufferCodec *codec;
  gboolean closed;
  gboolean keep_aspect_ratio;
  gint height;
  gint width;
  guchar header_buf[SNIFF_BUFFER_SIZE];
  gsize header_buf_offset;
};

enum
{
  PROP_0,
  PROP_BUFFER,
  PROP_FILE,
  PROP_HEIGHT,
  PROP_KEEP_ASPECT_RATIO,
  PROP_WIDTH
};

enum
{
  SIZE_PREPARED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };


G_DEFINE_TYPE (PhotosGeglBufferLoader, photos_gegl_buffer_loader, G_TYPE_OBJECT);


static PhotosGeglBufferCodec *
photos_gegl_buffer_loader_find_codec (PhotosGeglBufferLoader *self, GError **error)
{
  GIOExtensionPoint *extension_point;
  GList *extensions;
  GList *l;
  PhotosGeglBufferCodec *ret_val = NULL;
  g_autoptr (PhotosGeglBufferCodec) codec = NULL;
  gboolean uncertain;
  g_autofree gchar *content_type = NULL;
  g_autofree gchar *path = NULL;

  if (self->file != NULL)
    path = g_file_get_path (self->file);

  content_type = g_content_type_guess (NULL, self->header_buf, self->header_buf_offset, &uncertain);
  if ((uncertain
       || g_strcmp0 (content_type, "text/plain") == 0
       || g_strcmp0 (content_type, "application/gzip") == 0)
      && path != NULL)
    {
      g_free (content_type);
      content_type = g_content_type_guess (path, self->header_buf, self->header_buf_offset, NULL);
    }

  extension_point = g_io_extension_point_lookup (PHOTOS_GEGL_BUFFER_CODEC_EXTENSION_POINT_NAME);
  extensions = g_io_extension_point_get_extensions (extension_point);
  for (l = extensions; l != NULL; l = l->next)
    {
      GIOExtension *extension = (GIOExtension *) l->data;
      PhotosGeglBufferCodecClass *buffer_codec_class; /* TODO: use g_autoptr */
      gint i;

      buffer_codec_class = PHOTOS_GEGL_BUFFER_CODEC_CLASS (g_io_extension_ref_class (extension));
      for (i = 0; buffer_codec_class->mime_types[i] != NULL; i++)
        {
          g_autofree gchar *codec_content_type = NULL;

          codec_content_type = g_content_type_from_mime_type (buffer_codec_class->mime_types[i]);
          if (g_content_type_equals (codec_content_type, content_type))
            {
              GType type;

              type = g_io_extension_get_type (extension);
              codec = PHOTOS_GEGL_BUFFER_CODEC (g_object_new (type, NULL));
              break;
            }
        }

      g_type_class_unref (buffer_codec_class);
    }

  if (codec == NULL)
    {
      if (path == NULL)
        {
          g_set_error_literal (error, PHOTOS_ERROR, 0, _("Unrecognized image file format"));
        }
      else
        {
          g_autofree gchar *display_name = NULL;

          display_name = g_filename_display_name (path);
          g_set_error (error,
                       PHOTOS_ERROR,
                       0,
                       _("Couldn’t recognize the image file format for file “%s”"),
                       display_name);
        }

      goto out;
    }

  ret_val = g_object_ref (codec);

 out:
  return ret_val;
}


static void
photos_gegl_buffer_loader_notify_buffer (PhotosGeglBufferLoader *self)
{
  const Babl *format;
  GeglBuffer *buffer;
  GeglRectangle bbox;
  const gchar *type_name;
  const gchar *format_name;

  g_return_if_fail (PHOTOS_IS_GEGL_BUFFER_CODEC (self->codec));

  buffer = photos_gegl_buffer_codec_get_buffer (self->codec);
  g_return_if_fail (GEGL_IS_BUFFER (buffer));

  type_name = G_OBJECT_TYPE_NAME (self->codec);
  bbox = *gegl_buffer_get_extent (buffer);
  format = gegl_buffer_get_format (buffer);
  format_name = babl_get_name (format);
  photos_debug (PHOTOS_DEBUG_GEGL, "GeglBufferLoader: Buffer created by %s: %d, %d, %d×%d, %s",
                type_name,
                bbox.x,
                bbox.y,
                bbox.width,
                bbox.height,
                format_name);

  g_object_notify (G_OBJECT (self), "buffer");
}


static void
photos_gegl_buffer_loader_size_prepared (PhotosGeglBufferLoader *self, guint width, guint height)
{
  gdouble target_height = (gdouble) height;
  gdouble target_width = (gdouble) width;

  g_signal_emit (self, signals[SIZE_PREPARED], 0, width, height);

  if (self->keep_aspect_ratio && (self->height > 0 || self->width > 0))
    {
      if (self->width < 0)
        {
          target_width = (gdouble) width * (gdouble) self->height / (gdouble) height;
          target_height = (gdouble) self->height;
        }
      else if (self->height < 0)
        {
          target_height = (gdouble) height * (gdouble) self->width / (gdouble) width;
          target_width = (gdouble) self->width;
        }
      else if ((gdouble) height / (gdouble) width > (gdouble) self->height / (gdouble) self->width)
        {
          target_width = (gdouble) width * (gdouble) self->height / (gdouble) height;
          target_height = (gdouble) self->height;
        }
      else
        {
          target_height = (gdouble) height * (gdouble) self->width / (gdouble) width;
          target_width = (gdouble) self->width;
        }
    }
  else
    {
      if (self->height > 0)
        target_height = (gdouble) self->height;

      if (self->width > 0)
        target_width = (gdouble) self->width;
    }

  g_return_if_fail (target_height > 0.0);
  g_return_if_fail (target_width > 0.0);

  photos_gegl_buffer_codec_set_height (self->codec, target_height);
  photos_gegl_buffer_codec_set_width (self->codec, target_width);
}


static gboolean
photos_gegl_buffer_loader_load_codec (PhotosGeglBufferLoader *self, const gchar *codec_name, GError **error)
{
  gboolean ret_val = FALSE;

  g_return_val_if_fail (self->codec == NULL, FALSE);

  if (codec_name == NULL)
    {
      self->codec = photos_gegl_buffer_loader_find_codec (self, error);
      if (self->codec == NULL)
        goto out;
    }
  else
    {
      GIOExtension *extension;
      GIOExtensionPoint *extension_point;
      GType type;

      extension_point = g_io_extension_point_lookup (PHOTOS_GEGL_BUFFER_CODEC_EXTENSION_POINT_NAME);
      extension = g_io_extension_point_get_extension_by_name (extension_point, codec_name);
      if (extension == NULL)
        {
          g_set_error (error, PHOTOS_ERROR, 0, _("Codec type “%s” is not supported"), codec_name);
          goto out;
        }

      type = g_io_extension_get_type (extension);
      self->codec = PHOTOS_GEGL_BUFFER_CODEC (g_object_new (type, NULL));
    }

  g_signal_connect_swapped (self->codec,
                            "notify::buffer",
                            G_CALLBACK (photos_gegl_buffer_loader_notify_buffer),
                            self);

  g_signal_connect_swapped (self->codec,
                            "size-prepared",
                            G_CALLBACK (photos_gegl_buffer_loader_size_prepared),
                            self);

  if (!photos_gegl_buffer_codec_load_begin (self->codec, error))
    goto out;

  if (self->header_buf_offset > 0)
    {
      if (!photos_gegl_buffer_codec_load_increment (self->codec, self->header_buf, self->header_buf_offset, error))
        goto out;
    }

  ret_val = TRUE;

 out:
  return ret_val;
}


static gsize
photos_gegl_buffer_loader_eat_header (PhotosGeglBufferLoader *self, const guchar *buf, gsize count, GError **error)
{
  gsize n_bytes_to_eat;
  gsize ret_val = 0;

  g_return_val_if_fail ((gssize) self->header_buf_offset < SNIFF_BUFFER_SIZE, 0);

  n_bytes_to_eat = MIN (SNIFF_BUFFER_SIZE - self->header_buf_offset, count);
  memcpy (self->header_buf + self->header_buf_offset, buf, n_bytes_to_eat);

  self->header_buf_offset += n_bytes_to_eat;
  g_return_val_if_fail (self->header_buf_offset <= SNIFF_BUFFER_SIZE, 0);

  if (self->header_buf_offset == SNIFF_BUFFER_SIZE)
    {
      if (!photos_gegl_buffer_loader_load_codec (self, NULL, error))
        goto out;
    }
  else
    {
      g_return_val_if_fail (n_bytes_to_eat == count, 0);
    }

  ret_val = n_bytes_to_eat;

 out:
  return ret_val;
}


static void
photos_gegl_buffer_loader_dispose (GObject *object)
{
  PhotosGeglBufferLoader *self = PHOTOS_GEGL_BUFFER_LOADER (object);

  g_clear_object (&self->file);
  g_clear_object (&self->codec);

  G_OBJECT_CLASS (photos_gegl_buffer_loader_parent_class)->dispose (object);
}


static void
photos_gegl_buffer_loader_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
  PhotosGeglBufferLoader *self = PHOTOS_GEGL_BUFFER_LOADER (object);

  switch (prop_id)
    {
    case PROP_BUFFER:
      {
        GeglBuffer *buffer;

        buffer = photos_gegl_buffer_loader_get_buffer (self);
        g_value_set_object (value, buffer);
        break;
      }

    case PROP_FILE:
      g_value_set_object (value, self->file);
      break;

    case PROP_HEIGHT:
      g_value_set_int (value, self->height);
      break;

    case PROP_KEEP_ASPECT_RATIO:
      g_value_set_boolean (value, self->keep_aspect_ratio);
      break;

    case PROP_WIDTH:
      g_value_set_int (value, self->width);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
photos_gegl_buffer_loader_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
  PhotosGeglBufferLoader *self = PHOTOS_GEGL_BUFFER_LOADER (object);

  switch (prop_id)
    {
    case PROP_FILE:
      self->file = G_FILE (g_value_dup_object (value));
      break;

    case PROP_HEIGHT:
      self->height = g_value_get_int (value);
      break;

    case PROP_KEEP_ASPECT_RATIO:
      self->keep_aspect_ratio = g_value_get_boolean (value);
      break;

    case PROP_WIDTH:
      self->width = g_value_get_int (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
photos_gegl_buffer_loader_init (PhotosGeglBufferLoader *self)
{
}


static void
photos_gegl_buffer_loader_class_init (PhotosGeglBufferLoaderClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->dispose = photos_gegl_buffer_loader_dispose;
  object_class->get_property = photos_gegl_buffer_loader_get_property;
  object_class->set_property = photos_gegl_buffer_loader_set_property;

  g_object_class_install_property (object_class,
                                   PROP_BUFFER,
                                   g_param_spec_object ("buffer",
                                                        "Buffer",
                                                        "The GeglBuffer being loaded",
                                                        GEGL_TYPE_BUFFER,
                                                        G_PARAM_READABLE
                                                        | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class,
                                   PROP_FILE,
                                   g_param_spec_object ("file",
                                                        "File",
                                                        "The file from which to load a GeglBuffer",
                                                        G_TYPE_FILE,
                                                        G_PARAM_CONSTRUCT_ONLY
                                                        | G_PARAM_READWRITE
                                                        | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class,
                                   PROP_HEIGHT,
                                   g_param_spec_int ("height",
                                                     "Height",
                                                     "The desired height of the GeglBuffer being loaded",
                                                     -1,
                                                     G_MAXINT,
                                                     -1,
                                                     G_PARAM_CONSTRUCT_ONLY
                                                     | G_PARAM_READWRITE
                                                     | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class,
                                   PROP_KEEP_ASPECT_RATIO,
                                   g_param_spec_boolean ("keep-aspect-ratio",
                                                         "Keep aspect ratio",
                                                         "Whether to keep the aspect ratio of the GeglBuffer when "
                                                         "scaling, or not",
                                                         TRUE,
                                                         G_PARAM_CONSTRUCT_ONLY
                                                         | G_PARAM_READWRITE
                                                         | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class,
                                   PROP_WIDTH,
                                   g_param_spec_int ("width",
                                                     "Width",
                                                     "The desired width of the GeglBuffer being loaded",
                                                     -1,
                                                     G_MAXINT,
                                                     -1,
                                                     G_PARAM_CONSTRUCT_ONLY
                                                     | G_PARAM_READWRITE
                                                     | G_PARAM_STATIC_STRINGS));

  signals[SIZE_PREPARED] = g_signal_new ("size-prepared",
                                         G_TYPE_FROM_CLASS (class),
                                         G_SIGNAL_RUN_LAST,
                                         0,
                                         NULL, /* accumulator */
                                         NULL, /* accu_data */
                                         _photos_marshal_VOID__UINT_UINT,
                                         G_TYPE_NONE,
                                         2,
                                         G_TYPE_UINT,
                                         G_TYPE_UINT);
}


GeglBuffer *
photos_gegl_buffer_loader_get_buffer (PhotosGeglBufferLoader *self)
{
  GeglBuffer *ret_val = NULL;

  g_return_val_if_fail (PHOTOS_IS_GEGL_BUFFER_LOADER (self), NULL);

  if (self->codec == NULL)
    goto out;

  ret_val = photos_gegl_buffer_codec_get_buffer (self->codec);

 out:
  return ret_val;
}


GFile *
photos_gegl_buffer_loader_get_file (PhotosGeglBufferLoader *self)
{
  g_return_val_if_fail (PHOTOS_IS_GEGL_BUFFER_LOADER (self), NULL);
  return self->file;
}


gint
photos_gegl_buffer_loader_get_height (PhotosGeglBufferLoader *self)
{
  g_return_val_if_fail (PHOTOS_IS_GEGL_BUFFER_LOADER (self), -1);
  return self->height;
}


gboolean
photos_gegl_buffer_loader_get_keep_aspect_ratio (PhotosGeglBufferLoader *self)
{
  g_return_val_if_fail (PHOTOS_IS_GEGL_BUFFER_LOADER (self), FALSE);
  return self->keep_aspect_ratio;
}


gint
photos_gegl_buffer_loader_get_width (PhotosGeglBufferLoader *self)
{
  g_return_val_if_fail (PHOTOS_IS_GEGL_BUFFER_LOADER (self), -1);
  return self->width;
}


gboolean
photos_gegl_buffer_loader_close (PhotosGeglBufferLoader *self, GError **error)
{
  g_return_val_if_fail (PHOTOS_IS_GEGL_BUFFER_LOADER (self), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  if (self->closed)
    goto out;

  if (self->codec == NULL)
    {
      if (!photos_gegl_buffer_loader_load_codec (self, NULL, error))
        goto out;
    }

  g_return_val_if_fail (PHOTOS_IS_GEGL_BUFFER_CODEC (self->codec), FALSE);

  if (!photos_gegl_buffer_codec_load_stop (self->codec, error))
    goto out;

  self->closed = TRUE;

 out:
  return self->closed;
}


gboolean
photos_gegl_buffer_loader_write_bytes (PhotosGeglBufferLoader *self,
                                       GBytes *bytes,
                                       GCancellable *cancellable,
                                       GError **error)
{
  gboolean ret_val = FALSE;
  gsize count;
  const guchar *data;

  g_return_val_if_fail (PHOTOS_IS_GEGL_BUFFER_LOADER (self), FALSE);
  g_return_val_if_fail (bytes != NULL, FALSE);
  g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  data = (const guchar *) g_bytes_get_data (bytes, &count);
  g_return_val_if_fail (data != NULL, FALSE);
  g_return_val_if_fail (count > 0, FALSE);

  g_return_val_if_fail (!self->closed, FALSE);

  if (self->codec == NULL)
    {
      gssize eaten;

      eaten = photos_gegl_buffer_loader_eat_header (self, data, count, error);
      if (eaten == 0)
        goto out;

      count -= eaten;
      data += eaten;
    }

  g_return_val_if_fail (count == 0 || self->codec != NULL, FALSE);

  if (count > 0)
    {
      if (!photos_gegl_buffer_codec_load_increment (self->codec, data, count, error))
        goto out;
    }

  ret_val = TRUE;

 out:
  return ret_val;
}
