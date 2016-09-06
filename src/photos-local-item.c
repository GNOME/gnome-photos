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

#include <gexiv2/gexiv2.h>
#include <gio/gio.h>
#include <glib.h>
#include <glib/gi18n.h>

#include "photos-local-item.h"
#include "photos-utils.h"


struct _PhotosLocalItem
{
  PhotosBaseItem parent_instance;
  GCancellable *cancellable;
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
photos_local_item_create_filename_fallback (PhotosBaseItem *item)
{
  g_warn_if_reached ();
  return NULL;
}


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
      gtk_label_set_max_width_chars (GTK_LABEL (label), 40);

      g_object_unref (source_link);
      g_object_unref (file);
    }

  return source_widget;
}


static gboolean
photos_local_item_metadata_add_shared (PhotosBaseItem  *item,
                                       const gchar     *provider_type,
                                       const gchar     *account_identity,
                                       const gchar     *shared_id,
                                       GCancellable    *cancellable,
                                       GError         **error)
{
  GVariant *shared_variant = NULL;
  GVariantBuilder builder;
  const GVariantType *tuple_items[] = {G_VARIANT_TYPE_STRING,   /* provider-type */
                                       G_VARIANT_TYPE_STRING,   /* account-identity */
                                       G_VARIANT_TYPE_STRING};  /* shared-id */
  GVariantType *array_type = NULL;
  GVariantType *tuple_type = NULL;
  GExiv2Metadata *metadata = NULL;
  gboolean ret_val = FALSE;
  const gchar *mime_type;
  const gchar *shared_tag = "Xmp.gnome.photos-shared";
  const gchar *version_tag = "Xmp.gnome.photos-xmp-version";
  gchar *path = NULL;
  gchar *shared_string = NULL;
  gchar *tuple_type_format = NULL;

  mime_type = photos_base_item_get_mime_type (item);
  if (g_strcmp0 (mime_type, "image/png") != 0
      && g_strcmp0 (mime_type, "image/jp2") != 0
      && g_strcmp0 (mime_type, "image/jpeg") != 0)
    {
      ret_val = TRUE;
      goto out;
    }

  path = photos_base_item_download (item, cancellable, error);
  if (path == NULL)
    goto out;

  metadata = gexiv2_metadata_new ();

  if (!gexiv2_metadata_open_path (metadata, path, error))
    goto out;

  if (!gexiv2_metadata_set_tag_long (metadata, version_tag, 0))
    {
      g_set_error (error, PHOTOS_ERROR, 0, "Failed to update %s", version_tag);
      goto out;
    }

  tuple_type = g_variant_type_new_tuple (tuple_items, G_N_ELEMENTS (tuple_items));
  tuple_type_format = g_variant_type_dup_string (tuple_type);

  array_type = g_variant_type_new_array (tuple_type);

  g_variant_builder_init (&builder, array_type);
  g_variant_builder_add (&builder, tuple_type_format, provider_type, account_identity, shared_id);

  shared_string = gexiv2_metadata_get_tag_string (metadata, shared_tag);
  if (shared_string != NULL)
    {
      GVariant *old_shared_variant = NULL;
      GVariant *child = NULL;
      GVariantIter iter;

      old_shared_variant = g_variant_parse (array_type, shared_string, NULL, NULL, error);
      if (old_shared_variant == NULL)
        goto out;

      g_variant_iter_init (&iter, old_shared_variant);
      child = g_variant_iter_next_value (&iter);
      while (child != NULL)
        {
          g_variant_builder_add_value (&builder, child);
          g_variant_unref (child);
          child = g_variant_iter_next_value (&iter);
        }

      g_variant_unref (old_shared_variant);
    }

  shared_variant = g_variant_builder_end (&builder);

  g_free (shared_string);
  shared_string = g_variant_print (shared_variant, TRUE);

  if (!gexiv2_metadata_set_tag_string (metadata, shared_tag, shared_string))
    {
      g_set_error (error, PHOTOS_ERROR, 0, "Failed to update %s", shared_tag);
      goto out;
    }

  if (!gexiv2_metadata_save_file (metadata, path, error))
    goto out;

  ret_val = TRUE;

 out:
  g_clear_object (&metadata);
  g_clear_pointer (&array_type, (GDestroyNotify) g_variant_type_free);
  g_clear_pointer (&tuple_type, (GDestroyNotify) g_variant_type_free);
  g_clear_pointer (&shared_variant, (GDestroyNotify) g_variant_unref);
  g_free (path);
  g_free (shared_string);
  g_free (tuple_type_format);
  return ret_val;
}


static void
photos_local_item_trash_finish (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  GError *error;
  GFile *file = G_FILE (source_object);

  error = NULL;
  g_file_trash_finish (file, res, &error);
  if (error != NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        {
          PhotosLocalItem *self = PHOTOS_LOCAL_ITEM (user_data);
          const gchar *uri;

          uri = photos_base_item_get_uri (PHOTOS_BASE_ITEM (self));
          g_warning ("Unable to trash %s: %s", uri, error->message);
        }

      g_error_free (error);
    }
}


static void
photos_local_item_trash (PhotosBaseItem *item)
{
  GFile *file;
  const gchar *uri;
  PhotosLocalItem *self = PHOTOS_LOCAL_ITEM (item);

  if (photos_base_item_is_collection (item))
    return;

  uri = photos_base_item_get_uri (item);
  file = g_file_new_for_uri (uri);
  g_file_trash_async (file, G_PRIORITY_DEFAULT, self->cancellable, photos_local_item_trash_finish, self);

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
photos_local_item_dispose (GObject *object)
{
  PhotosLocalItem *self = PHOTOS_LOCAL_ITEM (object);

  if (self->cancellable != NULL)
    {
      g_cancellable_cancel (self->cancellable);
      g_clear_object (&self->cancellable);
    }

  G_OBJECT_CLASS (photos_local_item_parent_class)->dispose (object);
}


static void
photos_local_item_init (PhotosLocalItem *self)
{
  self->cancellable = g_cancellable_new ();
}


static void
photos_local_item_class_init (PhotosLocalItemClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  PhotosBaseItemClass *base_item_class = PHOTOS_BASE_ITEM_CLASS (class);

  object_class->constructed = photos_local_item_constructed;
  object_class->dispose = photos_local_item_dispose;
  base_item_class->create_filename_fallback = photos_local_item_create_filename_fallback;
  base_item_class->create_name_fallback = photos_local_item_create_name_fallback;
  base_item_class->create_pipeline_path = photos_local_item_create_pipeline_path;
  base_item_class->create_thumbnail = photos_local_item_create_thumbnail;
  base_item_class->download = photos_local_item_download;
  base_item_class->get_source_widget = photos_local_item_get_source_widget;
  base_item_class->metadata_add_shared = photos_local_item_metadata_add_shared;
  base_item_class->trash = photos_local_item_trash;
}
