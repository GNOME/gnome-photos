/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2014 Pranav Kant
 * Copyright © 2012 – 2016 Red Hat, Inc.
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

#include <gio/gio.h>
#include <glib.h>
#include <glib/gi18n.h>

#include "photos-local-item.h"
#include "photos-utils.h"


struct _PhotosLocalItem
{
  PhotosBaseItem parent_instance;
};

struct _PhotosLocalItemClass
{
  PhotosBaseItemClass parent_class;
};


G_DEFINE_TYPE_WITH_CODE (PhotosLocalItem, photos_local_item, PHOTOS_TYPE_BASE_ITEM,
                         photos_utils_ensure_extension_points ();
                         g_io_extension_point_implement (PHOTOS_BASE_ITEM_EXTENSION_POINT_NAME,
                                                         g_define_type_id,
                                                         "local",
                                                         0));


static gchar *
photos_local_item_create_name_fallback (PhotosBaseItem *item)
{
  const gchar *filename;
  gchar *ret_val;

  filename = photos_base_item_get_filename (item);
  ret_val = photos_utils_filename_strip_extension (filename);
  return ret_val;
}


static gchar *
photos_local_item_create_pipeline_path (PhotosBaseItem *item)
{
  const gchar *data_dir;
  const gchar *uri;
  gchar *app_data_dir;
  gchar *md5;
  gchar *path;

  uri = photos_base_item_get_uri (item);
  md5 = g_compute_checksum_for_string (G_CHECKSUM_MD5, uri, -1);
  data_dir = g_get_user_data_dir ();

  app_data_dir = g_build_filename (data_dir, PACKAGE_TARNAME, "local", NULL);
  g_mkdir_with_parents (app_data_dir, 0700);

  path = g_build_filename (app_data_dir, md5, NULL);

  g_free (app_data_dir);
  g_free (md5);
  return path;
}


static gboolean
photos_local_item_create_thumbnail (PhotosBaseItem *item, GCancellable *cancellable, GError **error)
{
  GFile *file;
  gboolean ret_val;
  const gchar *uri;

  uri = photos_base_item_get_uri (item);
  file = g_file_new_for_uri (uri);
  ret_val = photos_utils_create_thumbnail (file, cancellable, error);

  g_object_unref (file);
  return ret_val;
}


static gchar *
photos_local_item_download (PhotosBaseItem *item, GCancellable *cancellable, GError **error)
{
  const gchar *uri;
  gchar *path;

  uri = photos_base_item_get_uri (item);
  path = g_filename_from_uri (uri, NULL, error);
  return path;
}


static GtkWidget *
photos_local_item_get_source_widget (PhotosBaseItem *item)
{
  GtkWidget *source_widget;

  if (photos_base_item_is_collection (item))
    {
      source_widget = gtk_label_new (_("Local"));
      gtk_widget_set_halign (source_widget, GTK_ALIGN_START);
    }
  else
    {
      GFile *file;
      GFile *source_link;
      GtkWidget *label;
      const gchar *uri;
      gchar *source_path;
      gchar *source_uri;

      uri = photos_base_item_get_uri (item);
      file = g_file_new_for_uri (uri);
      source_link = g_file_get_parent (file);
      source_path = g_file_get_path (source_link);
      source_uri = g_file_get_uri (source_link);

      source_widget = gtk_link_button_new_with_label (source_uri, source_path);
      gtk_widget_set_halign (source_widget, GTK_ALIGN_START);

      label = gtk_bin_get_child (GTK_BIN (source_widget));
      gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);

      g_object_unref (source_link);
      g_object_unref (file);
    }

  return source_widget;
}


static void
photos_local_item_trash_finish (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosLocalItem *self = PHOTOS_LOCAL_ITEM (user_data);
  GError *error;
  GFile *file = G_FILE (source_object);

  error = NULL;
  g_file_trash_finish (file, res, &error);
  if (error != NULL)
    {
      const gchar *uri;

      uri = photos_base_item_get_uri (PHOTOS_BASE_ITEM (self));
      g_warning ("Unable to trash %s: %s", uri, error->message);
      g_error_free (error);
    }

  g_object_unref (self);
}


static void
photos_local_item_trash (PhotosBaseItem *item)
{
  GFile *file;
  const gchar *uri;

  if (photos_base_item_is_collection (item))
    return;

  uri = photos_base_item_get_uri (item);
  file = g_file_new_for_uri (uri);
  g_file_trash_async (file, G_PRIORITY_DEFAULT, NULL, photos_local_item_trash_finish, g_object_ref (item));

  g_object_unref (file);

}


static void
photos_local_item_constructed (GObject *object)
{
  PhotosLocalItem *self = PHOTOS_LOCAL_ITEM (object);
  GAppInfo *default_app = NULL;
  const gchar *mime_type;

  G_OBJECT_CLASS (photos_local_item_parent_class)->constructed (object);

  mime_type = photos_base_item_get_mime_type (PHOTOS_BASE_ITEM (self));
  if (mime_type == NULL)
    return;

  default_app = g_app_info_get_default_for_type (mime_type, FALSE);
  if (default_app == NULL)
    return;

  photos_base_item_set_default_app (PHOTOS_BASE_ITEM (self), default_app);
  g_object_unref (default_app);
}


static void
photos_local_item_init (PhotosLocalItem *self)
{
}


static void
photos_local_item_class_init (PhotosLocalItemClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  PhotosBaseItemClass *base_item_class = PHOTOS_BASE_ITEM_CLASS (class);

  object_class->constructed = photos_local_item_constructed;
  base_item_class->create_name_fallback = photos_local_item_create_name_fallback;
  base_item_class->create_pipeline_path = photos_local_item_create_pipeline_path;
  base_item_class->create_thumbnail = photos_local_item_create_thumbnail;
  base_item_class->download = photos_local_item_download;
  base_item_class->get_source_widget = photos_local_item_get_source_widget;
  base_item_class->trash = photos_local_item_trash;
}
