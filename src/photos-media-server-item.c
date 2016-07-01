/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2014 Pranav Kant
 * Copyright © 2014 – 2016 Red Hat, Inc.
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

#include <string.h>

#include <gio/gio.h>
#include <glib.h>

#include "photos-debug.h"
#include "photos-media-server-item.h"
#include "photos-source.h"
#include "photos-utils.h"


struct _PhotosMediaServerItem
{
  PhotosBaseItem parent_instance;
};

struct _PhotosMediaServerItemClass
{
  PhotosBaseItemClass parent_class;
};


G_DEFINE_TYPE_WITH_CODE (PhotosMediaServerItem, photos_media_server_item, PHOTOS_TYPE_BASE_ITEM,
                         photos_utils_ensure_extension_points ();
                         g_io_extension_point_implement (PHOTOS_BASE_ITEM_EXTENSION_POINT_NAME,
                                                         g_define_type_id,
                                                         "media-server",
                                                         0));


static gchar *
photos_media_server_item_create_filename_fallback (PhotosBaseItem *item)
{
  GFile *file = NULL;
  const gchar *uri;
  gchar *ret_val;

  uri = photos_base_item_get_uri (item);
  file = g_file_new_for_uri (uri);
  ret_val = g_file_get_basename (file);

  g_object_unref (file);
  return ret_val;
}


static gchar *
photos_media_server_item_create_name_fallback (PhotosBaseItem *item)
{
  /* TODO: provide a sane fallback */
  return g_strdup ("");
}


static gboolean
photos_media_server_item_create_thumbnail (PhotosBaseItem *item, GCancellable *cancellable, GError **error)
{
  GFile *file;
  gboolean ret_val;
  const gchar *mime_type;
  const gchar *uri;
  gint64 mtime;

  uri = photos_base_item_get_uri (item);
  file = g_file_new_for_uri (uri);
  mime_type = photos_base_item_get_mime_type (item);
  mtime = photos_base_item_get_mtime (item);
  ret_val = photos_utils_create_thumbnail (file, mime_type, mtime, cancellable, error);

  g_object_unref (file);
  return ret_val;
}


static gchar *
photos_media_server_item_download (PhotosBaseItem *item, GCancellable *cancellable, GError **error)
{
  GFile *local_file = NULL;
  GFile *remote_file = NULL;
  const gchar *cache_dir;
  const gchar *uri;
  gchar *local_dir = NULL;
  gchar *local_filename = NULL;
  gchar *local_path = NULL;
  gchar *ret_val = NULL;

  uri = photos_base_item_get_uri (item);
  remote_file = g_file_new_for_uri (uri);
  cache_dir = g_get_user_cache_dir ();

  local_dir = g_build_filename (cache_dir, PACKAGE_TARNAME, "media-server", NULL);
  g_mkdir_with_parents (local_dir, 0700);

  local_filename = g_file_get_basename (remote_file);
  local_path = g_build_filename (local_dir, local_filename, NULL);
  local_file = g_file_new_for_path (local_path);

  if (!g_file_test (local_path, G_FILE_TEST_EXISTS))
    {
      photos_debug (PHOTOS_DEBUG_NETWORK, "Downloading %s from Media Server to %s", uri, local_path);
      if (!g_file_copy (remote_file,
                        local_file,
                        G_FILE_COPY_ALL_METADATA | G_FILE_COPY_OVERWRITE,
                        cancellable,
                        NULL,
                        NULL,
                        error))
        {
          g_file_delete (local_file, NULL, NULL);
          goto out;
        }
    }

  ret_val = local_path;
  local_path = NULL;

 out:
  g_free (local_path);
  g_free (local_filename);
  g_free (local_dir);
  g_object_unref (local_file);
  g_object_unref (remote_file);
  return ret_val;
}


static GtkWidget *
photos_media_server_item_get_source_widget (PhotosBaseItem *item)
{
  GtkWidget *source_widget;
  const gchar *name;

  name = photos_utils_get_provider_name (item);
  source_widget = gtk_label_new (name);
  gtk_widget_set_halign (source_widget, GTK_ALIGN_START);

  return source_widget;
}


static void
photos_media_server_item_constructed (GObject *object)
{
  PhotosMediaServerItem *self = PHOTOS_MEDIA_SERVER_ITEM (object);
  const gchar *name;

  G_OBJECT_CLASS (photos_media_server_item_parent_class)->constructed (object);

  name = photos_utils_get_provider_name (PHOTOS_BASE_ITEM (self));
  photos_base_item_set_default_app_name (PHOTOS_BASE_ITEM (self), name);
}


static void
photos_media_server_item_init (PhotosMediaServerItem *self)
{
}


static void
photos_media_server_item_class_init (PhotosMediaServerItemClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  PhotosBaseItemClass *base_item_class = PHOTOS_BASE_ITEM_CLASS (class);

  base_item_class->miner_name = "org.gnome.OnlineMiners.MediaServer";
  base_item_class->miner_object_path = "/org/gnome/OnlineMiners/MediaServer";

  object_class->constructed = photos_media_server_item_constructed;
  base_item_class->create_filename_fallback = photos_media_server_item_create_filename_fallback;
  base_item_class->create_name_fallback = photos_media_server_item_create_name_fallback;
  base_item_class->create_thumbnail = photos_media_server_item_create_thumbnail;
  base_item_class->download = photos_media_server_item_download;
  base_item_class->get_source_widget = photos_media_server_item_get_source_widget;
}
