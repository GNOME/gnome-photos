/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2017 Red Hat, Inc.
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

#include "photos-pixbuf.h"


typedef struct _PhotosPixbufNewFromFileData PhotosPixbufNewFromFileData;

struct _PhotosPixbufNewFromFileData
{
  gchar *filename;
  gint height;
  gint width;
};


static PhotosPixbufNewFromFileData *
photos_pixbuf_new_from_file_data_new (const gchar *filename, gint height, gint width)
{
  PhotosPixbufNewFromFileData *data;

  data = g_slice_new0 (PhotosPixbufNewFromFileData);
  data->filename = g_strdup (filename);
  data->height = height;
  data->width = width;

  return data;
}


static void
photos_pixbuf_new_from_file_data_free (PhotosPixbufNewFromFileData *data)
{
  g_free (data->filename);
  g_slice_free (PhotosPixbufNewFromFileData, data);
}


static void
photos_pixbuf_new_from_file_at_size_in_thread_func (GTask *task,
                                                    gpointer source_object,
                                                    gpointer task_data,
                                                    GCancellable *cancellable)
{
  GError *error;
  GdkPixbuf *result = NULL;
  PhotosPixbufNewFromFileData *data = (PhotosPixbufNewFromFileData *) task_data;

  error = NULL;
  result = gdk_pixbuf_new_from_file_at_size (data->filename, data->width, data->height, &error);
  if (error != NULL)
    {
      g_task_return_error (task, error);
      goto out;
    }

  g_task_return_pointer (task, g_object_ref (result), g_object_unref);

 out:
  g_clear_object (&result);
}


void
photos_pixbuf_new_from_file_at_size_async (const gchar *filename,
                                           gint width,
                                           gint height,
                                           GCancellable *cancellable,
                                           GAsyncReadyCallback callback,
                                           gpointer user_data)
{
  GTask *task;
  PhotosPixbufNewFromFileData *data;

  g_return_if_fail (filename != NULL && filename[0] != '\0');
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  data = photos_pixbuf_new_from_file_data_new (filename, height, width);

  task = g_task_new (NULL, cancellable, callback, user_data);
  g_task_set_return_on_cancel (task, TRUE);
  g_task_set_source_tag (task, photos_pixbuf_new_from_file_at_size_async);
  g_task_set_task_data (task, data, (GDestroyNotify) photos_pixbuf_new_from_file_data_free);

  g_task_run_in_thread (task, photos_pixbuf_new_from_file_at_size_in_thread_func);
  g_object_unref (task);
}


GdkPixbuf *
photos_pixbuf_new_from_file_at_size_finish (GAsyncResult *res, GError **error)
{
  GTask *task = G_TASK (res);

  g_return_val_if_fail (g_task_is_valid (res, NULL), NULL);
  g_return_val_if_fail (g_task_get_source_tag (task) == photos_pixbuf_new_from_file_at_size_async, NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  return g_task_propagate_pointer (task, error);
}
