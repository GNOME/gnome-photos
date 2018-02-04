/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2015 – 2017 Red Hat, Inc.
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
 *   + Documents
 */


#include "config.h"

#include <string.h>

#include "photos-error.h"
#include "photos-glib.h"


typedef struct _PhotosGLibFileCopyData PhotosGLibFileCopyData;
typedef struct _PhotosGLibFileCreateData PhotosGLibFileCreateData;

struct _PhotosGLibFileCopyData
{
  GFile *unique_file;
  GFileOutputStream *ostream;
  gint io_priority;
};

struct _PhotosGLibFileCreateData
{
  GFile *dir;
  GFileCreateFlags flags;
  gchar *basename;
  gchar *extension;
  gint io_priority;
  guint count;
};


gboolean
photos_glib_app_info_launch_uri (GAppInfo *appinfo,
                                 const gchar *uri,
                                 GAppLaunchContext *launch_context,
                                 GError **error)
{
  GList *uris = NULL;
  gboolean ret_val;

  g_return_val_if_fail (G_IS_APP_INFO (appinfo), FALSE);
  g_return_val_if_fail (uri != NULL && uri[0] != '\0', FALSE);
  g_return_val_if_fail (launch_context == NULL || G_IS_APP_LAUNCH_CONTEXT (launch_context), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  uris = g_list_prepend (uris, g_strdup (uri));
  ret_val = g_app_info_launch_uris (appinfo, uris, launch_context, error);
  g_list_free_full (uris, g_free);
  return ret_val;
}


static void
photos_glib_file_copy_data_free (PhotosGLibFileCopyData *data)
{
  g_clear_object (&data->unique_file);
  g_clear_object (&data->ostream);
  g_slice_free (PhotosGLibFileCopyData, data);
}


G_DEFINE_AUTOPTR_CLEANUP_FUNC (PhotosGLibFileCopyData, photos_glib_file_copy_data_free);


static PhotosGLibFileCopyData *
photos_glib_file_copy_data_new (gint io_priority)
{
  PhotosGLibFileCopyData *data;

  data = g_slice_new0 (PhotosGLibFileCopyData);
  data->io_priority = io_priority;
  return data;
}


static void
photos_glib_file_copy_splice (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  GOutputStream *ostream = G_OUTPUT_STREAM (source_object);
  g_autoptr (GTask) task = G_TASK (user_data);
  PhotosGLibFileCopyData *data;

  data = (PhotosGLibFileCopyData *) g_task_get_task_data (task);

  g_assert_true (G_IS_FILE_OUTPUT_STREAM (ostream));
  g_assert_true (G_FILE_OUTPUT_STREAM (ostream) == data->ostream);

  {
    g_autoptr (GError) error = NULL;

    g_output_stream_splice_finish (ostream, res, &error);
    if (error != NULL)
      {
        g_task_return_error (task, g_steal_pointer (&error));
        goto out;
      }
  }

  g_task_return_pointer (task, g_object_ref (data->unique_file), g_object_unref);

 out:
  return;
}


static void
photos_glib_file_copy_read (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  GCancellable *cancellable;
  GFile *source = G_FILE (source_object);
  g_autoptr (GFileInputStream) istream = NULL;
  g_autoptr (GTask) task = G_TASK (user_data);
  PhotosGLibFileCopyData *data;

  cancellable = g_task_get_cancellable (task);
  data = (PhotosGLibFileCopyData *) g_task_get_task_data (task);

  {
    g_autoptr (GError) error = NULL;

    istream = g_file_read_finish (source, res, &error);
    if (error != NULL)
      {
        g_task_return_error (task, g_steal_pointer (&error));
        goto out;
      }
  }

  g_output_stream_splice_async (G_OUTPUT_STREAM (data->ostream),
                                G_INPUT_STREAM (istream),
                                G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE | G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET,
                                data->io_priority,
                                cancellable,
                                photos_glib_file_copy_splice,
                                g_object_ref (task));

 out:
  return;
}


static void
photos_glib_file_copy_create (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  GCancellable *cancellable;
  g_autoptr (GFile) unique_file = NULL;
  GFile *destination = G_FILE (source_object);
  GFile *source;
  g_autoptr (GFileOutputStream) ostream = NULL;
  g_autoptr (GTask) task = G_TASK (user_data);
  PhotosGLibFileCopyData *data;

  cancellable = g_task_get_cancellable (task);
  data = (PhotosGLibFileCopyData *) g_task_get_task_data (task);
  source = G_FILE (g_task_get_source_object (task));

  {
    g_autoptr (GError) error = NULL;

    ostream = photos_glib_file_create_finish (destination, res, &unique_file, &error);
    if (error != NULL)
      {
        g_task_return_error (task, g_steal_pointer (&error));
        goto out;
      }
  }

  g_assert_null (data->ostream);
  g_assert_true (G_IS_FILE_OUTPUT_STREAM (ostream));
  data->ostream = g_object_ref (ostream);

  g_assert_null (data->unique_file);
  g_assert_true (G_IS_FILE (unique_file));
  data->unique_file = g_object_ref (unique_file);

  g_file_read_async (source, data->io_priority, cancellable, photos_glib_file_copy_read, g_object_ref (task));

 out:
  return;
}


void
photos_glib_file_copy_async (GFile *source,
                             GFile *destination,
                             GFileCopyFlags flags,
                             gint io_priority,
                             GCancellable *cancellable,
                             GAsyncReadyCallback callback,
                             gpointer user_data)
{
  GFileCreateFlags create_flags = G_FILE_CREATE_NONE;
  g_autoptr (GTask) task = NULL;
  g_autoptr (PhotosGLibFileCopyData) data = NULL;

  g_return_if_fail (G_IS_FILE (source));
  g_return_if_fail (G_IS_FILE (destination));
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (source, cancellable, callback, user_data);
  g_task_set_source_tag (task, photos_glib_file_copy_async);

  data = photos_glib_file_copy_data_new (io_priority);
  g_task_set_task_data (task, g_steal_pointer (&data), (GDestroyNotify) photos_glib_file_copy_data_free);

  if ((flags & G_FILE_COPY_OVERWRITE) != 0)
    create_flags |= G_FILE_CREATE_REPLACE_DESTINATION;

  photos_glib_file_create_async (destination,
                                 create_flags,
                                 io_priority,
                                 cancellable,
                                 photos_glib_file_copy_create,
                                 g_object_ref (task));
}


GFile *
photos_glib_file_copy_finish (GFile *source, GAsyncResult *res, GError **error)
{
  GTask *task;

  g_return_val_if_fail (G_IS_FILE (source), FALSE);

  g_return_val_if_fail (g_task_is_valid (res, source), FALSE);
  task = G_TASK (res);

  g_return_val_if_fail (g_task_get_source_tag (task) == photos_glib_file_copy_async, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  return g_task_propagate_pointer (task, error);
}


static gchar *
photos_glib_filename_get_extension_offset (const gchar *filename)
{
  gchar *end;
  gchar *end2;

  end = strrchr (filename, '.');

  if (end != NULL && end != filename)
    {
      if (g_strcmp0 (end, ".gz") == 0
          || g_strcmp0 (end, ".bz2") == 0
          || g_strcmp0 (end, ".sit") == 0
          || g_strcmp0 (end, ".Z") == 0)
        {
          end2 = end - 1;
          while (end2 > filename && *end2 != '.')
            end2--;
          if (end2 != filename)
            end = end2;
        }
  }

  return end;
}


static void
photos_glib_file_create_data_free (PhotosGLibFileCreateData *data)
{
  g_object_unref (data->dir);
  g_free (data->basename);
  g_free (data->extension);
  g_slice_free (PhotosGLibFileCreateData, data);
}


G_DEFINE_AUTOPTR_CLEANUP_FUNC (PhotosGLibFileCreateData, photos_glib_file_create_data_free);


static gchar *
photos_glib_file_create_data_get_filename (PhotosGLibFileCreateData *data)
{
  gchar *ret_val;

  if (data->count > 0)
    ret_val = g_strdup_printf ("%s(%u)%s", data->basename, data->count, data->extension);
  else
    ret_val = g_strdup_printf ("%s%s", data->basename, data->extension);

  return ret_val;
}


static PhotosGLibFileCreateData *
photos_glib_file_create_data_new (GFile *file, GFileCreateFlags flags, gint io_priority)
{
  PhotosGLibFileCreateData *data;
  g_autofree gchar *filename = NULL;

  data = g_slice_new0 (PhotosGLibFileCreateData);

  filename = g_file_get_basename (file);
  data->dir = g_file_get_parent (file);
  data->basename = photos_glib_filename_strip_extension (filename);
  data->extension = g_strdup (photos_glib_filename_get_extension_offset (filename));
  data->count = 0;
  data->flags = flags;
  data->io_priority = io_priority;

  return data;
}


static void
photos_glib_file_create_create (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  GCancellable *cancellable;
  GFile *file = G_FILE (source_object);
  g_autoptr (GFileOutputStream) stream = NULL;
  g_autoptr (GTask) task = G_TASK (user_data);
  PhotosGLibFileCreateData *data;

  cancellable = g_task_get_cancellable (task);
  data = (PhotosGLibFileCreateData *) g_task_get_task_data (task);

  {
    g_autoptr (GError) error = NULL;

    stream = g_file_create_finish (file, res, &error);
    if (error != NULL)
      {
        g_autoptr (GFile) unique_file = NULL;
        g_autofree gchar *filename = NULL;

        if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_EXISTS))
          {
            g_task_return_error (task, g_steal_pointer (&error));
            goto out;
          }

        if (data->count == G_MAXUINT)
          {
            g_task_return_new_error (task, PHOTOS_ERROR, 0, "Exceeded number of copies of a file");
            goto out;
          }

        data->count++;

        filename = photos_glib_file_create_data_get_filename (data);
        unique_file = g_file_get_child (data->dir, filename);

        g_file_create_async (unique_file,
                             data->flags,
                             data->io_priority,
                             cancellable,
                             photos_glib_file_create_create,
                             g_object_ref (task));

        goto out;
      }
  }

  g_task_return_pointer (task, g_object_ref (stream), g_object_unref);

 out:
  return;
}


void
photos_glib_file_create_async (GFile *file,
                               GFileCreateFlags flags,
                               gint io_priority,
                               GCancellable *cancellable,
                               GAsyncReadyCallback callback,
                               gpointer user_data)
{
  g_autoptr (GTask) task = NULL;
  g_autoptr (PhotosGLibFileCreateData) data = NULL;

  g_return_if_fail (G_IS_FILE (file));
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (file, cancellable, callback, user_data);
  g_task_set_source_tag (task, photos_glib_file_create_async);

  data = photos_glib_file_create_data_new (file, flags, io_priority);
  g_task_set_task_data (task, g_steal_pointer (&data), (GDestroyNotify) photos_glib_file_create_data_free);

  g_file_create_async (file, flags, io_priority, cancellable, photos_glib_file_create_create, g_object_ref (task));
}


GFileOutputStream *
photos_glib_file_create_finish (GFile *file, GAsyncResult *res, GFile **out_unique_file, GError **error)
{
  GTask *task = G_TASK (res);
  GFileOutputStream *ret_val = NULL;
  PhotosGLibFileCreateData *data;

  g_return_val_if_fail (G_IS_FILE (file), NULL);
  g_return_val_if_fail (g_task_is_valid (res, file), NULL);
  g_return_val_if_fail (g_task_get_source_tag (task) == photos_glib_file_create_async, NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  data = (PhotosGLibFileCreateData *) g_task_get_task_data (task);
  g_return_val_if_fail (data != NULL, NULL);

  ret_val = g_task_propagate_pointer (task, error);
  if (ret_val == NULL)
    goto out;

  if (out_unique_file != NULL)
    {
      GFile *unique_file;
      g_autofree gchar *filename = NULL;

      filename = photos_glib_file_create_data_get_filename (data);
      unique_file = g_file_get_child (data->dir, filename);
      *out_unique_file = unique_file;
    }

 out:
  return ret_val;
}


gchar *
photos_glib_filename_strip_extension (const gchar *filename_with_extension)
{
  gchar *end;
  gchar *filename;

  if (filename_with_extension == NULL)
    return NULL;

  filename = g_strdup (filename_with_extension);
  end = photos_glib_filename_get_extension_offset (filename);

  if (end != NULL && end != filename)
    *end = '\0';

  return filename;
}


gboolean
photos_glib_make_directory_with_parents (GFile *file, GCancellable *cancellable, GError **error)
{
  GError *local_error = NULL;
  gboolean ret_val;

  g_return_val_if_fail (G_IS_FILE (file), FALSE);
  g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  ret_val = g_file_make_directory_with_parents (file, cancellable, &local_error);
  if (local_error != NULL)
    {
      if (g_error_matches (local_error, G_IO_ERROR, G_IO_ERROR_EXISTS))
        {
          g_clear_error (&local_error);
          ret_val = TRUE;
        }
    }

  if (local_error != NULL)
    g_propagate_error (error, local_error);

  return ret_val;
}
