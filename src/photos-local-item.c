/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2014 Pranav Kant
 * Copyright © 2012 – 2018 Red Hat, Inc.
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

#include <dazzle.h>
#include <gexiv2/gexiv2.h>
#include <gio/gio.h>
#include <glib.h>
#include <glib/gi18n.h>

#include "photos-error.h"
#include "photos-glib.h"
#include "photos-local-item.h"
#include "photos-utils.h"


struct _PhotosLocalItem
{
  PhotosBaseItem parent_instance;
  GCancellable *cancellable;
};


G_DEFINE_TYPE_WITH_CODE (PhotosLocalItem, photos_local_item, PHOTOS_TYPE_BASE_ITEM,
                         photos_utils_ensure_extension_points ();
                         g_io_extension_point_implement (PHOTOS_BASE_ITEM_EXTENSION_POINT_NAME,
                                                         g_define_type_id,
                                                         "local",
                                                         0));


static gboolean
photos_local_item_source_widget_activate_link (GtkLinkButton *button, gpointer user_data)
{
  g_autoptr (GFile) file = NULL;
  PhotosLocalItem *self;
  gboolean ret_val = GDK_EVENT_PROPAGATE;
  const gchar *uri;

  g_return_val_if_fail (GTK_IS_LINK_BUTTON (button), GDK_EVENT_PROPAGATE);
  g_return_val_if_fail (PHOTOS_IS_LOCAL_ITEM (user_data), GDK_EVENT_PROPAGATE);

  self = PHOTOS_LOCAL_ITEM (user_data);

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
  ret_val = photos_glib_filename_strip_extension (filename);
  return ret_val;
}


static gchar *
photos_local_item_get_pipeline_path (PhotosLocalItem *self)
{
  const gchar *uri;
  g_autofree gchar *item_path = NULL;
  gchar *path = NULL;

  uri = photos_base_item_get_uri (PHOTOS_BASE_ITEM (self));

  {
    g_autoptr (GError) error = NULL;

    item_path = g_filename_from_uri (uri, NULL, &error);
    if (error != NULL)
      {
        g_warning ("Unable to get filename from URI %s: %s", uri, error->message);
        goto out;
      }
  }

  path = g_strconcat (item_path, ".gnome-photos", NULL);

 out:
  return path;
}


static gchar *
photos_local_item_get_pipeline_path_legacy (PhotosLocalItem *self)
{
  const gchar *data_dir;
  const gchar *uri;
  g_autofree gchar *md5 = NULL;
  gchar *path;

  uri = photos_base_item_get_uri (PHOTOS_BASE_ITEM (self));
  md5 = g_compute_checksum_for_string (G_CHECKSUM_MD5, uri, -1);
  data_dir = g_get_user_data_dir ();

  path = g_build_filename (data_dir, PACKAGE_TARNAME, "local", md5, NULL);

  return path;
}


static GStrv
photos_local_item_create_pipeline_paths (PhotosBaseItem *item)
{
  PhotosLocalItem *self = PHOTOS_LOCAL_ITEM (item);
  GStrv paths = NULL;
  guint i = 0;

  paths = (GStrv) g_malloc0_n (3, sizeof (gchar *));

  paths[i] = photos_local_item_get_pipeline_path (self);
  if (paths[i] != NULL)
    i++;

  paths[i] = photos_local_item_get_pipeline_path_legacy (self);

  return paths;
}


static gboolean
photos_local_item_create_thumbnail (PhotosBaseItem *item, GCancellable *cancellable, GError **error)
{
  PhotosLocalItem *self = PHOTOS_LOCAL_ITEM (item);
  g_autoptr (GFile) file = NULL;
  GQuark orientation;
  g_auto (GStrv) pipeline_paths = NULL;
  g_auto (GStrv) pipeline_uris = NULL;
  gboolean ret_val = FALSE;
  const gchar *mime_type;
  const gchar *uri;
  g_autofree gchar *pipeline_uri = NULL;
  g_autofree gchar *thumbnail_path = NULL;
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

  pipeline_paths = photos_base_item_create_pipeline_paths (PHOTOS_BASE_ITEM (self));
  pipeline_uris = photos_utils_convert_paths_to_uris ((const gchar *const *) pipeline_paths);

  if (!photos_utils_create_thumbnail (file,
                                      mime_type,
                                      mtime,
                                      orientation,
                                      height,
                                      width,
                                      (const gchar *const *) pipeline_uris,
                                      thumbnail_path,
                                      cancellable,
                                      error))
    goto out;

  ret_val = TRUE;

 out:
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
      g_autoptr (GFile) file = NULL;
      g_autoptr (GFile) source_link = NULL;
      GtkWidget *label;
      const gchar *uri;
      g_autofree gchar *source_path = NULL;
      g_autofree gchar *source_path_display_name = NULL;
      g_autofree gchar *source_uri = NULL;

      uri = photos_base_item_get_uri (item);
      file = g_file_new_for_uri (uri);
      source_link = g_file_get_parent (file);
      source_path = g_file_get_path (source_link);
      source_path_display_name = g_filename_display_name (source_path);
      source_uri = g_file_get_uri (source_link);

      source_widget = gtk_link_button_new_with_label (source_uri, source_path_display_name);
      gtk_widget_set_halign (source_widget, GTK_ALIGN_START);
      g_signal_connect_object (source_widget,
                               "activate-link",
                               G_CALLBACK (photos_local_item_source_widget_activate_link),
                               item,
                               0);

      label = gtk_bin_get_child (GTK_BIN (source_widget));
      gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
      gtk_label_set_max_width_chars (GTK_LABEL (label), 40);
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
  g_autoptr (GVariant) shared_variant = NULL;
  const GVariantType *tuple_items[] =
    {
      G_VARIANT_TYPE_STRING, /* provider-type */
      G_VARIANT_TYPE_STRING, /* account-identity */
      G_VARIANT_TYPE_STRING  /* shared-id */
    };
  g_autoptr (GVariantType) array_type = NULL;
  g_autoptr (GVariantType) tuple_type = NULL;
  GExiv2Metadata *metadata = NULL; /* TODO: Use g_autoptr */
  gboolean ret_val = FALSE;
  const gchar *mime_type;
  const gchar *shared_tag = "Xmp.gnome.photos-shared";
  const gchar *version_tag = "Xmp.gnome.photos-xmp-version";
  g_autofree gchar *path = NULL;
  g_autofree gchar *tuple_type_format = NULL;

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

  {
    g_auto (GVariantBuilder) builder = G_VARIANT_BUILDER_INIT (array_type);
    g_autofree gchar *shared_string = NULL;

    g_variant_builder_add (&builder, tuple_type_format, provider_type, account_identity, shared_id);

    shared_string = gexiv2_metadata_get_tag_string (metadata, shared_tag);
    if (shared_string != NULL)
      {
        g_autoptr (GVariant) old_shared_variant = NULL;
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
      }

    shared_variant = g_variant_builder_end (&builder);
  }

  {
    g_autofree gchar *shared_string = NULL;

    shared_string = g_variant_print (shared_variant, TRUE);
    if (!gexiv2_metadata_set_tag_string (metadata, shared_tag, shared_string))
      {
        g_set_error (error, PHOTOS_ERROR, 0, "Failed to update %s", shared_tag);
        goto out;
      }
  }

  if (!gexiv2_metadata_save_file (metadata, path, error))
    goto out;

  ret_val = TRUE;

 out:
  g_clear_object (&metadata);
  return ret_val;
}


static void
photos_local_item_trash_finish (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  GFile *file = G_FILE (source_object);

  {
    g_autoptr (GError) error = NULL;

    if (!g_file_trash_finish (file, res, &error))
      {
        if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
          {
            PhotosLocalItem *self = PHOTOS_LOCAL_ITEM (user_data);
            const gchar *uri;

            uri = photos_base_item_get_uri (PHOTOS_BASE_ITEM (self));
            g_warning ("Unable to trash %s: %s", uri, error->message);
          }
      }
  }
}


static void
photos_local_item_trash (PhotosBaseItem *item)
{
  g_autoptr (GFile) file = NULL;
  const gchar *uri;
  PhotosLocalItem *self = PHOTOS_LOCAL_ITEM (item);

  if (photos_base_item_is_collection (item))
    return;

  uri = photos_base_item_get_uri (item);
  file = g_file_new_for_uri (uri);
  g_file_trash_async (file, G_PRIORITY_DEFAULT, self->cancellable, photos_local_item_trash_finish, self);
}


static void
photos_local_item_constructed (GObject *object)
{
  PhotosLocalItem *self = PHOTOS_LOCAL_ITEM (object);
  g_autoptr (GAppInfo) default_app = NULL;
  const gchar *mime_type;

  G_OBJECT_CLASS (photos_local_item_parent_class)->constructed (object);

  mime_type = photos_base_item_get_mime_type (PHOTOS_BASE_ITEM (self));
  if (mime_type == NULL)
    return;

  default_app = g_app_info_get_default_for_type (mime_type, FALSE);
  if (default_app == NULL)
    return;

  photos_base_item_set_default_app (PHOTOS_BASE_ITEM (self), default_app);
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
  base_item_class->create_pipeline_paths = photos_local_item_create_pipeline_paths;
  base_item_class->create_thumbnail = photos_local_item_create_thumbnail;
  base_item_class->download = photos_local_item_download;
  base_item_class->get_source_widget = photos_local_item_get_source_widget;
  base_item_class->metadata_add_shared = photos_local_item_metadata_add_shared;
  base_item_class->trash = photos_local_item_trash;
}
