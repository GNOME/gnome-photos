/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2014 – 2015 Pranav Kant
 * Copyright © 2012 – 2021 Red Hat, Inc.
 * Copyright © 2016 Umang Jain
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

#include <stdarg.h>
#include <string.h>

#include <dazzle.h>
#include <gdk/gdk.h>
#include <gegl-plugin.h>
#include <gexiv2/gexiv2.h>
#include <gio/gio.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <libgd/gd.h>
#include <tracker-sparql.h>

#include "photos-application.h"
#include "photos-base-item.h"
#include "photos-collection-icon-watcher.h"
#include "photos-debug.h"
#include "photos-error.h"
#include "photos-filterable.h"
#include "photos-gegl.h"
#include "photos-glib.h"
#include "photos-local-item.h"
#include "photos-pipeline.h"
#include "photos-print-notification.h"
#include "photos-print-operation.h"
#include "photos-quarks.h"
#include "photos-query.h"
#include "photos-search-context.h"
#include "photos-single-item-job.h"
#include "photos-utils.h"


struct _PhotosBaseItemPrivate
{
  cairo_surface_t *surface;
  GAppInfo *default_app;
  GCancellable *cancellable;
  GdkPixbuf *original_icon;
  GeglBuffer *preview_source_buffer;
  GeglNode *buffer_source;
  GeglNode *edit_graph;
  GeglProcessor *processor;
  GMutex mutex_download;
  GMutex mutex_save_metadata;
  GQuark equipment;
  GQuark flash;
  GQuark orientation;
  PhotosCollectionIconWatcher *watcher;
  TrackerSparqlCursor *cursor;
  gboolean collection;
  gboolean failed_thumbnailing;
  gboolean favorite;
  gchar *author;
  gchar *default_app_name;
  gchar *filename;
  gchar *id;
  gchar *identifier;
  gchar *location;
  gchar *mime_type;
  gchar *name;
  gchar *name_fallback;
  gchar *rdf_type;
  gchar *resource_urn;
  gchar *thumb_path;
  gchar *type_description;
  gchar *uri;
  gdouble exposure_time;
  gdouble fnumber;
  gdouble focal_length;
  gdouble iso_speed;
  gint64 ctime;
  gint64 height;
  gint64 mtime;
  gint64 width;
  guint busy_count;
};

enum
{
  PROP_0,
  PROP_CURSOR,
  PROP_FAILED_THUMBNAILING,
  PROP_ICON,
  PROP_ID,
  PROP_MTIME,
  PROP_PRIMARY_TEXT,
  PROP_PULSE,
  PROP_SECONDARY_TEXT,
  PROP_URI
};

enum
{
  INFO_UPDATED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static void photos_base_item_filterable_iface_init (PhotosFilterableInterface *iface);
static void photos_base_item_main_box_item_iface_init (GdMainBoxItemInterface *iface);


G_DEFINE_ABSTRACT_TYPE_WITH_CODE (PhotosBaseItem, photos_base_item, G_TYPE_OBJECT,
                                  G_ADD_PRIVATE (PhotosBaseItem)
                                  G_IMPLEMENT_INTERFACE (GD_TYPE_MAIN_BOX_ITEM,
                                                         photos_base_item_main_box_item_iface_init)
                                  G_IMPLEMENT_INTERFACE (PHOTOS_TYPE_FILTERABLE,
                                                         photos_base_item_filterable_iface_init));

typedef struct _PhotosBaseItemMetadataAddSharedData PhotosBaseItemMetadataAddSharedData;
typedef struct _PhotosBaseItemQueryInfoData PhotosBaseItemQueryInfoData;
typedef struct _PhotosBaseItemSaveData PhotosBaseItemSaveData;
typedef struct _PhotosBaseItemSaveBufferData PhotosBaseItemSaveBufferData;
typedef struct _PhotosBaseItemSaveToFileData PhotosBaseItemSaveToFileData;
typedef struct _PhotosBaseItemSaveToStreamData PhotosBaseItemSaveToStreamData;

struct _PhotosBaseItemMetadataAddSharedData
{
  gchar *account_identity;
  gchar *provider_type;
  gchar *shared_id;
};

struct _PhotosBaseItemQueryInfoData
{
  GFileQueryInfoFlags flags;
  gchar *attributes;
};

struct _PhotosBaseItemSaveData
{
  GFile *dir;
  GFile *unique_file;
  GeglBuffer *buffer;
  gchar *type;
  gdouble zoom;
};

struct _PhotosBaseItemSaveBufferData
{
  GFile *file;
  GFileOutputStream *stream;
};

struct _PhotosBaseItemSaveToFileData
{
  GFile *file;
  GFileCreateFlags flags;
  GeglBuffer *buffer;
  gdouble zoom;
};

struct _PhotosBaseItemSaveToStreamData
{
  GFile *file;
  GFileIOStream *iostream;
  GOutputStream *ostream;
  gdouble zoom;
};

static DzlTaskCache *pipeline_cache;
static GdkPixbuf *failed_icon;
static GdkPixbuf *thumbnailing_icon;
static GThreadPool *create_thumbnail_pool;
static const gint PIXEL_SIZES[] = {2048, 1024};

enum
{
  THUMBNAIL_GENERATION = 0
};


static void photos_base_item_populate_from_cursor (PhotosBaseItem *self, TrackerSparqlCursor *cursor);


static PhotosBaseItemMetadataAddSharedData *
photos_base_item_metadata_add_shared_data_new (const gchar *provider_type,
                                               const gchar *account_identity,
                                               const gchar *shared_id)
{
  PhotosBaseItemMetadataAddSharedData *data;

  data = g_slice_new0 (PhotosBaseItemMetadataAddSharedData);
  data->account_identity = g_strdup (account_identity);
  data->provider_type = g_strdup (provider_type);
  data->shared_id = g_strdup (shared_id);

  return data;
}


static void
photos_base_item_metadata_add_shared_data_free (PhotosBaseItemMetadataAddSharedData *data)
{
  g_free (data->account_identity);
  g_free (data->provider_type);
  g_free (data->shared_id);
  g_slice_free (PhotosBaseItemMetadataAddSharedData, data);
}


static PhotosBaseItemQueryInfoData *
photos_base_item_query_info_data_new (const gchar *attributes, GFileQueryInfoFlags flags)
{
  PhotosBaseItemQueryInfoData *data;

  data = g_slice_new0 (PhotosBaseItemQueryInfoData);
  data->flags = flags;
  data->attributes = g_strdup (attributes);

  return data;
}


static void
photos_base_item_query_info_data_free (PhotosBaseItemQueryInfoData *data)
{
  g_free (data->attributes);
  g_slice_free (PhotosBaseItemQueryInfoData, data);
}


static PhotosBaseItemSaveData *
photos_base_item_save_data_new (GFile *dir, GeglBuffer *buffer, const gchar *type, gdouble zoom)
{
  PhotosBaseItemSaveData *data;

  data = g_slice_new0 (PhotosBaseItemSaveData);

  if (dir != NULL)
    data->dir = g_object_ref (dir);

  if (buffer != NULL)
    data->buffer = g_object_ref (buffer);

  data->type = g_strdup (type);
  data->zoom = zoom;

  return data;
}


static void
photos_base_item_save_data_free (PhotosBaseItemSaveData *data)
{
  g_clear_object (&data->dir);
  g_clear_object (&data->unique_file);
  g_clear_object (&data->buffer);
  g_free (data->type);
  g_slice_free (PhotosBaseItemSaveData, data);
}


static PhotosBaseItemSaveBufferData *
photos_base_item_save_buffer_data_new (GFile *file, GFileOutputStream *stream)
{
  PhotosBaseItemSaveBufferData *data;

  data = g_slice_new0 (PhotosBaseItemSaveBufferData);
  data->file = g_object_ref (file);
  data->stream = g_object_ref (stream);

  return data;
}


static void
photos_base_item_save_buffer_data_free (PhotosBaseItemSaveBufferData *data)
{
  g_clear_object (&data->file);
  g_clear_object (&data->stream);
  g_slice_free (PhotosBaseItemSaveBufferData, data);
}


static PhotosBaseItemSaveToFileData *
photos_base_item_save_to_file_data_new (GFile *file, GFileCreateFlags flags, gdouble zoom)
{
  PhotosBaseItemSaveToFileData *data;

  data = g_slice_new0 (PhotosBaseItemSaveToFileData);
  data->file = g_object_ref (file);
  data->flags = flags;
  data->zoom = zoom;

  return data;
}


static void
photos_base_item_save_to_file_data_free (PhotosBaseItemSaveToFileData *data)
{
  g_clear_object (&data->file);
  g_clear_object (&data->buffer);
  g_slice_free (PhotosBaseItemSaveToFileData, data);
}


static PhotosBaseItemSaveToStreamData *
photos_base_item_save_to_stream_data_new (GOutputStream *ostream, gdouble zoom)
{
  PhotosBaseItemSaveToStreamData *data;

  data = g_slice_new0 (PhotosBaseItemSaveToStreamData);
  data->ostream = g_object_ref (ostream);
  data->zoom = zoom;

  return data;
}


static void
photos_base_item_save_to_stream_data_free (PhotosBaseItemSaveToStreamData *data)
{
  g_clear_object (&data->file);
  g_clear_object (&data->iostream);
  g_clear_object (&data->ostream);
  g_slice_free (PhotosBaseItemSaveToStreamData, data);
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
  PhotosBaseItemPrivate *priv;
  GApplication *app;
  g_autolist (GIcon) emblem_icons = NULL;
  GList *windows;
  g_autoptr (GdkPixbuf) emblemed_pixbuf = NULL;
  GdkWindow *window = NULL;
  gint scale;

  priv = photos_base_item_get_instance_private (self);

  if (priv->original_icon == NULL)
    goto out;

  app = g_application_get_default ();
  scale = photos_application_get_scale_factor (PHOTOS_APPLICATION (app));

  emblemed_pixbuf = g_object_ref (priv->original_icon);

  if (priv->favorite)
    {
      g_autoptr (GIcon) emblem_icon = NULL;

      emblem_icon = photos_base_item_create_symbolic_emblem ("starred", scale);
      emblem_icons = g_list_prepend (emblem_icons, g_object_ref (emblem_icon));
    }

  if (emblem_icons != NULL)
    {
      g_autoptr (GIcon) emblemed_icon = NULL;
      GList *l;
      g_autoptr (GtkIconInfo) emblemed_icon_info = NULL;
      GtkIconTheme *theme;
      gint height;
      gint size;
      gint width;

      emblem_icons = g_list_reverse (emblem_icons);
      emblemed_icon = g_emblemed_icon_new (G_ICON (priv->original_icon), NULL);
      for (l = emblem_icons; l != NULL; l = g_list_next (l))
        {
          g_autoptr (GEmblem) emblem = NULL;
          GIcon *emblem_icon = G_ICON (l->data);

          emblem = g_emblem_new (emblem_icon);
          g_emblemed_icon_add_emblem (G_EMBLEMED_ICON (emblemed_icon), emblem);
        }

      theme = gtk_icon_theme_get_default ();

      width = gdk_pixbuf_get_width (priv->original_icon);
      height = gdk_pixbuf_get_height (priv->original_icon);
      size = (width > height) ? width : height;

      emblemed_icon_info = gtk_icon_theme_lookup_by_gicon (theme, emblemed_icon, size, GTK_ICON_LOOKUP_FORCE_SIZE);

      if (emblemed_icon_info != NULL)
        {
          g_autoptr (GError) error = NULL;
          g_autoptr (GdkPixbuf) emblemed_icon_pixbuf = NULL;

          emblemed_icon_pixbuf = gtk_icon_info_load_icon (emblemed_icon_info, &error);
          if (error != NULL)
            g_warning ("Unable to render the emblem: %s", error->message);
          else
            g_set_object (&emblemed_pixbuf, emblemed_icon_pixbuf);
        }
    }

  g_clear_pointer (&priv->surface, cairo_surface_destroy);

  windows = gtk_application_get_windows (GTK_APPLICATION (app));
  if (windows != NULL)
    window = gtk_widget_get_window (GTK_WIDGET (windows->data));

  priv->surface = gdk_cairo_surface_create_from_pixbuf (emblemed_pixbuf, scale, window);

  g_object_notify (G_OBJECT (self), "icon");
  g_signal_emit (self, signals[INFO_UPDATED], 0);

 out:
  return;
}


static void
photos_base_item_clear_pixels (PhotosBaseItem *self)
{
  PhotosBaseItemPrivate *priv;

  priv = photos_base_item_get_instance_private (self);

  priv->buffer_source = NULL;
  dzl_task_cache_evict (pipeline_cache, self);

  g_clear_object (&priv->edit_graph);
  g_clear_object (&priv->preview_source_buffer);
  g_clear_object (&priv->processor);
}


static void
photos_base_item_create_thumbnail_in_thread_func (gpointer data, gpointer user_data)
{
  g_autoptr (GTask) task = G_TASK (data);
  PhotosBaseItem *self;
  GCancellable *cancellable;

  self = PHOTOS_BASE_ITEM (g_task_get_source_object (task));
  cancellable = g_task_get_cancellable (task);

  {
    g_autoptr (GError) error = NULL;

    if (!PHOTOS_BASE_ITEM_GET_CLASS (self)->create_thumbnail (self, cancellable, &error))
      {
        if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
          {
            g_autoptr (GFile) file = NULL;
            g_autofree gchar *path = NULL;

            path = photos_base_item_create_thumbnail_path (self);
            file = g_file_new_for_path (path);
            g_file_delete (file, NULL, NULL);
          }

        g_task_return_error (task, g_steal_pointer (&error));
        goto out;
      }
  }

  g_task_return_boolean (task, TRUE);

 out:
  return;
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
  g_autoptr (GTask) task = NULL;

  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, photos_base_item_create_thumbnail_async);

  g_thread_pool_push (create_thumbnail_pool, g_object_ref (task), NULL);
}


static gboolean
photos_base_item_create_thumbnail_finish (PhotosBaseItem *self, GAsyncResult *res, GError **error)
{
  GTask *task;

  g_return_val_if_fail (g_task_is_valid (res, self), FALSE);
  task = G_TASK (res);

  g_return_val_if_fail (g_task_get_source_tag (task) == photos_base_item_create_thumbnail_async, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  return g_task_propagate_boolean (task, error);
}


static void
photos_base_item_set_original_icon (PhotosBaseItem *self, GdkPixbuf *icon)
{
  PhotosBaseItemPrivate *priv;

  priv = photos_base_item_get_instance_private (self);

  if (icon != NULL)
    g_set_object (&priv->original_icon, icon);

  photos_base_item_check_effects_and_update_info (self);
}


static void
photos_base_item_set_failed_icon (PhotosBaseItem *self)
{
  if (failed_icon == NULL)
    failed_icon = photos_base_item_create_placeholder_icon ("image-x-generic-symbolic");

  photos_base_item_set_original_icon (self, failed_icon);
}


static void
photos_base_item_set_thumbnailing_icon (PhotosBaseItem *self)
{
  if (thumbnailing_icon == NULL)
    thumbnailing_icon = photos_base_item_create_placeholder_icon ("content-loading-symbolic");

  photos_base_item_set_original_icon (self, thumbnailing_icon);
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
  PhotosBaseItemPrivate *priv;

  priv = photos_base_item_get_instance_private (self);

  if (priv->watcher == NULL)
    {
      priv->watcher = photos_collection_icon_watcher_new (self);
      g_signal_connect_swapped (priv->watcher, "icon-updated", G_CALLBACK (photos_base_item_icon_updated), self);
    }
  else
    photos_collection_icon_watcher_refresh (priv->watcher);
}


static void
photos_base_item_refresh_thumb_path_pixbuf (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosBaseItem *self;
  PhotosBaseItemPrivate *priv;
  GApplication *app;
  g_autoptr (GdkPixbuf) centered_pixbuf = NULL;
  g_autoptr (GdkPixbuf) pixbuf = NULL;
  g_autoptr (GdkPixbuf) scaled_pixbuf = NULL;
  GInputStream *stream = G_INPUT_STREAM (source_object);
  gint icon_size;
  gint scale;

  {
    g_autoptr (GError) error = NULL;

    pixbuf = gdk_pixbuf_new_from_stream_finish (res, &error);
    if (error != NULL)
      {
        GFile *file;
        g_autofree gchar *uri = NULL;

        if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
          goto out;

        file = G_FILE (g_object_get_data (G_OBJECT (stream), "file"));
        uri = g_file_get_uri (file);
        g_warning ("Unable to create pixbuf from %s: %s", uri, error->message);
        g_file_delete_async (file, G_PRIORITY_DEFAULT, NULL, NULL, NULL);
      }
  }

  self = PHOTOS_BASE_ITEM (user_data);
  priv = photos_base_item_get_instance_private (self);

  if (pixbuf == NULL)
    {
      priv->failed_thumbnailing = TRUE;
      g_clear_pointer (&priv->thumb_path, g_free);
      photos_base_item_set_failed_icon (self);
      goto out;
    }

  app = g_application_get_default ();
  scale = photos_application_get_scale_factor (PHOTOS_APPLICATION (app));
  icon_size = photos_utils_get_icon_size_unscaled ();
  scaled_pixbuf = photos_utils_downscale_pixbuf_for_scale (pixbuf, icon_size, scale);

  icon_size = photos_utils_get_icon_size ();
  centered_pixbuf = photos_utils_center_pixbuf (scaled_pixbuf, icon_size);
  photos_base_item_set_original_icon (self, centered_pixbuf);

 out:
  g_input_stream_close_async (stream, G_PRIORITY_DEFAULT, NULL, NULL, NULL);
}


static void
photos_base_item_refresh_thumb_path_read (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosBaseItem *self;
  PhotosBaseItemPrivate *priv;
  GFile *file = G_FILE (source_object);
  g_autoptr (GFileInputStream) stream = NULL;

  {
    g_autoptr (GError) error = NULL;

    stream = g_file_read_finish (file, res, &error);
    if (error != NULL)
      {
        g_autofree gchar *uri = NULL;

        if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
          goto out;

        uri = g_file_get_uri (file);
        g_warning ("Unable to read file at %s: %s", uri, error->message);
        g_file_delete_async (file, G_PRIORITY_DEFAULT, NULL, NULL, NULL);
      }
  }

  self = PHOTOS_BASE_ITEM (user_data);
  priv = photos_base_item_get_instance_private (self);

  if (stream == NULL)
    {
      priv->failed_thumbnailing = TRUE;
      g_clear_pointer (&priv->thumb_path, g_free);
      photos_base_item_set_failed_icon (self);
      goto out;
    }

  g_object_set_data_full (G_OBJECT (stream), "file", g_object_ref (file), g_object_unref);
  gdk_pixbuf_new_from_stream_async (G_INPUT_STREAM (stream),
                                    priv->cancellable,
                                    photos_base_item_refresh_thumb_path_pixbuf,
                                    self);
 out:
  return;
}


static void
photos_base_item_refresh_thumb_path (PhotosBaseItem *self)
{
  PhotosBaseItemPrivate *priv;
  g_autoptr (GFile) thumb_file = NULL;

  priv = photos_base_item_get_instance_private (self);

  thumb_file = g_file_new_for_path (priv->thumb_path);
  g_file_read_async (thumb_file,
                     G_PRIORITY_DEFAULT,
                     priv->cancellable,
                     photos_base_item_refresh_thumb_path_read,
                     self);
}


static void
photos_base_item_thumbnail_path_info (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosBaseItem *self = PHOTOS_BASE_ITEM (source_object);
  PhotosBaseItemPrivate *priv;
  g_autoptr (GFileInfo) info = NULL;

  priv = photos_base_item_get_instance_private (self);

  {
    g_autoptr (GError) error = NULL;

    info = photos_base_item_query_info_finish (self, res, &error);
    if (error != NULL)
      {
        if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
          goto out;

        g_warning ("Unable to query info for item at %s: %s", priv->uri, error->message);
      }
  }

  g_clear_pointer (&priv->thumb_path, g_free);

  if (info == NULL)
    {
      priv->failed_thumbnailing = TRUE;
      photos_base_item_set_failed_icon (self);
      goto out;
    }

  priv->thumb_path = g_strdup (g_file_info_get_attribute_byte_string (info, G_FILE_ATTRIBUTE_THUMBNAIL_PATH));
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
  return;
}


static void
photos_base_item_create_thumbnail_cb (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosBaseItem *self = PHOTOS_BASE_ITEM (source_object);
  PhotosBaseItemPrivate *priv;
  gboolean success;

  {
    g_autoptr (GError) error = NULL;

    success = photos_base_item_create_thumbnail_finish (self, res, &error);
    if (error != NULL)
      {
        if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
          goto out;

        g_warning ("Unable to create thumbnail: %s", error->message);
      }
  }

  priv = photos_base_item_get_instance_private (self);

  if (!success)
    {
      priv->failed_thumbnailing = TRUE;
      photos_base_item_set_failed_icon (self);
      goto out;
    }

  photos_base_item_query_info_async (self,
                                     G_FILE_ATTRIBUTE_THUMBNAIL_PATH,
                                     G_FILE_QUERY_INFO_NONE,
                                     G_PRIORITY_DEFAULT,
                                     priv->cancellable,
                                     photos_base_item_thumbnail_path_info,
                                     NULL);

 out:
  return;
}


static void
photos_base_item_file_query_info (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosBaseItem *self = PHOTOS_BASE_ITEM (source_object);
  PhotosBaseItemPrivate *priv;
  g_autoptr (GFileInfo) info = NULL;

  priv = photos_base_item_get_instance_private (self);

  {
    g_autoptr (GError) error = NULL;

    info = photos_base_item_query_info_finish (self, res, &error);
    if (error != NULL)
      {
        if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
          goto out;

        g_warning ("Unable to query info for item at %s: %s", priv->uri, error->message);
      }
  }

  g_clear_pointer (&priv->thumb_path, g_free);

  if (info == NULL)
    {
      priv->failed_thumbnailing = TRUE;
      photos_base_item_set_failed_icon (self);
      goto out;
    }

  priv->thumb_path = g_strdup (g_file_info_get_attribute_byte_string (info, G_FILE_ATTRIBUTE_THUMBNAIL_PATH));
  if (priv->thumb_path != NULL)
    photos_base_item_refresh_thumb_path (self);
  else
    {
      photos_base_item_create_thumbnail_async (self,
                                               priv->cancellable,
                                               photos_base_item_create_thumbnail_cb,
                                               NULL);
    }

 out:
  return;
}


static gchar *
photos_base_item_default_create_thumbnail_path (PhotosBaseItem *self)
{
  PhotosBaseItemPrivate *priv;
  const gchar *cache_dir;
  g_autofree gchar *filename = NULL;
  g_autofree gchar *md5 = NULL;
  gchar *path;
  g_autofree gchar *thumbnails_subdir = NULL;
  gint size;

  priv = photos_base_item_get_instance_private (self);

  md5 = g_compute_checksum_for_string (G_CHECKSUM_MD5, priv->uri, -1);
  filename = g_strconcat (md5, ".png", NULL);

  cache_dir = g_get_user_cache_dir ();
  size = photos_utils_get_icon_size ();
  thumbnails_subdir = g_strdup_printf ("%d-%d", size, THUMBNAIL_GENERATION);

  path = g_build_filename (cache_dir,
                           PACKAGE_TARNAME,
                           "thumbnails",
                           thumbnails_subdir,
                           filename,
                           NULL);

  return path;
}


static void
photos_base_item_default_set_favorite (PhotosBaseItem *self, gboolean favorite)
{
  PhotosBaseItemPrivate *priv;

  priv = photos_base_item_get_instance_private (self);

  if (favorite == priv->favorite)
    return;

  priv->favorite = favorite;
  photos_base_item_check_effects_and_update_info (self);
  photos_utils_set_favorite (priv->id, favorite);
}


static gboolean
photos_base_item_default_metadata_add_shared (PhotosBaseItem  *self,
                                              const gchar     *provider_type,
                                              const gchar     *account_identity,
                                              const gchar     *shared_id,
                                              GCancellable    *cancellable,
                                              GError         **error)
{
  return TRUE;
}


static void
photos_base_item_default_open (PhotosBaseItem *self, GtkWindow *parent, guint32 timestamp)
{
  PhotosBaseItemPrivate *priv;

  priv = photos_base_item_get_instance_private (self);

  if (priv->default_app_name == NULL)
    return;

  /* Without a default_app, launch in the web browser, otherwise use
   * that system application.
   */

  if (priv->default_app != NULL)
    {
      g_autoptr (GAppLaunchContext) ctx = NULL;

      ctx = photos_utils_new_app_launch_context_from_widget (GTK_WIDGET (parent));

      {
        g_autoptr (GError) error = NULL;

        photos_glib_app_info_launch_uri (priv->default_app, priv->uri, ctx, &error);
        if (error != NULL)
          g_warning ("Unable to show URI %s: %s", priv->uri, error->message);
      }
    }
  else
    {
      g_autoptr (GError) error = NULL;

      gtk_show_uri_on_window (parent, priv->uri, timestamp, &error);
      if (error != NULL)
        g_warning ("Unable to show URI %s: %s", priv->uri, error->message);
    }
}


static void
photos_base_item_default_refresh_icon (PhotosBaseItem *self)
{
  PhotosBaseItemPrivate *priv;

  priv = photos_base_item_get_instance_private (self);

  if (priv->thumb_path != NULL)
    {
      photos_base_item_refresh_thumb_path (self);
      return;
    }

  photos_base_item_set_thumbnailing_icon (self);

  if (priv->failed_thumbnailing)
    return;

  if (priv->collection)
    {
      photos_base_item_refresh_collection_icon (self);
      return;
    }

  photos_base_item_query_info_async (self,
                                     G_FILE_ATTRIBUTE_THUMBNAIL_PATH,
                                     G_FILE_QUERY_INFO_NONE,
                                     G_PRIORITY_DEFAULT,
                                     priv->cancellable,
                                     photos_base_item_file_query_info,
                                     NULL);
}


static void
photos_base_item_default_update_type_description (PhotosBaseItem *self)
{
  PhotosBaseItemPrivate *priv;
  gchar *description = NULL;

  priv = photos_base_item_get_instance_private (self);

  if (priv->collection)
    description = g_strdup (_("Album"));
  else if (priv->mime_type != NULL)
    description = g_content_type_get_description (priv->mime_type);

  photos_utils_take_string (&priv->type_description, description);
}


static void
photos_base_item_download_in_thread_func (GTask *task,
                                          gpointer source_object,
                                          gpointer task_data,
                                          GCancellable *cancellable)
{
  PhotosBaseItem *self = PHOTOS_BASE_ITEM (source_object);
  g_autoptr (GFile) file = NULL;

  {
    g_autoptr (GError) error = NULL;

    file = photos_base_item_download (self, cancellable, &error);
    if (error != NULL)
      {
        g_task_return_error (task, g_steal_pointer (&error));
        goto out;
      }
  }

  g_task_return_pointer (task, g_object_ref (file), g_object_unref);

 out:
  return;
}


static const gchar *
photos_base_item_filterable_get_id (PhotosFilterable *filterable)
{
  PhotosBaseItem *self = PHOTOS_BASE_ITEM (filterable);
  PhotosBaseItemPrivate *priv;

  priv = photos_base_item_get_instance_private (self);
  return priv->id;
}


static gboolean
photos_base_item_filterable_is_search_criterion (PhotosFilterable *filterable)
{
  return FALSE;
}


static cairo_surface_t *
photos_base_item_main_box_item_get_icon (GdMainBoxItem *box_item)
{
  PhotosBaseItem *self = PHOTOS_BASE_ITEM (box_item);
  PhotosBaseItemPrivate *priv;

  priv = photos_base_item_get_instance_private (self);
  return priv->surface;
}


static const gchar *
photos_base_item_main_box_item_get_id (GdMainBoxItem *box_item)
{
  PhotosBaseItem *self = PHOTOS_BASE_ITEM (box_item);
  PhotosBaseItemPrivate *priv;

  priv = photos_base_item_get_instance_private (self);
  return priv->id;
}


static const gchar *
photos_base_item_main_box_item_get_primary_text (GdMainBoxItem *box_item)
{
  PhotosBaseItem *self = PHOTOS_BASE_ITEM (box_item);
  PhotosBaseItemPrivate *priv;
  const gchar *primary_text;

  priv = photos_base_item_get_instance_private (self);

  if (priv->collection)
    primary_text = photos_base_item_get_name (self);
  else
    primary_text = NULL;

  return primary_text;
}


static const gchar *
photos_base_item_main_box_item_get_secondary_text (GdMainBoxItem *box_item)
{
  const gchar *author;

  author = photos_base_item_get_author (PHOTOS_BASE_ITEM (box_item));
  return author;
}


static const gchar *
photos_base_item_main_box_item_get_uri (GdMainBoxItem *box_item)
{
  const gchar *uri;

  uri = photos_base_item_get_uri (PHOTOS_BASE_ITEM (box_item));
  return uri;
}


static void
photos_base_item_refresh_executed (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  g_autoptr (PhotosBaseItem) self = PHOTOS_BASE_ITEM (user_data);
  PhotosSingleItemJob *job = PHOTOS_SINGLE_ITEM_JOB (source_object);
  TrackerSparqlCursor *cursor = NULL; /* TODO: Use g_autoptr */

  {
    g_autoptr (GError) error = NULL;

    cursor = photos_single_item_job_finish (job, res, &error);
    if (error != NULL)
      {
        g_warning ("Unable to query single item: %s", error->message);
        goto out;
      }
  }

  if (cursor == NULL)
    goto out;

  photos_base_item_populate_from_cursor (self, cursor);

 out:
  g_clear_object (&cursor);
}


static GeglBuffer *
photos_base_item_get_preview_source_buffer (PhotosBaseItem *self, gint size, gint scale)
{
  PhotosBaseItemPrivate *priv;
  const Babl *format;
  g_autoptr (GeglBuffer) buffer_cropped = NULL;
  g_autoptr (GeglBuffer) buffer_orig = NULL;
  g_autoptr (GeglBuffer) buffer = NULL;
  GeglBuffer *ret_val = NULL;
  GeglOperation *op;
  GeglRectangle bbox;
  GeglRectangle roi;
  const gchar *name;
  gdouble zoom;
  gint bpp;
  gint min_dimension;
  gint size_scaled;
  gint x;
  gint y;
  gint64 end;
  gint64 start;
  guchar *buf = NULL;

  priv = photos_base_item_get_instance_private (self);

  g_return_val_if_fail (!priv->collection, NULL);
  g_return_val_if_fail (priv->buffer_source != NULL, NULL);
  g_return_val_if_fail (priv->edit_graph != NULL, NULL);

  op = gegl_node_get_gegl_operation (priv->buffer_source);
  g_return_val_if_fail (op != NULL, NULL);

  name = gegl_operation_get_name (op);
  g_return_val_if_fail (g_strcmp0 (name, "gegl:buffer-source") == 0, NULL);

  size_scaled = size * scale;

  if (priv->preview_source_buffer != NULL)
    {
      bbox = *gegl_buffer_get_extent (priv->preview_source_buffer);
      if (bbox.height == size_scaled && bbox.width == size_scaled)
        {
          ret_val = priv->preview_source_buffer;
          goto out;
        }
      else
        {
          g_clear_object (&priv->preview_source_buffer);
        }
    }

  gegl_node_get (priv->buffer_source, "buffer", &buffer_orig, NULL);
  buffer = gegl_buffer_dup (buffer_orig);

  bbox = *gegl_buffer_get_extent (buffer);
  min_dimension = MIN (bbox.height, bbox.width);
  x = (gint) ((gdouble) (bbox.width - min_dimension) / 2.0 + 0.5);
  y = (gint) ((gdouble) (bbox.height - min_dimension) / 2.0 + 0.5);
  zoom = (gdouble) size_scaled / (gdouble) min_dimension;

  bbox.height = min_dimension;
  bbox.width = min_dimension;
  bbox.x = x;
  bbox.y = y;
  buffer_cropped = gegl_buffer_create_sub_buffer (buffer, &bbox);

  roi.height = size_scaled;
  roi.width = size_scaled;
  roi.x = (gint) ((gdouble) x * zoom + 0.5);
  roi.y = (gint) ((gdouble) y * zoom + 0.5);

  format = gegl_buffer_get_format (buffer_cropped);
  bpp = babl_format_get_bytes_per_pixel (format);
  buf = g_malloc0_n (roi.height * roi.width, bpp);

  start = g_get_monotonic_time ();

  gegl_buffer_get (buffer_cropped, &roi, zoom, format, buf, GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_NONE);

  end = g_get_monotonic_time ();
  photos_debug (PHOTOS_DEBUG_GEGL, "Get Preview Buffer: Downscale: %" G_GINT64_FORMAT, end - start);

  roi.x = 0;
  roi.y = 0;
  priv->preview_source_buffer = gegl_buffer_linear_new_from_data (buf,
                                                                  format,
                                                                  &roi,
                                                                  GEGL_AUTO_ROWSTRIDE,
                                                                  g_free,
                                                                  NULL);

  ret_val = priv->preview_source_buffer;

 out:
  return ret_val;
}


static void
photos_base_item_guess_save_sizes_from_buffer (GeglBuffer *buffer,
                                               const gchar *mime_type,
                                               gsize *out_full_size,
                                               gsize *out_reduced_size,
                                               GCancellable *cancellable)
{
  GeglNode *buffer_source;
  g_autoptr (GeglNode) graph = NULL;
  GeglNode *guess_sizes;
  guint64 sizes[2];

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
  if (out_full_size != NULL)
    *out_full_size = (gsize) sizes[0];
  if (out_reduced_size != NULL)
    *out_reduced_size = (gsize) sizes[1];
}


static void
photos_base_item_guess_save_sizes_in_thread_func (GTask *task,
                                                  gpointer source_object,
                                                  gpointer task_data,
                                                  GCancellable *cancellable)
{
  PhotosBaseItemSaveData *data = (PhotosBaseItemSaveData *) task_data;
  gsize *sizes;

  sizes = g_malloc0_n (2, sizeof (gsize));
  photos_base_item_guess_save_sizes_from_buffer (data->buffer, data->type, &sizes[0], &sizes[1], cancellable);
  g_task_return_pointer (task, sizes, g_free);
}


static void
photos_base_item_guess_save_sizes_load (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosBaseItem *self = PHOTOS_BASE_ITEM (source_object);
  PhotosBaseItemPrivate *priv;
  g_autoptr (GTask) task = G_TASK (user_data);
  g_autoptr (GeglBuffer) buffer = NULL;
  g_autoptr (GeglNode) graph = NULL;
  PhotosBaseItemSaveData *data;

  priv = photos_base_item_get_instance_private (self);

  {
    g_autoptr (GError) error = NULL;

    graph = photos_base_item_load_finish (self, res, &error);
    if (error != NULL)
      {
        g_task_return_error (task, g_steal_pointer (&error));
        goto out;
      }
  }

  buffer = photos_gegl_get_buffer_from_node (graph, NULL);
  data = photos_base_item_save_data_new (NULL, buffer, priv->mime_type, 0.0);
  g_task_set_task_data (task, data, (GDestroyNotify) photos_base_item_save_data_free);

  g_task_run_in_thread (task, photos_base_item_guess_save_sizes_in_thread_func);

 out:
  return;
}


static void
photos_base_item_process_process (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  g_autoptr (GTask) task = G_TASK (user_data);
  GeglProcessor *processor = GEGL_PROCESSOR (source_object);

  {
    g_autoptr (GError) error = NULL;

    if (!photos_gegl_processor_process_finish (processor, res, &error))
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
photos_base_item_process_async (PhotosBaseItem *self,
                                GCancellable *cancellable,
                                GAsyncReadyCallback callback,
                                gpointer user_data)
{
  PhotosBaseItemPrivate *priv;
  g_autoptr (GTask) task = NULL;
  PhotosPipeline *pipeline;

  g_return_if_fail (PHOTOS_IS_BASE_ITEM (self));
  priv = photos_base_item_get_instance_private (self);

  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
  g_return_if_fail (!priv->collection);

  pipeline = PHOTOS_PIPELINE (dzl_task_cache_peek (pipeline_cache, self));
  g_return_if_fail (PHOTOS_IS_PIPELINE (pipeline));

  g_clear_object (&priv->processor);
  priv->processor = photos_pipeline_new_processor (pipeline);

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, photos_base_item_process_async);

  photos_gegl_processor_process_async (priv->processor,
                                       cancellable,
                                       photos_base_item_process_process,
                                       g_object_ref (task));
}


static gboolean
photos_base_item_process_finish (PhotosBaseItem *self, GAsyncResult *res, GError **error)
{
  GTask *task;

  g_return_val_if_fail (PHOTOS_IS_BASE_ITEM (self), FALSE);

  g_return_val_if_fail (g_task_is_valid (res, self), FALSE);
  task = G_TASK (res);

  g_return_val_if_fail (g_task_get_source_tag (task) == photos_base_item_process_async, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  return g_task_propagate_boolean (task, error);
}


static void
photos_base_item_common_process (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosBaseItem *self = PHOTOS_BASE_ITEM (source_object);
  g_autoptr (GTask) task = G_TASK (user_data);

  {
    g_autoptr (GError) error = NULL;

    if (!photos_base_item_process_finish (self, res, &error))
      {
        g_task_return_error (task, g_steal_pointer (&error));
        goto out;
      }
  }

  g_task_return_boolean (task, TRUE);

 out:
  return;
}


static GeglBuffer *
photos_base_item_load_buffer (PhotosBaseItem *self, GCancellable *cancellable, GError **error)
{
  PhotosBaseItemPrivate *priv;
  g_autoptr (GFile) file = NULL;
  g_autoptr (GeglBuffer) buffer = NULL;
  GeglBuffer *ret_val = NULL;
  GeglNode *buffer_sink;
  g_autoptr (GeglNode) graph = NULL;
  GeglNode *load;
  g_autofree gchar *path = NULL;
  gint64 end;
  gint64 start;

  priv = photos_base_item_get_instance_private (self);

  file = photos_base_item_download (self, cancellable, error);
  if (file == NULL)
    goto out;

  path = g_file_get_path (file);
  if (!g_utf8_validate (path, -1, NULL))
    {
      g_set_error (error, PHOTOS_ERROR, 0, "Path is not UTF-8 encoded");
      goto out;
    }

  graph = gegl_node_new ();
  load = gegl_node_new_child (graph, "operation", "gegl:load", "path", path, NULL);
  buffer_sink = gegl_node_new_child (graph, "operation", "gegl:buffer-sink", "buffer", &buffer, NULL);

  gegl_node_link (load, buffer_sink);

  start = g_get_monotonic_time ();

  gegl_node_process (buffer_sink);
  ret_val = photos_gegl_buffer_apply_orientation (buffer, priv->orientation);

  end = g_get_monotonic_time ();
  photos_debug (PHOTOS_DEBUG_GEGL, "Buffer Load: From Local: %" G_GINT64_FORMAT, end - start);

 out:
  return ret_val;
}


static void
photos_base_item_load_buffer_in_thread_func (GTask *task,
                                             gpointer source_object,
                                             gpointer task_data,
                                             GCancellable *cancellable)
{
  PhotosBaseItem *self = PHOTOS_BASE_ITEM (source_object);
  g_autoptr (GeglBuffer) buffer = NULL;

  {
    g_autoptr (GError) error = NULL;

    buffer = photos_base_item_load_buffer (self, cancellable, &error);
    if (error != NULL)
      {
        g_task_return_error (task, g_steal_pointer (&error));
        goto out;
      }
  }

  g_task_return_pointer (task, g_object_ref (buffer), g_object_unref);

 out:
  return;
}


static void
photos_base_item_load_buffer_async (PhotosBaseItem *self,
                                    GCancellable *cancellable,
                                    GAsyncReadyCallback callback,
                                    gpointer user_data)
{
  g_autoptr (GTask) task = NULL;

  g_return_if_fail (PHOTOS_IS_BASE_ITEM (self));
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_return_on_cancel (task, TRUE);
  g_task_set_source_tag (task, photos_base_item_load_buffer_async);

  g_task_run_in_thread (task, photos_base_item_load_buffer_in_thread_func);
}


static GeglBuffer *
photos_base_item_load_buffer_finish (PhotosBaseItem *self, GAsyncResult *res, GError **error)
{
  GTask *task;

  g_return_val_if_fail (g_task_is_valid (res, self), NULL);
  task = G_TASK (res);

  g_return_val_if_fail (g_task_get_source_tag (task) == photos_base_item_load_buffer_async, NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  return g_task_propagate_pointer (task, error);
}


static void
photos_base_item_load_pipeline_task_cache_populate_new (GObject *source_object,
                                                        GAsyncResult *res,
                                                        gpointer user_data)
{
  g_autoptr (GTask) task = G_TASK (user_data);
  g_autoptr (PhotosPipeline) pipeline = NULL;

  {
    g_autoptr (GError) error = NULL;

    pipeline = photos_pipeline_new_finish (res, &error);
    if (error != NULL)
      {
        g_task_return_error (task, g_steal_pointer (&error));
        goto out;
      }
  }

  g_task_return_pointer (task, g_object_ref (pipeline), g_object_unref);

 out:
  return;
}


static void
photos_base_item_load_pipeline_task_cache_populate (DzlTaskCache *cache,
                                                    gconstpointer key,
                                                    GTask *task,
                                                    gpointer user_data)
{
  PhotosBaseItem *self = PHOTOS_BASE_ITEM ((gpointer) key);
  PhotosBaseItemClass *class;
  GCancellable *cancellable;
  g_auto (GStrv) uris = NULL;

  cancellable = g_task_get_cancellable (task);

  class = PHOTOS_BASE_ITEM_GET_CLASS (self);
  if (class->create_pipeline_paths != NULL)
    {
      g_auto (GStrv) paths = NULL;

      paths = photos_base_item_create_pipeline_paths (self);
      uris = photos_utils_convert_paths_to_uris ((const gchar *const *) paths);
    }

  photos_pipeline_new_async (NULL,
                             (const gchar *const *) uris,
                             cancellable,
                             photos_base_item_load_pipeline_task_cache_populate_new,
                             g_object_ref (task));
}


static void
photos_base_item_load_pipeline_task_cache_get (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  g_autoptr (GTask) task = G_TASK (user_data);
  DzlTaskCache *cache = DZL_TASK_CACHE (source_object);
  g_autoptr (PhotosPipeline) pipeline = NULL;

  g_assert_true (cache == pipeline_cache);

  {
    g_autoptr (GError) error = NULL;

    pipeline = dzl_task_cache_get_finish (cache, res, &error);
    if (error != NULL)
      {
        g_task_return_error (task, g_steal_pointer (&error));
        goto out;
      }
  }

  g_task_return_pointer (task, g_object_ref (pipeline), g_object_unref);

 out:
  return;
}


static void
photos_base_item_load_pipeline_async (PhotosBaseItem *self,
                                      GCancellable *cancellable,
                                      GAsyncReadyCallback callback,
                                      gpointer user_data)
{
  PhotosBaseItemPrivate *priv;
  g_autoptr (GTask) task = NULL;

  g_return_if_fail (PHOTOS_IS_BASE_ITEM (self));
  priv = photos_base_item_get_instance_private (self);

  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
  g_return_if_fail (!priv->collection);

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, photos_base_item_load_pipeline_async);

  dzl_task_cache_get_async (pipeline_cache,
                            self,
                            FALSE,
                            cancellable,
                            photos_base_item_load_pipeline_task_cache_get,
                            g_object_ref (task));
}


static PhotosPipeline *
photos_base_item_load_pipeline_finish (PhotosBaseItem *self, GAsyncResult *res, GError **error)
{
  GTask *task;

  g_return_val_if_fail (PHOTOS_IS_BASE_ITEM (self), NULL);

  g_return_val_if_fail (g_task_is_valid (res, self), NULL);
  task = G_TASK (res);

  g_return_val_if_fail (g_task_get_source_tag (task) == photos_base_item_load_pipeline_async, NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  return g_task_propagate_pointer (task, error);
}


static void
photos_base_item_load_process (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  g_autoptr (GTask) task = G_TASK (user_data);
  PhotosBaseItem *self;
  GeglNode *graph;
  PhotosPipeline *pipeline;

  self = PHOTOS_BASE_ITEM (g_task_get_source_object (task));

  {
    g_autoptr (GError) error = NULL;

    photos_base_item_process_finish (self, res, &error);
    if (error != NULL)
      {
        photos_base_item_clear_pixels (self);
        g_task_return_error (task, g_steal_pointer (&error));
        goto out;
      }
  }

  pipeline = PHOTOS_PIPELINE (dzl_task_cache_peek (pipeline_cache, self));
  g_assert_true (PHOTOS_IS_PIPELINE (pipeline));

  graph = photos_pipeline_get_graph (pipeline);
  g_task_return_pointer (task, g_object_ref (graph), g_object_unref);

 out:
  return;
}


static void
photos_base_item_load_load_buffer (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  g_autoptr (GTask) task = G_TASK (user_data);
  PhotosBaseItem *self;
  PhotosBaseItemPrivate *priv;
  const Babl *format;
  GCancellable *cancellable;
  g_autoptr (GeglBuffer) buffer = NULL;
  GeglRectangle bbox;
  const gchar *format_name;

  self = PHOTOS_BASE_ITEM (g_task_get_source_object (task));
  priv = photos_base_item_get_instance_private (self);

  cancellable = g_task_get_cancellable (task);

  {
    g_autoptr (GError) error = NULL;

    buffer = photos_base_item_load_buffer_finish (self, res, &error);
    if (error != NULL)
      {
        photos_base_item_clear_pixels (self);
        g_task_return_error (task, g_steal_pointer (&error));
        goto out;
      }
  }

  bbox = *gegl_buffer_get_extent (buffer);
  format = gegl_buffer_get_format (buffer);
  format_name = babl_get_name (format);
  photos_debug (PHOTOS_DEBUG_GEGL, "Buffer loaded: %d, %d, %d×%d, %s",
                bbox.x,
                bbox.y,
                bbox.width,
                bbox.height,
                format_name);

  gegl_node_set (priv->buffer_source, "buffer", buffer, NULL);

  photos_base_item_process_async (self, cancellable, photos_base_item_load_process, g_object_ref (task));

 out:
  return;
}


static void
photos_base_item_load_load_pipeline (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  g_autoptr (GTask) task = G_TASK (user_data);
  PhotosBaseItem *self;
  PhotosBaseItemPrivate *priv;
  GeglNode *graph;
  GCancellable *cancellable;
  g_autoptr (PhotosPipeline) pipeline = NULL;

  self = PHOTOS_BASE_ITEM (g_task_get_source_object (task));
  priv = photos_base_item_get_instance_private (self);

  cancellable = g_task_get_cancellable (task);

  {
    g_autoptr (GError) error = NULL;

    pipeline = photos_base_item_load_pipeline_finish (self, res, &error);
    if (error != NULL)
      {
        photos_base_item_clear_pixels (self);
        g_task_return_error (task, g_steal_pointer (&error));
        goto out;
      }
  }

  if (priv->edit_graph == NULL)
    priv->edit_graph = gegl_node_new ();

  photos_pipeline_set_parent (pipeline, priv->edit_graph);

  priv->buffer_source = gegl_node_new_child (priv->edit_graph, "operation", "gegl:buffer-source", NULL);
  graph = photos_pipeline_get_graph (pipeline);
  gegl_node_link (priv->buffer_source, graph);

  photos_base_item_load_buffer_async (self, cancellable, photos_base_item_load_load_buffer, g_object_ref (task));

 out:
  return;
}


static void
photos_base_item_metadata_add_shared_in_thread_func (GTask *task,
                                                     gpointer source_object,
                                                     gpointer task_data,
                                                     GCancellable *cancellable)
{
  PhotosBaseItem *self = PHOTOS_BASE_ITEM (source_object);
  PhotosBaseItemMetadataAddSharedData *data = (PhotosBaseItemMetadataAddSharedData *) task_data;
  gboolean result;

  {
    g_autoptr (GError) error = NULL;

    result = PHOTOS_BASE_ITEM_GET_CLASS (self)->metadata_add_shared (self,
                                                                     data->provider_type,
                                                                     data->account_identity,
                                                                     data->shared_id,
                                                                     cancellable,
                                                                     &error);
    if (error != NULL)
      {
        g_task_return_error (task, g_steal_pointer (&error));
        return;
      }
  }

  g_task_return_boolean (task, result);
}


static void
photos_base_item_pipeline_is_edited_load_pipeline (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosBaseItem *self = PHOTOS_BASE_ITEM (source_object);
  g_autoptr (GTask) task = G_TASK (user_data);
  g_autoptr (PhotosPipeline) pipeline = NULL;
  gboolean is_edited;

  {
    g_autoptr (GError) error = NULL;

    pipeline = photos_base_item_load_pipeline_finish (self, res, &error);
    if (error != NULL)
      {
        g_task_return_error (task, g_steal_pointer (&error));
        goto out;
      }
  }

  is_edited = photos_pipeline_is_edited (pipeline);
  g_task_return_boolean (task, is_edited);

  if (!photos_base_item_can_edit (self))
    g_return_if_fail (!is_edited);

 out:
  return;
}


static void
photos_base_item_pipeline_save_delete (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosBaseItem *self;
  PhotosBaseItemPrivate *priv;
  GFile *thumbnail_file = G_FILE (source_object);
  g_autoptr (GTask) task = G_TASK (user_data);

  self = PHOTOS_BASE_ITEM (g_task_get_source_object (task));
  priv = photos_base_item_get_instance_private (self);

  {
    g_autoptr (GError) error = NULL;

    if (!g_file_delete_finish (thumbnail_file, res, &error))
      {
        if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND))
          {
            g_autofree gchar *uri = NULL;

            uri = g_file_get_uri (thumbnail_file);
            g_warning ("Unable to delete thumbnail %s: %s", uri, error->message);
          }
      }
  }

  /* Mark the task as a success, no matter what. The pipeline has
   * already been saved, so it doesn't make sense to fail the task
   * just because we failed to delete the old thumbnail.
   */

  g_clear_pointer (&priv->thumb_path, g_free);
  photos_base_item_refresh (self);
  g_task_return_boolean (task, TRUE);
}


static void
photos_base_item_pipeline_save_save (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  g_autoptr (GTask) task = G_TASK (user_data);
  PhotosBaseItem *self;
  PhotosBaseItemPrivate *priv;
  GCancellable *cancellable;
  g_autoptr (GFile) thumbnail_file = NULL;
  PhotosPipeline *pipeline = PHOTOS_PIPELINE (source_object);
  g_autofree gchar *thumbnail_path = NULL;

  self = PHOTOS_BASE_ITEM (g_task_get_source_object (task));
  priv = photos_base_item_get_instance_private (self);

  cancellable = g_task_get_cancellable (task);

  {
    g_autoptr (GError) error = NULL;

    if (!photos_pipeline_save_finish (pipeline, res, &error))
      {
        g_task_return_error (task, g_steal_pointer (&error));
        goto out;
      }
  }

  g_cancellable_cancel (priv->cancellable);
  g_clear_object (&priv->cancellable);
  priv->cancellable = g_cancellable_new ();

  thumbnail_path = photos_base_item_create_thumbnail_path (self);
  thumbnail_file = g_file_new_for_path (thumbnail_path);
  g_file_delete_async (thumbnail_file,
                       G_PRIORITY_DEFAULT,
                       cancellable,
                       photos_base_item_pipeline_save_delete,
                       g_object_ref (task));

 out:
  return;
}


static void
photos_base_item_save_metadata_in_thread_func (GTask *task,
                                               gpointer source_object,
                                               gpointer task_data,
                                               GCancellable *cancellable)
{
  PhotosBaseItem *self = PHOTOS_BASE_ITEM (source_object);
  PhotosBaseItemPrivate *priv;
  GFile *export_file = G_FILE (task_data);
  g_autoptr (GFile) source_file = NULL;
  g_autoptr (GExiv2Metadata) metadata = NULL;
  g_autofree gchar *export_path = NULL;
  g_autofree gchar *source_path = NULL;

  priv = photos_base_item_get_instance_private (self);

  g_mutex_lock (&priv->mutex_save_metadata);

  {
    g_autoptr (GError) error = NULL;

    source_file = photos_base_item_download (self, cancellable, &error);
    if (error != NULL)
      {
        g_task_return_error (task, g_steal_pointer (&error));
        goto out;
      }
  }

  metadata = gexiv2_metadata_new ();
  source_path = g_file_get_path (source_file);

  {
    g_autoptr (GError) error = NULL;

    if (!gexiv2_metadata_open_path (metadata, source_path, &error))
      {
        g_task_return_error (task, g_steal_pointer (&error));
        goto out;
      }
  }

  gexiv2_metadata_try_set_orientation (metadata, GEXIV2_ORIENTATION_NORMAL, NULL);
  export_path = g_file_get_path (export_file);

  {
    g_autoptr (GError) error = NULL;

    if (!gexiv2_metadata_save_file (metadata, export_path, &error))
      {
        g_task_return_error (task, g_steal_pointer (&error));
        goto out;
      }
  }

  g_task_return_boolean (task, TRUE);

 out:
  g_mutex_unlock (&priv->mutex_save_metadata);
}


static void
photos_base_item_save_metadata_async (PhotosBaseItem *self,
                                      GFile *file,
                                      GCancellable *cancellable,
                                      GAsyncReadyCallback callback,
                                      gpointer user_data)
{
  g_autoptr (GTask) task = NULL;

  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, photos_base_item_save_metadata_async);
  g_task_set_task_data (task, g_object_ref (file), g_object_unref);

  g_task_run_in_thread (task, photos_base_item_save_metadata_in_thread_func);
}


static gboolean
photos_base_item_save_metadata_finish (PhotosBaseItem *self, GAsyncResult *res, GError **error)
{
  GTask *task;

  g_return_val_if_fail (g_task_is_valid (res, self), FALSE);
  task = G_TASK (res);

  g_return_val_if_fail (g_task_get_source_tag (task) == photos_base_item_save_metadata_async, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  return g_task_propagate_boolean (task, error);
}


static void
photos_base_item_save_buffer_save_metadata (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosBaseItem *self = PHOTOS_BASE_ITEM (source_object);
  g_autoptr (GTask) task = G_TASK (user_data);

  {
    g_autoptr (GError) error = NULL;

    if (!photos_base_item_save_metadata_finish (self, res, &error))
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
photos_base_item_save_buffer_stream_close (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  g_autoptr (GTask) task = G_TASK (user_data);
  PhotosBaseItem *self;
  PhotosBaseItemSaveBufferData *data;
  GCancellable *cancellable;
  GOutputStream *stream = G_OUTPUT_STREAM (source_object);

  self = PHOTOS_BASE_ITEM (g_task_get_source_object (task));
  cancellable = g_task_get_cancellable (task);
  data = (PhotosBaseItemSaveBufferData *) g_task_get_task_data (task);

  {
    g_autoptr (GError) error = NULL;

    if (!g_output_stream_close_finish (stream, res, &error))
      {
        g_task_return_error (task, g_steal_pointer (&error));
        goto out;
      }
  }

  photos_base_item_save_metadata_async (self,
                                        data->file,
                                        cancellable,
                                        photos_base_item_save_buffer_save_metadata,
                                        g_object_ref (task));

 out:
  return;
}


static void
photos_base_item_save_buffer_save_to_stream (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  g_autoptr (GTask) task = G_TASK (user_data);
  PhotosBaseItemSaveBufferData *data;
  GCancellable *cancellable;

  cancellable = g_task_get_cancellable (task);
  data = (PhotosBaseItemSaveBufferData *) g_task_get_task_data (task);

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
                               photos_base_item_save_buffer_stream_close,
                               g_object_ref (task));

 out:
  return;
}


static void
photos_base_item_save_buffer_async (PhotosBaseItem *self,
                                    GeglBuffer *buffer,
                                    GFile *file,
                                    GFileOutputStream *stream,
                                    GCancellable *cancellable,
                                    GAsyncReadyCallback callback,
                                    gpointer user_data)
{
  PhotosBaseItemPrivate *priv;
  g_autoptr (GTask) task = NULL;
  g_autoptr (GdkPixbuf) pixbuf = NULL;
  GeglNode *buffer_source;
  g_autoptr (GeglNode) graph = NULL;
  PhotosBaseItemSaveBufferData *data;

  priv = photos_base_item_get_instance_private (self);

  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  data = photos_base_item_save_buffer_data_new (file, stream);

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, photos_base_item_save_buffer_async);
  g_task_set_task_data (task, data, (GDestroyNotify) photos_base_item_save_buffer_data_free);

  graph = gegl_node_new ();
  buffer_source = gegl_node_new_child (graph, "operation", "gegl:buffer-source", "buffer", buffer, NULL);
  pixbuf = photos_gegl_create_pixbuf_from_node (buffer_source);
  if (pixbuf == NULL)
    {
      g_task_return_new_error (task, PHOTOS_ERROR, 0, "Failed to create a GdkPixbuf from the GeglBuffer");
      goto out;
    }

  if (g_strcmp0 (priv->mime_type, "image/png") == 0)
    {
      gdk_pixbuf_save_to_stream_async (pixbuf,
                                       G_OUTPUT_STREAM (stream),
                                       "png",
                                       cancellable,
                                       photos_base_item_save_buffer_save_to_stream,
                                       g_object_ref (task),
                                       NULL);
    }
  else
    {
      gdk_pixbuf_save_to_stream_async (pixbuf,
                                       G_OUTPUT_STREAM (stream),
                                       "jpeg",
                                       cancellable,
                                       photos_base_item_save_buffer_save_to_stream,
                                       g_object_ref (task),
                                       "quality", "90",
                                       NULL);
    }

 out:
  return;
}


static gboolean
photos_base_item_save_buffer_finish (PhotosBaseItem *self, GAsyncResult *res, GError **error)
{
  GTask *task;

  g_return_val_if_fail (g_task_is_valid (res, self), FALSE);
  task = G_TASK (res);

  g_return_val_if_fail (g_task_get_source_tag (task) == photos_base_item_save_buffer_async, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  return g_task_propagate_boolean (task, error);
}


static void
photos_base_item_save_to_dir_save_buffer (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosBaseItem *self = PHOTOS_BASE_ITEM (source_object);
  g_autoptr (GTask) task = G_TASK (user_data);
  PhotosBaseItemSaveData *data;

  data = (PhotosBaseItemSaveData *) g_task_get_task_data (task);

  {
    g_autoptr (GError) error = NULL;

    if (!photos_base_item_save_buffer_finish (self, res, &error))
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
photos_base_item_save_to_dir_file_create (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosBaseItem *self;
  GCancellable *cancellable;
  GFile *file = G_FILE (source_object);
  g_autoptr (GFile) unique_file = NULL;
  g_autoptr (GFileOutputStream) stream = NULL;
  g_autoptr (GTask) task = G_TASK (user_data);
  PhotosBaseItemSaveData *data;

  self = PHOTOS_BASE_ITEM (g_task_get_source_object (task));
  cancellable = g_task_get_cancellable (task);
  data = (PhotosBaseItemSaveData *) g_task_get_task_data (task);

  {
    g_autoptr (GError) error = NULL;

    stream = photos_glib_file_create_finish (file, res, &unique_file, &error);
    if (error != NULL)
      {
        g_task_return_error (task, g_steal_pointer (&error));
        goto out;
      }
  }

  g_assert_true (G_IS_FILE_OUTPUT_STREAM (stream));

  g_assert_null (data->unique_file);
  g_assert_true (G_IS_FILE (unique_file));
  data->unique_file = g_object_ref (unique_file);

  photos_base_item_save_buffer_async (self,
                                      data->buffer,
                                      unique_file,
                                      stream,
                                      cancellable,
                                      photos_base_item_save_to_dir_save_buffer,
                                      g_object_ref (task));

 out:
  return;
}


static void
photos_base_item_save_to_dir_buffer_zoom (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosBaseItem *self;
  PhotosBaseItemPrivate *priv;
  GCancellable *cancellable;
  g_autoptr (GFile) file = NULL;
  g_autoptr (GTask) task = G_TASK (user_data);
  GeglBuffer *buffer = GEGL_BUFFER (source_object);
  g_autoptr (GeglBuffer) buffer_zoomed = NULL;
  PhotosBaseItemSaveData *data;
  const gchar *extension;
  g_autofree gchar *basename = NULL;
  g_autofree gchar *filename = NULL;

  self = PHOTOS_BASE_ITEM (g_task_get_source_object (task));
  priv = photos_base_item_get_instance_private (self);

  cancellable = g_task_get_cancellable (task);
  data = (PhotosBaseItemSaveData *) g_task_get_task_data (task);

  {
    g_autoptr (GError) error = NULL;

    buffer_zoomed = photos_gegl_buffer_zoom_finish (buffer, res, &error);
    if (error != NULL)
      {
        g_task_return_error (task, g_steal_pointer (&error));
        goto out;
      }
  }

  g_assert_null (data->buffer);
  data->buffer = g_object_ref (buffer_zoomed);

  basename = photos_glib_filename_strip_extension (priv->filename);
  extension = g_strcmp0 (priv->mime_type, "image/png") == 0 ? ".png" : ".jpg";
  filename = g_strconcat (basename, extension, NULL);

  file = g_file_get_child (data->dir, filename);
  photos_glib_file_create_async (file,
                                 G_FILE_CREATE_NONE,
                                 G_PRIORITY_DEFAULT,
                                 cancellable,
                                 photos_base_item_save_to_dir_file_create,
                                 g_object_ref (task));

 out:
  return;
}


static void
photos_base_item_save_to_dir_load (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosBaseItem *self = PHOTOS_BASE_ITEM (source_object);
  GCancellable *cancellable;
  g_autoptr (GTask) task = G_TASK (user_data);
  g_autoptr (GeglBuffer) buffer = NULL;
  g_autoptr (GeglNode) graph = NULL;
  PhotosBaseItemSaveData *data;

  cancellable = g_task_get_cancellable (task);
  data = (PhotosBaseItemSaveData *) g_task_get_task_data (task);

  {
    g_autoptr (GError) error = NULL;

    graph = photos_base_item_load_finish (self, res, &error);
    if (error != NULL)
      {
        g_task_return_error (task, g_steal_pointer (&error));
        goto out;
      }
  }

  buffer = photos_gegl_get_buffer_from_node (graph, NULL);
  photos_gegl_buffer_zoom_async (buffer,
                                 data->zoom,
                                 cancellable,
                                 photos_base_item_save_to_dir_buffer_zoom,
                                 g_object_ref (task));

 out:
  return;
}


static void
photos_base_item_save_to_file_save_buffer (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosBaseItem *self = PHOTOS_BASE_ITEM (source_object);
  g_autoptr (GTask) task = G_TASK (user_data);

  {
    g_autoptr (GError) error = NULL;

    if (!photos_base_item_save_buffer_finish (self, res, &error))
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
photos_base_item_save_to_file_file_replace (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosBaseItem *self;
  GCancellable *cancellable;
  GFile *file = G_FILE (source_object);
  g_autoptr (GFileOutputStream) stream = NULL;
  g_autoptr (GTask) task = G_TASK (user_data);
  PhotosBaseItemSaveToFileData *data;

  self = PHOTOS_BASE_ITEM (g_task_get_source_object (task));
  cancellable = g_task_get_cancellable (task);
  data = (PhotosBaseItemSaveToFileData *) g_task_get_task_data (task);

  {
    g_autoptr (GError) error = NULL;

    stream = g_file_replace_finish (file, res, &error);
    if (error != NULL)
      {
        g_task_return_error (task, g_steal_pointer (&error));
        goto out;
      }
  }

  photos_base_item_save_buffer_async (self,
                                      data->buffer,
                                      file,
                                      stream,
                                      cancellable,
                                      photos_base_item_save_to_file_save_buffer,
                                      g_object_ref (task));

 out:
  return;
}


static void
photos_base_item_save_to_file_buffer_zoom (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  GCancellable *cancellable;
  g_autoptr (GTask) task = G_TASK (user_data);
  GeglBuffer *buffer = GEGL_BUFFER (source_object);
  g_autoptr (GeglBuffer) buffer_zoomed = NULL;
  PhotosBaseItemSaveToFileData *data;

  cancellable = g_task_get_cancellable (task);
  data = (PhotosBaseItemSaveToFileData *) g_task_get_task_data (task);

  {
    g_autoptr (GError) error = NULL;

    buffer_zoomed = photos_gegl_buffer_zoom_finish (buffer, res, &error);
    if (error != NULL)
      {
        g_task_return_error (task, g_steal_pointer (&error));
        goto out;
      }
  }

  g_assert_null (data->buffer);
  data->buffer = g_object_ref (buffer_zoomed);

  g_file_replace_async (data->file,
                        NULL,
                        FALSE,
                        data->flags,
                        G_PRIORITY_DEFAULT,
                        cancellable,
                        photos_base_item_save_to_file_file_replace,
                        g_object_ref (task));

 out:
  return;
}


static void
photos_base_item_save_to_file_load (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosBaseItem *self = PHOTOS_BASE_ITEM (source_object);
  GCancellable *cancellable;
  g_autoptr (GTask) task = G_TASK (user_data);
  g_autoptr (GeglBuffer) buffer = NULL;
  g_autoptr (GeglNode) graph = NULL;
  PhotosBaseItemSaveToFileData *data;

  cancellable = g_task_get_cancellable (task);
  data = (PhotosBaseItemSaveToFileData *) g_task_get_task_data (task);

  {
    g_autoptr (GError) error = NULL;

    graph = photos_base_item_load_finish (self, res, &error);
    if (error != NULL)
      {
        g_task_return_error (task, g_steal_pointer (&error));
        goto out;
      }
  }

  buffer = photos_gegl_get_buffer_from_node (graph, NULL);
  photos_gegl_buffer_zoom_async (buffer,
                                 data->zoom,
                                 cancellable,
                                 photos_base_item_save_to_file_buffer_zoom,
                                 g_object_ref (task));

 out:
  return;
}


static void
photos_base_item_save_to_stream_file_delete (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  GFile *file = G_FILE (source_object);
  g_autoptr (GTask) task = G_TASK (user_data);

  {
    g_autoptr (GError) error = NULL;

    if (!g_file_delete_finish (file, res, &error))
      {
        g_autofree gchar *uri = NULL;

        uri = g_file_get_uri (file);
        g_warning ("Unable to delete temporary file %s: %s", uri, error->message);
      }
  }

  /* Mark the task as a success, no matter what. The item has already
   * been saved to the GOutputStream, so it doesn't make sense to
   * fail the task just because we failed to delete the temporary
   * file created during the process.
   */
  g_task_return_boolean (task, TRUE);
}


static void
photos_base_item_save_to_stream_stream_close (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  GCancellable *cancellable;
  GIOStream *iostream = G_IO_STREAM (source_object);
  g_autoptr (GTask) task = G_TASK (user_data);
  PhotosBaseItemSaveToStreamData *data;

  cancellable = g_task_get_cancellable (task);
  data = (PhotosBaseItemSaveToStreamData *) g_task_get_task_data (task);

  {
    g_autoptr (GError) error = NULL;

    g_io_stream_close_finish (iostream, res, &error);
    if (error != NULL)
      {
        g_task_return_error (task, g_steal_pointer (&error));
        goto out;
      }
  }

  g_file_delete_async (data->file,
                       G_PRIORITY_DEFAULT,
                       cancellable,
                       photos_base_item_save_to_stream_file_delete,
                       g_object_ref (task));

 out:
  return;
}


static void
photos_base_item_save_to_stream_stream_splice (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  GCancellable *cancellable;
  GOutputStream *ostream = G_OUTPUT_STREAM (source_object);
  g_autoptr (GTask) task = G_TASK (user_data);
  PhotosBaseItemSaveToStreamData *data;

  cancellable = g_task_get_cancellable (task);
  data = (PhotosBaseItemSaveToStreamData *) g_task_get_task_data (task);

  {
    g_autoptr (GError) error = NULL;

    g_output_stream_splice_finish (ostream, res, &error);
    if (error != NULL)
      {
        g_task_return_error (task, g_steal_pointer (&error));
        goto out;
      }
  }

  g_io_stream_close_async (G_IO_STREAM (data->iostream),
                           G_PRIORITY_DEFAULT,
                           cancellable,
                           photos_base_item_save_to_stream_stream_close,
                           g_object_ref (task));

 out:
  return;
}


static void
photos_base_item_save_to_stream_save_buffer (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosBaseItem *self = PHOTOS_BASE_ITEM (source_object);
  GCancellable *cancellable;
  GInputStream *istream;
  g_autoptr (GTask) task = G_TASK (user_data);
  PhotosBaseItemSaveToStreamData *data;

  cancellable = g_task_get_cancellable (task);
  data = (PhotosBaseItemSaveToStreamData *) g_task_get_task_data (task);

  {
    g_autoptr (GError) error = NULL;

    if (!photos_base_item_save_buffer_finish (self, res, &error))
      {
        g_task_return_error (task, g_steal_pointer (&error));
        goto out;
      }
  }

  istream = g_io_stream_get_input_stream (G_IO_STREAM (data->iostream));
  g_assert_true (g_seekable_can_seek (G_SEEKABLE (istream)));

  {
    g_autoptr (GError) error = NULL;

    if (!g_seekable_seek (G_SEEKABLE (istream), 0, G_SEEK_SET, cancellable, &error))
      {
        g_task_return_error (task, g_steal_pointer (&error));
        goto out;
      }
  }

  g_output_stream_splice_async (data->ostream,
                                istream,
                                G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE | G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET,
                                G_PRIORITY_DEFAULT,
                                cancellable,
                                photos_base_item_save_to_stream_stream_splice,
                                g_object_ref (task));

 out:
  return;
}


static void
photos_base_item_save_to_stream_buffer_zoom (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  g_autoptr (GTask) task = G_TASK (user_data);
  PhotosBaseItem *self;
  GCancellable *cancellable;
  g_autoptr (GFile) file = NULL;
  g_autoptr (GFileIOStream) iostream = NULL;
  GOutputStream *ostream;
  GeglBuffer *buffer = GEGL_BUFFER (source_object);
  g_autoptr (GeglBuffer) buffer_zoomed = NULL;
  PhotosBaseItemSaveToStreamData *data;

  self = PHOTOS_BASE_ITEM (g_task_get_source_object (task));
  cancellable = g_task_get_cancellable (task);
  data = (PhotosBaseItemSaveToStreamData *) g_task_get_task_data (task);

  {
    g_autoptr (GError) error = NULL;

    buffer_zoomed = photos_gegl_buffer_zoom_finish (buffer, res, &error);
    if (error != NULL)
      {
        g_task_return_error (task, g_steal_pointer (&error));
        goto out;
      }
  }

  {
    g_autoptr (GError) error = NULL;

    file = g_file_new_tmp (PACKAGE_TARNAME "-XXXXXX", &iostream, &error);
    if (error != NULL)
      {
        g_task_return_error (task, g_steal_pointer (&error));
        goto out;
      }
  }

  g_assert_null (data->file);
  data->file = g_object_ref (file);

  g_assert_null (data->iostream);
  data->iostream = g_object_ref (iostream);

  ostream = g_io_stream_get_output_stream (G_IO_STREAM (iostream));
  photos_base_item_save_buffer_async (self,
                                      buffer_zoomed,
                                      file,
                                      G_FILE_OUTPUT_STREAM (ostream),
                                      cancellable,
                                      photos_base_item_save_to_stream_save_buffer,
                                      g_object_ref (task));

 out:
  return;
}


static void
photos_base_item_save_to_stream_load (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosBaseItem *self = PHOTOS_BASE_ITEM (source_object);
  GCancellable *cancellable;
  g_autoptr (GTask) task = G_TASK (user_data);
  g_autoptr (GeglBuffer) buffer = NULL;
  g_autoptr (GeglNode) graph = NULL;
  PhotosBaseItemSaveToStreamData *data;

  cancellable = g_task_get_cancellable (task);
  data = (PhotosBaseItemSaveToStreamData *) g_task_get_task_data (task);

  {
    g_autoptr (GError) error = NULL;

    graph = photos_base_item_load_finish (self, res, &error);
    if (error != NULL)
      {
        g_task_return_error (task, g_steal_pointer (&error));
        goto out;
      }
  }

  buffer = photos_gegl_get_buffer_from_node (graph, NULL);
  photos_gegl_buffer_zoom_async (buffer,
                                 data->zoom,
                                 cancellable,
                                 photos_base_item_save_to_stream_buffer_zoom,
                                 g_object_ref (task));

 out:
  return;
}


static void
photos_base_item_trash_trash (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosBaseItem *self = PHOTOS_BASE_ITEM (source_object);
  GApplication *app;
  g_autoptr (GTask) task = G_TASK (user_data);

  {
    g_autoptr (GError) error = NULL;

    if (!PHOTOS_BASE_ITEM_GET_CLASS (self)->trash_finish (self, res, &error))
      {
        g_task_return_error (task, g_steal_pointer (&error));
        goto out;
      }
  }

  g_task_return_boolean (task, TRUE);

 out:
  app = g_application_get_default ();
  g_application_release (app);
}


static void
photos_base_item_update_info_from_type (PhotosBaseItem *self)
{
  PhotosBaseItemPrivate *priv;

  priv = photos_base_item_get_instance_private (self);

  if (strstr (priv->rdf_type, "nfo#DataContainer") != NULL)
    priv->collection = TRUE;

  PHOTOS_BASE_ITEM_GET_CLASS (self)->update_type_description (self);
}


static void
photos_base_item_populate_from_cursor (PhotosBaseItem *self, TrackerSparqlCursor *cursor)
{
  PhotosBaseItemPrivate *priv;
  gboolean favorite;
  const gchar *author;
  const gchar *ctime;
  const gchar *equipment;
  const gchar *flash;
  const gchar *id;
  const gchar *identifier;
  const gchar *location;
  const gchar *mime_type;
  const gchar *orientation;
  const gchar *rdf_type;
  const gchar *resource_urn;
  const gchar *title;
  const gchar *uri;
  gchar *filename;
  gchar *name_fallback;
  gint64 height;
  gint64 width;

  priv = photos_base_item_get_instance_private (self);

  uri = tracker_sparql_cursor_get_string (cursor, PHOTOS_QUERY_COLUMNS_URI, NULL);
  if (uri == NULL)
    uri = "";
  photos_utils_set_string (&priv->uri, uri);
  g_object_notify (G_OBJECT (self), "uri");

  id = tracker_sparql_cursor_get_string (cursor, PHOTOS_QUERY_COLUMNS_URN, NULL);
  photos_utils_set_string (&priv->id, id);
  g_object_notify (G_OBJECT (self), "id");

  identifier = tracker_sparql_cursor_get_string (cursor, PHOTOS_QUERY_COLUMNS_IDENTIFIER, NULL);
  photos_utils_set_string (&priv->identifier, identifier);

  author = tracker_sparql_cursor_get_string (cursor, PHOTOS_QUERY_COLUMNS_AUTHOR, NULL);
  photos_utils_set_string (&priv->author, author);
  g_object_notify (G_OBJECT (self), "secondary-text");

  location = tracker_sparql_cursor_get_string (cursor, PHOTOS_QUERY_COLUMNS_LOCATION, NULL);
  photos_utils_set_string (&priv->location, location);

  resource_urn = tracker_sparql_cursor_get_string (cursor, PHOTOS_QUERY_COLUMNS_RESOURCE_URN, NULL);
  photos_utils_set_string (&priv->resource_urn, resource_urn);

  favorite = tracker_sparql_cursor_get_boolean (cursor, PHOTOS_QUERY_COLUMNS_RESOURCE_FAVORITE);

  priv->mtime = photos_utils_get_mtime_from_sparql_cursor (cursor);
  g_object_notify (G_OBJECT (self), "mtime");

  mime_type = tracker_sparql_cursor_get_string (cursor, PHOTOS_QUERY_COLUMNS_MIME_TYPE, NULL);
  photos_utils_set_string (&priv->mime_type, mime_type);

  rdf_type = tracker_sparql_cursor_get_string (cursor, PHOTOS_QUERY_COLUMNS_RDF_TYPE, NULL);
  photos_utils_set_string (&priv->rdf_type, rdf_type);

  photos_base_item_update_info_from_type (self);
  priv->favorite = favorite && !priv->collection;

  priv->ctime = -1;
  ctime = tracker_sparql_cursor_get_string (cursor, PHOTOS_QUERY_COLUMNS_DATE_CREATED, NULL);
  if (ctime != NULL)
    {
      g_autoptr (GDateTime) date_created = NULL;

      date_created = g_date_time_new_from_iso8601 (ctime, NULL);
      if (date_created != NULL)
        priv->ctime = g_date_time_to_unix (date_created);
    }

  if (g_strcmp0 (priv->id, PHOTOS_COLLECTION_SCREENSHOT) == 0)
    title = _("Screenshots");
  else
    title = tracker_sparql_cursor_get_string (cursor, PHOTOS_QUERY_COLUMNS_TITLE, NULL);

  if (title == NULL)
    title = "";
  photos_utils_set_string (&priv->name, title);
  g_object_notify (G_OBJECT (self), "primary-text");

  filename = g_strdup (tracker_sparql_cursor_get_string (cursor, PHOTOS_QUERY_COLUMNS_FILENAME, NULL));
  if ((filename == NULL || filename[0] == '\0') && !priv->collection)
    filename = PHOTOS_BASE_ITEM_GET_CLASS (self)->create_filename_fallback (self);
  photos_utils_take_string (&priv->filename, filename);

  equipment = tracker_sparql_cursor_get_string (cursor, PHOTOS_QUERY_COLUMNS_EQUIPMENT, NULL);
  priv->equipment = g_quark_from_string (equipment);

  orientation = tracker_sparql_cursor_get_string (cursor, PHOTOS_QUERY_COLUMNS_ORIENTATION, NULL);
  priv->orientation = g_quark_from_string (orientation);
  if (priv->orientation != PHOTOS_ORIENTATION_BOTTOM
      && priv->orientation != PHOTOS_ORIENTATION_BOTTOM_MIRROR
      && priv->orientation != PHOTOS_ORIENTATION_LEFT
      && priv->orientation != PHOTOS_ORIENTATION_LEFT_MIRROR
      && priv->orientation != PHOTOS_ORIENTATION_RIGHT
      && priv->orientation != PHOTOS_ORIENTATION_RIGHT_MIRROR
      && priv->orientation != PHOTOS_ORIENTATION_TOP
      && priv->orientation != PHOTOS_ORIENTATION_TOP_MIRROR)
    {
      if (orientation != NULL)
        g_warning ("Unknown value for nfo:orientation: %s", orientation);
      priv->orientation = PHOTOS_ORIENTATION_TOP;
    }

  height = photos_utils_get_integer_from_sparql_cursor_with_default (cursor,
                                                                     PHOTOS_QUERY_COLUMNS_HEIGHT,
                                                                     0);
  width = photos_utils_get_integer_from_sparql_cursor_with_default (cursor,
                                                                    PHOTOS_QUERY_COLUMNS_WIDTH,
                                                                    0);
  if (priv->orientation == PHOTOS_ORIENTATION_BOTTOM
      || priv->orientation == PHOTOS_ORIENTATION_BOTTOM_MIRROR
      || priv->orientation == PHOTOS_ORIENTATION_TOP
      || priv->orientation == PHOTOS_ORIENTATION_TOP_MIRROR)
    {
      priv->height = height;
      priv->width = width;
    }
  else
    {
      priv->height = width;
      priv->width = height;
    }

  priv->exposure_time
    = photos_utils_get_double_from_sparql_cursor_with_default (cursor,
                                                               PHOTOS_QUERY_COLUMNS_EXPOSURE_TIME,
                                                               0.0);

  priv->fnumber = photos_utils_get_double_from_sparql_cursor_with_default (cursor,
                                                                           PHOTOS_QUERY_COLUMNS_FNUMBER,
                                                                           0.0);

  priv->focal_length
    = photos_utils_get_double_from_sparql_cursor_with_default (cursor,
                                                               PHOTOS_QUERY_COLUMNS_FOCAL_LENGTH,
                                                               0.0);

  priv->iso_speed = photos_utils_get_double_from_sparql_cursor_with_default (cursor,
                                                                             PHOTOS_QUERY_COLUMNS_ISO_SPEED,
                                                                             0.0);

  flash = tracker_sparql_cursor_get_string (cursor, PHOTOS_QUERY_COLUMNS_FLASH, NULL);
  priv->flash = g_quark_from_string (flash);

  name_fallback = PHOTOS_BASE_ITEM_GET_CLASS (self)->create_name_fallback (self);
  photos_utils_take_string (&priv->name_fallback, name_fallback);

  PHOTOS_BASE_ITEM_GET_CLASS (self)->refresh_icon (self);
}


static void
photos_base_item_print_load (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosBaseItem *self = PHOTOS_BASE_ITEM (source_object);
  g_autoptr (GtkWindow) toplevel = GTK_WINDOW (user_data);
  g_autoptr (GeglNode) node = NULL;
  g_autoptr (GtkPrintOperation) print_op = NULL;
  GtkPrintOperationResult print_res;

  {
    g_autoptr (GError) error = NULL;

    node = photos_base_item_load_finish (self, res, &error);
    if (error != NULL)
      {
        g_warning ("Unable to load the item: %s", error->message);
        goto out;
      }
  }

  print_op = photos_print_operation_new (self, node);

  /* It is self managing. */
  photos_print_notification_new (print_op);

  {
    g_autoptr (GError) error = NULL;

    print_res = gtk_print_operation_run (print_op, GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG, toplevel, &error);
    if (print_res == GTK_PRINT_OPERATION_RESULT_APPLY)
      {
        GAction *action;
        GApplication *app;
        GVariant *new_state;

        app = g_application_get_default ();
        action = g_action_map_lookup_action (G_ACTION_MAP (app), "selection-mode");
        new_state = g_variant_new ("b", FALSE);
        g_action_change_state (action, new_state);
      }
    else if (print_res == GTK_PRINT_OPERATION_RESULT_ERROR)
      {
        g_warning ("Unable to print file: %s", error->message);
      }
  }

 out:
  return;
}


static void
photos_base_item_constructed (GObject *object)
{
  PhotosBaseItem *self = PHOTOS_BASE_ITEM (object);
  PhotosBaseItemPrivate *priv;

  priv = photos_base_item_get_instance_private (self);

  G_OBJECT_CLASS (photos_base_item_parent_class)->constructed (object);

  photos_base_item_populate_from_cursor (self, priv->cursor);
  g_clear_object (&priv->cursor); /* We will not need it any more */
}


static void
photos_base_item_dispose (GObject *object)
{
  PhotosBaseItem *self = PHOTOS_BASE_ITEM (object);
  PhotosBaseItemPrivate *priv;

  priv = photos_base_item_get_instance_private (self);

  if (priv->cancellable != NULL)
    {
       g_cancellable_cancel (priv->cancellable);
       g_clear_object (&priv->cancellable);
    }

  photos_base_item_clear_pixels (self);

  g_clear_pointer (&priv->surface, cairo_surface_destroy);
  g_clear_object (&priv->default_app);
  g_clear_object (&priv->original_icon);
  g_clear_object (&priv->watcher);
  g_clear_object (&priv->cursor);

  G_OBJECT_CLASS (photos_base_item_parent_class)->dispose (object);
}


static void
photos_base_item_finalize (GObject *object)
{
  PhotosBaseItem *self = PHOTOS_BASE_ITEM (object);
  PhotosBaseItemPrivate *priv;

  priv = photos_base_item_get_instance_private (self);

  g_free (priv->author);
  g_free (priv->default_app_name);
  g_free (priv->filename);
  g_free (priv->id);
  g_free (priv->identifier);
  g_free (priv->location);
  g_free (priv->mime_type);
  g_free (priv->name);
  g_free (priv->name_fallback);
  g_free (priv->rdf_type);
  g_free (priv->resource_urn);
  g_free (priv->thumb_path);
  g_free (priv->type_description);
  g_free (priv->uri);

  g_mutex_clear (&priv->mutex_download);
  g_mutex_clear (&priv->mutex_save_metadata);

  G_OBJECT_CLASS (photos_base_item_parent_class)->finalize (object);
}


static void
photos_base_item_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
  PhotosBaseItem *self = PHOTOS_BASE_ITEM (object);
  PhotosBaseItemPrivate *priv;

  priv = photos_base_item_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_ICON:
      g_value_set_boxed (value, priv->surface);
      break;

    case PROP_ID:
      g_value_set_string (value, priv->id);
      break;

    case PROP_MTIME:
      g_value_set_int64 (value, priv->mtime);
      break;

    case PROP_PRIMARY_TEXT:
      {
        const gchar *primary_text;

        primary_text = photos_base_item_main_box_item_get_primary_text (GD_MAIN_BOX_ITEM (self));
        g_value_set_string (value, primary_text);
        break;
      }

    case PROP_PULSE:
      {
        gboolean pulse = priv->busy_count > 0;

        g_value_set_boolean (value, pulse);
        break;
      }

    case PROP_SECONDARY_TEXT:
      g_value_set_string (value, priv->author);
      break;

    case PROP_URI:
      g_value_set_string (value, priv->uri);
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
  PhotosBaseItemPrivate *priv;

  priv = photos_base_item_get_instance_private (self);

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

  priv = photos_base_item_get_instance_private (self);

  priv->cancellable = g_cancellable_new ();

  g_mutex_init (&priv->mutex_download);
  g_mutex_init (&priv->mutex_save_metadata);
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
  class->create_thumbnail_path = photos_base_item_default_create_thumbnail_path;
  class->metadata_add_shared = photos_base_item_default_metadata_add_shared;
  class->open = photos_base_item_default_open;
  class->refresh_icon = photos_base_item_default_refresh_icon;
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

  signals[INFO_UPDATED] = g_signal_new ("info-updated",
                                        G_TYPE_FROM_CLASS (class),
                                        G_SIGNAL_RUN_LAST,
                                        G_STRUCT_OFFSET (PhotosBaseItemClass, info_updated),
                                        NULL, /* accumulator */
                                        NULL, /* accu_data */
                                        g_cclosure_marshal_VOID__VOID,
                                        G_TYPE_NONE,
                                        0);

  g_object_class_override_property (object_class, PROP_ICON, "icon");
  g_object_class_override_property (object_class, PROP_ID, "id");
  g_object_class_override_property (object_class, PROP_MTIME, "mtime");
  g_object_class_override_property (object_class, PROP_PRIMARY_TEXT, "primary-text");
  g_object_class_override_property (object_class, PROP_PULSE, "pulse");
  g_object_class_override_property (object_class, PROP_SECONDARY_TEXT, "secondary-text");
  g_object_class_override_property (object_class, PROP_URI, "uri");

  pipeline_cache = dzl_task_cache_new (g_direct_hash,
                                       g_direct_equal,
                                       NULL,
                                       NULL,
                                       g_object_ref,
                                       g_object_unref,
                                       0,
                                       photos_base_item_load_pipeline_task_cache_populate,
                                       NULL,
                                       NULL);
  dzl_task_cache_set_name (pipeline_cache, "PhotosPipeline cache");

  create_thumbnail_pool = g_thread_pool_new (photos_base_item_create_thumbnail_in_thread_func,
                                             NULL,
                                             1,
                                             FALSE,
                                             NULL);
  g_thread_pool_set_sort_function (create_thumbnail_pool, photos_base_item_create_thumbnail_sort_func, NULL);
}


static void
photos_base_item_filterable_iface_init (PhotosFilterableInterface *iface)
{
  iface->get_id = photos_base_item_filterable_get_id;
  iface->is_search_criterion = photos_base_item_filterable_is_search_criterion;
}


static void
photos_base_item_main_box_item_iface_init (GdMainBoxItemInterface *iface)
{
  iface->get_icon = photos_base_item_main_box_item_get_icon;
  iface->get_id = photos_base_item_main_box_item_get_id;
  iface->get_primary_text = photos_base_item_main_box_item_get_primary_text;
  iface->get_secondary_text = photos_base_item_main_box_item_get_secondary_text;
  iface->get_uri = photos_base_item_main_box_item_get_uri;
}


gboolean
photos_base_item_can_edit (PhotosBaseItem *self)
{
  PhotosBaseItemPrivate *priv;

  g_return_val_if_fail (PHOTOS_IS_BASE_ITEM (self), FALSE);
  priv = photos_base_item_get_instance_private (self);

  g_return_val_if_fail (!priv->collection, FALSE);

  return PHOTOS_BASE_ITEM_GET_CLASS (self)->create_pipeline_paths != NULL;
}


gboolean
photos_base_item_can_trash (PhotosBaseItem *self)
{
  g_return_val_if_fail (PHOTOS_IS_BASE_ITEM (self), FALSE);
  return PHOTOS_BASE_ITEM_GET_CLASS (self)->trash_async != NULL;
}


GStrv
photos_base_item_create_pipeline_paths (PhotosBaseItem *self)
{
  GStrv paths;

  g_return_val_if_fail (PHOTOS_IS_BASE_ITEM (self), NULL);

  paths = PHOTOS_BASE_ITEM_GET_CLASS (self)->create_pipeline_paths (self);
  return paths;
}


cairo_surface_t *
photos_base_item_create_preview (PhotosBaseItem *self,
                                 gint size,
                                 gint scale,
                                 const gchar *operation,
                                 const gchar *first_property_name,
                                 ...)
{
  PhotosBaseItemPrivate *priv;
  const Babl *format;
  GeglBuffer *preview_source_buffer;
  GeglNode *buffer_source;
  g_autoptr (GeglNode) graph = NULL;
  GeglNode *operation_node;
  GeglOperation *op;
  GeglRectangle bbox;
  cairo_surface_t *surface = NULL;
  static const cairo_user_data_key_t key;
  const gchar *name;
  gint stride;
  gint64 end;
  gint64 start;
  guchar *buf = NULL;
  va_list ap;

  g_return_val_if_fail (PHOTOS_IS_BASE_ITEM (self), NULL);
  priv = photos_base_item_get_instance_private (self);

  g_return_val_if_fail (!priv->collection, NULL);
  g_return_val_if_fail (operation != NULL && operation[0] != '\0', NULL);
  g_return_val_if_fail (priv->buffer_source != NULL, NULL);
  g_return_val_if_fail (priv->edit_graph != NULL, NULL);

  op = gegl_node_get_gegl_operation (priv->buffer_source);
  g_return_val_if_fail (op != NULL, NULL);

  name = gegl_operation_get_name (op);
  g_return_val_if_fail (g_strcmp0 (name, "gegl:buffer-source") == 0, NULL);

  preview_source_buffer = photos_base_item_get_preview_source_buffer (self, size, scale);
  g_return_val_if_fail (GEGL_IS_BUFFER (preview_source_buffer), NULL);

  graph = gegl_node_new ();
  buffer_source = gegl_node_new_child (graph,
                                       "operation", "gegl:buffer-source",
                                       "buffer", preview_source_buffer,
                                       NULL);

  operation_node = gegl_node_new_child (graph, "operation", operation, NULL);

  va_start (ap, first_property_name);
  gegl_node_set_valist (operation_node, first_property_name, ap);
  va_end (ap);

  gegl_node_link_many (buffer_source, operation_node, NULL);

  start = g_get_monotonic_time ();

  gegl_node_process (operation_node);

  end = g_get_monotonic_time ();
  photos_debug (PHOTOS_DEBUG_GEGL, "Create Preview: Process: %" G_GINT64_FORMAT, end - start);

  bbox = gegl_node_get_bounding_box (operation_node);
  stride = cairo_format_stride_for_width (CAIRO_FORMAT_ARGB32, bbox.width);
  buf = g_malloc0 (stride * bbox.height);
  format = babl_format ("cairo-ARGB32");

  start = g_get_monotonic_time ();

  gegl_node_blit (operation_node, 1.0, &bbox, format, buf, stride, GEGL_BLIT_DEFAULT);

  end = g_get_monotonic_time ();
  photos_debug (PHOTOS_DEBUG_GEGL, "Create Preview: Node Blit: %" G_GINT64_FORMAT, end - start);

  surface = cairo_image_surface_create_for_data (buf, CAIRO_FORMAT_ARGB32, bbox.width, bbox.height, stride);
  cairo_surface_set_device_scale (surface, (gdouble) scale, (gdouble) scale);
  cairo_surface_set_user_data (surface, &key, buf, (cairo_destroy_func_t) g_free);

  return surface;
}


gchar *
photos_base_item_create_thumbnail_path (PhotosBaseItem *self)
{
  gchar *path;

  g_return_val_if_fail (PHOTOS_IS_BASE_ITEM (self), NULL);

  path = PHOTOS_BASE_ITEM_GET_CLASS (self)->create_thumbnail_path (self);
  return path;
}


void
photos_base_item_destroy (PhotosBaseItem *self)
{
  PhotosBaseItemPrivate *priv;

  g_return_if_fail (PHOTOS_IS_BASE_ITEM (self));
  priv = photos_base_item_get_instance_private (self);

  /* TODO: SearchCategoryManager */
  g_clear_object (&priv->watcher);
}


GFile *
photos_base_item_download (PhotosBaseItem *self, GCancellable *cancellable, GError **error)
{
  PhotosBaseItemPrivate *priv;
  g_autoptr (GFile) file = NULL;
  GFile *ret_val = NULL;

  g_return_val_if_fail (PHOTOS_IS_BASE_ITEM (self), NULL);
  priv = photos_base_item_get_instance_private (self);

  g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), NULL);

  g_mutex_lock (&priv->mutex_download);
  file = PHOTOS_BASE_ITEM_GET_CLASS (self)->download (self, cancellable, error);
  g_mutex_unlock (&priv->mutex_download);

  if (file == NULL)
    goto out;

  g_return_val_if_fail (g_file_is_native (file), NULL);
  ret_val = g_object_ref (file);

 out:
  return ret_val;
}


void
photos_base_item_download_async (PhotosBaseItem *self,
                                 GCancellable *cancellable,
                                 GAsyncReadyCallback callback,
                                 gpointer user_data)
{
  g_autoptr (GTask) task = NULL;

  g_return_if_fail (PHOTOS_IS_BASE_ITEM (self));
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, photos_base_item_download_async);

  g_task_run_in_thread (task, photos_base_item_download_in_thread_func);
}


GFile *
photos_base_item_download_finish (PhotosBaseItem *self, GAsyncResult *res, GError **error)
{
  GTask *task;

  g_return_val_if_fail (PHOTOS_IS_BASE_ITEM (self), NULL);

  g_return_val_if_fail (g_task_is_valid (res, self), NULL);
  task = G_TASK (res);

  g_return_val_if_fail (g_task_get_source_tag (task) == photos_base_item_download_async, NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  return g_task_propagate_pointer (task, error);
}


const gchar *
photos_base_item_get_author (PhotosBaseItem *self)
{
  PhotosBaseItemPrivate *priv;

  g_return_val_if_fail (PHOTOS_IS_BASE_ITEM (self), NULL);
  priv = photos_base_item_get_instance_private (self);

  return priv->author;
}


gboolean
photos_base_item_get_bbox_edited (PhotosBaseItem *self, GeglRectangle *out_bbox)
{
  PhotosBaseItemPrivate *priv;
  GeglNode *graph;
  GeglRectangle bbox;
  PhotosPipeline *pipeline;

  g_return_val_if_fail (PHOTOS_IS_BASE_ITEM (self), FALSE);
  priv = photos_base_item_get_instance_private (self);

  g_return_val_if_fail (!priv->collection, FALSE);
  g_return_val_if_fail (priv->edit_graph != NULL, FALSE);

  pipeline = PHOTOS_PIPELINE (dzl_task_cache_peek (pipeline_cache, self));
  g_return_val_if_fail (pipeline != NULL, FALSE);

  g_return_val_if_fail (priv->processor != NULL, FALSE);
  g_return_val_if_fail (!gegl_processor_work (priv->processor, NULL), FALSE);

  graph = photos_pipeline_get_graph (pipeline);
  bbox = gegl_node_get_bounding_box (graph);

  if (out_bbox != NULL)
    *out_bbox = bbox;

  return TRUE;
}


gboolean
photos_base_item_get_bbox_source (PhotosBaseItem *self, GeglRectangle *bbox)
{
  PhotosBaseItemPrivate *priv;
  gboolean ret_val = FALSE;

  g_return_val_if_fail (PHOTOS_IS_BASE_ITEM (self), FALSE);
  priv = photos_base_item_get_instance_private (self);

  g_return_val_if_fail (!priv->collection, FALSE);

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
  PhotosBaseItemPrivate *priv;

  g_return_val_if_fail (PHOTOS_IS_BASE_ITEM (self), 0);
  priv = photos_base_item_get_instance_private (self);

  return priv->ctime;
}


const gchar *
photos_base_item_get_default_app_name (PhotosBaseItem *self)
{
  PhotosBaseItemPrivate *priv;

  g_return_val_if_fail (PHOTOS_IS_BASE_ITEM (self), NULL);
  priv = photos_base_item_get_instance_private (self);

  return priv->default_app_name;
}


GQuark
photos_base_item_get_equipment (PhotosBaseItem *self)
{
  PhotosBaseItemPrivate *priv;

  g_return_val_if_fail (PHOTOS_IS_BASE_ITEM (self), 0);
  priv = photos_base_item_get_instance_private (self);

  return priv->equipment;
}


gdouble
photos_base_item_get_exposure_time (PhotosBaseItem *self)
{
  PhotosBaseItemPrivate *priv;

  g_return_val_if_fail (PHOTOS_IS_BASE_ITEM (self), 0.0);
  priv = photos_base_item_get_instance_private (self);

  return priv->exposure_time;
}


GQuark
photos_base_item_get_flash (PhotosBaseItem *self)
{
  PhotosBaseItemPrivate *priv;

  g_return_val_if_fail (PHOTOS_IS_BASE_ITEM (self), 0);
  priv = photos_base_item_get_instance_private (self);

  return priv->flash;
}


const gchar *
photos_base_item_get_filename (PhotosBaseItem *self)
{
  PhotosBaseItemPrivate *priv;

  g_return_val_if_fail (PHOTOS_IS_BASE_ITEM (self), NULL);
  priv = photos_base_item_get_instance_private (self);

  return priv->filename;
}


gdouble
photos_base_item_get_fnumber (PhotosBaseItem *self)
{
  PhotosBaseItemPrivate *priv;

  g_return_val_if_fail (PHOTOS_IS_BASE_ITEM (self), 0.0);
  priv = photos_base_item_get_instance_private (self);

  return priv->fnumber;
}


gdouble
photos_base_item_get_focal_length (PhotosBaseItem *self)
{
  PhotosBaseItemPrivate *priv;

  g_return_val_if_fail (PHOTOS_IS_BASE_ITEM (self), 0.0);
  priv = photos_base_item_get_instance_private (self);

  return priv->focal_length;
}


gint64
photos_base_item_get_height (PhotosBaseItem *self)
{
  PhotosBaseItemPrivate *priv;

  g_return_val_if_fail (PHOTOS_IS_BASE_ITEM (self), 0);
  priv = photos_base_item_get_instance_private (self);

  return priv->height;
}


const gchar *
photos_base_item_get_identifier (PhotosBaseItem *self)
{
  PhotosBaseItemPrivate *priv;

  g_return_val_if_fail (PHOTOS_IS_BASE_ITEM (self), NULL);
  priv = photos_base_item_get_instance_private (self);

  return priv->identifier;
}


gdouble
photos_base_item_get_iso_speed (PhotosBaseItem *self)
{
  PhotosBaseItemPrivate *priv;

  g_return_val_if_fail (PHOTOS_IS_BASE_ITEM (self), 0.0);
  priv = photos_base_item_get_instance_private (self);

  return priv->iso_speed;
}


const gchar *
photos_base_item_get_location (PhotosBaseItem *self)
{
  PhotosBaseItemPrivate *priv;

  g_return_val_if_fail (PHOTOS_IS_BASE_ITEM (self), NULL);
  priv = photos_base_item_get_instance_private (self);

  return priv->location;
}


const gchar *
photos_base_item_get_mime_type (PhotosBaseItem *self)
{
  PhotosBaseItemPrivate *priv;

  g_return_val_if_fail (PHOTOS_IS_BASE_ITEM (self), NULL);
  priv = photos_base_item_get_instance_private (self);

  return priv->mime_type;
}


gint64
photos_base_item_get_mtime (PhotosBaseItem *self)
{
  PhotosBaseItemPrivate *priv;

  g_return_val_if_fail (PHOTOS_IS_BASE_ITEM (self), 0);
  priv = photos_base_item_get_instance_private (self);

  return priv->mtime;
}


const gchar *
photos_base_item_get_name (PhotosBaseItem *self)
{
  PhotosBaseItemPrivate *priv;

  g_return_val_if_fail (PHOTOS_IS_BASE_ITEM (self), NULL);
  priv = photos_base_item_get_instance_private (self);

  return priv->name;
}


const gchar *
photos_base_item_get_name_with_fallback (PhotosBaseItem *self)
{
  PhotosBaseItemPrivate *priv;
  const gchar *name;

  g_return_val_if_fail (PHOTOS_IS_BASE_ITEM (self), NULL);
  priv = photos_base_item_get_instance_private (self);

  name = priv->name;
  if (name == NULL || name[0] == '\0')
    name = priv->name_fallback;

  return name;
}


GQuark
photos_base_item_get_orientation (PhotosBaseItem *self)
{
  PhotosBaseItemPrivate *priv;

  g_return_val_if_fail (PHOTOS_IS_BASE_ITEM (self), 0);
  priv = photos_base_item_get_instance_private (self);

  return priv->orientation;
}


GdkPixbuf *
photos_base_item_get_original_icon (PhotosBaseItem *self)
{
  PhotosBaseItemPrivate *priv;

  g_return_val_if_fail (PHOTOS_IS_BASE_ITEM (self), NULL);
  priv = photos_base_item_get_instance_private (self);

  return priv->original_icon;
}


const gchar *
photos_base_item_get_resource_urn (PhotosBaseItem *self)
{
  PhotosBaseItemPrivate *priv;

  g_return_val_if_fail (PHOTOS_IS_BASE_ITEM (self), NULL);
  priv = photos_base_item_get_instance_private (self);

  return priv->resource_urn;
}


GtkWidget *
photos_base_item_get_source_widget (PhotosBaseItem *self)
{
  g_return_val_if_fail (PHOTOS_IS_BASE_ITEM (self), NULL);
  return PHOTOS_BASE_ITEM_GET_CLASS (self)->get_source_widget(self);
}


cairo_surface_t *
photos_base_item_get_surface (PhotosBaseItem *self)
{
  PhotosBaseItemPrivate *priv;

  g_return_val_if_fail (PHOTOS_IS_BASE_ITEM (self), NULL);
  priv = photos_base_item_get_instance_private (self);

  return priv->surface;
}


const gchar *
photos_base_item_get_type_description (PhotosBaseItem *self)
{
  PhotosBaseItemPrivate *priv;

  g_return_val_if_fail (PHOTOS_IS_BASE_ITEM (self), NULL);
  priv = photos_base_item_get_instance_private (self);

  return priv->type_description;
}


const gchar *
photos_base_item_get_uri (PhotosBaseItem *self)
{
  PhotosBaseItemPrivate *priv;

  g_return_val_if_fail (PHOTOS_IS_BASE_ITEM (self), NULL);
  priv = photos_base_item_get_instance_private (self);

  return priv->uri;
}


gchar *
photos_base_item_get_where (PhotosBaseItem *self)
{
  PhotosBaseItemPrivate *priv;
  gchar *ret_val;

  g_return_val_if_fail (PHOTOS_IS_BASE_ITEM (self), NULL);
  priv = photos_base_item_get_instance_private (self);

  if (priv->collection)
    ret_val = g_strconcat ("{ ?urn nie:isLogicalPartOf <", priv->id, "> }", NULL);
  else
    ret_val = g_strdup ("");

  return ret_val;
}


gint64
photos_base_item_get_width (PhotosBaseItem *self)
{
  PhotosBaseItemPrivate *priv;

  g_return_val_if_fail (PHOTOS_IS_BASE_ITEM (self), 0);
  priv = photos_base_item_get_instance_private (self);

  return priv->width;
}


void
photos_base_item_guess_save_sizes_async (PhotosBaseItem *self,
                                         GCancellable *cancellable,
                                         GAsyncReadyCallback callback,
                                         gpointer user_data)
{
  PhotosBaseItemPrivate *priv;
  g_autoptr (GTask) task = NULL;

  g_return_if_fail (PHOTOS_IS_BASE_ITEM (self));
  priv = photos_base_item_get_instance_private (self);

  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
  g_return_if_fail (!priv->collection);

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, photos_base_item_guess_save_sizes_async);

  photos_base_item_load_async (self, cancellable, photos_base_item_guess_save_sizes_load, g_object_ref (task));
}


gboolean
photos_base_item_guess_save_sizes_finish (PhotosBaseItem *self,
                                          GAsyncResult *res,
                                          PhotosBaseItemSize *out_full_size,
                                          PhotosBaseItemSize *out_reduced_size,
                                          GError **error)
{
  GTask *task;
  GeglRectangle bbox;
  gboolean ret_val = FALSE;
  gint max_dimension;
  gdouble reduced_zoom = -1.0;
  guint i;
  gsize *sizes;

  g_return_val_if_fail (PHOTOS_IS_BASE_ITEM (self), FALSE);

  g_return_val_if_fail (g_task_is_valid (res, self), FALSE);
  task = G_TASK (res);

  g_return_val_if_fail (g_task_get_source_tag (task) == photos_base_item_guess_save_sizes_async, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  sizes = g_task_propagate_pointer (task, error);
  if (sizes == NULL)
    goto out;

  if (!photos_base_item_get_bbox_edited (self, &bbox))
    {
      g_set_error (error, PHOTOS_ERROR, 0, "Failed to get the bounding box");
      goto out;
    }

  ret_val = TRUE;

  max_dimension = MAX (bbox.height, bbox.width);
  for (i = 0; i < G_N_ELEMENTS (PIXEL_SIZES); i++)
    {
      if (max_dimension > PIXEL_SIZES[i])
        {
          reduced_zoom = (gdouble) PIXEL_SIZES[i] / (gdouble) max_dimension;
          break;
        }
    }

  if (out_full_size != NULL)
    {
      out_full_size->height = bbox.height;
      out_full_size->width = bbox.width;
      out_full_size->bytes = sizes[0];
      out_full_size->zoom = 1.0;
    }

  if (out_reduced_size != NULL)
    {
      out_reduced_size->zoom = reduced_zoom;
      if (reduced_zoom > 0.0)
        {
          out_reduced_size->height = (gint) ((gdouble) bbox.height * reduced_zoom + 0.5);
          out_reduced_size->width = (gint) ((gdouble) bbox.width * reduced_zoom + 0.5);
          out_reduced_size->bytes = (gsize) (sizes[1]
                                             + (sizes[0] - sizes[1]) * (reduced_zoom - 0.5) / (1.0 - 0.5)
                                             + 0.5);
        }
    }

 out:
  return ret_val;
}


gboolean
photos_base_item_is_collection (PhotosBaseItem *self)
{
  PhotosBaseItemPrivate *priv;

  g_return_val_if_fail (PHOTOS_IS_BASE_ITEM (self), FALSE);
  priv = photos_base_item_get_instance_private (self);

  return priv->collection;
}


gboolean
photos_base_item_is_favorite (PhotosBaseItem *self)
{
  PhotosBaseItemPrivate *priv;

  g_return_val_if_fail (PHOTOS_IS_BASE_ITEM (self), FALSE);
  priv = photos_base_item_get_instance_private (self);

  return priv->favorite;
}


void
photos_base_item_load_async (PhotosBaseItem *self,
                             GCancellable *cancellable,
                             GAsyncReadyCallback callback,
                             gpointer user_data)
{
  PhotosBaseItemPrivate *priv;
  g_autoptr (GTask) task = NULL;
  PhotosPipeline *pipeline;

  g_return_if_fail (PHOTOS_IS_BASE_ITEM (self));
  priv = photos_base_item_get_instance_private (self);

  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
  g_return_if_fail (!priv->collection);

  pipeline = PHOTOS_PIPELINE (dzl_task_cache_peek (pipeline_cache, self));
  g_return_if_fail (priv->edit_graph == NULL || (GEGL_IS_NODE (priv->edit_graph) && PHOTOS_IS_PIPELINE (pipeline)));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, photos_base_item_load_async);

  if (priv->edit_graph != NULL)
    {
      GeglNode *graph;

      graph = photos_pipeline_get_graph (pipeline);
      g_task_return_pointer (task, g_object_ref (graph), g_object_unref);
      goto out;
    }

  photos_base_item_load_pipeline_async (self,
                                        cancellable,
                                        photos_base_item_load_load_pipeline,
                                        g_object_ref (task));

 out:
  return;
}


GeglNode *
photos_base_item_load_finish (PhotosBaseItem *self, GAsyncResult *res, GError **error)
{
  GTask *task;

  g_return_val_if_fail (PHOTOS_IS_BASE_ITEM (self), NULL);

  g_return_val_if_fail (g_task_is_valid (res, self), NULL);
  task = G_TASK (res);

  g_return_val_if_fail (g_task_get_source_tag (task) == photos_base_item_load_async, NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  return g_task_propagate_pointer (task, error);
}


void
photos_base_item_mark_busy (PhotosBaseItem *self)
{
  PhotosBaseItemPrivate *priv;

  g_return_if_fail (PHOTOS_IS_BASE_ITEM (self));
  priv = photos_base_item_get_instance_private (self);

  priv->busy_count++;
  if (priv->busy_count == 1)
    g_object_notify (G_OBJECT (self), "pulse");
}


void
photos_base_item_metadata_add_shared_async (PhotosBaseItem *self,
                                            const gchar *provider_type,
                                            const gchar *account_identity,
                                            const gchar *shared_id,
                                            GCancellable *cancellable,
                                            GAsyncReadyCallback callback,
                                            gpointer user_data)
{
  PhotosBaseItemPrivate *priv;
  g_autoptr (GTask) task = NULL;
  PhotosBaseItemMetadataAddSharedData *data;

  g_return_if_fail (PHOTOS_IS_BASE_ITEM (self));
  priv = photos_base_item_get_instance_private (self);

  g_return_if_fail (!priv->collection);
  g_return_if_fail (provider_type != NULL && provider_type[0] != '\0');
  g_return_if_fail (account_identity != NULL && account_identity[0] != '\0');
  g_return_if_fail (shared_id != NULL && shared_id[0] != '\0');
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  data = photos_base_item_metadata_add_shared_data_new (provider_type, account_identity, shared_id);

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, photos_base_item_metadata_add_shared_async);
  g_task_set_task_data (task, data, (GDestroyNotify) photos_base_item_metadata_add_shared_data_free);

  g_task_run_in_thread (task, photos_base_item_metadata_add_shared_in_thread_func);
}


gboolean
photos_base_item_metadata_add_shared_finish (PhotosBaseItem *self, GAsyncResult *res, GError **error)
{
  GTask *task;

  g_return_val_if_fail (PHOTOS_IS_BASE_ITEM (self), FALSE);

  g_return_val_if_fail (g_task_is_valid (res, self), FALSE);
  task = G_TASK (res);

  g_return_val_if_fail (g_task_get_source_tag (task) == photos_base_item_metadata_add_shared_async, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  return g_task_propagate_boolean (task, error);
}


void
photos_base_item_open (PhotosBaseItem *self, GtkWindow *parent, guint32 timestamp)
{
  g_return_if_fail (PHOTOS_IS_BASE_ITEM (self));
  PHOTOS_BASE_ITEM_GET_CLASS (self)->open (self, parent, timestamp);
}


void
photos_base_item_operation_add_async (PhotosBaseItem *self,
                                      GCancellable *cancellable,
                                      GAsyncReadyCallback callback,
                                      gpointer user_data,
                                      const gchar *operation,
                                      const gchar *first_property_name,
                                      ...)
{
  PhotosBaseItemPrivate *priv;
  g_autoptr (GTask) task = NULL;
  PhotosPipeline *pipeline;
  va_list ap;

  g_return_if_fail (PHOTOS_IS_BASE_ITEM (self));
  priv = photos_base_item_get_instance_private (self);

  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
  g_return_if_fail (!priv->collection);

  pipeline = PHOTOS_PIPELINE (dzl_task_cache_peek (pipeline_cache, self));
  g_return_if_fail (PHOTOS_IS_PIPELINE (pipeline));

  va_start (ap, first_property_name);
  photos_pipeline_add_valist (pipeline, operation, first_property_name, ap);
  va_end (ap);

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, photos_base_item_operation_add_async);

  photos_base_item_process_async (self, cancellable, photos_base_item_common_process, g_object_ref (task));
}


gboolean
photos_base_item_operation_add_finish (PhotosBaseItem *self, GAsyncResult *res, GError **error)
{
  GTask *task;

  g_return_val_if_fail (PHOTOS_IS_BASE_ITEM (self), FALSE);

  g_return_val_if_fail (g_task_is_valid (res, self), FALSE);
  task = G_TASK (res);

  g_return_val_if_fail (g_task_get_source_tag (task) == photos_base_item_operation_add_async, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  return g_task_propagate_boolean (task, error);
}


gboolean
photos_base_item_operation_get (PhotosBaseItem *self, const gchar *operation, const gchar *first_property_name, ...)
{
  PhotosBaseItemPrivate *priv;
  PhotosPipeline *pipeline;
  gboolean ret_val;
  va_list ap;

  g_return_val_if_fail (PHOTOS_IS_BASE_ITEM (self), FALSE);
  priv = photos_base_item_get_instance_private (self);

  g_return_val_if_fail (!priv->collection, FALSE);

  pipeline = PHOTOS_PIPELINE (dzl_task_cache_peek (pipeline_cache, self));
  g_return_val_if_fail (PHOTOS_IS_PIPELINE (pipeline), FALSE);

  va_start (ap, first_property_name);
  ret_val = photos_pipeline_get_valist (pipeline, operation, first_property_name, ap);
  va_end (ap);

  return ret_val;
}


void
photos_base_item_operation_remove_async (PhotosBaseItem *self,
                                         const gchar *operation,
                                         GCancellable *cancellable,
                                         GAsyncReadyCallback callback,
                                         gpointer user_data)
{
  PhotosBaseItemPrivate *priv;
  g_autoptr (GTask) task = NULL;
  PhotosPipeline *pipeline;

  g_return_if_fail (PHOTOS_IS_BASE_ITEM (self));
  priv = photos_base_item_get_instance_private (self);

  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
  g_return_if_fail (!priv->collection);

  pipeline = PHOTOS_PIPELINE (dzl_task_cache_peek (pipeline_cache, self));
  g_return_if_fail (PHOTOS_IS_PIPELINE (pipeline));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, photos_base_item_operation_remove_async);

  if (!photos_pipeline_remove (pipeline, operation))
    {
      g_task_return_new_error (task, PHOTOS_ERROR, 0, "Failed to find a GeglNode for %s", operation);
      goto out;
    }

  photos_base_item_process_async (self, cancellable, photos_base_item_common_process, g_object_ref (task));

 out:
  return;
}


gboolean
photos_base_item_operation_remove_finish (PhotosBaseItem *self, GAsyncResult *res, GError **error)
{
  GTask *task;

  g_return_val_if_fail (PHOTOS_IS_BASE_ITEM (self), FALSE);

  g_return_val_if_fail (g_task_is_valid (res, self), FALSE);
  task = G_TASK (res);

  g_return_val_if_fail (g_task_get_source_tag (task) == photos_base_item_operation_remove_async, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  return g_task_propagate_boolean (task, error);
}


void
photos_base_item_pipeline_is_edited_async (PhotosBaseItem *self,
                                           GCancellable *cancellable,
                                           GAsyncReadyCallback callback,
                                           gpointer user_data)
{
  PhotosBaseItemPrivate *priv;
  g_autoptr (GTask) task = NULL;

  g_return_if_fail (PHOTOS_IS_BASE_ITEM (self));
  priv = photos_base_item_get_instance_private (self);

  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
  g_return_if_fail (!priv->collection);

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, photos_base_item_pipeline_is_edited_async);

  photos_base_item_load_pipeline_async (self,
                                        cancellable,
                                        photos_base_item_pipeline_is_edited_load_pipeline,
                                        g_object_ref (task));
}


gboolean
photos_base_item_pipeline_is_edited_finish (PhotosBaseItem *self, GAsyncResult *res, GError **error)
{
  GTask *task;

  g_return_val_if_fail (PHOTOS_IS_BASE_ITEM (self), FALSE);

  g_return_val_if_fail (g_task_is_valid (res, self), FALSE);
  task = G_TASK (res);

  g_return_val_if_fail (g_task_get_source_tag (task) == photos_base_item_pipeline_is_edited_async, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  return g_task_propagate_boolean (task, error);
}


void
photos_base_item_pipeline_revert_async (PhotosBaseItem *self,
                                        GCancellable *cancellable,
                                        GAsyncReadyCallback callback,
                                        gpointer user_data)
{
  PhotosBaseItemPrivate *priv;
  g_autoptr (GTask) task = NULL;
  PhotosPipeline *pipeline;

  g_return_if_fail (PHOTOS_IS_BASE_ITEM (self));
  priv = photos_base_item_get_instance_private (self);

  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
  g_return_if_fail (!priv->collection);

  pipeline = PHOTOS_PIPELINE (dzl_task_cache_peek (pipeline_cache, self));
  g_return_if_fail (PHOTOS_IS_PIPELINE (pipeline));

  photos_pipeline_revert (pipeline);

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, photos_base_item_pipeline_revert_async);

  if (priv->edit_graph == NULL)
    {
      g_task_return_boolean (task, TRUE);
      goto out;
    }

  photos_base_item_process_async (self, cancellable, photos_base_item_common_process, g_object_ref (task));

 out:
  return;
}


gboolean
photos_base_item_pipeline_revert_finish (PhotosBaseItem *self, GAsyncResult *res, GError **error)
{
  GTask *task;

  g_return_val_if_fail (PHOTOS_IS_BASE_ITEM (self), FALSE);

  g_return_val_if_fail (g_task_is_valid (res, self), FALSE);
  task = G_TASK (res);

  g_return_val_if_fail (g_task_get_source_tag (task) == photos_base_item_pipeline_revert_async, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  return g_task_propagate_boolean (task, error);
}


void
photos_base_item_pipeline_revert_to_original_async (PhotosBaseItem *self,
                                                    GCancellable *cancellable,
                                                    GAsyncReadyCallback callback,
                                                    gpointer user_data)

{
  PhotosBaseItemPrivate *priv;
  g_autoptr (GTask) task = NULL;
  PhotosPipeline *pipeline;

  g_return_if_fail (PHOTOS_IS_BASE_ITEM (self));
  priv = photos_base_item_get_instance_private (self);

  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
  g_return_if_fail (!priv->collection);

  pipeline = PHOTOS_PIPELINE (dzl_task_cache_peek (pipeline_cache, self));
  g_return_if_fail (PHOTOS_IS_PIPELINE (pipeline));

  photos_pipeline_revert_to_original (pipeline);

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, photos_base_item_pipeline_revert_to_original_async);

  if (priv->edit_graph == NULL)
    {
      g_task_return_boolean (task, TRUE);
      goto out;
    }

  photos_base_item_process_async (self, cancellable, photos_base_item_common_process, g_object_ref (task));

 out:
  return;
}


gboolean
photos_base_item_pipeline_revert_to_original_finish (PhotosBaseItem *self, GAsyncResult *res, GError **error)
{
  GTask *task;

  g_return_val_if_fail (PHOTOS_IS_BASE_ITEM (self), FALSE);

  g_return_val_if_fail (g_task_is_valid (res, self), FALSE);
  task = G_TASK (res);

  g_return_val_if_fail (g_task_get_source_tag (task) == photos_base_item_pipeline_revert_to_original_async, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  return g_task_propagate_boolean (task, error);
}


void
photos_base_item_pipeline_save_async (PhotosBaseItem *self,
                                      GCancellable *cancellable,
                                      GAsyncReadyCallback callback,
                                      gpointer user_data)
{
  PhotosBaseItemPrivate *priv;
  g_autoptr (GTask) task = NULL;
  PhotosPipeline *pipeline;

  g_return_if_fail (PHOTOS_IS_BASE_ITEM (self));
  priv = photos_base_item_get_instance_private (self);

  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
  g_return_if_fail (!priv->collection);

  pipeline = PHOTOS_PIPELINE (dzl_task_cache_peek (pipeline_cache, self));
  g_return_if_fail (pipeline != NULL);

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, photos_base_item_pipeline_save_async);

  photos_pipeline_save_async (pipeline, cancellable, photos_base_item_pipeline_save_save, g_object_ref (task));
}


gboolean
photos_base_item_pipeline_save_finish (PhotosBaseItem *self, GAsyncResult *res, GError **error)
{
  GTask *task;

  g_return_val_if_fail (PHOTOS_IS_BASE_ITEM (self), FALSE);

  g_return_val_if_fail (g_task_is_valid (res, self), FALSE);
  task = G_TASK (res);

  g_return_val_if_fail (g_task_get_source_tag (task) == photos_base_item_pipeline_save_async, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  return g_task_propagate_boolean (task, error);
}


void
photos_base_item_pipeline_snapshot (PhotosBaseItem *self)
{
  PhotosBaseItemPrivate *priv;
  PhotosPipeline *pipeline;

  g_return_if_fail (PHOTOS_IS_BASE_ITEM (self));
  priv = photos_base_item_get_instance_private (self);

  g_return_if_fail (!priv->collection);

  pipeline = PHOTOS_PIPELINE (dzl_task_cache_peek (pipeline_cache, self));
  g_return_if_fail (PHOTOS_IS_PIPELINE (pipeline));

  photos_pipeline_snapshot (pipeline);
}


void
photos_base_item_print (PhotosBaseItem *self, GtkWidget *toplevel)
{
  PhotosBaseItemPrivate *priv;

  g_return_if_fail (PHOTOS_IS_BASE_ITEM (self));
  priv = photos_base_item_get_instance_private (self);

  g_return_if_fail (!priv->collection);

  photos_base_item_load_async (self, NULL, photos_base_item_print_load, g_object_ref (toplevel));
}


GFileInfo *
photos_base_item_query_info (PhotosBaseItem *self,
                             const gchar *attributes,
                             GFileQueryInfoFlags flags,
                             GCancellable *cancellable,
                             GError **error)
{
  PhotosBaseItemPrivate *priv;
  GFileAttributeMatcher *matcher = NULL; /* TODO: use g_autoptr */
  g_autoptr (GFile) file = NULL;
  g_autoptr (GFileInfo) info = NULL;
  GFileInfo *ret_val = NULL;

  g_return_val_if_fail (PHOTOS_IS_BASE_ITEM (self), NULL);
  priv = photos_base_item_get_instance_private (self);

  g_return_val_if_fail (attributes != NULL && attributes[0] != '\0', NULL);
  g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  file = g_file_new_for_uri (priv->uri);
  info = g_file_query_info (file, attributes, flags, cancellable, error);
  if (info == NULL)
    goto out;

  matcher = g_file_attribute_matcher_new (attributes);
  if (g_file_attribute_matcher_matches (matcher, G_FILE_ATTRIBUTE_THUMBNAIL_IS_VALID)
      || g_file_attribute_matcher_matches (matcher, G_FILE_ATTRIBUTE_THUMBNAIL_PATH)
      || g_file_attribute_matcher_matches (matcher, G_FILE_ATTRIBUTE_THUMBNAILING_FAILED))
    {
      g_autofree gchar *path = NULL;

      g_file_info_remove_attribute (info, G_FILE_ATTRIBUTE_THUMBNAIL_IS_VALID);
      g_file_info_remove_attribute (info, G_FILE_ATTRIBUTE_THUMBNAIL_PATH);
      g_file_info_remove_attribute (info, G_FILE_ATTRIBUTE_THUMBNAILING_FAILED);

      path = photos_base_item_create_thumbnail_path (self);
      if (g_file_test (path, G_FILE_TEST_IS_REGULAR))
        {
          g_file_info_set_attribute_boolean (info, G_FILE_ATTRIBUTE_THUMBNAIL_IS_VALID, TRUE);
          g_file_info_set_attribute_byte_string (info, G_FILE_ATTRIBUTE_THUMBNAIL_PATH, path);
          g_file_info_set_attribute_boolean (info, G_FILE_ATTRIBUTE_THUMBNAILING_FAILED, FALSE);
        }
    }

  ret_val = g_object_ref (info);

 out:
  g_clear_pointer (&matcher, g_file_attribute_matcher_unref);
  return ret_val;
}


static void
photos_base_item_query_info_in_thread_func (GTask *task,
                                            gpointer source_object,
                                            gpointer task_data,
                                            GCancellable *cancellable)
{
  PhotosBaseItem *self = PHOTOS_BASE_ITEM (source_object);
  g_autoptr (GFileInfo) info = NULL;
  PhotosBaseItemQueryInfoData *data = (PhotosBaseItemQueryInfoData *) task_data;

  {
    g_autoptr (GError) error = NULL;

    info = photos_base_item_query_info (self, data->attributes, data->flags, cancellable, &error);
    if (error != NULL)
      {
        g_task_return_error (task, g_steal_pointer (&error));
        goto out;
      }
  }

  g_task_return_pointer (task, g_object_ref (info), g_object_unref);

 out:
  return;
}


void
photos_base_item_query_info_async (PhotosBaseItem *self,
                                   const gchar *attributes,
                                   GFileQueryInfoFlags flags,
                                   gint io_priority,
                                   GCancellable *cancellable,
                                   GAsyncReadyCallback callback,
                                   gpointer user_data)
{
  g_autoptr (GTask) task = NULL;
  PhotosBaseItemQueryInfoData *data;
  const gchar *wildcard;

  g_return_if_fail (PHOTOS_IS_BASE_ITEM (self));
  g_return_if_fail (attributes != NULL && attributes[0] != '\0');
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  wildcard = strstr (attributes, "*");
  g_return_if_fail (wildcard == NULL);

  data = photos_base_item_query_info_data_new (attributes, flags);

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_priority (task, io_priority);
  g_task_set_source_tag (task, photos_base_item_query_info_async);
  g_task_set_task_data (task, data, (GDestroyNotify) photos_base_item_query_info_data_free);

  g_task_run_in_thread (task, photos_base_item_query_info_in_thread_func);
}


GFileInfo *
photos_base_item_query_info_finish (PhotosBaseItem *self, GAsyncResult *res, GError **error)
{
  GTask *task;

  g_return_val_if_fail (g_task_is_valid (res, self), NULL);
  task = G_TASK (res);

  g_return_val_if_fail (g_task_get_source_tag (task) == photos_base_item_query_info_async, NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  return g_task_propagate_pointer (task, error);
}


void
photos_base_item_refresh (PhotosBaseItem *self)
{
  PhotosBaseItemPrivate *priv;
  GApplication *app;
  PhotosSearchContextState *state;
  g_autoptr (PhotosSingleItemJob) job = NULL;

  g_return_if_fail (PHOTOS_IS_BASE_ITEM (self));
  priv = photos_base_item_get_instance_private (self);

  app = g_application_get_default ();
  state = photos_search_context_get_state (PHOTOS_SEARCH_CONTEXT (app));

  job = photos_single_item_job_new (priv->id);
  photos_single_item_job_run (job,
                              state,
                              PHOTOS_QUERY_FLAGS_UNFILTERED,
                              NULL,
                              photos_base_item_refresh_executed,
                              g_object_ref (self));
}


void
photos_base_item_save_to_dir_async (PhotosBaseItem *self,
                                    GFile *dir,
                                    gdouble zoom,
                                    GCancellable *cancellable,
                                    GAsyncReadyCallback callback,
                                    gpointer user_data)
{
  PhotosBaseItemPrivate *priv;
  g_autoptr (GTask) task = NULL;
  PhotosBaseItemSaveData *data;

  g_return_if_fail (PHOTOS_IS_BASE_ITEM (self));
  priv = photos_base_item_get_instance_private (self);

  g_return_if_fail (!priv->collection);
  g_return_if_fail (G_IS_FILE (dir));
  g_return_if_fail (priv->filename != NULL && priv->filename[0] != '\0');
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  data = photos_base_item_save_data_new (dir, NULL, NULL, zoom);

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, photos_base_item_save_to_dir_async);
  g_task_set_task_data (task, data, (GDestroyNotify) photos_base_item_save_data_free);

  photos_base_item_load_async (self, cancellable, photos_base_item_save_to_dir_load, g_object_ref (task));
}


GFile *
photos_base_item_save_to_dir_finish (PhotosBaseItem *self, GAsyncResult *res, GError **error)
{
  GTask *task;

  g_return_val_if_fail (PHOTOS_IS_BASE_ITEM (self), FALSE);

  g_return_val_if_fail (g_task_is_valid (res, self), FALSE);
  task = G_TASK (res);

  g_return_val_if_fail (g_task_get_source_tag (task) == photos_base_item_save_to_dir_async, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  return g_task_propagate_pointer (task, error);
}


void
photos_base_item_save_to_file_async (PhotosBaseItem *self,
                                     GFile *file,
                                     GFileCreateFlags flags,
                                     gdouble zoom,
                                     GCancellable *cancellable,
                                     GAsyncReadyCallback callback,
                                     gpointer user_data)
{
  PhotosBaseItemPrivate *priv;
  g_autoptr (GTask) task = NULL;
  PhotosBaseItemSaveToFileData *data;

  g_return_if_fail (PHOTOS_IS_BASE_ITEM (self));
  priv = photos_base_item_get_instance_private (self);

  g_return_if_fail (!priv->collection);
  g_return_if_fail (G_IS_FILE (file));
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  data = photos_base_item_save_to_file_data_new (file, flags, zoom);

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, photos_base_item_save_to_file_async);
  g_task_set_task_data (task, data, (GDestroyNotify) photos_base_item_save_to_file_data_free);

  photos_base_item_load_async (self, cancellable, photos_base_item_save_to_file_load, g_object_ref (task));
}


gboolean
photos_base_item_save_to_file_finish (PhotosBaseItem *self, GAsyncResult *res, GError **error)
{
  GTask *task;

  g_return_val_if_fail (PHOTOS_IS_BASE_ITEM (self), FALSE);

  g_return_val_if_fail (g_task_is_valid (res, self), FALSE);
  task = G_TASK (res);

  g_return_val_if_fail (g_task_get_source_tag (task) == photos_base_item_save_to_file_async, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  return g_task_propagate_boolean (task, error);
}


void
photos_base_item_save_to_stream_async (PhotosBaseItem *self,
                                       GOutputStream *stream,
                                       gdouble zoom,
                                       GCancellable *cancellable,
                                       GAsyncReadyCallback callback,
                                       gpointer user_data)
{
  PhotosBaseItemPrivate *priv;
  g_autoptr (GTask) task = NULL;
  PhotosBaseItemSaveToStreamData *data;

  g_return_if_fail (PHOTOS_IS_BASE_ITEM (self));
  priv = photos_base_item_get_instance_private (self);

  g_return_if_fail (!priv->collection);
  g_return_if_fail (G_IS_OUTPUT_STREAM (stream));
  g_return_if_fail (priv->filename != NULL && priv->filename[0] != '\0');
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  data = photos_base_item_save_to_stream_data_new (stream, zoom);

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, photos_base_item_save_to_stream_async);
  g_task_set_task_data (task, data, (GDestroyNotify) photos_base_item_save_to_stream_data_free);

  photos_base_item_load_async (self, cancellable, photos_base_item_save_to_stream_load, g_object_ref (task));
}


gboolean
photos_base_item_save_to_stream_finish (PhotosBaseItem *self, GAsyncResult *res, GError **error)
{
  GTask *task;

  g_return_val_if_fail (PHOTOS_IS_BASE_ITEM (self), FALSE);

  g_return_val_if_fail (g_task_is_valid (res, self), FALSE);
  task = G_TASK (res);

  g_return_val_if_fail (g_task_get_source_tag (task) == photos_base_item_save_to_stream_async, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  return g_task_propagate_boolean (task, error);
}


void
photos_base_item_set_default_app (PhotosBaseItem *self, GAppInfo *default_app)
{
  PhotosBaseItemPrivate *priv;
  const gchar *default_app_name;

  g_return_if_fail (PHOTOS_IS_BASE_ITEM (self));
  priv = photos_base_item_get_instance_private (self);

  if (priv->default_app == NULL && default_app == NULL)
    return;

  if (priv->default_app != NULL && default_app != NULL && g_app_info_equal (priv->default_app, default_app))
    return;

  g_set_object (&priv->default_app, default_app);
  g_clear_pointer (&priv->default_app_name, g_free);

  if (default_app == NULL)
    return;

  default_app_name = g_app_info_get_name (default_app);
  priv->default_app_name = g_strdup (default_app_name);
}


void
photos_base_item_set_default_app_name (PhotosBaseItem *self, const gchar *default_app_name)
{
  PhotosBaseItemPrivate *priv;

  g_return_if_fail (PHOTOS_IS_BASE_ITEM (self));
  priv = photos_base_item_get_instance_private (self);

  g_clear_object (&priv->default_app);
  g_free (priv->default_app_name);
  priv->default_app_name = g_strdup (default_app_name);
}


void
photos_base_item_set_favorite (PhotosBaseItem *self, gboolean favorite)
{
  PhotosBaseItemPrivate *priv;

  g_return_if_fail (PHOTOS_IS_BASE_ITEM (self));
  priv = photos_base_item_get_instance_private (self);

  g_return_if_fail (!priv->collection);

  PHOTOS_BASE_ITEM_GET_CLASS (self)->set_favorite (self, favorite);
}


void
photos_base_item_trash_async (PhotosBaseItem *self,
                              GCancellable *cancellable,
                              GAsyncReadyCallback callback,
                              gpointer user_data)
{
  GApplication *app;
  g_autoptr (GTask) task = NULL;

  g_return_if_fail (PHOTOS_IS_BASE_ITEM (self));
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  app = g_application_get_default ();

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, photos_base_item_trash_async);

  g_application_hold (app);
  PHOTOS_BASE_ITEM_GET_CLASS (self)->trash_async (self,
                                                  cancellable,
                                                  photos_base_item_trash_trash,
                                                  g_object_ref (task));
}


gboolean
photos_base_item_trash_finish (PhotosBaseItem *self, GAsyncResult *res, GError **error)
{
  GTask *task;

  g_return_val_if_fail (PHOTOS_IS_BASE_ITEM (self), FALSE);

  g_return_val_if_fail (g_task_is_valid (res, self), FALSE);
  task = G_TASK (res);

  g_return_val_if_fail (g_task_get_source_tag (task) == photos_base_item_trash_async, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  return g_task_propagate_boolean (task, error);
}


void
photos_base_item_unmark_busy (PhotosBaseItem *self)
{
  PhotosBaseItemPrivate *priv;

  g_return_if_fail (PHOTOS_IS_BASE_ITEM (self));
  priv = photos_base_item_get_instance_private (self);

  g_return_if_fail (priv->busy_count > 0);

  priv->busy_count--;
  if (priv->busy_count == 0)
    g_object_notify (G_OBJECT (self), "pulse");
}
