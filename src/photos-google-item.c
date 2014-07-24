/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2014 Saurav Agarwalla
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

#include <gdata/gdata.h>
#include <gio/gio.h>
#include <glib.h>
#include <libgnome-desktop/gnome-desktop-thumbnail.h>

#include "photos-base-manager.h"
#include "photos-google-item.h"
#include "photos-search-context.h"
#include "photos-source.h"
#include "photos-utils.h"


struct _PhotosGoogleItemPrivate
{
  PhotosBaseManager *src_mngr;
};


G_DEFINE_TYPE_WITH_CODE (PhotosGoogleItem, photos_google_item, PHOTOS_TYPE_BASE_ITEM,
                         G_ADD_PRIVATE (PhotosGoogleItem)
                         photos_utils_ensure_extension_points ();
                         g_io_extension_point_implement (PHOTOS_BASE_ITEM_EXTENSION_POINT_NAME,
                                                         g_define_type_id,
                                                         "google",
                                                         0));


static GDataEntry *
photos_google_get_picasaweb_file (PhotosBaseItem *item, GCancellable *cancellable, GError **error)
{
  PhotosGoogleItemPrivate *priv = PHOTOS_GOOGLE_ITEM (item)->priv;
  PhotosSource *source;
  GDataAuthorizationDomain *authorization_domain;
  GDataEntry *entry;
  GDataGoaAuthorizer *authorizer;
  GDataPicasaWebQuery *query;
  GDataPicasaWebService *service;
  const gchar *identifier;
  const gchar *resource_urn;

  resource_urn = photos_base_item_get_resource_urn (item);
  source = PHOTOS_SOURCE (photos_base_manager_get_object_by_id (priv->src_mngr, resource_urn));
  authorizer = gdata_goa_authorizer_new (photos_source_get_goa_object (source));
  identifier = photos_base_item_get_identifier (item) + strlen ("google:picasaweb:");
  service = gdata_picasaweb_service_new (GDATA_AUTHORIZER (authorizer));
  authorization_domain = gdata_picasaweb_service_get_primary_authorization_domain ();

  query = gdata_picasaweb_query_new (NULL);
  gdata_picasaweb_query_set_image_size (query, "d");

  entry = gdata_service_query_single_entry (GDATA_SERVICE (service),
                                            authorization_domain,
                                            identifier,
                                            GDATA_QUERY (query),
                                            GDATA_TYPE_PICASAWEB_FILE,
                                            cancellable,
                                            error);

  g_object_unref (service);
  g_object_unref (authorizer);
  g_object_unref (query);

  return entry;
}


static gboolean
photos_google_item_create_thumbnail (PhotosBaseItem *item, GCancellable *cancellable, GError **error)
{
  GDataEntry *entry = NULL;
  GList *l;
  GList *thumbnails;
  GDataMediaThumbnail *thumbnail = NULL;
  gchar *local_path = NULL;
  gchar *local_dir = NULL;
  GFile *local_file = NULL;
  GFile *remote_file = NULL;
  gboolean ret_val = FALSE;
  const gchar *thumbnail_uri;
  const gchar *uri;
  guint max_width = 0;
  guint current_width;

  entry = photos_google_get_picasaweb_file (item, cancellable, error);
  if (entry == NULL)
    goto out;

  thumbnails = gdata_picasaweb_file_get_thumbnails (GDATA_PICASAWEB_FILE (entry));
  if (thumbnails == NULL)
    {
      g_set_error (error, PHOTOS_ERROR, 0, "Failed to find an image for the thumbnail");
      goto out;
    }

  for (l = thumbnails; l != NULL; l = l->next)
    {
      current_width = gdata_media_thumbnail_get_width (GDATA_MEDIA_THUMBNAIL (l->data));
      if (current_width > max_width)
        {
          max_width = current_width;
          thumbnail = GDATA_MEDIA_THUMBNAIL (l->data);
        }
    }

  thumbnail_uri = gdata_media_thumbnail_get_uri (thumbnail);
  remote_file = g_file_new_for_uri (thumbnail_uri);

  uri = photos_base_item_get_uri (item);
  local_path = gnome_desktop_thumbnail_path_for_uri (uri, GNOME_DESKTOP_THUMBNAIL_SIZE_LARGE);
  local_file = g_file_new_for_path (local_path);

  local_dir = g_path_get_dirname (local_path);
  g_mkdir_with_parents (local_dir, 0700);

  g_debug ("Downloading %s from Google to %s", thumbnail_uri, local_path);
  if (!g_file_copy (remote_file,
                    local_file,
                    G_FILE_COPY_ALL_METADATA | G_FILE_COPY_OVERWRITE,
                    cancellable,
                    NULL,
                    NULL,
                    error))
    goto out;

  ret_val = TRUE;

 out:
  g_free (local_path);
  g_free (local_dir);
  g_clear_object (&local_file);
  g_clear_object (&remote_file);
  g_clear_object (&entry);
  return ret_val;
}


static gchar *
photos_google_item_download (PhotosBaseItem *item, GCancellable *cancellable, GError **error)
{
  GDataEntry *entry = NULL;
  GFile *local_file = NULL;
  GFile *remote_file = NULL;
  const gchar *cache_dir;
  const gchar *uri;
  gchar *local_dir = NULL;
  gchar *local_filename = NULL;
  gchar *local_path = NULL;
  gchar *ret_val = NULL;

  entry = photos_google_get_picasaweb_file (item, cancellable, error);
  if (entry == NULL)
    goto out;

  uri = gdata_entry_get_content_uri (entry);
  remote_file = g_file_new_for_uri (uri);
  cache_dir = g_get_user_cache_dir ();

  local_dir = g_build_filename (cache_dir, PACKAGE_TARNAME, "google", NULL);
  g_mkdir_with_parents (local_dir, 0700);

  local_filename = g_file_get_basename (remote_file);
  local_path = g_build_filename (local_dir, local_filename, NULL);
  local_file = g_file_new_for_path (local_path);

  if (!g_file_test (local_path, G_FILE_TEST_EXISTS))
    {
      g_debug ("Downloading %s from Google to %s", uri, local_path);
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
  g_clear_object (&local_file);
  g_clear_object (&remote_file);
  g_clear_object (&entry);
  return ret_val;
}


static GtkWidget *
photos_google_item_get_source_widget (PhotosBaseItem *item)
{
  PhotosGoogleItem *self = PHOTOS_GOOGLE_ITEM (item);
  GtkWidget *source_widget;
  const gchar *name;

  name = photos_utils_get_provider_name (self->priv->src_mngr, item);
  source_widget = gtk_link_button_new_with_label ("https://picasaweb.google.com/", name);
  gtk_widget_set_halign (source_widget, GTK_ALIGN_START);

  return source_widget;
}


/* NOTE: For private photos, opening the URI in a browser results in a
 * 'Sorry, that page was not found.' if the user is not logged in with the respective account
 */
static void
photos_google_item_open (PhotosBaseItem *item, GdkScreen *screen, guint32 timestamp)
{
  GError *error;
  const gchar *google_uri;

  google_uri = photos_base_item_get_uri (item);

  error = NULL;
  gtk_show_uri (screen, google_uri, timestamp, &error);
  if (error != NULL)
    {
      g_warning ("Unable to show URI %s: %s", google_uri, error->message);
      g_error_free (error);
    }
}


static void
photos_google_item_constructed (GObject *object)
{
  PhotosGoogleItem *self = PHOTOS_GOOGLE_ITEM (object);
  const gchar *name;

  G_OBJECT_CLASS (photos_google_item_parent_class)->constructed (object);

  name = photos_utils_get_provider_name (self->priv->src_mngr, PHOTOS_BASE_ITEM (self));
  photos_base_item_set_default_app_name (PHOTOS_BASE_ITEM (self), name);
}


static void
photos_google_item_dispose (GObject *object)
{
  PhotosGoogleItem *self = PHOTOS_GOOGLE_ITEM (object);

  g_clear_object (&self->priv->src_mngr);

  G_OBJECT_CLASS (photos_google_item_parent_class)->dispose (object);
}


static void
photos_google_item_init (PhotosGoogleItem *self)
{
  PhotosGoogleItemPrivate *priv;
  GApplication *app;
  PhotosSearchContextState *state;

  self->priv = photos_google_item_get_instance_private (self);
  priv = self->priv;

  app = g_application_get_default ();
  state = photos_search_context_get_state (PHOTOS_SEARCH_CONTEXT (app));

  priv->src_mngr = g_object_ref (state->src_mngr);
}


static void
photos_google_item_class_init (PhotosGoogleItemClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  PhotosBaseItemClass *base_item_class = PHOTOS_BASE_ITEM_CLASS (class);

  object_class->constructed = photos_google_item_constructed;
  object_class->dispose = photos_google_item_dispose;
  base_item_class->create_thumbnail = photos_google_item_create_thumbnail;
  base_item_class->download = photos_google_item_download;
  base_item_class->get_source_widget = photos_google_item_get_source_widget;
  base_item_class->open = photos_google_item_open;
}
