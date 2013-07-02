/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2013 Red Hat, Inc.
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

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gio/gio.h>
#include <glib.h>
#include <libgnome-desktop/gnome-desktop-thumbnail.h>

#include "photos-base-item.h"
#include "photos-flickr-item.h"


G_DEFINE_TYPE (PhotosFlickrItem, photos_flickr_item, PHOTOS_TYPE_BASE_ITEM);


static gboolean
photos_flickr_item_create_thumbnail (PhotosBaseItem *item, GCancellable *cancellable, GError **error)
{
  g_set_error (error,
               g_quark_from_static_string ("gnome-photos-error"),
               0,
               "Thumbnailing Flickr items is unsupported");
  return FALSE;
}


static gchar *
photos_flickr_item_download (PhotosBaseItem *item, GCancellable *cancellable, GError **error)
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

  local_dir = g_build_filename (cache_dir, PACKAGE_TARNAME, "flickr", NULL);
  g_mkdir_with_parents (local_dir, 0700);

  local_filename = g_file_get_basename (remote_file);
  local_path = g_build_filename (local_dir, local_filename, NULL);
  local_file = g_file_new_for_path (local_path);

  if (!g_file_test (local_path, G_FILE_TEST_EXISTS))
    {
      g_debug ("Downloading %s from Flickr to %s", uri, local_path);
      if (!g_file_copy (remote_file,
                        local_file,
                        G_FILE_COPY_ALL_METADATA | G_FILE_COPY_OVERWRITE,
                        cancellable,
                        NULL,
                        NULL,
                        error))
        goto out;
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


static void
photos_flickr_item_constructed (GObject *object)
{
  PhotosFlickrItem *self = PHOTOS_FLICKR_ITEM (object);
  PhotosFlickrItemPrivate *priv = self->priv;
  GAppInfo *default_app = NULL;
  const gchar *default_app_name;
  const gchar *mime_type;

  G_OBJECT_CLASS (photos_flickr_item_parent_class)->constructed (object);

  mime_type = photos_base_item_get_mime_type (PHOTOS_BASE_ITEM (self));
  if (mime_type == NULL)
    return;

  default_app = g_app_info_get_default_for_type (mime_type, TRUE);
  if (default_app == NULL)
    return;

  default_app_name = g_app_info_get_name (default_app);
  photos_base_item_set_default_app_name (PHOTOS_BASE_ITEM (self), default_app_name);

  g_object_unref (default_app);
}


static void
photos_flickr_item_init (PhotosFlickrItem *self)
{
}


static void
photos_flickr_item_class_init (PhotosFlickrItemClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  PhotosBaseItemClass *base_item_class = PHOTOS_BASE_ITEM_CLASS (class);

  object_class->constructed= photos_flickr_item_constructed;
  base_item_class->create_thumbnail = photos_flickr_item_create_thumbnail;
  base_item_class->download = photos_flickr_item_download;
}


PhotosBaseItem *
photos_flickr_item_new (TrackerSparqlCursor *cursor)
{
  return g_object_new (PHOTOS_TYPE_FLICKR_ITEM,
                       "cursor", cursor,
                       "failed-thumbnailing", FALSE,
                       NULL);
}
