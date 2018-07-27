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

#include "photos-gegl-buffer-io.h"
#include "photos-gegl-buffer-loader.h"
#include "photos-gegl-buffer-loader-builder.h"


enum
{
  LOAD_BUFFER_SIZE = 32768
};


static GeglBuffer *
photos_gegl_buffer_load_from_stream (PhotosGeglBufferLoader *loader,
                                     GInputStream *stream,
                                     GCancellable *cancellable,
                                     GError **error)
{
  GeglBuffer *ret_val = NULL;

  while (TRUE)
    {
      g_autoptr (GBytes) bytes = NULL;
      gsize size;

      bytes = g_input_stream_read_bytes (stream, LOAD_BUFFER_SIZE, cancellable, error);
      if (bytes == NULL)
        {
          photos_gegl_buffer_loader_close (loader, NULL);
          goto out;
        }

      size = g_bytes_get_size (bytes);
      if (size == 0)
        break;

      if (!photos_gegl_buffer_loader_write_bytes (loader, bytes, cancellable, error))
        {
          photos_gegl_buffer_loader_close (loader, NULL);
          goto out;
        }
    }

  if (!photos_gegl_buffer_loader_close (loader, error))
    goto out;

  ret_val = photos_gegl_buffer_loader_get_buffer (loader);
  g_object_ref (ret_val);

 out:
  return ret_val;
}


static void
photos_gegl_buffer_new_from_stream_read_bytes (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  g_autoptr (GBytes) bytes = NULL;
  GCancellable *cancellable;
  GInputStream *stream = G_INPUT_STREAM (source_object);
  g_autoptr (GTask) task = G_TASK (user_data);
  PhotosGeglBufferLoader *loader;
  gint priority;
  gsize size;

  cancellable = g_task_get_cancellable (task);
  loader = PHOTOS_GEGL_BUFFER_LOADER (g_task_get_source_object (task));
  priority = GPOINTER_TO_INT (g_task_get_task_data (task));

  {
    g_autoptr (GError) error = NULL;

    bytes = g_input_stream_read_bytes_finish (stream, res, &error);
    if (error != NULL)
      {
        photos_gegl_buffer_loader_close (loader, NULL);
        g_task_return_error (task, g_steal_pointer (&error));
        goto out;
      }
  }

  size = g_bytes_get_size (bytes);
  if (size > 0)
    {
      {
        g_autoptr (GError) error = NULL;

        if (!photos_gegl_buffer_loader_write_bytes (loader, bytes, cancellable, &error))
          {
            photos_gegl_buffer_loader_close (loader, NULL);
            g_task_return_error (task, g_steal_pointer (&error));
            goto out;
          }
      }

      g_input_stream_read_bytes_async (stream,
                                       LOAD_BUFFER_SIZE,
                                       priority,
                                       cancellable,
                                       photos_gegl_buffer_new_from_stream_read_bytes,
                                       g_object_ref (task));
    }
  else
    {
      GeglBuffer *buffer;

      {
        g_autoptr (GError) error = NULL;

        if (!photos_gegl_buffer_loader_close (loader, &error))
          {
            g_task_return_error (task, g_steal_pointer (&error));
            goto out;
          }
      }

      buffer = photos_gegl_buffer_loader_get_buffer (loader);
      g_task_return_pointer (task, g_object_ref (buffer), g_object_unref);
    }

  out:
    return;
}


static void
photos_gegl_buffer_new_from_file_read (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  GCancellable *cancellable;
  GFile *file = G_FILE (source_object);
  g_autoptr (GFileInputStream) stream = NULL;
  g_autoptr (GTask) task = G_TASK (user_data);
  gint priority;

  cancellable = g_task_get_cancellable (task);
  priority = GPOINTER_TO_INT (g_task_get_task_data (task));

  {
    g_autoptr (GError) error = NULL;

    stream = g_file_read_finish (file, res, &error);
    if (error != NULL)
      {
        g_task_return_error (task, g_steal_pointer (&error));
        goto out;
      }
  }

  g_input_stream_read_bytes_async (G_INPUT_STREAM (stream),
                                   LOAD_BUFFER_SIZE,
                                   priority,
                                   cancellable,
                                   photos_gegl_buffer_new_from_stream_read_bytes,
                                   g_object_ref (task));

 out:
  return;
}


GeglBuffer *
photos_gegl_buffer_new_from_file (GFile *file, GCancellable *cancellable, GError **error)
{
  g_autoptr (GFileInputStream) stream = NULL;
  GeglBuffer *ret_val = NULL;
  g_autoptr (GeglBuffer) buffer = NULL;
  g_autoptr (PhotosGeglBufferLoader) loader = NULL;
  g_autoptr (PhotosGeglBufferLoaderBuilder) builder = NULL;

  g_return_val_if_fail (G_IS_FILE (file), NULL);
  g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  stream = g_file_read (file, cancellable, error);
  if (stream == NULL)
    goto out;

  builder = photos_gegl_buffer_loader_builder_new ();
  photos_gegl_buffer_loader_builder_set_file (builder, file);
  loader = photos_gegl_buffer_loader_builder_to_loader (builder);

  buffer = photos_gegl_buffer_load_from_stream (loader, G_INPUT_STREAM (stream), cancellable, error);
  if (buffer == NULL)
    goto out;

  ret_val = g_object_ref (buffer);

 out:
  return ret_val;
}


void
photos_gegl_buffer_new_from_file_async (GFile *file,
                                        gint priority,
                                        GCancellable *cancellable,
                                        GAsyncReadyCallback callback,
                                        gpointer user_data)
{
  g_autoptr (GTask) task = NULL;
  g_autoptr (PhotosGeglBufferLoader) loader = NULL;
  g_autoptr (PhotosGeglBufferLoaderBuilder) builder = NULL;

  g_return_if_fail (G_IS_FILE (file));
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  builder = photos_gegl_buffer_loader_builder_new ();
  photos_gegl_buffer_loader_builder_set_file (builder, file);
  loader = photos_gegl_buffer_loader_builder_to_loader (builder);

  task = g_task_new (loader, cancellable, callback, user_data);
  g_task_set_source_tag (task, photos_gegl_buffer_new_from_file_async);
  g_task_set_task_data (task, GINT_TO_POINTER (priority), NULL);

  g_file_read_async (file, priority, cancellable, photos_gegl_buffer_new_from_file_read, g_object_ref (task));
}


GeglBuffer *
photos_gegl_buffer_new_from_file_at_scale (GFile *file,
                                           gint width,
                                           gint height,
                                           gboolean keep_aspect_ratio,
                                           GCancellable *cancellable,
                                           GError **error)
{
  g_autoptr (GFileInputStream) stream = NULL;
  GeglBuffer *ret_val = NULL;
  g_autoptr (GeglBuffer) buffer = NULL;
  g_autoptr (PhotosGeglBufferLoader) loader = NULL;
  g_autoptr (PhotosGeglBufferLoaderBuilder) builder = NULL;

  g_return_val_if_fail (G_IS_FILE (file), NULL);
  g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  stream = g_file_read (file, cancellable, error);
  if (stream == NULL)
    goto out;

  builder = photos_gegl_buffer_loader_builder_new ();
  photos_gegl_buffer_loader_builder_set_file (builder, file);
  photos_gegl_buffer_loader_builder_set_height (builder, height);
  photos_gegl_buffer_loader_builder_set_keep_aspect_ratio (builder, keep_aspect_ratio);
  photos_gegl_buffer_loader_builder_set_width (builder, width);
  loader = photos_gegl_buffer_loader_builder_to_loader (builder);

  buffer = photos_gegl_buffer_load_from_stream (loader, G_INPUT_STREAM (stream), cancellable, error);
  if (buffer == NULL)
    goto out;

  ret_val = g_object_ref (buffer);

 out:
  return ret_val;
}


void
photos_gegl_buffer_new_from_file_at_scale_async (GFile *file,
                                                 gint width,
                                                 gint height,
                                                 gboolean keep_aspect_ratio,
                                                 gint priority,
                                                 GCancellable *cancellable,
                                                 GAsyncReadyCallback callback,
                                                 gpointer user_data)
{
  g_autoptr (GTask) task = NULL;
  g_autoptr (PhotosGeglBufferLoader) loader = NULL;
  g_autoptr (PhotosGeglBufferLoaderBuilder) builder = NULL;

  g_return_if_fail (G_IS_FILE (file));
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  builder = photos_gegl_buffer_loader_builder_new ();
  photos_gegl_buffer_loader_builder_set_file (builder, file);
  photos_gegl_buffer_loader_builder_set_height (builder, height);
  photos_gegl_buffer_loader_builder_set_keep_aspect_ratio (builder, keep_aspect_ratio);
  photos_gegl_buffer_loader_builder_set_width (builder, width);
  loader = photos_gegl_buffer_loader_builder_to_loader (builder);

  task = g_task_new (loader, cancellable, callback, user_data);
  g_task_set_source_tag (task, photos_gegl_buffer_new_from_file_at_scale_async);
  g_task_set_task_data (task, GINT_TO_POINTER (priority), NULL);

  g_file_read_async (file, priority, cancellable, photos_gegl_buffer_new_from_file_read, g_object_ref (task));
}


GeglBuffer *
photos_gegl_buffer_new_from_file_finish (GAsyncResult *res, GError **error)
{
  GTask *task;

  g_return_val_if_fail (G_IS_TASK (res), NULL);
  task = G_TASK (res);

  g_return_val_if_fail (g_task_get_source_tag (task) == photos_gegl_buffer_new_from_file_async
                        || g_task_get_source_tag (task) == photos_gegl_buffer_new_from_file_at_scale_async,
                        NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  return g_task_propagate_pointer (task, error);
}


GeglBuffer *
photos_gegl_buffer_new_from_stream (GInputStream *stream, GCancellable *cancellable, GError **error)
{
  GeglBuffer *ret_val = NULL;
  g_autoptr (GeglBuffer) buffer = NULL;
  g_autoptr (PhotosGeglBufferLoader) loader = NULL;
  g_autoptr (PhotosGeglBufferLoaderBuilder) builder = NULL;

  g_return_val_if_fail (G_IS_INPUT_STREAM (stream), NULL);
  g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  builder = photos_gegl_buffer_loader_builder_new ();
  loader = photos_gegl_buffer_loader_builder_to_loader (builder);

  buffer = photos_gegl_buffer_load_from_stream (loader, stream, cancellable, error);
  if (buffer == NULL)
    goto out;

  ret_val = g_object_ref (buffer);

 out:
  return ret_val;
}


void
photos_gegl_buffer_new_from_stream_async (GInputStream *stream,
                                          gint priority,
                                          GCancellable *cancellable,
                                          GAsyncReadyCallback callback,
                                          gpointer user_data)
{
  g_autoptr (GTask) task = NULL;
  g_autoptr (PhotosGeglBufferLoader) loader = NULL;
  g_autoptr (PhotosGeglBufferLoaderBuilder) builder = NULL;

  g_return_if_fail (G_IS_INPUT_STREAM (stream));
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  builder = photos_gegl_buffer_loader_builder_new ();
  loader = photos_gegl_buffer_loader_builder_to_loader (builder);

  task = g_task_new (loader, cancellable, callback, user_data);
  g_task_set_source_tag (task, photos_gegl_buffer_new_from_stream_async);
  g_task_set_task_data (task, GINT_TO_POINTER (priority), NULL);

  g_input_stream_read_bytes_async (stream,
                                   LOAD_BUFFER_SIZE,
                                   priority,
                                   cancellable,
                                   photos_gegl_buffer_new_from_stream_read_bytes,
                                   g_object_ref (task));
}


GeglBuffer *
photos_gegl_buffer_new_from_stream_at_scale (GInputStream *stream,
                                             gint width,
                                             gint height,
                                             gboolean keep_aspect_ratio,
                                             GCancellable *cancellable,
                                             GError **error)
{
  GeglBuffer *ret_val = NULL;
  g_autoptr (GeglBuffer) buffer = NULL;
  g_autoptr (PhotosGeglBufferLoader) loader = NULL;
  g_autoptr (PhotosGeglBufferLoaderBuilder) builder = NULL;

  g_return_val_if_fail (G_IS_INPUT_STREAM (stream), NULL);
  g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  builder = photos_gegl_buffer_loader_builder_new ();
  photos_gegl_buffer_loader_builder_set_height (builder, height);
  photos_gegl_buffer_loader_builder_set_keep_aspect_ratio (builder, keep_aspect_ratio);
  photos_gegl_buffer_loader_builder_set_width (builder, width);
  loader = photos_gegl_buffer_loader_builder_to_loader (builder);

  buffer = photos_gegl_buffer_load_from_stream (loader, stream, cancellable, error);
  if (buffer == NULL)
    goto out;

  ret_val = g_object_ref (buffer);

 out:
  return ret_val;
}


void
photos_gegl_buffer_new_from_stream_at_scale_async (GInputStream *stream,
                                                   gint width,
                                                   gint height,
                                                   gboolean keep_aspect_ratio,
                                                   gint priority,
                                                   GCancellable *cancellable,
                                                   GAsyncReadyCallback callback,
                                                   gpointer user_data)
{
  g_autoptr (GTask) task = NULL;
  g_autoptr (PhotosGeglBufferLoader) loader = NULL;
  g_autoptr (PhotosGeglBufferLoaderBuilder) builder = NULL;

  g_return_if_fail (G_IS_INPUT_STREAM (stream));
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  builder = photos_gegl_buffer_loader_builder_new ();
  photos_gegl_buffer_loader_builder_set_height (builder, height);
  photos_gegl_buffer_loader_builder_set_keep_aspect_ratio (builder, keep_aspect_ratio);
  photos_gegl_buffer_loader_builder_set_width (builder, width);
  loader = photos_gegl_buffer_loader_builder_to_loader (builder);

  task = g_task_new (loader, cancellable, callback, user_data);
  g_task_set_source_tag (task, photos_gegl_buffer_new_from_stream_at_scale_async);
  g_task_set_task_data (task, GINT_TO_POINTER (priority), NULL);

  g_input_stream_read_bytes_async (stream,
                                   LOAD_BUFFER_SIZE,
                                   priority,
                                   cancellable,
                                   photos_gegl_buffer_new_from_stream_read_bytes,
                                   g_object_ref (task));
}


GeglBuffer *
photos_gegl_buffer_new_from_stream_finish (GAsyncResult *res, GError **error)
{
  GTask *task;

  g_return_val_if_fail (G_IS_TASK (res), NULL);
  task = G_TASK (res);

  g_return_val_if_fail (g_task_get_source_tag (task) == photos_gegl_buffer_new_from_stream_async
                        || g_task_get_source_tag (task) == photos_gegl_buffer_new_from_stream_at_scale_async,
                        NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  return g_task_propagate_pointer (task, error);
}
