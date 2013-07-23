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
#include <glib/gi18n.h>
#include <goa/goa.h>
#include <libgnome-desktop/gnome-desktop-thumbnail.h>

#include "photos-base-item.h"
#include "photos-flickr-item.h"
#include "photos-source.h"
#include "photos-source-manager.h"


struct _PhotosFlickrItemPrivate
{
  PhotosBaseManager *src_mngr;
};


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


static const gchar *
photos_flickr_item_get_source_name (PhotosBaseItem *item)
{
  return _("Flickr");
}


static void
photos_flickr_item_open (PhotosBaseItem *item, GdkScreen *screen, guint32 timestamp)
{
  PhotosFlickrItem *self = PHOTOS_FLICKR_ITEM (item);
  PhotosFlickrItemPrivate *priv = self->priv;
  GError *error;
  GoaAccount *account;
  GoaObject *object;
  PhotosSource *source;
  const gchar *identifier;
  const gchar *identity;
  const gchar *resource_urn;
  gchar *flickr_uri;

  identifier = photos_base_item_get_identifier (item) + strlen("flickr:");

  resource_urn = photos_base_item_get_resource_urn (item);
  source = PHOTOS_SOURCE (photos_base_manager_get_object_by_id (priv->src_mngr, resource_urn));
  object = photos_source_get_goa_object (source);
  account = goa_object_peek_account (object);
  identity = goa_account_get_identity (account);

  flickr_uri = g_strdup_printf ("https://www.flickr.com/photos/%s/%s", identity, identifier);

  error = NULL;
  gtk_show_uri (screen, flickr_uri, timestamp, &error);
  if (error != NULL)
    {
      g_warning ("Unable to show URI %s: %s", flickr_uri, error->message);
      g_error_free (error);
    }

  g_free (flickr_uri);
}


static void
photos_flickr_item_constructed (GObject *object)
{
  PhotosFlickrItem *self = PHOTOS_FLICKR_ITEM (object);

  G_OBJECT_CLASS (photos_flickr_item_parent_class)->constructed (object);

  photos_base_item_set_default_app_name (PHOTOS_BASE_ITEM (self), _("Flickr"));
}


static void
photos_flickr_item_dispose (GObject *object)
{
  PhotosFlickrItem *self = PHOTOS_FLICKR_ITEM (object);

  g_clear_object (&self->priv->src_mngr);

  G_OBJECT_CLASS (photos_flickr_item_parent_class)->dispose (object);
}


static void
photos_flickr_item_init (PhotosFlickrItem *self)
{
  PhotosFlickrItemPrivate *priv;

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, PHOTOS_TYPE_FLICKR_ITEM, PhotosFlickrItemPrivate);
  priv = self->priv;

  priv->src_mngr = photos_source_manager_new ();
}


static void
photos_flickr_item_class_init (PhotosFlickrItemClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  PhotosBaseItemClass *base_item_class = PHOTOS_BASE_ITEM_CLASS (class);

  object_class->constructed= photos_flickr_item_constructed;
  object_class->dispose = photos_flickr_item_dispose;
  base_item_class->create_thumbnail = photos_flickr_item_create_thumbnail;
  base_item_class->download = photos_flickr_item_download;
  base_item_class->get_source_name = photos_flickr_item_get_source_name;
  base_item_class->open = photos_flickr_item_open;

  g_type_class_add_private (class, sizeof (PhotosFlickrItemPrivate));
}


PhotosBaseItem *
photos_flickr_item_new (TrackerSparqlCursor *cursor)
{
  return g_object_new (PHOTOS_TYPE_FLICKR_ITEM,
                       "cursor", cursor,
                       "failed-thumbnailing", FALSE,
                       NULL);
}
