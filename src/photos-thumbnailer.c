/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2017 – 2021 Red Hat, Inc.
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

#include <locale.h>
#include <stdlib.h>

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gegl.h>
#include <glib/gi18n.h>

#include "photos-debug.h"
#include "photos-error.h"
#include "photos-gegl.h"
#include "photos-pipeline.h"
#include "photos-pixbuf.h"
#include "photos-thumbnailer.h"
#include "photos-thumbnailer-dbus.h"


struct _PhotosThumbnailer
{
  GApplication parent_instance;
  GDBusConnection *connection;
  GHashTable *cancellables;
  PhotosThumbnailerDBus *skeleton;
  gchar *address;
};


G_DEFINE_TYPE (PhotosThumbnailer, photos_thumbnailer, G_TYPE_APPLICATION);


typedef struct _PhotosThumbnailerGenerateData PhotosThumbnailerGenerateData;

struct _PhotosThumbnailerGenerateData
{
  GFile *file;
  GFileOutputStream *stream;
  GQuark orientation;
  GdkPixbuf *pixbuf_thumbnail;
  GeglNode *graph;
  PhotosPipeline *pipeline;
  gchar *thumbnail_path;
  gint thumbnail_size;
  gint64 original_height;
  gint64 original_width;
};

enum
{
  INACTIVITY_TIMEOUT = 12000 /* ms */
};

static const GOptionEntry COMMAND_LINE_OPTIONS[] =
{
  { "address", 0, G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING, NULL, N_("D-Bus address to use"), NULL},
  { NULL }
};

static const gchar *THUMBNAILER_PATH = "/org/gnome/Photos/Thumbnailer";


static void
photos_thumbnailer_generate_data_free (PhotosThumbnailerGenerateData *data)
{
  g_free (data->thumbnail_path);
  g_clear_object (&data->file);
  g_clear_object (&data->graph);
  g_clear_object (&data->pipeline);
  g_clear_object (&data->pixbuf_thumbnail);
  g_clear_object (&data->stream);
  g_slice_free (PhotosThumbnailerGenerateData, data);
}


static PhotosThumbnailerGenerateData *
photos_thumbnailer_generate_data_new (GFile *file,
                                      GQuark orientation,
                                      gint64 original_height,
                                      gint64 original_width,
                                      const gchar *thumbnail_path,
                                      gint thumbnail_size,
                                      GeglNode *graph)
{
  PhotosThumbnailerGenerateData *data;

  data = g_slice_new0 (PhotosThumbnailerGenerateData);
  data->file = g_object_ref (file);
  data->orientation = orientation;
  data->original_height = original_height;
  data->original_width = original_width;
  data->thumbnail_path = g_strdup (thumbnail_path);
  data->thumbnail_size = thumbnail_size;
  data->graph = g_object_ref (graph);

  return data;
}


static gboolean
photos_thumbnailer_authorize_authenticated_peer (PhotosThumbnailer *self,
                                                 GIOStream *iostream,
                                                 GCredentials *credentials)
{
  g_autoptr (GCredentials) own_credentials = NULL;
  gboolean ret_val = FALSE;

  if (credentials == NULL)
    goto out;

  own_credentials = g_credentials_new ();

  {
    g_autoptr (GError) error = NULL;

    if (!g_credentials_is_same_user (credentials, own_credentials, &error))
      {
        g_warning ("Unable to authorize peer: %s", error->message);
        goto out;
      }
  }

  ret_val = TRUE;

 out:
  return ret_val;
}


static void
photos_thumbnailer_generate_thumbnail_stream_close (GObject *source_object,
                                                    GAsyncResult *res,
                                                    gpointer user_data)
{
  GOutputStream *stream = G_OUTPUT_STREAM (source_object);
  g_autoptr (GTask) task = G_TASK (user_data);

  {
    g_autoptr (GError) error = NULL;

    if (!g_output_stream_close_finish (stream, res, &error))
      {
        g_task_return_error (task, g_steal_pointer (&error));
        goto out;
      }
  }

  g_task_return_boolean (task, TRUE);

 out:
  return;
}


static void
photos_thumbnailer_generate_thumbnail_save_to_stream (GObject *source_object,
                                                      GAsyncResult *res,
                                                      gpointer user_data)
{
  GCancellable *cancellable;
  g_autoptr (GTask) task = G_TASK (user_data);
  PhotosThumbnailerGenerateData *data;

  cancellable = g_task_get_cancellable (task);
  data = (PhotosThumbnailerGenerateData *) g_task_get_task_data (task);

  {
    g_autoptr (GError) error = NULL;

    if (!gdk_pixbuf_save_to_stream_finish (res, &error))
      {
        g_task_return_error (task, g_steal_pointer (&error));
        goto out;
      }
  }

  g_output_stream_close_async (G_OUTPUT_STREAM (data->stream),
                               G_PRIORITY_DEFAULT,
                               cancellable,
                               photos_thumbnailer_generate_thumbnail_stream_close,
                               g_object_ref (task));

 out:
  return;
}


static void
photos_thumbnailer_generate_thumbnail_replace (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  GCancellable *cancellable;
  GFile *thumbnail_file = G_FILE (source_object);
  g_autoptr (GFileOutputStream) stream = NULL;
  g_autoptr (GTask) task = G_TASK (user_data);
  PhotosThumbnailerGenerateData *data;
  const gchar *prgname;
  g_autofree gchar *original_height_str = NULL;
  g_autofree gchar *original_width_str = NULL;
  g_autofree gchar *uri = NULL;

  cancellable = g_task_get_cancellable (task);
  data = (PhotosThumbnailerGenerateData *) g_task_get_task_data (task);

  {
    g_autoptr (GError) error = NULL;

    stream = g_file_replace_finish (thumbnail_file, res, &error);
    if (error != NULL)
      {
        g_task_return_error (task, g_steal_pointer (&error));
        goto out;
      }
  }

  g_assert_null (data->stream);
  data->stream = g_object_ref (stream);

  original_height_str = g_strdup_printf ("%" G_GINT64_FORMAT, data->original_height);
  original_width_str = g_strdup_printf ("%" G_GINT64_FORMAT, data->original_width);
  prgname = g_get_prgname ();
  uri = g_file_get_uri (data->file);
  gdk_pixbuf_save_to_stream_async (data->pixbuf_thumbnail,
                                   G_OUTPUT_STREAM (stream),
                                   "png",
                                   cancellable,
                                   photos_thumbnailer_generate_thumbnail_save_to_stream,
                                   g_object_ref (task),
                                   "tEXt::Software", prgname,
                                   "tEXt::Thumb::URI", uri,
                                   "tEXt::Thumb::Image::Height", original_height_str,
                                   "tEXt::Thumb::Image::Width", original_width_str,
                                   NULL);

 out:
  return;
}


static void
photos_thumbnailer_generate_thumbnail_process (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  GCancellable *cancellable;
  g_autoptr (GFile) thumbnail_file = NULL;
  g_autoptr (GTask) task = G_TASK (user_data);
  GeglNode *pipeline_node;
  GeglProcessor *processor = GEGL_PROCESSOR (source_object);
  PhotosThumbnailerGenerateData *data;
  g_autofree gchar *thumbnail_dir = NULL;
  gdouble zoom = 0.0;
  gint pixbuf_height;
  gint pixbuf_width;
  gint pixbuf_zoomed_height;
  gint pixbuf_zoomed_width;

  cancellable = g_task_get_cancellable (task);
  data = (PhotosThumbnailerGenerateData *) g_task_get_task_data (task);

  {
    g_autoptr (GError) error = NULL;

    if (!photos_gegl_processor_process_finish (processor, res, &error))
      {
        g_task_return_error (task, g_steal_pointer (&error));
        goto out;
      }
  }

  g_assert_null (data->pixbuf_thumbnail);

  pipeline_node = photos_pipeline_get_graph (data->pipeline);
  data->pixbuf_thumbnail = photos_gegl_create_pixbuf_from_node (pipeline_node);
  pixbuf_height = gdk_pixbuf_get_height (data->pixbuf_thumbnail);
  pixbuf_width = gdk_pixbuf_get_width (data->pixbuf_thumbnail);

  if (pixbuf_height > pixbuf_width && pixbuf_height > data->thumbnail_size)
    {
      zoom = (gdouble) data->thumbnail_size / (gdouble) pixbuf_height;
      pixbuf_zoomed_height = data->thumbnail_size;
      pixbuf_zoomed_width = (gint) (zoom * (gdouble) pixbuf_width + 0.5);
    }
  else if (pixbuf_height <= pixbuf_width && pixbuf_width > data->thumbnail_size)
    {
      zoom = (gdouble) data->thumbnail_size / (gdouble) pixbuf_width;
      pixbuf_zoomed_height = (gint) (zoom * (gdouble) pixbuf_height + 0.5);
      pixbuf_zoomed_width = data->thumbnail_size;
    }

  if (zoom > 0.0)
    {
      g_autoptr (GdkPixbuf) pixbuf_scaled = NULL;

      photos_debug (PHOTOS_DEBUG_THUMBNAILER,
                    "Scaling thumbnail to %d×%d",
                    pixbuf_zoomed_width,
                    pixbuf_zoomed_height);

      pixbuf_scaled = gdk_pixbuf_scale_simple (data->pixbuf_thumbnail,
                                               pixbuf_zoomed_width,
                                               pixbuf_zoomed_height,
                                               GDK_INTERP_BILINEAR);

      g_set_object (&data->pixbuf_thumbnail, pixbuf_scaled);
    }

  thumbnail_dir = g_path_get_dirname (data->thumbnail_path);
  g_mkdir_with_parents (thumbnail_dir, 0700);

  thumbnail_file = g_file_new_for_path (data->thumbnail_path);

  photos_debug (PHOTOS_DEBUG_THUMBNAILER, "Saving thumbnail to %s", data->thumbnail_path);
  g_file_replace_async (thumbnail_file,
                        NULL,
                        FALSE,
                        G_FILE_CREATE_PRIVATE | G_FILE_CREATE_REPLACE_DESTINATION,
                        G_PRIORITY_DEFAULT,
                        cancellable,
                        photos_thumbnailer_generate_thumbnail_replace,
                        g_object_ref (task));

 out:
  return;
}


static void
photos_thumbnailer_generate_thumbnail_pixbuf (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  GCancellable *cancellable;
  g_autoptr (GTask) task = G_TASK (user_data);
  g_autoptr (GdkPixbuf) pixbuf = NULL;
  g_autoptr (GeglBuffer) buffer = NULL;
  g_autoptr (GeglBuffer) buffer_oriented = NULL;
  GeglNode *buffer_source;
  GeglNode *pipeline_node;
  g_autoptr (GeglProcessor) processor = NULL;
  PhotosThumbnailerGenerateData *data;

  cancellable = g_task_get_cancellable (task);
  data = (PhotosThumbnailerGenerateData *) g_task_get_task_data (task);

  {
    g_autoptr (GError) error = NULL;

    pixbuf = photos_pixbuf_new_from_file_at_size_finish (res, &error);
    if (error != NULL)
      {
        g_task_return_error (task, g_steal_pointer (&error));
        goto out;
      }
  }

  buffer = photos_gegl_buffer_new_from_pixbuf (pixbuf);
  buffer_oriented = photos_gegl_buffer_apply_orientation (buffer, data->orientation);

  buffer_source = gegl_node_new_child (data->graph, "operation", "gegl:buffer-source", "buffer", buffer_oriented, NULL);
  pipeline_node = photos_pipeline_get_graph (data->pipeline);
  gegl_node_link (buffer_source, pipeline_node);

  processor = gegl_node_new_processor (pipeline_node, NULL);
  photos_gegl_processor_process_async (processor,
                                       cancellable,
                                       photos_thumbnailer_generate_thumbnail_process,
                                       g_object_ref (task));

 out:
  return;
}


static void
photos_thumbnailer_generate_thumbnail_pipeline (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  GCancellable *cancellable;
  g_autoptr (GTask) task = G_TASK (user_data);
  g_autoptr (PhotosPipeline) pipeline = NULL;
  PhotosThumbnailerGenerateData *data;
  gboolean has_crop;
  g_autofree gchar *path = NULL;
  g_autofree gchar *uri = NULL;
  gdouble height;
  gdouble width;
  gdouble x;
  gdouble y;
  gint load_height;
  gint load_width;

  cancellable = g_task_get_cancellable (task);
  data = (PhotosThumbnailerGenerateData *) g_task_get_task_data (task);

  {
    g_autoptr (GError) error = NULL;

    pipeline = photos_pipeline_new_finish (res, &error);
    if (error != NULL)
      {
        g_task_return_error (task, g_steal_pointer (&error));
        goto out;
      }
  }

  g_assert_null (data->pipeline);
  data->pipeline = g_object_ref (pipeline);

  uri = g_file_get_uri (data->file);

  has_crop = photos_pipeline_get (pipeline,
                                  "gegl:crop",
                                  "height", &height,
                                  "width", &width,
                                  "x", &x,
                                  "y", &y,
                                  NULL);
  if (has_crop)
    {
      if (height < 0.0 || width < 0.0 || x < 0.0 || y < 0.0)
        {
          g_warning ("Unable to crop the thumbnail for %s: Invalid parameters", uri);

          photos_pipeline_remove (pipeline, "gegl:crop");
          has_crop = FALSE;
        }
    }

  if (has_crop
      || (0 < data->original_height
          && data->original_height < data->thumbnail_size
          && 0 < data->original_width
          && data->original_width < data->thumbnail_size))
    {
      load_height = (gint) data->original_height;
      load_width = (gint) data->original_width;
    }
  else
    {
      load_height = data->thumbnail_size;
      load_width = data->thumbnail_size;
    }

  path = g_file_get_path (data->file);
  if (!g_file_is_native (data->file))
    photos_debug (PHOTOS_DEBUG_NETWORK, "Downloading %s (%s)", uri, path);

  photos_debug (PHOTOS_DEBUG_THUMBNAILER, "Loading %s at %d×%d", uri, load_width, load_height);
  photos_pixbuf_new_from_file_at_size_async (path,
                                             load_width,
                                             load_height,
                                             cancellable,
                                             photos_thumbnailer_generate_thumbnail_pixbuf,
                                             g_object_ref (task));

 out:
  return;
}


static void
photos_thumbnailer_generate_thumbnail_async (PhotosThumbnailer *self,
                                             const gchar *uri,
                                             const gchar *mime_type,
                                             const gchar *orientation,
                                             gint64 original_height,
                                             gint64 original_width,
                                             const gchar *const *pipeline_uris,
                                             const gchar *thumbnail_path,
                                             gint thumbnail_size,
                                             GCancellable *cancellable,
                                             GAsyncReadyCallback callback,
                                             gpointer user_data)
{
  g_autoptr (GFile) file = NULL;
  g_autoptr (GTask) task = NULL;
  GQuark orientation_quark;
  g_autoptr (GeglNode) graph = NULL;
  PhotosThumbnailerGenerateData *data;

  g_return_if_fail (PHOTOS_IS_THUMBNAILER (self));
  g_return_if_fail (uri != NULL && uri[0] != '\0');
  g_return_if_fail (mime_type != NULL && mime_type[0] != '\0');
  g_return_if_fail (orientation != NULL && orientation[0] != '\0');
  g_return_if_fail (thumbnail_path != NULL && thumbnail_path[0] != '\0');
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  file = g_file_new_for_uri (uri);
  orientation_quark = g_quark_from_string (orientation);
  graph = gegl_node_new ();
  data = photos_thumbnailer_generate_data_new (file,
                                               orientation_quark,
                                               original_height,
                                               original_width,
                                               thumbnail_path,
                                               thumbnail_size,
                                               graph);

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, photos_thumbnailer_generate_thumbnail_async);
  g_task_set_task_data (task, data, (GDestroyNotify) photos_thumbnailer_generate_data_free);

  photos_pipeline_new_async (graph,
                             pipeline_uris,
                             cancellable,
                             photos_thumbnailer_generate_thumbnail_pipeline,
                             g_object_ref (task));
}


static gboolean
photos_thumbnailer_generate_thumbnail_finish (PhotosThumbnailer *self, GAsyncResult *res, GError **error)
{
  GTask *task = G_TASK (res);

  g_return_val_if_fail (g_task_is_valid (res, self), FALSE);
  g_return_val_if_fail (g_task_get_source_tag (task) == photos_thumbnailer_generate_thumbnail_async, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  return g_task_propagate_boolean (task, error);
}


static gboolean
photos_thumbnailer_handle_cancel (PhotosThumbnailer *self,
                                  GDBusMethodInvocation *invocation,
                                  guint serial)
{
  GCancellable *cancellable;
  GDBusConnection *connection;
  GDBusMethodInvocation *invocation_ongoing;
  GHashTableIter iter;

  g_return_val_if_fail (PHOTOS_IS_THUMBNAILER (self), FALSE);
  g_return_val_if_fail (G_IS_DBUS_METHOD_INVOCATION (invocation), FALSE);

  photos_debug (PHOTOS_DEBUG_THUMBNAILER, "Handling Cancel for %u", serial);
  g_application_hold (G_APPLICATION (self));

  connection = g_dbus_method_invocation_get_connection (invocation);

  g_hash_table_iter_init (&iter, self->cancellables);
  while (g_hash_table_iter_next (&iter, (gpointer *) &invocation_ongoing, (gpointer *) &cancellable))
    {
      GDBusConnection *connection_ongoing;
      GDBusMessage *message_ongoing;
      guint32 serial_ongoing;

      connection_ongoing = g_dbus_method_invocation_get_connection (invocation_ongoing);
      message_ongoing = g_dbus_method_invocation_get_message (invocation_ongoing);
      serial_ongoing = g_dbus_message_get_serial (message_ongoing);

      if (connection == connection_ongoing && (guint32) serial == serial_ongoing)
        {
          g_cancellable_cancel (cancellable);
          photos_thumbnailer_dbus_complete_cancel (self->skeleton, invocation);
          goto out;
        }
    }

  g_dbus_method_invocation_return_error_literal (invocation, PHOTOS_ERROR, 0, "Invalid serial");

 out:
  photos_debug (PHOTOS_DEBUG_THUMBNAILER, "Completed Cancel");
  g_application_release (G_APPLICATION (self));
  return TRUE;
}


static void
photos_thumbnailer_handle_generate_thumbnail_generate_thumbnail (GObject *source_object,
                                                                 GAsyncResult *res,
                                                                 gpointer user_data)
{
  PhotosThumbnailer *self = PHOTOS_THUMBNAILER (source_object);
  g_autoptr (GDBusMethodInvocation) invocation = G_DBUS_METHOD_INVOCATION (user_data);

  {
    g_autoptr (GError) error = NULL;

    if (!photos_thumbnailer_generate_thumbnail_finish (self, res, &error))
      {
        g_dbus_method_invocation_take_error (invocation, g_steal_pointer (&error));
        goto out;
      }
  }

  photos_thumbnailer_dbus_complete_generate_thumbnail (self->skeleton, invocation);

 out:
  photos_debug (PHOTOS_DEBUG_THUMBNAILER, "Completed GenerateThumbnail");
  g_application_release (G_APPLICATION (self));
  g_hash_table_remove (self->cancellables, invocation);
}


static gboolean
photos_thumbnailer_handle_generate_thumbnail (PhotosThumbnailer *self,
                                              GDBusMethodInvocation *invocation,
                                              const gchar *uri,
                                              const gchar *mime_type,
                                              const gchar *orientation,
                                              gint64 original_height,
                                              gint64 original_width,
                                              GVariant *pipeline_uris_variant,
                                              const gchar *thumbnail_path,
                                              gint thumbnail_size)
{
  g_autoptr (GCancellable) cancellable = NULL;
  g_auto (GStrv) pipeline_uris = NULL;

  g_return_val_if_fail (PHOTOS_IS_THUMBNAILER (self), FALSE);
  g_return_val_if_fail (G_IS_DBUS_METHOD_INVOCATION (invocation), FALSE);
  g_return_val_if_fail (uri != NULL && uri[0] != '\0', FALSE);
  g_return_val_if_fail (mime_type != NULL && mime_type[0] != '\0', FALSE);
  g_return_val_if_fail (orientation != NULL && orientation[0] != '\0', FALSE);
  g_return_val_if_fail (g_variant_is_of_type (pipeline_uris_variant, G_VARIANT_TYPE ("as")), FALSE);
  g_return_val_if_fail (thumbnail_path != NULL && thumbnail_path[0] != '\0', FALSE);

  photos_debug (PHOTOS_DEBUG_THUMBNAILER, "Handling GenerateThumbnail for %s", uri);

  pipeline_uris = g_variant_dup_strv (pipeline_uris_variant, NULL);

  cancellable = g_cancellable_new ();
  g_hash_table_insert (self->cancellables, g_object_ref (invocation), g_object_ref (cancellable));

  g_application_hold (G_APPLICATION (self));
  photos_thumbnailer_generate_thumbnail_async (self,
                                               uri,
                                               mime_type,
                                               orientation,
                                               original_height,
                                               original_width,
                                               (const gchar *const *) pipeline_uris,
                                               thumbnail_path,
                                               thumbnail_size,
                                               cancellable,
                                               photos_thumbnailer_handle_generate_thumbnail_generate_thumbnail,
                                               g_object_ref (invocation));

  return TRUE;
}


static gboolean
photos_thumbnailer_dbus_register (GApplication *application,
                                  GDBusConnection *connection,
                                  const gchar *object_path,
                                  GError **error)
{
  PhotosThumbnailer *self = PHOTOS_THUMBNAILER (application);
  g_autoptr (GDBusAuthObserver) observer = NULL;
  gboolean ret_val = FALSE;

  g_return_val_if_fail (self->skeleton == NULL, FALSE);

  if (!G_APPLICATION_CLASS (photos_thumbnailer_parent_class)->dbus_register (application,
                                                                             connection,
                                                                             object_path,
                                                                             error))
    goto out;

  observer = g_dbus_auth_observer_new ();
  g_signal_connect_swapped (observer,
                            "authorize-authenticated-peer",
                            G_CALLBACK (photos_thumbnailer_authorize_authenticated_peer),
                            self);

  self->connection = g_dbus_connection_new_for_address_sync (self->address,
                                                             G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT,
                                                             observer,
                                                             NULL,
                                                             error);
  if (self->connection == NULL)
    goto out;

  self->skeleton = photos_thumbnailer_dbus_skeleton_new ();
  g_signal_connect_swapped (self->skeleton,
                            "handle-cancel",
                            G_CALLBACK (photos_thumbnailer_handle_cancel),
                            self);
  g_signal_connect_swapped (self->skeleton,
                            "handle-generate-thumbnail",
                            G_CALLBACK (photos_thumbnailer_handle_generate_thumbnail),
                            self);

  if (!g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (self->skeleton),
                                         self->connection,
                                         THUMBNAILER_PATH,
                                         error))
    {
      g_clear_object (&self->skeleton);
      goto out;
    }

  ret_val = TRUE;

 out:
  return ret_val;
}


static void
photos_thumbnailer_dbus_unregister (GApplication *application,
                                    GDBusConnection *connection,
                                    const gchar *object_path)
{
  PhotosThumbnailer *self = PHOTOS_THUMBNAILER (application);

  if (self->skeleton != NULL)
    {
      g_dbus_interface_skeleton_unexport_from_connection (G_DBUS_INTERFACE_SKELETON (self->skeleton),
                                                          self->connection);
      g_clear_object (&self->skeleton);
    }

  G_APPLICATION_CLASS (photos_thumbnailer_parent_class)->dbus_unregister (application, connection, object_path);
}


static gint
photos_thumbnailer_handle_local_options (GApplication *application, GVariantDict *options)
{
  PhotosThumbnailer *self = PHOTOS_THUMBNAILER (application);
  gint ret_val = EXIT_FAILURE;

  if (g_variant_dict_lookup (options, "address", "s", &self->address))
    ret_val = -1;

  return ret_val;
}


static void
photos_thumbnailer_shutdown (GApplication *application)
{
  photos_debug (PHOTOS_DEBUG_THUMBNAILER, "Thumbnailer exiting");

  G_APPLICATION_CLASS (photos_thumbnailer_parent_class)->shutdown (application);
}


static void
photos_thumbnailer_startup (GApplication *application)
{
  G_APPLICATION_CLASS (photos_thumbnailer_parent_class)->startup (application);

  photos_gegl_init ();
  photos_debug (PHOTOS_DEBUG_THUMBNAILER, "Thumbnailer ready");
}


static void
photos_thumbnailer_dispose (GObject *object)
{
  PhotosThumbnailer *self = PHOTOS_THUMBNAILER (object);

  g_assert_null (self->skeleton);

  g_clear_object (&self->connection);
  g_clear_pointer (&self->cancellables, g_hash_table_unref);

  G_OBJECT_CLASS (photos_thumbnailer_parent_class)->dispose (object);
}


static void
photos_thumbnailer_finalize (GObject *object)
{
  PhotosThumbnailer *self = PHOTOS_THUMBNAILER (object);

  g_free (self->address);

  if (g_application_get_is_registered (G_APPLICATION (self)))
    gegl_exit ();

  G_OBJECT_CLASS (photos_thumbnailer_parent_class)->finalize (object);
}


static void
photos_thumbnailer_init (PhotosThumbnailer *self)
{
  setlocale (LC_ALL, "");

  bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);

  photos_gegl_ensure_builtins ();

  self->cancellables = g_hash_table_new_full (g_direct_hash, g_direct_equal, g_object_unref, g_object_unref);
  g_application_add_main_option_entries (G_APPLICATION (self), COMMAND_LINE_OPTIONS);
}


static void
photos_thumbnailer_class_init (PhotosThumbnailerClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GApplicationClass *application_class = G_APPLICATION_CLASS (class);

  object_class->dispose = photos_thumbnailer_dispose;
  object_class->finalize = photos_thumbnailer_finalize;
  application_class->dbus_register = photos_thumbnailer_dbus_register;
  application_class->dbus_unregister = photos_thumbnailer_dbus_unregister;
  application_class->handle_local_options = photos_thumbnailer_handle_local_options;
  application_class->shutdown = photos_thumbnailer_shutdown;
  application_class->startup = photos_thumbnailer_startup;
}


GApplication *
photos_thumbnailer_new (void)
{
  return g_object_new (PHOTOS_TYPE_THUMBNAILER,
                       "flags", G_APPLICATION_IS_SERVICE | G_APPLICATION_NON_UNIQUE,
                       "inactivity-timeout", INACTIVITY_TIMEOUT,
                       NULL);
}
