/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2014 Pranav Kant
 * Copyright © 2014 – 2017 Red Hat, Inc.
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

#include <gio/gio.h>
#include <glib.h>

#include "photos-base-manager.h"
#include "photos-debug.h"
#include "photos-media-server-item.h"
#include "photos-search-context.h"
#include "photos-source.h"
#include "photos-utils.h"


struct _PhotosMediaServerItem
{
  PhotosBaseItem parent_instance;
  PhotosBaseManager *src_mngr;
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
  g_autoptr (GFile) file = NULL;
  const gchar *uri;
  gchar *ret_val;

  uri = photos_base_item_get_uri (item);
  file = g_file_new_for_uri (uri);
  ret_val = g_file_get_basename (file);

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
  g_autoptr (GFile) file = NULL;
  GQuark orientation;
  gboolean ret_val;
  g_autofree gchar *thumbnail_path = NULL;
  const gchar *mime_type;
  const gchar *uri;
  gint64 height;
  gint64 mtime;
  gint64 width;

  uri = photos_base_item_get_uri (item);
  file = g_file_new_for_uri (uri);
  mime_type = photos_base_item_get_mime_type (item);
  mtime = photos_base_item_get_mtime (item);
  orientation = photos_base_item_get_orientation (item);
  height = photos_base_item_get_height (item);
  width = photos_base_item_get_width (item);
  thumbnail_path = photos_base_item_create_thumbnail_path (item);

  ret_val = photos_utils_create_thumbnail (file,
                                           mime_type,
                                           mtime,
                                           orientation,
                                           height,
                                           width,
                                           NULL,
                                           thumbnail_path,
                                           cancellable,
                                           error);

  return ret_val;
}


static GFile *
photos_media_server_item_download (PhotosBaseItem *item, GCancellable *cancellable, GError **error)
{
  GFile *ret_val = NULL;
  g_autoptr (GFile) local_file = NULL;
  g_autoptr (GFile) remote_file = NULL;
  const gchar *cache_dir;
  const gchar *uri;
  g_autofree gchar *local_dir = NULL;
  g_autofree gchar *local_filename = NULL;
  g_autofree gchar *local_path = NULL;

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

  ret_val = g_object_ref (local_file);

 out:
  return ret_val;
}


static GtkWidget *
photos_media_server_item_get_source_widget (PhotosBaseItem *item)
{
  PhotosMediaServerItem *self = PHOTOS_MEDIA_SERVER_ITEM (item);
  GtkWidget *source_widget;
  const gchar *name;

  name = photos_utils_get_provider_name (self->src_mngr, item);
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

  name = photos_utils_get_provider_name (self->src_mngr, PHOTOS_BASE_ITEM (self));
  photos_base_item_set_default_app_name (PHOTOS_BASE_ITEM (self), name);
}


static void
photos_media_server_item_dispose (GObject *object)
{
  PhotosMediaServerItem *self = PHOTOS_MEDIA_SERVER_ITEM (object);

  g_clear_object (&self->src_mngr);

  G_OBJECT_CLASS (photos_media_server_item_parent_class)->dispose (object);
}


static void
photos_media_server_item_init (PhotosMediaServerItem *self)
{
  GApplication *app;
  PhotosSearchContextState *state;

  app = g_application_get_default ();
  state = photos_search_context_get_state (PHOTOS_SEARCH_CONTEXT (app));

  self->src_mngr = g_object_ref (state->src_mngr);
}


static void
photos_media_server_item_class_init (PhotosMediaServerItemClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  PhotosBaseItemClass *base_item_class = PHOTOS_BASE_ITEM_CLASS (class);

  base_item_class->miner_name = "org.gnome.OnlineMiners.MediaServer";
  base_item_class->miner_object_path = "/org/gnome/OnlineMiners/MediaServer";

  object_class->constructed = photos_media_server_item_constructed;
  object_class->dispose = photos_media_server_item_dispose;
  base_item_class->create_filename_fallback = photos_media_server_item_create_filename_fallback;
  base_item_class->create_name_fallback = photos_media_server_item_create_name_fallback;
  base_item_class->create_thumbnail = photos_media_server_item_create_thumbnail;
  base_item_class->download = photos_media_server_item_download;
  base_item_class->get_source_widget = photos_media_server_item_get_source_widget;
}
