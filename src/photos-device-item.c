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
 *   + Documents
 */


#include "config.h"

#include <dazzle.h>
#include <gio/gio.h>
#include <tracker-sparql.h>

#include "photos-base-manager.h"
#include "photos-device-item.h"
#include "photos-error.h"
#include "photos-filterable.h"
#include "photos-glib.h"
#include "photos-query.h"
#include "photos-search-context.h"
#include "photos-source.h"
#include "photos-utils.h"


struct _PhotosDeviceItem
{
  PhotosBaseItem parent_instance;
  GCancellable *cancellable;
  GVolume *volume;
  PhotosBaseManager *src_mngr;
};


G_DEFINE_TYPE_WITH_CODE (PhotosDeviceItem, photos_device_item, PHOTOS_TYPE_BASE_ITEM,
                         photos_utils_ensure_extension_points ();
                         g_io_extension_point_implement (PHOTOS_BASE_ITEM_EXTENSION_POINT_NAME,
                                                         g_define_type_id,
                                                         "device",
                                                         0));


static GVolume *
photos_device_item_get_volume_from_enclosing_mount (PhotosDeviceItem *self, GMount *enclosing_mount)
{
  g_autoptr (GVolume) volume = NULL;
  GVolume *ret_val = NULL;

  volume = g_mount_get_volume (enclosing_mount);
  if (volume == NULL)
    {
      g_autoptr (GFile) enclosing_root = NULL;
      guint i;
      guint n_items;

      enclosing_root = g_mount_get_root (enclosing_mount);

      n_items = g_list_model_get_n_items (G_LIST_MODEL (self->src_mngr));
      for (i = 0; i < n_items; i++)
        {
          g_autoptr (GFile) root = NULL;
          GMount *mount;
          g_autoptr (PhotosSource) source = NULL;

          source = PHOTOS_SOURCE (g_list_model_get_object (G_LIST_MODEL (self->src_mngr), i));
          mount = photos_source_get_mount (source);
          if (mount == NULL)
            continue;

          root = g_mount_get_root (mount);
          if (g_file_equal (enclosing_root, root))
            {
              volume = g_mount_get_volume (mount);
              if (volume != NULL)
                break;
            }
        }

      if (volume == NULL)
        {
          for (i = 0; i < n_items; i++)
            {
              g_autoptr (GFile) root = NULL;
              GMount *mount;
              g_autoptr (PhotosSource) source = NULL;

              source = PHOTOS_SOURCE (g_list_model_get_object (G_LIST_MODEL (self->src_mngr), i));
              mount = photos_source_get_mount (source);
              if (mount == NULL)
                continue;

              root = g_mount_get_root (mount);
              if (g_file_has_prefix (root, enclosing_root))
                {
                  volume = g_mount_get_volume (mount);
                  if (volume != NULL)
                    break;
                }
            }
        }
    }

  if (volume == NULL)
    goto out;

  ret_val = g_object_ref (volume);

 out:
  return ret_val;
}


static void
photos_device_item_refresh_icon_find_enclosing_mount (GObject *source_object,
                                                      GAsyncResult *res,
                                                      gpointer user_data)
{
  PhotosDeviceItem *self;
  GFile *file = G_FILE (source_object);
  g_autoptr (GMount) mount = NULL;

  {
    g_autoptr (GError) error = NULL;

    mount = g_file_find_enclosing_mount_finish (file, res, &error);
    if (error != NULL)
      {
        if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
          goto out;

        g_warning ("Unable to find enclosing mount: %s", error->message);
      }
  }

  self = PHOTOS_DEVICE_ITEM (user_data);

  g_clear_object (&self->volume);
  if (mount != NULL)
    {
      self->volume = photos_device_item_get_volume_from_enclosing_mount (self, mount);
      if (self->volume == NULL)
        {
          const gchar *id;

          id = photos_filterable_get_id (PHOTOS_FILTERABLE (self));
          g_warning ("Unable to find volume for %s", id);
        }
    }

  PHOTOS_BASE_ITEM_CLASS (photos_device_item_parent_class)->refresh_icon (PHOTOS_BASE_ITEM (self));

 out:
  return;
}


static gboolean
photos_device_item_source_widget_activate_link (GtkLinkButton *button, gpointer user_data)
{
  g_autoptr (GFile) file = NULL;
  PhotosDeviceItem *self;
  gboolean ret_val = GDK_EVENT_PROPAGATE;
  const gchar *uri;

  g_return_val_if_fail (GTK_IS_LINK_BUTTON (button), GDK_EVENT_PROPAGATE);
  g_return_val_if_fail (PHOTOS_IS_DEVICE_ITEM (user_data), GDK_EVENT_PROPAGATE);

  self = PHOTOS_DEVICE_ITEM (user_data);

  uri = photos_base_item_get_uri (PHOTOS_BASE_ITEM (self));
  file = g_file_new_for_uri (uri);

  {
    g_autoptr (GError) error = NULL;

    if (!dzl_file_manager_show (file, &error))
      {
        g_warning ("Unable to use org.freedesktop.FileManager1 for %s: %s", uri, error->message);
        goto out;
      }
  }

  ret_val = GDK_EVENT_STOP;

 out:
  return ret_val;
}


static gchar *
photos_device_item_create_filename_fallback (PhotosBaseItem *item)
{
  g_warn_if_reached ();
  return NULL;
}


static gchar *
photos_device_item_create_name_fallback (PhotosBaseItem *item)
{
  const gchar *filename;
  gchar *ret_val;

  filename = photos_base_item_get_filename (item);
  ret_val = photos_glib_filename_strip_extension (filename);
  return ret_val;
}


static gboolean
photos_device_item_create_thumbnail (PhotosBaseItem *item, GCancellable *cancellable, GError **error)
{
  PhotosDeviceItem *self = PHOTOS_DEVICE_ITEM (item);
  g_autoptr (GFile) file = NULL;
  GQuark orientation;
  gboolean ret_val = FALSE;
  g_autofree gchar *thumbnail_path = NULL;
  const gchar *mime_type;
  const gchar *uri;
  gint64 height;
  gint64 mtime;
  gint64 width;

  uri = photos_base_item_get_uri (PHOTOS_BASE_ITEM (self));
  file = g_file_new_for_uri (uri);
  mime_type = photos_base_item_get_mime_type (PHOTOS_BASE_ITEM (self));
  mtime = photos_base_item_get_mtime (PHOTOS_BASE_ITEM (self));
  orientation = photos_base_item_get_orientation (PHOTOS_BASE_ITEM (self));
  height = photos_base_item_get_height (PHOTOS_BASE_ITEM (self));
  width = photos_base_item_get_width (PHOTOS_BASE_ITEM (self));
  thumbnail_path = photos_base_item_create_thumbnail_path (PHOTOS_BASE_ITEM (self));

  if (!photos_utils_create_thumbnail (file,
                                      mime_type,
                                      mtime,
                                      orientation,
                                      height,
                                      width,
                                      NULL,
                                      thumbnail_path,
                                      cancellable,
                                      error))
    goto out;

  ret_val = TRUE;

 out:
  return ret_val;
}


static gchar *
photos_device_item_create_thumbnail_path (PhotosBaseItem *item)
{
  PhotosDeviceItem *self = PHOTOS_DEVICE_ITEM (item);
  g_autofree gchar *device_dir = NULL;
  g_autofree gchar *path_default = NULL;
  g_autofree gchar *path_default_basename = NULL;
  g_autofree gchar *path_default_dir = NULL;
  g_autofree gchar *uuid = NULL;
  gchar *path = NULL;

  path_default
    = PHOTOS_BASE_ITEM_CLASS (photos_device_item_parent_class)->create_thumbnail_path (PHOTOS_BASE_ITEM (self));

  path_default_basename = g_path_get_basename (path_default);
  path_default_dir = g_path_get_dirname (path_default);

  if (self->volume == NULL)
    {
      device_dir = g_strdup ("unknown");
      goto out;
    }

  uuid = g_volume_get_uuid (self->volume);
  if (uuid == NULL || uuid[0] == '\0')
    {
      device_dir = g_strdup ("unknown");
      goto out;
    }

  device_dir = g_steal_pointer (&uuid);

 out:
  path = g_build_filename (path_default_dir, "devices", device_dir, path_default_basename, NULL);
  return path;
}


static gchar *
photos_device_item_download (PhotosBaseItem *item, GCancellable *cancellable, GError **error)
{
  g_assert_not_reached ();
  return NULL;
}


static GtkWidget *
photos_device_item_get_source_widget (PhotosBaseItem *item)
{
  g_autoptr (GFile) file = NULL;
  g_autoptr (GFile) source_link = NULL;
  GtkWidget *label;
  GtkWidget *source_widget;
  const gchar *uri;
  g_autofree gchar *source_parse_name = NULL;
  g_autofree gchar *source_uri = NULL;

  g_return_val_if_fail (!photos_base_item_is_collection (item), NULL);

  uri = photos_base_item_get_uri (item);
  file = g_file_new_for_uri (uri);
  source_link = g_file_get_parent (file);
  source_parse_name = g_file_get_parse_name (source_link);
  source_uri = g_file_get_uri (source_link);

  source_widget = gtk_link_button_new_with_label (source_uri, source_parse_name);
  gtk_widget_set_halign (source_widget, GTK_ALIGN_START);
  g_signal_connect_object (source_widget,
                           "activate-link",
                           G_CALLBACK (photos_device_item_source_widget_activate_link),
                           item,
                           0);

  label = gtk_bin_get_child (GTK_BIN (source_widget));
  gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
  gtk_label_set_max_width_chars (GTK_LABEL (label), 40);

  return source_widget;
}


static gboolean
photos_device_item_metadata_add_shared (PhotosBaseItem  *item,
                                        const gchar     *provider_type,
                                        const gchar     *account_identity,
                                        const gchar     *shared_id,
                                        GCancellable    *cancellable,
                                        GError         **error)
{
  g_assert_not_reached ();
  return FALSE;
}


static void
photos_device_item_refresh_icon (PhotosBaseItem *item)
{
  PhotosDeviceItem *self = PHOTOS_DEVICE_ITEM (item);
  g_autoptr (GFile) file = NULL;
  const gchar *uri;

  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);
  self->cancellable = g_cancellable_new ();

  uri = photos_base_item_get_uri (item);
  file = g_file_new_for_uri (uri);
  g_file_find_enclosing_mount_async (file,
                                     G_PRIORITY_DEFAULT,
                                     self->cancellable,
                                     photos_device_item_refresh_icon_find_enclosing_mount,
                                     self);
}


static void
photos_device_item_trash (PhotosBaseItem *item)
{
  g_assert_not_reached ();
}


static void
photos_device_item_constructed (GObject *object)
{
  PhotosDeviceItem *self = PHOTOS_DEVICE_ITEM (object);
  const gchar *mime_type;

  G_OBJECT_CLASS (photos_device_item_parent_class)->constructed (object);

  mime_type = photos_base_item_get_mime_type (PHOTOS_BASE_ITEM (self));
  if (mime_type != NULL)
    {
      g_autoptr (GAppInfo) default_app = NULL;

      default_app = g_app_info_get_default_for_type (mime_type, FALSE);
      if (default_app != NULL)
        photos_base_item_set_default_app (PHOTOS_BASE_ITEM (self), default_app);
    }
}


static void
photos_device_item_dispose (GObject *object)
{
  PhotosDeviceItem *self = PHOTOS_DEVICE_ITEM (object);

  if (self->cancellable != NULL)
    {
      g_cancellable_cancel (self->cancellable);
      g_clear_object (&self->cancellable);
    }

  g_clear_object (&self->volume);
  g_clear_object (&self->src_mngr);

  G_OBJECT_CLASS (photos_device_item_parent_class)->dispose (object);
}


static void
photos_device_item_init (PhotosDeviceItem *self)
{
  GApplication *app;
  PhotosSearchContextState *state;

  app = g_application_get_default ();
  state = photos_search_context_get_state (PHOTOS_SEARCH_CONTEXT (app));

  self->cancellable = g_cancellable_new ();
  self->src_mngr = g_object_ref (state->src_mngr);
}


static void
photos_device_item_class_init (PhotosDeviceItemClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  PhotosBaseItemClass *base_item_class = PHOTOS_BASE_ITEM_CLASS (class);

  object_class->constructed = photos_device_item_constructed;
  object_class->dispose = photos_device_item_dispose;
  base_item_class->create_filename_fallback = photos_device_item_create_filename_fallback;
  base_item_class->create_name_fallback = photos_device_item_create_name_fallback;
  base_item_class->create_thumbnail = photos_device_item_create_thumbnail;
  base_item_class->create_thumbnail_path = photos_device_item_create_thumbnail_path;
  base_item_class->download = photos_device_item_download;
  base_item_class->get_source_widget = photos_device_item_get_source_widget;
  base_item_class->metadata_add_shared = photos_device_item_metadata_add_shared;
  base_item_class->refresh_icon = photos_device_item_refresh_icon;
  base_item_class->trash = photos_device_item_trash;
}
