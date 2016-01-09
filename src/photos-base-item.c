/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2014, 2015 Pranav Kant
 * Copyright © 2012, 2013, 2014, 2015, 2016 Red Hat, Inc.
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

/* Based on code from:
 *   + Documents
 */


#include "config.h"

#include <stdarg.h>
#include <string.h>

#include <gdk/gdk.h>
#include <gegl-plugin.h>
#include <gio/gio.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <libgd/gd.h>
#include <tracker-sparql.h>

#include "photos-application.h"
#include "photos-base-item.h"
#include "photos-collection-icon-watcher.h"
#include "photos-debug.h"
#include "photos-delete-item-job.h"
#include "photos-filterable.h"
#include "photos-icons.h"
#include "photos-local-item.h"
#include "photos-pipeline.h"
#include "photos-print-notification.h"
#include "photos-print-operation.h"
#include "photos-query.h"
#include "photos-search-context.h"
#include "photos-selection-controller.h"
#include "photos-single-item-job.h"
#include "photos-utils.h"


struct _PhotosBaseItemPrivate
{
  cairo_surface_t *surface;
  GAppInfo *default_app;
  GdkPixbuf *original_icon;
  GeglNode *buffer_sink;
  GeglNode *buffer_source;
  GeglNode *edit_graph;
  GeglNode *load_graph;
  GeglNode *load;
  GeglProcessor *processor;
  GMutex mutex_download;
  GMutex mutex;
  GQuark equipment;
  GQuark flash;
  GQuark orientation;
  PhotosCollectionIconWatcher *watcher;
  PhotosPipeline *pipeline;
  PhotosSelectionController *sel_cntrlr;
  TrackerSparqlCursor *cursor;
  gboolean collection;
  gboolean failed_thumbnailing;
  gboolean favorite;
  const gchar *thumb_path;
  gchar *author;
  gchar *default_app_name;
  gchar *filename;
  gchar *id;
  gchar *identifier;
  gchar *mime_type;
  gchar *name;
  gchar *name_fallback;
  gchar *rdf_type;
  gchar *resource_urn;
  gchar *type_description;
  gchar *uri;
  gdouble exposure_time;
  gdouble fnumber;
  gdouble focal_length;
  gdouble iso_speed;
  gint64 date_created;
  gint64 height;
  gint64 mtime;
  gint64 width;
};

enum
{
  PROP_0,
  PROP_CURSOR,
  PROP_FAILED_THUMBNAILING,
  PROP_ID,
};

enum
{
  INFO_UPDATED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static void photos_base_item_filterable_iface_init (PhotosFilterableInterface *iface);


G_DEFINE_ABSTRACT_TYPE_WITH_CODE (PhotosBaseItem, photos_base_item, G_TYPE_OBJECT,
                                  G_ADD_PRIVATE (PhotosBaseItem)
                                  G_IMPLEMENT_INTERFACE (PHOTOS_TYPE_FILTERABLE,
                                                         photos_base_item_filterable_iface_init));


typedef struct _PhotosBaseItemSaveData PhotosBaseItemSaveData;

struct _PhotosBaseItemSaveData
{
  GFile *dir;
  GeglBuffer *buffer;
  gchar *type;
};

static GdkPixbuf *failed_icon;
static GdkPixbuf *thumbnailing_icon;
static GThreadPool *create_thumbnail_pool;


static void photos_base_item_populate_from_cursor (PhotosBaseItem *self, TrackerSparqlCursor *cursor);


static PhotosBaseItemSaveData *
photos_base_item_save_data_new (GFile *dir, GeglBuffer *buffer, const gchar *type)
{
  PhotosBaseItemSaveData *data;

  data = g_slice_new0 (PhotosBaseItemSaveData);

  if (dir != NULL)
    data->dir = g_object_ref (dir);

  if (buffer != NULL)
    data->buffer = g_object_ref (buffer);

  data->type = g_strdup (type);

  return data;
}


static void
photos_base_item_save_data_free (PhotosBaseItemSaveData *data)
{
  g_clear_object (&data->dir);
  g_clear_object (&data->buffer);
  g_free (data->type);
  g_slice_free (PhotosBaseItemSaveData, data);
}


static GdkPixbuf *
photos_base_item_create_placeholder_icon (const gchar *icon_name)
{
  GApplication *app;
  GdkPixbuf *ret_val = NULL;
  gint icon_size;
  gint scale;

  app = g_application_get_default ();
  icon_size = photos_utils_get_icon_size_unscaled ();
  scale = photos_application_get_scale_factor (PHOTOS_APPLICATION (app));
  ret_val = photos_utils_create_placeholder_icon_for_scale (icon_name, icon_size, scale);
  return ret_val;
}


static GIcon *
photos_base_item_create_symbolic_emblem (const gchar *name, gint scale)
{
  GIcon *pix;
  gint size;

  size = photos_utils_get_icon_size_unscaled ();
  pix = photos_utils_create_symbolic_icon_for_scale (name, size, scale);
  if (pix == NULL)
    pix = g_themed_icon_new (name);

  return pix;
}


static void
photos_base_item_check_effects_and_update_info (PhotosBaseItem *self)
{
  PhotosBaseItemPrivate *priv = self->priv;
  GApplication *app;
  GIcon *pix;
  GList *emblem_icons = NULL;
  GList *windows;
  GdkPixbuf *emblemed_pixbuf = NULL;
  GdkPixbuf *thumbnailed_pixbuf = NULL;
  GdkWindow *window = NULL;
  gint scale;

  if (priv->original_icon == NULL)
    goto out;

  app = g_application_get_default ();
  scale = photos_application_get_scale_factor (PHOTOS_APPLICATION (app));

  emblemed_pixbuf = g_object_ref (priv->original_icon);

  if (priv->favorite)
    {
      pix = photos_base_item_create_symbolic_emblem (PHOTOS_ICON_FAVORITE, scale);
      emblem_icons = g_list_prepend (emblem_icons, pix);
    }

  if (emblem_icons != NULL)
    {
      GIcon *emblemed_icon;
      GList *l;
      GtkIconInfo *icon_info;
      GtkIconTheme *theme;
      gint height;
      gint size;
      gint width;

      emblem_icons = g_list_reverse (emblem_icons);
      emblemed_icon = g_emblemed_icon_new (G_ICON (priv->original_icon), NULL);
      for (l = emblem_icons; l != NULL; l = g_list_next (l))
        {
          GEmblem *emblem;
          GIcon *emblem_icon = G_ICON (l->data);

          emblem = g_emblem_new (emblem_icon);
          g_emblemed_icon_add_emblem (G_EMBLEMED_ICON (emblemed_icon), emblem);
          g_object_unref (emblem);
        }

      theme = gtk_icon_theme_get_default ();

      width = gdk_pixbuf_get_width (priv->original_icon);
      height = gdk_pixbuf_get_height (priv->original_icon);
      size = (width > height) ? width : height;

      icon_info = gtk_icon_theme_lookup_by_gicon (theme, emblemed_icon, size, GTK_ICON_LOOKUP_FORCE_SIZE);

      if (icon_info != NULL)
        {
          GError *error = NULL;
          GdkPixbuf *tmp;

          tmp = gtk_icon_info_load_icon (icon_info, &error);
          if (error != NULL)
            {
              g_warning ("Unable to render the emblem: %s", error->message);
              g_error_free (error);
            }
          else
            {
              g_object_unref (emblemed_pixbuf);
              emblemed_pixbuf = tmp;
            }

          g_object_unref (icon_info);
        }

      g_object_unref (emblemed_icon);
    }

  g_clear_pointer (&priv->surface, (GDestroyNotify) cairo_surface_destroy);

  if (priv->thumb_path != NULL)
    {
      GtkBorder *slice;

      slice = photos_utils_get_thumbnail_frame_border ();
      thumbnailed_pixbuf = gd_embed_image_in_frame (emblemed_pixbuf,
                                                    "resource:///org/gnome/Photos/thumbnail-frame.png",
                                                    slice,
                                                    slice);
      gtk_border_free (slice);
    }
  else
    thumbnailed_pixbuf = g_object_ref (emblemed_pixbuf);

  windows = gtk_application_get_windows (GTK_APPLICATION (app));
  if (windows != NULL)
    window = gtk_widget_get_window (GTK_WIDGET (windows->data));

  priv->surface = gdk_cairo_surface_create_from_pixbuf (thumbnailed_pixbuf, scale, window);

  g_signal_emit (self, signals[INFO_UPDATED], 0);

 out:
  g_clear_object (&thumbnailed_pixbuf);
  g_clear_object (&emblemed_pixbuf);
  g_list_free_full (emblem_icons, g_object_unref);
}


static void
photos_base_item_set_original_icon (PhotosBaseItem *self, GdkPixbuf *icon)
{
  PhotosBaseItemPrivate *priv = self->priv;

  if (icon != NULL)
    {
      g_clear_object (&priv->original_icon);
      priv->original_icon = g_object_ref (icon);
    }

  photos_base_item_check_effects_and_update_info (self);
}


static void
photos_base_item_create_thumbnail_in_thread_func (gpointer data, gpointer user_data)
{
  GTask *task = G_TASK (data);
  PhotosBaseItem *self;
  GCancellable *cancellable;
  GError *error;
  gboolean result;

  self = PHOTOS_BASE_ITEM (g_task_get_source_object (task));
  cancellable = g_task_get_cancellable (task);

  error = NULL;
  result = PHOTOS_BASE_ITEM_GET_CLASS (self)->create_thumbnail (self, cancellable, &error);
  if (error != NULL)
    {
      g_task_return_error (task, error);
      goto out;
    }

  g_task_return_boolean (task, result);

 out:
  g_object_unref (task);
}


static gint
photos_base_item_create_thumbnail_sort_func (gconstpointer a, gconstpointer b, gpointer user_data)
{
  GTask *task_a = G_TASK (a);
  GTask *task_b = G_TASK (b);
  PhotosBaseItem *item_a;
  PhotosBaseItem *item_b;
  gint ret_val = 0;

  item_a = PHOTOS_BASE_ITEM (g_task_get_source_object (task_a));
  item_b = PHOTOS_BASE_ITEM (g_task_get_source_object (task_b));

  if (PHOTOS_IS_LOCAL_ITEM (item_a))
    ret_val = -1;
  else if (PHOTOS_IS_LOCAL_ITEM (item_b))
    ret_val = 1;

  return ret_val;
}


static void
photos_base_item_create_thumbnail_async (PhotosBaseItem *self,
                                         GCancellable *cancellable,
                                         GAsyncReadyCallback callback,
                                         gpointer user_data)
{
  GTask *task;

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, photos_base_item_create_thumbnail_async);

  g_thread_pool_push (create_thumbnail_pool, g_object_ref (task), NULL);
  g_object_unref (task);
}


static gboolean
photos_base_item_create_thumbnail_finish (PhotosBaseItem *self, GAsyncResult *res, GError **error)
{
  GTask *task = G_TASK (res);

  g_return_val_if_fail (g_task_is_valid (res, self), FALSE);
  g_return_val_if_fail (g_task_get_source_tag (task) == photos_base_item_create_thumbnail_async, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  return g_task_propagate_boolean (task, error);
}


static void
photos_base_item_default_set_favorite (PhotosBaseItem *self, gboolean favorite)
{
  PhotosBaseItemPrivate *priv = self->priv;

  if (favorite == priv->favorite)
    return;

  priv->favorite = favorite;
  photos_base_item_check_effects_and_update_info (self);
  photos_utils_set_favorite (priv->id, favorite);
}


static void
photos_base_item_default_open (PhotosBaseItem *self, GdkScreen *screen, guint32 timestamp)
{
  PhotosBaseItemPrivate *priv = self->priv;
  GError *error;

  if (priv->default_app_name == NULL)
    return;

  /* Without a default_app, launch in the web browser, otherwise use
   * that system application.
   */

  if (priv->default_app != NULL)
    {
      GList *uris = NULL;

      uris = g_list_prepend (uris, g_strdup (priv->uri));

      error = NULL;
      g_app_info_launch_uris (priv->default_app, uris, NULL, &error);
      if (error != NULL)
        {
          g_warning ("Unable to show URI %s: %s", priv->uri, error->message);
          g_error_free (error);
        }

      g_list_free_full (uris, g_free);
    }
  else
    {
      error = NULL;
      gtk_show_uri (screen, priv->uri, timestamp, &error);
      if (error != NULL)
        {
          g_warning ("Unable to show URI %s: %s", priv->uri, error->message);
          g_error_free (error);
        }
    }
}


static void
photos_base_item_default_update_type_description (PhotosBaseItem *self)
{
  PhotosBaseItemPrivate *priv = self->priv;
  gchar *description = NULL;

  if (priv->collection)
    description = g_strdup (_("Album"));
  else if (priv->mime_type != NULL)
    description = g_content_type_get_description (priv->mime_type);

  priv->type_description = description;
}


static void
photos_base_item_download_in_thread_func (GTask *task,
                                          gpointer source_object,
                                          gpointer task_data,
                                          GCancellable *cancellable)
{
  PhotosBaseItem *self = PHOTOS_BASE_ITEM (source_object);
  PhotosBaseItemPrivate *priv = self->priv;
  GError *error;
  gchar *path;

  g_mutex_lock (&priv->mutex_download);

  error = NULL;
  path = photos_base_item_download (self, cancellable, &error);
  if (error != NULL)
    {
      g_task_return_error (task, error);
      goto out;
    }

  g_task_return_pointer (task, path, g_free);

 out:
  g_mutex_unlock (&priv->mutex_download);
}


static const gchar *
photos_base_item_get_id (PhotosFilterable *filterable)
{
  PhotosBaseItem *self = PHOTOS_BASE_ITEM (filterable);
  return self->priv->id;
}


static void
photos_base_item_icon_updated (PhotosBaseItem *self, GIcon *icon)
{
  if (icon == NULL)
    return;

  photos_base_item_set_original_icon (self, GDK_PIXBUF (icon));
}


static void
photos_base_item_refresh_collection_icon (PhotosBaseItem *self)
{
  PhotosBaseItemPrivate *priv = self->priv;

  if (priv->watcher == NULL)
    {
      priv->watcher = photos_collection_icon_watcher_new (self);
      g_signal_connect_swapped (priv->watcher, "icon-updated", G_CALLBACK (photos_base_item_icon_updated), self);
    }
  else
    photos_collection_icon_watcher_refresh (priv->watcher);
}


static void
photos_base_item_refresh_executed (TrackerSparqlCursor *cursor, gpointer user_data)
{
  PhotosBaseItem *self = PHOTOS_BASE_ITEM (user_data);

  if (cursor == NULL)
    goto out;

  photos_base_item_populate_from_cursor (self, cursor);

 out:
  g_object_unref (self);
}


static void
photos_base_item_set_failed_icon (PhotosBaseItem *self)
{
  if (failed_icon == NULL)
    failed_icon = photos_base_item_create_placeholder_icon (PHOTOS_ICON_IMAGE_X_GENERIC_SYMBOLIC);

  photos_base_item_set_original_icon (self, failed_icon);
}


static void
photos_base_item_refresh_thumb_path_pixbuf (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosBaseItem *self = PHOTOS_BASE_ITEM (user_data);
  PhotosBaseItemPrivate *priv = self->priv;
  GApplication *app;
  GdkPixbuf *pixbuf = NULL;
  GdkPixbuf *scaled_pixbuf = NULL;
  GError *error = NULL;
  GInputStream *stream = G_INPUT_STREAM (source_object);
  gint icon_size;
  gint scale;

  pixbuf = gdk_pixbuf_new_from_stream_finish (res, &error);
  if (error != NULL)
    {
      GFile *file;
      gchar *uri;

      file = G_FILE (g_object_get_data (G_OBJECT (stream), "file"));
      uri = g_file_get_uri (file);
      g_warning ("Unable to create pixbuf from %s: %s", uri, error->message);
      priv->failed_thumbnailing = TRUE;
      priv->thumb_path = NULL;
      g_file_delete_async (file, G_PRIORITY_DEFAULT, NULL, NULL, NULL);
      photos_base_item_set_failed_icon (self);
      g_free (uri);
      g_error_free (error);
      goto out;
    }

  app = g_application_get_default ();
  scale = photos_application_get_scale_factor (PHOTOS_APPLICATION (app));
  icon_size = photos_utils_get_icon_size_unscaled ();
  scaled_pixbuf = photos_utils_downscale_pixbuf_for_scale (pixbuf, icon_size, scale);
  photos_base_item_set_original_icon (self, scaled_pixbuf);

 out:
  g_input_stream_close_async (stream, G_PRIORITY_DEFAULT, NULL, NULL, NULL);
  g_clear_object (&scaled_pixbuf);
  g_clear_object (&pixbuf);
  g_object_unref (self);
}


static void
photos_base_item_refresh_thumb_path_read (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosBaseItem *self = PHOTOS_BASE_ITEM (user_data);
  PhotosBaseItemPrivate *priv = self->priv;
  GError *error = NULL;
  GFile *file = G_FILE (source_object);
  GFileInputStream *stream;

  stream = g_file_read_finish (file, res, &error);
  if (error != NULL)
    {
      gchar *uri;

      uri = g_file_get_uri (file);
      g_warning ("Unable to read file at %s: %s", uri, error->message);
      priv->failed_thumbnailing = TRUE;
      priv->thumb_path = NULL;
      g_file_delete_async (file, G_PRIORITY_DEFAULT, NULL, NULL, NULL);
      photos_base_item_set_failed_icon (self);
      g_free (uri);
      g_error_free (error);
      goto out;
    }

  g_object_set_data_full (G_OBJECT (stream), "file", g_object_ref (file), g_object_unref);
  gdk_pixbuf_new_from_stream_async (G_INPUT_STREAM (stream),
                                    NULL,
                                    photos_base_item_refresh_thumb_path_pixbuf,
                                    g_object_ref (self));
  g_object_unref (stream);

 out:
  g_object_unref (self);
}


static void
photos_base_item_refresh_thumb_path (PhotosBaseItem *self)
{
  PhotosBaseItemPrivate *priv = self->priv;
  GFile *thumb_file;

  thumb_file = g_file_new_for_path (priv->thumb_path);
  g_file_read_async (thumb_file,
                     G_PRIORITY_DEFAULT,
                     NULL,
                     photos_base_item_refresh_thumb_path_read,
                     g_object_ref (self));
  g_object_unref (thumb_file);
}


static void
photos_base_item_thumbnail_path_info (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosBaseItem *self = PHOTOS_BASE_ITEM (user_data);
  PhotosBaseItemPrivate *priv = self->priv;
  GError *error = NULL;
  GFile *file = G_FILE (source_object);
  GFileInfo *info;

  info = g_file_query_info_finish (file, res, &error);
  if (error != NULL)
    {
      g_warning ("Unable to query info for file at %s: %s", priv->uri, error->message);
      priv->failed_thumbnailing = TRUE;
      photos_base_item_set_failed_icon (self);
      g_error_free (error);
      goto out;
    }

  priv->thumb_path = g_file_info_get_attribute_byte_string (info, G_FILE_ATTRIBUTE_THUMBNAIL_PATH);
  if (priv->thumb_path != NULL)
    {
      photos_base_item_refresh_thumb_path (self);
    }
  else
    {
      priv->failed_thumbnailing = TRUE;
      photos_base_item_set_failed_icon (self);
    }

 out:
  g_object_unref (self);
}


static void
photos_base_item_create_thumbnail_cb (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosBaseItem *self = PHOTOS_BASE_ITEM (source_object);
  PhotosBaseItemPrivate *priv = self->priv;
  GError *error;
  GFile *file = G_FILE (user_data);

  error = NULL;
  photos_base_item_create_thumbnail_finish (self, res, &error);
  if (error != NULL)
    {
      priv->failed_thumbnailing = TRUE;
      g_warning ("Unable to create thumbnail: %s", error->message);
      photos_base_item_set_failed_icon (self);
      g_error_free (error);
      goto out;
    }

  g_file_query_info_async (file,
                           G_FILE_ATTRIBUTE_THUMBNAIL_PATH,
                           G_FILE_QUERY_INFO_NONE,
                           G_PRIORITY_DEFAULT,
                           NULL,
                           photos_base_item_thumbnail_path_info,
                           g_object_ref (self));

 out:
  g_object_unref (file);
}


static void
photos_base_item_file_query_info (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosBaseItem *self = PHOTOS_BASE_ITEM (user_data);
  PhotosBaseItemPrivate *priv = self->priv;
  GError *error = NULL;
  GFile *file = G_FILE (source_object);
  GFileInfo *info;

  info = g_file_query_info_finish (file, res, &error);
  if (error != NULL)
    {
      g_warning ("Unable to query info for file at %s: %s", priv->uri, error->message);
      priv->failed_thumbnailing = TRUE;
      photos_base_item_set_failed_icon (self);
      g_error_free (error);
      goto out;
    }

  priv->thumb_path = g_file_info_get_attribute_byte_string (info, G_FILE_ATTRIBUTE_THUMBNAIL_PATH);
  if (priv->thumb_path != NULL)
    photos_base_item_refresh_thumb_path (self);
  else
    {
      photos_base_item_create_thumbnail_async (self,
                                               NULL,
                                               photos_base_item_create_thumbnail_cb,
                                               g_object_ref (file));
    }

 out:
  g_object_unref (self);
}


static GeglBuffer *
photos_base_item_load_buffer (PhotosBaseItem *self, GCancellable *cancellable, GError **error)
{
  PhotosBaseItemPrivate *priv = self->priv;
  GeglBuffer *ret_val = NULL;
  gchar *path = NULL;

  path = photos_base_item_download (self, cancellable, error);
  if (path == NULL)
    goto out;

  gegl_node_set (priv->load, "path", path, NULL);
  gegl_node_set (priv->buffer_sink, "buffer", &ret_val, NULL);
  gegl_node_process (priv->buffer_sink);

 out:
  g_free (path);
  return ret_val;
}


static void
photos_base_item_load_buffer_in_thread_func (GTask *task,
                                             gpointer source_object,
                                             gpointer task_data,
                                             GCancellable *cancellable)
{
  PhotosBaseItem *self = PHOTOS_BASE_ITEM (source_object);
  PhotosBaseItemPrivate *priv = self->priv;
  GeglBuffer *buffer;
  GError *error = NULL;

  g_mutex_lock (&priv->mutex);

  buffer = photos_base_item_load_buffer (self, cancellable, &error);
  if (error != NULL)
    {
      g_task_return_error (task, error);
      goto out;
    }

  g_task_return_pointer (task, buffer, g_object_unref);

 out:
  g_mutex_unlock (&priv->mutex);
}


static void
photos_base_item_load_buffer_async (PhotosBaseItem *self,
                                    GCancellable *cancellable,
                                    GAsyncReadyCallback callback,
                                    gpointer user_data)
{
  PhotosBaseItemPrivate *priv;
  GTask *task;

  g_return_if_fail (PHOTOS_IS_BASE_ITEM (self));
  priv = self->priv;

  if (priv->load_graph == NULL)
    {
      GeglNode *orientation;

      priv->load_graph = gegl_node_new ();
      priv->load = gegl_node_new_child (priv->load_graph, "operation", "gegl:load", NULL);
      orientation = photos_utils_create_orientation_node (priv->load_graph, priv->orientation);
      priv->buffer_sink = gegl_node_new_child (priv->load_graph, "operation", "gegl:buffer-sink", NULL);
      gegl_node_link_many (priv->load, orientation, priv->buffer_sink, NULL);
    }

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, photos_base_item_load_buffer_async);

  g_task_run_in_thread (task, photos_base_item_load_buffer_in_thread_func);
  g_object_unref (task);
}


static GeglBuffer *
photos_base_item_load_buffer_finish (PhotosBaseItem *self, GAsyncResult *res, GError **error)
{
  GTask *task = G_TASK (res);

  g_return_val_if_fail (g_task_is_valid (res, self), NULL);
  g_return_val_if_fail (g_task_get_source_tag (task) == photos_base_item_load_buffer_async, NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  return g_task_propagate_pointer (task, error);
}


static void
photos_base_item_load_process (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  GTask *task = G_TASK (user_data);
  PhotosBaseItem *self;
  GeglNode *graph;
  GError *error;

  self = PHOTOS_BASE_ITEM (g_task_get_source_object (task));

  error = NULL;
  photos_base_item_process_finish (self, res, &error);
  if (error != NULL)
    {
      g_task_return_error (task, error);
      goto out;
    }

  graph = photos_pipeline_get_graph (self->priv->pipeline);
  g_task_return_pointer (task, g_object_ref (graph), g_object_unref);

 out:
  g_object_unref (task);
}


static void
photos_base_item_load_load_buffer (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  GTask *task = G_TASK (user_data);
  PhotosBaseItem *self;
  PhotosBaseItemPrivate *priv;
  const Babl *format;
  GCancellable *cancellable;
  GeglBuffer *buffer = NULL;
  GError *error;
  const gchar *format_name;

  self = PHOTOS_BASE_ITEM (g_task_get_source_object (task));
  priv = self->priv;

  cancellable = g_task_get_cancellable (task);

  error = NULL;
  buffer = photos_base_item_load_buffer_finish (self, res, &error);
  if (error != NULL)
    {
      g_task_return_error (task, error);
      goto out;
    }

  format = gegl_buffer_get_format (buffer);
  format_name = babl_get_name (format);
  photos_debug (PHOTOS_DEBUG_GEGL, "Buffer loaded: %s", format_name);

  gegl_node_set (priv->buffer_source, "buffer", buffer, NULL);

  g_clear_object (&priv->processor);
  priv->processor = photos_pipeline_new_processor (priv->pipeline);

  photos_base_item_process_async (self, cancellable, photos_base_item_load_process, g_object_ref (task));

 out:
  g_clear_object (&buffer);
  g_object_unref (task);
}


static void
photos_base_item_load_pipeline (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  GTask *task = G_TASK (user_data);
  PhotosBaseItem *self;
  PhotosBaseItemPrivate *priv;
  GeglNode *graph;
  GCancellable *cancellable;
  GError *error;

  self = PHOTOS_BASE_ITEM (g_task_get_source_object (task));
  priv = self->priv;

  cancellable = g_task_get_cancellable (task);

  error = NULL;
  priv->pipeline = photos_pipeline_new_finish (res, &error);
  if (error != NULL)
    {
      g_task_return_error (task, error);
      goto out;
    }

  graph = photos_pipeline_get_graph (priv->pipeline);
  gegl_node_link (priv->buffer_source, graph);

  photos_base_item_load_buffer_async (self, cancellable, photos_base_item_load_load_buffer, g_object_ref (task));

 out:
  g_object_unref (task);
}


static void
photos_base_item_pipeline_save_save (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  GTask *task = G_TASK (user_data);
  PhotosBaseItem *self;
  GError *error;

  self = PHOTOS_BASE_ITEM (g_task_get_source_object (task));

  error = NULL;
  if (!photos_pipeline_save_finish (self->priv->pipeline, res, &error))
    {
      g_task_return_error (task, error);
      goto out;
    }

  g_task_return_boolean (task, TRUE);

 out:
  g_object_unref (task);
}


static gboolean
photos_base_item_process_idle (gpointer user_data)
{
  GTask *task = G_TASK (user_data);
  PhotosBaseItem *self;

  self = PHOTOS_BASE_ITEM (g_task_get_source_object (task));

  if (g_task_return_error_if_cancelled (task))
    goto done;

  if (gegl_processor_work (self->priv->processor, NULL))
    return G_SOURCE_CONTINUE;

  g_task_return_boolean (task, TRUE);

 done:
  return G_SOURCE_REMOVE;
}


static void
photos_base_item_save_guess_sizes_from_buffer (GeglBuffer *buffer,
                                               const gchar *mime_type,
                                               gsize *out_size,
                                               gsize *out_size_1,
                                               GCancellable *cancellable)
{
  GeglNode *buffer_source;
  GeglNode *graph;
  GeglNode *guess_sizes;
  gsize sizes[2];

  graph = gegl_node_new ();
  buffer_source = gegl_node_new_child (graph, "operation", "gegl:buffer-source", "buffer", buffer, NULL);

  if (g_strcmp0 (mime_type, "image/png") == 0)
    guess_sizes = gegl_node_new_child (graph,
                                       "operation", "photos:png-guess-sizes",
                                       "background", FALSE,
                                       "bitdepth", 8,
                                       "compression", -1,
                                       NULL);
  else
    guess_sizes = gegl_node_new_child (graph,
                                       "operation", "photos:jpg-guess-sizes",
                                       "optimize", FALSE,
                                       "progressive", FALSE,
                                       "sampling", TRUE,
                                       NULL);

  gegl_node_link (buffer_source, guess_sizes);
  gegl_node_process (guess_sizes);

  gegl_node_get (guess_sizes, "size", &sizes[0], "size-1", &sizes[1], NULL);
  if (out_size != NULL)
    *out_size = sizes[0];
  if (out_size_1 != NULL)
    *out_size_1 = sizes[1];

  g_object_unref (graph);
}


static void
photos_base_item_save_guess_sizes_in_thread_func (GTask *task,
                                                  gpointer source_object,
                                                  gpointer task_data,
                                                  GCancellable *cancellable)
{
  PhotosBaseItemSaveData *data = (PhotosBaseItemSaveData *) task_data;
  gsize *sizes;

  sizes = g_malloc0_n (2, sizeof (gsize));
  photos_base_item_save_guess_sizes_from_buffer (data->buffer, data->type, &sizes[0], &sizes[1], cancellable);
  g_task_return_pointer (task, sizes, g_free);
}


static void
photos_base_item_save_save_to_stream (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  GTask *task = G_TASK (user_data);
  GError *error = NULL;

  if (!gdk_pixbuf_save_to_stream_finish (res, &error))
    {
      g_task_return_error (task, error);
      goto out;
    }

  g_task_return_boolean (task, TRUE);

 out:
  g_object_unref (task);
}


static void
photos_base_item_save_replace (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  GTask *task = G_TASK (user_data);
  GCancellable *cancellable;
  GdkPixbuf *pixbuf = NULL;
  GError *error = NULL;
  GeglNode *buffer_source;
  GeglNode *graph = NULL;
  GFile *file = G_FILE (source_object);
  GFileOutputStream *stream = NULL;
  PhotosBaseItemSaveData *data;

  cancellable = g_task_get_cancellable (task);
  data = (PhotosBaseItemSaveData *) g_task_get_task_data (task);

  stream = g_file_replace_finish (file, res, &error);
  if (error != NULL)
    {
      g_task_return_error (task, error);
      goto out;
    }

  graph = gegl_node_new ();
  buffer_source = gegl_node_new_child (graph, "operation", "gegl:buffer-source", "buffer", data->buffer, NULL);
  pixbuf = photos_utils_create_pixbuf_from_node (buffer_source);
  if (pixbuf == NULL)
    {
      g_task_return_new_error (task, PHOTOS_ERROR, 0, "Failed to create a GdkPixbuf from the GeglBuffer");
      goto out;
    }

  if (g_strcmp0 (data->type, "jpeg") == 0)
    {
      gdk_pixbuf_save_to_stream_async (pixbuf,
                                       G_OUTPUT_STREAM (stream),
                                       data->type,
                                       cancellable,
                                       photos_base_item_save_save_to_stream,
                                       g_object_ref (task),
                                       "quality", "90",
                                       NULL);
    }
  else
    {
      gdk_pixbuf_save_to_stream_async (pixbuf,
                                       G_OUTPUT_STREAM (stream),
                                       data->type,
                                       cancellable,
                                       photos_base_item_save_save_to_stream,
                                       g_object_ref (task),
                                       NULL);
    }

 out:
  g_clear_object (&graph);
  g_clear_object (&pixbuf);
  g_clear_object (&stream);
  g_object_unref (task);
}


static void
photos_base_item_save_buffer_zoom (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  GTask *task = G_TASK (user_data);
  PhotosBaseItem *self;
  GCancellable *cancellable;
  GError *error;
  GFile *file = NULL;
  GeglBuffer *buffer = GEGL_BUFFER (source_object);
  GeglBuffer *buffer_zoomed = NULL;
  PhotosBaseItemSaveData *data;

  self = PHOTOS_BASE_ITEM (g_task_get_source_object (task));
  cancellable = g_task_get_cancellable (task);
  data = (PhotosBaseItemSaveData *) g_task_get_task_data (task);

  error = NULL;
  buffer_zoomed = photos_utils_buffer_zoom_finish (buffer, res, &error);
  if (error != NULL)
    {
      g_task_return_error (task, error);
      goto out;
    }

  g_assert_null (data->buffer);
  data->buffer = g_object_ref (buffer_zoomed);

  file = g_file_get_child (data->dir, self->priv->filename);
  g_file_replace_async (file,
                        NULL,
                        TRUE,
                        G_FILE_CREATE_REPLACE_DESTINATION,
                        G_PRIORITY_DEFAULT,
                        cancellable,
                        photos_base_item_save_replace,
                        g_object_ref (task));

 out:
  g_clear_object (&buffer_zoomed);
  g_clear_object (&file);
  g_object_unref (task);
}


static void
photos_base_item_set_thumbnailing_icon (PhotosBaseItem *self)
{
  if (thumbnailing_icon == NULL)
    thumbnailing_icon = photos_base_item_create_placeholder_icon (PHOTOS_ICON_CONTENT_LOADING_SYMBOLIC);

  photos_base_item_set_original_icon (self, thumbnailing_icon);
}


static void
photos_base_item_refresh_icon (PhotosBaseItem *self)
{
  PhotosBaseItemPrivate *priv = self->priv;
  GFile *file;

  if (priv->thumb_path != NULL)
    {
      photos_base_item_refresh_thumb_path (self);
      return;
    }

  photos_base_item_set_thumbnailing_icon (self);

  if (priv->collection)
    {
      photos_base_item_refresh_collection_icon (self);
      return;
    }

  if (priv->failed_thumbnailing)
    return;

  file = g_file_new_for_uri (priv->uri);
  g_file_query_info_async (file,
                           G_FILE_ATTRIBUTE_THUMBNAIL_PATH,
                           G_FILE_QUERY_INFO_NONE,
                           G_PRIORITY_DEFAULT,
                           NULL,
                           photos_base_item_file_query_info,
                           g_object_ref (self));
  g_object_unref (file);
}


static void
photos_base_item_update_info_from_type (PhotosBaseItem *self)
{
  PhotosBaseItemPrivate *priv = self->priv;

  if (strstr (priv->rdf_type, "nfo#DataContainer") != NULL)
    priv->collection = TRUE;

  PHOTOS_BASE_ITEM_GET_CLASS (self)->update_type_description (self);
}


static void
photos_base_item_populate_from_cursor (PhotosBaseItem *self, TrackerSparqlCursor *cursor)
{
  PhotosBaseItemPrivate *priv = self->priv;
  GTimeVal timeval;
  const gchar *date_created;
  const gchar *equipment;
  const gchar *flash;
  const gchar *mtime;
  const gchar *orientation;
  const gchar *title;
  const gchar *uri;

  uri = tracker_sparql_cursor_get_string (cursor, PHOTOS_QUERY_COLUMNS_URI, NULL);
  priv->uri = g_strdup ((uri == NULL) ? "" : uri);

  priv->id = g_strdup (tracker_sparql_cursor_get_string (cursor, PHOTOS_QUERY_COLUMNS_URN, NULL));
  priv->identifier = g_strdup (tracker_sparql_cursor_get_string (cursor, PHOTOS_QUERY_COLUMNS_IDENTIFIER, NULL));
  priv->author = g_strdup (tracker_sparql_cursor_get_string (cursor, PHOTOS_QUERY_COLUMNS_AUTHOR, NULL));
  priv->resource_urn = g_strdup (tracker_sparql_cursor_get_string (cursor, PHOTOS_QUERY_COLUMNS_RESOURCE_URN, NULL));
  priv->favorite = tracker_sparql_cursor_get_boolean (cursor, PHOTOS_QUERY_COLUMNS_RESOURCE_FAVORITE);

  mtime = tracker_sparql_cursor_get_string (cursor, PHOTOS_QUERY_COLUMNS_MTIME, NULL);
  if (mtime != NULL)
    {
      g_time_val_from_iso8601 (mtime, &timeval);
      priv->mtime = (gint64) timeval.tv_sec;
    }
  else
    priv->mtime = g_get_real_time () / 1000000;

  priv->mime_type = g_strdup (tracker_sparql_cursor_get_string (cursor, PHOTOS_QUERY_COLUMNS_MIME_TYPE, NULL));
  priv->rdf_type = g_strdup (tracker_sparql_cursor_get_string (cursor, PHOTOS_QUERY_COLUMNS_RDF_TYPE, NULL));
  photos_base_item_update_info_from_type (self);

  date_created = tracker_sparql_cursor_get_string (cursor, PHOTOS_QUERY_COLUMNS_DATE_CREATED, NULL);
  if (date_created != NULL)
    {
      g_time_val_from_iso8601 (date_created, &timeval);
      priv->date_created = (gint64) timeval.tv_sec;
    }
  else
    priv->date_created = -1;

  if (g_strcmp0 (priv->id, PHOTOS_COLLECTION_SCREENSHOT) == 0)
    title = _("Screenshots");
  else
    title = tracker_sparql_cursor_get_string (cursor, PHOTOS_QUERY_COLUMNS_TITLE, NULL);

  if (title == NULL)
    title = "";
  priv->name = g_strdup (title);

  priv->filename = g_strdup (tracker_sparql_cursor_get_string (cursor, PHOTOS_QUERY_COLUMNS_FILENAME, NULL));

  priv->width = tracker_sparql_cursor_get_integer (cursor, PHOTOS_QUERY_COLUMNS_WIDTH);
  priv->height = tracker_sparql_cursor_get_integer (cursor, PHOTOS_QUERY_COLUMNS_HEIGHT);

  equipment = tracker_sparql_cursor_get_string (cursor, PHOTOS_QUERY_COLUMNS_EQUIPMENT, NULL);
  priv->equipment = g_quark_from_string (equipment);

  orientation = tracker_sparql_cursor_get_string (cursor, PHOTOS_QUERY_COLUMNS_ORIENTATION, NULL);
  priv->orientation = g_quark_from_string (orientation);

  priv->exposure_time = tracker_sparql_cursor_get_double (cursor, PHOTOS_QUERY_COLUMNS_EXPOSURE_TIME);
  priv->fnumber = tracker_sparql_cursor_get_double (cursor, PHOTOS_QUERY_COLUMNS_FNUMBER);
  priv->focal_length = tracker_sparql_cursor_get_double (cursor, PHOTOS_QUERY_COLUMNS_FOCAL_LENGTH);
  priv->iso_speed = tracker_sparql_cursor_get_double (cursor, PHOTOS_QUERY_COLUMNS_ISO_SPEED);

  flash = tracker_sparql_cursor_get_string (cursor, PHOTOS_QUERY_COLUMNS_FLASH, NULL);
  priv->flash = g_quark_from_string (flash);

  priv->name_fallback = PHOTOS_BASE_ITEM_GET_CLASS (self)->create_name_fallback (self);
  photos_base_item_refresh_icon (self);
}


static void
photos_base_item_print_operation_done (PhotosBaseItem *self, GtkPrintOperationResult result)
{
  if (result == GTK_PRINT_OPERATION_RESULT_APPLY)
      photos_selection_controller_set_selection_mode (self->priv->sel_cntrlr, FALSE);
}


static void
photos_base_item_print_load (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosBaseItem *self = PHOTOS_BASE_ITEM (source_object);
  GtkWindow *toplevel = GTK_WINDOW (user_data);
  GeglNode *node;
  GtkPrintOperation *print_op;

  node = photos_base_item_load_finish (self, res, NULL);
  if (node == NULL)
    goto out;

  print_op = photos_print_operation_new (self, node);
  g_signal_connect_data (print_op,
                         "done",
                         G_CALLBACK (photos_base_item_print_operation_done),
                         g_object_ref (self),
                         (GClosureNotify) g_object_unref,
                         G_CONNECT_SWAPPED);


  /* It is self managing. */
  photos_print_notification_new (print_op);

  gtk_print_operation_run (print_op, GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG, toplevel, NULL);

 out:
  g_clear_object (&node);
  g_object_unref (toplevel);
}


static void
photos_base_item_constructed (GObject *object)
{
  PhotosBaseItem *self = PHOTOS_BASE_ITEM (object);
  PhotosBaseItemPrivate *priv = self->priv;

  G_OBJECT_CLASS (photos_base_item_parent_class)->constructed (object);

  photos_base_item_populate_from_cursor (self, priv->cursor);
  g_clear_object (&priv->cursor); /* We will not need it any more */
}


static void
photos_base_item_dispose (GObject *object)
{
  PhotosBaseItem *self = PHOTOS_BASE_ITEM (object);
  PhotosBaseItemPrivate *priv = self->priv;

  g_clear_pointer (&priv->surface, (GDestroyNotify) cairo_surface_destroy);
  g_clear_object (&priv->default_app);
  g_clear_object (&priv->edit_graph);
  g_clear_object (&priv->load_graph);
  g_clear_object (&priv->processor);
  g_clear_object (&priv->original_icon);
  g_clear_object (&priv->watcher);
  g_clear_object (&priv->pipeline);
  g_clear_object (&priv->sel_cntrlr);
  g_clear_object (&priv->cursor);

  G_OBJECT_CLASS (photos_base_item_parent_class)->dispose (object);
}


static void
photos_base_item_finalize (GObject *object)
{
  PhotosBaseItem *self = PHOTOS_BASE_ITEM (object);
  PhotosBaseItemPrivate *priv = self->priv;

  g_free (priv->author);
  g_free (priv->default_app_name);
  g_free (priv->filename);
  g_free (priv->id);
  g_free (priv->identifier);
  g_free (priv->mime_type);
  g_free (priv->name);
  g_free (priv->name_fallback);
  g_free (priv->rdf_type);
  g_free (priv->resource_urn);
  g_free (priv->type_description);
  g_free (priv->uri);

  g_mutex_clear (&priv->mutex_download);
  g_mutex_clear (&priv->mutex);

  G_OBJECT_CLASS (photos_base_item_parent_class)->finalize (object);
}


static void
photos_base_item_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
  PhotosBaseItem *self = PHOTOS_BASE_ITEM (object);

  switch (prop_id)
    {
    case PROP_ID:
      g_value_set_string (value, self->priv->id);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
photos_base_item_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
  PhotosBaseItem *self = PHOTOS_BASE_ITEM (object);
  PhotosBaseItemPrivate *priv = self->priv;

  switch (prop_id)
    {
    case PROP_CURSOR:
      priv->cursor = TRACKER_SPARQL_CURSOR (g_value_dup_object (value));
      break;

    case PROP_FAILED_THUMBNAILING:
      priv->failed_thumbnailing = g_value_get_boolean (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
photos_base_item_init (PhotosBaseItem *self)
{
  PhotosBaseItemPrivate *priv;

  self->priv = photos_base_item_get_instance_private (self);
  priv = self->priv;

  g_mutex_init (&priv->mutex_download);
  g_mutex_init (&priv->mutex);

  priv->sel_cntrlr = photos_selection_controller_dup_singleton ();
}


static void
photos_base_item_class_init (PhotosBaseItemClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->constructed = photos_base_item_constructed;
  object_class->dispose = photos_base_item_dispose;
  object_class->finalize = photos_base_item_finalize;
  object_class->get_property = photos_base_item_get_property;
  object_class->set_property = photos_base_item_set_property;
  class->open = photos_base_item_default_open;
  class->set_favorite = photos_base_item_default_set_favorite;
  class->update_type_description = photos_base_item_default_update_type_description;

  g_object_class_install_property (object_class,
                                   PROP_CURSOR,
                                   g_param_spec_object ("cursor",
                                                        "TrackerSparqlCursor object",
                                                        "A cursor to iterate over the results of a query",
                                                        TRACKER_SPARQL_TYPE_CURSOR,
                                                        G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE));

  g_object_class_install_property (object_class,
                                   PROP_FAILED_THUMBNAILING,
                                   g_param_spec_boolean ("failed-thumbnailing",
                                                         "Thumbnailing failed",
                                                         "Failed to create a thumbnail",
                                                         FALSE,
                                                         G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE));

  g_object_class_install_property (object_class,
                                   PROP_ID,
                                   g_param_spec_string ("id",
                                                        "Uniform Resource Name",
                                                        "An unique ID associated with this item",
                                                        "",
                                                        G_PARAM_READABLE));

  signals[INFO_UPDATED] = g_signal_new ("info-updated",
                                        G_TYPE_FROM_CLASS (class),
                                        G_SIGNAL_RUN_LAST,
                                        G_STRUCT_OFFSET (PhotosBaseItemClass, info_updated),
                                        NULL, /* accumulator */
                                        NULL, /* accu_data */
                                        g_cclosure_marshal_VOID__VOID,
                                        G_TYPE_NONE,
                                        0);

  create_thumbnail_pool = g_thread_pool_new (photos_base_item_create_thumbnail_in_thread_func,
                                             NULL,
                                             1,
                                             FALSE,
                                             NULL);
  g_thread_pool_set_sort_function (create_thumbnail_pool,
                                   (GCompareDataFunc) photos_base_item_create_thumbnail_sort_func,
                                   NULL);
}


static void
photos_base_item_filterable_iface_init (PhotosFilterableInterface *iface)
{
  iface->get_id = photos_base_item_get_id;
}


gboolean
photos_base_item_can_edit (PhotosBaseItem *self)
{
  PhotosBaseItemPrivate *priv;

  g_return_val_if_fail (PHOTOS_IS_BASE_ITEM (self), FALSE);
  priv = self->priv;

  return PHOTOS_BASE_ITEM_GET_CLASS (self)->create_pipeline_path != NULL
    && priv->filename != NULL
    && priv->filename[0] != '\0';
}


gboolean
photos_base_item_can_trash (PhotosBaseItem *self)
{
  return PHOTOS_BASE_ITEM_GET_CLASS (self)->trash != NULL;
}


cairo_surface_t *
photos_base_item_create_preview (PhotosBaseItem *self,
                                 gint size,
                                 gint scale,
                                 const gchar *operation,
                                 const gchar *first_property_name,
                                 ...)
{
  PhotosBaseItemPrivate *priv = self->priv;
  const Babl *format;
  GeglBuffer *buffer_orig = NULL;
  GeglBuffer *buffer = NULL;
  GeglNode *buffer_source;
  GeglNode *crop;
  GeglNode *graph = NULL;
  GeglNode *operation_node;
  GeglOperation *op;
  GeglProcessor *processor = NULL;
  GeglRectangle bbox;
  GeglRectangle roi;
  cairo_surface_t *surface = NULL;
  static const cairo_user_data_key_t key;
  const gchar *name;
  gdouble x;
  gdouble y;
  gdouble zoom;
  gint min_dimension;
  gint size_scaled;
  gint stride;
  gint64 end;
  gint64 start;
  guchar *buf = NULL;
  va_list ap;

  g_return_val_if_fail (operation != NULL && operation[0] != '\0', NULL);
  g_return_val_if_fail (priv->buffer_source != NULL, NULL);
  g_return_val_if_fail (priv->edit_graph != NULL, NULL);
  g_return_val_if_fail (priv->load_graph != NULL, NULL);

  op = gegl_node_get_gegl_operation (priv->buffer_source);
  g_return_val_if_fail (op != NULL, NULL);

  name = gegl_operation_get_name (op);
  g_return_val_if_fail (g_strcmp0 (name, "gegl:buffer-source") == 0, NULL);

  gegl_node_get (priv->buffer_source, "buffer", &buffer_orig, NULL);
  buffer = gegl_buffer_dup (buffer_orig);

  bbox = *gegl_buffer_get_extent (buffer);
  min_dimension = MIN (bbox.height, bbox.width);
  x = (gdouble) (bbox.width - min_dimension) / 2.0;
  y = (gdouble) (bbox.height - min_dimension) / 2.0;
  size_scaled = size * scale;
  zoom = (gdouble) size_scaled / (gdouble) min_dimension;

  graph = gegl_node_new ();
  buffer_source = gegl_node_new_child (graph, "operation", "gegl:buffer-source", "buffer", buffer, NULL);
  crop = gegl_node_new_child (graph,
                              "operation", "gegl:crop",
                              "height", (gdouble) min_dimension,
                              "width", (gdouble) min_dimension,
                              "x", x,
                              "y", y,
                              NULL);

  operation_node = gegl_node_new_child (graph, "operation", operation, NULL);

  va_start (ap, first_property_name);
  gegl_node_set_valist (operation_node, first_property_name, ap);
  va_end (ap);

  gegl_node_link_many (buffer_source, crop, operation_node, NULL);
  processor = gegl_node_new_processor (operation_node, NULL);

  start = g_get_monotonic_time ();

  while (gegl_processor_work (processor, NULL));

  end = g_get_monotonic_time ();
  photos_debug (PHOTOS_DEBUG_GEGL, "Create Preview: Process: %" G_GINT64_FORMAT, end - start);

  roi.height = size_scaled;
  roi.width = size_scaled;
  roi.x = (gint) (x * zoom + 0.5);
  roi.y = (gint) (y * zoom + 0.5);

  stride = cairo_format_stride_for_width (CAIRO_FORMAT_ARGB32, roi.width);
  buf = g_malloc0 (stride * roi.height);
  format = babl_format ("cairo-ARGB32");

  start = g_get_monotonic_time ();

  gegl_node_blit (operation_node, zoom, &roi, format, buf, GEGL_AUTO_ROWSTRIDE, GEGL_BLIT_DEFAULT);

  end = g_get_monotonic_time ();
  photos_debug (PHOTOS_DEBUG_GEGL, "Create Preview: Node Blit: %" G_GINT64_FORMAT, end - start);

  surface = cairo_image_surface_create_for_data (buf, CAIRO_FORMAT_ARGB32, roi.width, roi.height, stride);
  cairo_surface_set_device_scale (surface, (gdouble) scale, (gdouble) scale);
  cairo_surface_set_user_data (surface, &key, buf, (cairo_destroy_func_t) g_free);

  g_object_unref (buffer);
  g_object_unref (buffer_orig);
  g_object_unref (graph);
  g_object_unref (processor);

  return surface;
}


void
photos_base_item_destroy (PhotosBaseItem *self)
{
  /* TODO: SearchCategoryManager */
  g_clear_object (&self->priv->watcher);
}


gchar *
photos_base_item_download (PhotosBaseItem *self, GCancellable *cancellable, GError **error)
{
  return PHOTOS_BASE_ITEM_GET_CLASS (self)->download (self, cancellable, error);
}


void
photos_base_item_download_async (PhotosBaseItem *self,
                                 GCancellable *cancellable,
                                 GAsyncReadyCallback callback,
                                 gpointer user_data)
{
  GTask *task;

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, photos_base_item_download_async);

  g_task_run_in_thread (task, photos_base_item_download_in_thread_func);
  g_object_unref (task);
}


gchar *
photos_base_item_download_finish (PhotosBaseItem *self, GAsyncResult *res, GError **error)
{
  GTask *task = G_TASK (res);

  g_return_val_if_fail (g_task_is_valid (res, self), NULL);
  g_return_val_if_fail (g_task_get_source_tag (task) == photos_base_item_download_async, NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  return g_task_propagate_pointer (task, error);
}


const gchar *
photos_base_item_get_author (PhotosBaseItem *self)
{
  return self->priv->author;
}


gboolean
photos_base_item_get_bbox_edited (PhotosBaseItem *self, GeglRectangle *out_bbox)
{
  PhotosBaseItemPrivate *priv;
  GeglNode *graph;
  GeglRectangle bbox;

  g_return_val_if_fail (PHOTOS_IS_BASE_ITEM (self), FALSE);
  priv = self->priv;

  g_return_val_if_fail (priv->edit_graph != NULL, FALSE);
  g_return_val_if_fail (priv->load_graph != NULL, FALSE);
  g_return_val_if_fail (priv->pipeline != NULL, FALSE);
  g_return_val_if_fail (priv->processor != NULL, FALSE);
  g_return_val_if_fail (!gegl_processor_work (priv->processor, NULL), FALSE);

  graph = photos_pipeline_get_graph (priv->pipeline);
  bbox = gegl_node_get_bounding_box (graph);

  if (out_bbox != NULL)
    *out_bbox = bbox;

  return TRUE;
}


gboolean
photos_base_item_get_bbox_source (PhotosBaseItem *self, GeglRectangle *bbox)
{
  PhotosBaseItemPrivate *priv = self->priv;
  gboolean ret_val = FALSE;

  if (priv->buffer_source == NULL)
    goto out;

  *bbox = gegl_node_get_bounding_box (priv->buffer_source);
  ret_val = TRUE;

 out:
  return ret_val;
}


gint64
photos_base_item_get_date_created (PhotosBaseItem *self)
{
  return self->priv->date_created;
}


const gchar *
photos_base_item_get_default_app_name (PhotosBaseItem *self)
{
  return self->priv->default_app_name;
}


GQuark
photos_base_item_get_equipment (PhotosBaseItem *self)
{
  return self->priv->equipment;
}


gdouble
photos_base_item_get_exposure_time (PhotosBaseItem *self)
{
  return self->priv->exposure_time;
}


GQuark
photos_base_item_get_flash (PhotosBaseItem *self)
{
  return self->priv->flash;
}


const gchar *
photos_base_item_get_filename (PhotosBaseItem *self)
{
  return self->priv->filename;
}


gdouble
photos_base_item_get_fnumber (PhotosBaseItem *self)
{
  return self->priv->fnumber;
}


gdouble
photos_base_item_get_focal_length (PhotosBaseItem *self)
{
  return self->priv->focal_length;
}


gint64
photos_base_item_get_height (PhotosBaseItem *self)
{
  return self->priv->height;
}


const gchar *
photos_base_item_get_identifier (PhotosBaseItem *self)
{
  return self->priv->identifier;
}


gdouble
photos_base_item_get_iso_speed (PhotosBaseItem *self)
{
  return self->priv->iso_speed;
}


const gchar *
photos_base_item_get_mime_type (PhotosBaseItem *self)
{
  return self->priv->mime_type;
}


gint64
photos_base_item_get_mtime (PhotosBaseItem *self)
{
  return self->priv->mtime;
}


const gchar *
photos_base_item_get_name (PhotosBaseItem *self)
{
  return self->priv->name;
}


const gchar *
photos_base_item_get_name_with_fallback (PhotosBaseItem *self)
{
  PhotosBaseItemPrivate *priv = self->priv;
  const gchar *name;

  name = priv->name;
  if (name == NULL || name[0] == '\0')
    name = priv->name_fallback;

  return name;
}


GdkPixbuf *
photos_base_item_get_original_icon (PhotosBaseItem *self)
{
  return self->priv->original_icon;
}


const gchar *
photos_base_item_get_resource_urn (PhotosBaseItem *self)
{
  return self->priv->resource_urn;
}


GtkWidget *
photos_base_item_get_source_widget (PhotosBaseItem *self)
{
  return PHOTOS_BASE_ITEM_GET_CLASS (self)->get_source_widget(self);
}


cairo_surface_t *
photos_base_item_get_surface (PhotosBaseItem *self)
{
  return self->priv->surface;
}


const gchar *
photos_base_item_get_type_description (PhotosBaseItem *self)
{
  return self->priv->type_description;
}


const gchar *
photos_base_item_get_uri (PhotosBaseItem *self)
{
  return self->priv->uri;
}


gchar *
photos_base_item_get_where (PhotosBaseItem *self)
{
  PhotosBaseItemPrivate *priv = self->priv;
  gchar *ret_val;

  if (priv->collection)
    ret_val = g_strconcat ("{ ?urn nie:isPartOf <", priv->id, "> }", NULL);
  else
    ret_val = g_strdup ("");

  return ret_val;
}


gint64
photos_base_item_get_width (PhotosBaseItem *self)
{
  return self->priv->width;
}


gboolean
photos_base_item_is_collection (PhotosBaseItem *self)
{
  return self->priv->collection;
}


gboolean
photos_base_item_is_favorite (PhotosBaseItem *self)
{
  return self->priv->favorite;
}


void
photos_base_item_load_async (PhotosBaseItem *self,
                             GCancellable *cancellable,
                             GAsyncReadyCallback callback,
                             gpointer user_data)
{
  PhotosBaseItemPrivate *priv;
  GTask *task;

  g_return_if_fail (PHOTOS_IS_BASE_ITEM (self));
  priv = self->priv;

  g_return_if_fail (priv->edit_graph == NULL || priv->pipeline != NULL);

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, photos_base_item_load_async);

  if (priv->pipeline != NULL)
    {
      GeglNode *graph;

      graph = photos_pipeline_get_graph (priv->pipeline);
      g_task_return_pointer (task, g_object_ref (graph), g_object_unref);
    }
  else
    {
      PhotosBaseItemClass *class;
      gchar *uri = NULL;

      class = PHOTOS_BASE_ITEM_GET_CLASS (self);
      if (class->create_pipeline_path != NULL)
        {
          gchar *path;

          path = class->create_pipeline_path (self);
          uri = photos_utils_convert_path_to_uri (path);
          g_free (path);
        }

      priv->edit_graph = gegl_node_new ();
      priv->buffer_source = gegl_node_new_child (priv->edit_graph, "operation", "gegl:buffer-source", NULL);
      photos_pipeline_new_async (priv->edit_graph,
                                 uri,
                                 cancellable,
                                 photos_base_item_load_pipeline,
                                 g_object_ref (task));
      g_free (uri);
    }

  g_object_unref (task);
}


GeglNode *
photos_base_item_load_finish (PhotosBaseItem *self, GAsyncResult *res, GError **error)
{
  GTask *task = G_TASK (res);

  g_return_val_if_fail (g_task_is_valid (res, self), NULL);
  g_return_val_if_fail (g_task_get_source_tag (task) == photos_base_item_load_async, NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  return g_task_propagate_pointer (task, error);
}


void
photos_base_item_open (PhotosBaseItem *self, GdkScreen *screen, guint32 timestamp)
{
  PHOTOS_BASE_ITEM_GET_CLASS (self)->open (self, screen, timestamp);
}


void
photos_base_item_operation_add (PhotosBaseItem *self, const gchar *operation, const gchar *first_property_name, ...)
{
  va_list ap;

  va_start (ap, first_property_name);
  photos_pipeline_add (self->priv->pipeline, operation, first_property_name, ap);
  va_end (ap);
}


gboolean
photos_base_item_operation_get (PhotosBaseItem *self, const gchar *operation, const gchar *first_property_name, ...)
{
  gboolean ret_val;
  va_list ap;

  va_start (ap, first_property_name);
  ret_val = photos_pipeline_get (self->priv->pipeline, operation, first_property_name, ap);
  va_end (ap);

  return ret_val;
}


gboolean
photos_base_item_operation_undo (PhotosBaseItem *self)
{
  return photos_pipeline_undo (self->priv->pipeline);
}


void
photos_base_item_pipeline_save_async (PhotosBaseItem *self,
                                      GCancellable *cancellable,
                                      GAsyncReadyCallback callback,
                                      gpointer user_data)
{
  PhotosBaseItemPrivate *priv;
  GTask *task;

  g_return_if_fail (PHOTOS_IS_BASE_ITEM (self));
  priv = self->priv;

  g_return_if_fail (priv->edit_graph != NULL);
  g_return_if_fail (priv->load_graph != NULL);
  g_return_if_fail (priv->pipeline != NULL);

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, photos_base_item_pipeline_save_async);

  photos_pipeline_save_async (priv->pipeline,
                              cancellable,
                              photos_base_item_pipeline_save_save,
                              g_object_ref (task));

  g_object_unref (task);
}


gboolean
photos_base_item_pipeline_save_finish (PhotosBaseItem *self, GAsyncResult *res, GError **error)
{
  GTask *task = G_TASK (res);

  g_return_val_if_fail (g_task_is_valid (res, self), FALSE);
  g_return_val_if_fail (g_task_get_source_tag (task) == photos_base_item_pipeline_save_async, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  return g_task_propagate_boolean (task, error);
}


void
photos_base_item_print (PhotosBaseItem *self, GtkWidget *toplevel)
{
  photos_base_item_load_async (self, NULL, photos_base_item_print_load, g_object_ref (toplevel));
}


void
photos_base_item_process_async (PhotosBaseItem *self,
                                GCancellable *cancellable,
                                GAsyncReadyCallback callback,
                                gpointer user_data)
{
  GTask *task;

  g_return_if_fail (PHOTOS_IS_BASE_ITEM (self));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, photos_base_item_process_async);

  g_idle_add_full (G_PRIORITY_DEFAULT_IDLE, photos_base_item_process_idle, g_object_ref (task), g_object_unref);
  g_object_unref (task);
}


gboolean
photos_base_item_process_finish (PhotosBaseItem *self, GAsyncResult *res, GError **error)
{
  GTask *task = G_TASK (res);

  g_return_val_if_fail (g_task_is_valid (res, self), FALSE);
  g_return_val_if_fail (g_task_get_source_tag (task) == photos_base_item_process_async, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  return g_task_propagate_boolean (task, error);
}


void
photos_base_item_refresh (PhotosBaseItem *self)
{
  GApplication *app;
  PhotosSearchContextState *state;
  PhotosSingleItemJob *job;

  app = g_application_get_default ();
  state = photos_search_context_get_state (PHOTOS_SEARCH_CONTEXT (app));

  job = photos_single_item_job_new (self->priv->id);
  photos_single_item_job_run (job,
                              state,
                              PHOTOS_QUERY_FLAGS_NONE,
                              photos_base_item_refresh_executed,
                              g_object_ref (self));
  g_object_unref (job);
}


void
photos_base_item_save_async (PhotosBaseItem *self,
                             GFile *dir,
                             gdouble zoom,
                             GCancellable *cancellable,
                             GAsyncReadyCallback callback,
                             gpointer user_data)
{
  PhotosBaseItemPrivate *priv;
  GTask *task;
  GeglBuffer *buffer;
  GeglNode *graph;
  PhotosBaseItemSaveData *data;
  const gchar *type;

  g_return_if_fail (PHOTOS_IS_BASE_ITEM (self));
  priv = self->priv;

  g_return_if_fail (G_IS_FILE (dir));
  g_return_if_fail (priv->edit_graph != NULL);
  g_return_if_fail (priv->filename != NULL && priv->filename[0] != '\0');
  g_return_if_fail (priv->load_graph != NULL);
  g_return_if_fail (priv->pipeline != NULL);
  g_return_if_fail (priv->processor != NULL);
  g_return_if_fail (!gegl_processor_work (priv->processor, NULL));

  if (g_strcmp0 (priv->mime_type, "image/png") == 0)
    type = "png";
  else
    type = "jpeg";

  data = photos_base_item_save_data_new (dir, NULL, type);

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, photos_base_item_save_async);
  g_task_set_task_data (task, data, (GDestroyNotify) photos_base_item_save_data_free);

  graph = photos_pipeline_get_graph (priv->pipeline);
  buffer = photos_utils_create_buffer_from_node (graph);
  photos_utils_buffer_zoom_async (buffer,
                                  zoom,
                                  cancellable,
                                  photos_base_item_save_buffer_zoom,
                                  g_object_ref (task));

  g_object_unref (buffer);
  g_object_unref (task);
}


gboolean
photos_base_item_save_finish (PhotosBaseItem *self, GAsyncResult *res, GError **error)
{
  GTask *task = G_TASK (res);

  g_return_val_if_fail (g_task_is_valid (res, self), FALSE);
  g_return_val_if_fail (g_task_get_source_tag (task) == photos_base_item_save_async, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  return g_task_propagate_boolean (task, error);
}


void
photos_base_item_save_guess_sizes_async (PhotosBaseItem *self,
                                         GCancellable *cancellable,
                                         GAsyncReadyCallback callback,
                                         gpointer user_data)
{
  PhotosBaseItemPrivate *priv;
  GeglBuffer *buffer;
  GeglNode *graph;
  GTask *task;
  PhotosBaseItemSaveData *data;

  g_return_if_fail (PHOTOS_IS_BASE_ITEM (self));
  priv = self->priv;

  g_return_if_fail (priv->edit_graph != NULL);
  g_return_if_fail (priv->load_graph != NULL);
  g_return_if_fail (priv->pipeline != NULL);
  g_return_if_fail (priv->processor != NULL);
  g_return_if_fail (!gegl_processor_work (priv->processor, NULL));

  graph = photos_pipeline_get_graph (priv->pipeline);
  buffer = photos_utils_create_buffer_from_node (graph);

  data = photos_base_item_save_data_new (NULL, buffer, priv->mime_type);

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, photos_base_item_save_guess_sizes_async);
  g_task_set_task_data (task, data, (GDestroyNotify) photos_base_item_save_data_free);

  g_task_run_in_thread (task, photos_base_item_save_guess_sizes_in_thread_func);

  g_object_unref (buffer);
  g_object_unref (task);
}


gboolean
photos_base_item_save_guess_sizes_finish (PhotosBaseItem *self,
                                          GAsyncResult *res,
                                          gsize *out_size,
                                          gsize *out_size_1,
                                          GError **error)
{
  GTask *task = G_TASK (res);
  gboolean ret_val = FALSE;
  gsize *sizes;

  g_return_val_if_fail (g_task_is_valid (res, self), FALSE);
  g_return_val_if_fail (g_task_get_source_tag (task) == photos_base_item_save_guess_sizes_async, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  sizes = g_task_propagate_pointer (task, error);
  if (g_task_had_error (task))
    goto out;

  ret_val = TRUE;

  if (out_size != NULL)
    *out_size = sizes[0];
  if (out_size_1 != NULL)
    *out_size_1 = sizes[1];

 out:
  return ret_val;
}


void
photos_base_item_set_default_app (PhotosBaseItem *self, GAppInfo *default_app)
{
  PhotosBaseItemPrivate *priv = self->priv;
  const gchar *default_app_name;

  if (priv->default_app == NULL && default_app == NULL)
    return;

  if (priv->default_app != NULL && default_app != NULL && g_app_info_equal (priv->default_app, default_app))
    return;

  g_clear_object (&priv->default_app);
  g_free (priv->default_app_name);

  if (default_app == NULL)
    return;

  priv->default_app = g_object_ref (default_app);

  default_app_name = g_app_info_get_name (default_app);
  priv->default_app_name = g_strdup (default_app_name);
}


void
photos_base_item_set_default_app_name (PhotosBaseItem *self, const gchar *default_app_name)
{
  PhotosBaseItemPrivate *priv = self->priv;

  g_clear_object (&priv->default_app);
  g_free (priv->default_app_name);
  priv->default_app_name = g_strdup (default_app_name);
}


void
photos_base_item_set_favorite (PhotosBaseItem *self, gboolean favorite)
{
  PHOTOS_BASE_ITEM_GET_CLASS (self)->set_favorite (self, favorite);
}


void
photos_base_item_trash (PhotosBaseItem *self)
{
  PhotosDeleteItemJob *job;

  PHOTOS_BASE_ITEM_GET_CLASS (self)->trash (self);

  job = photos_delete_item_job_new (self->priv->id);
  photos_delete_item_job_run (job, NULL, NULL);
  g_object_unref (job);
}
