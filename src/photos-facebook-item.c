/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2013 Álvaro Peña
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

#include <gfbgraph/gfbgraph.h>
#include <gfbgraph/gfbgraph-goa-authorizer.h>
#include <gio/gio.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <goa/goa.h>

#include "photos-base-manager.h"
#include "photos-debug.h"
#include "photos-error.h"
#include "photos-facebook-item.h"
#include "photos-search-context.h"
#include "photos-source.h"
#include "photos-utils.h"


struct _PhotosFacebookItem
{
  PhotosBaseItem parent_instance;
  PhotosBaseManager *src_mngr;
};


G_DEFINE_TYPE_WITH_CODE (PhotosFacebookItem, photos_facebook_item, PHOTOS_TYPE_BASE_ITEM,
                         photos_utils_ensure_extension_points ();
                         g_io_extension_point_implement (PHOTOS_BASE_ITEM_EXTENSION_POINT_NAME,
                                                         g_define_type_id,
                                                         "facebook",
                                                         0));


static gchar *
photos_facebook_item_create_filename_fallback (PhotosBaseItem *item)
{
  const gchar *const facebook_prefix = "facebook:";
  const gchar *identifier;
  const gchar *mime_type;
  g_autofree gchar *extension = NULL;
  gchar *ret_val;
  gsize prefix_len;

  prefix_len = strlen (facebook_prefix);
  identifier = photos_base_item_get_identifier (item) + prefix_len;
  mime_type = photos_base_item_get_mime_type (item);
  extension = photos_utils_get_extension_from_mime_type (mime_type);
  if (extension == NULL)
    extension = g_strdup ("tmp");

  ret_val = g_strdup_printf ("%s.%s", identifier, extension);

  return ret_val;
}


static gchar *
photos_facebook_item_create_name_fallback (PhotosBaseItem *item)
{
  PhotosFacebookItem *self = PHOTOS_FACEBOOK_ITEM (item);
  g_autoptr (GDateTime) date_modified = NULL;
  const gchar *provider_name;
  g_autofree gchar *date_modified_str = NULL;
  gchar *ret_val;
  gint64 mtime;

  provider_name = photos_utils_get_provider_name (self->src_mngr, item);

  mtime = photos_base_item_get_mtime (item);
  date_modified = g_date_time_new_from_unix_local (mtime);
  date_modified_str = g_date_time_format (date_modified, "%x");

  /* Translators: this is the fallback title in the form
   * "Facebook — 2nd January 2013".
   */
  ret_val = g_strdup_printf (_("%s — %s"), provider_name, date_modified_str);

  return ret_val;
}


static GFBGraphPhoto *
photos_facebook_get_gfbgraph_photo (PhotosBaseItem *item, GCancellable *cancellable, GError **error)
{
  PhotosFacebookItem *self = PHOTOS_FACEBOOK_ITEM (item);
  GFBGraphGoaAuthorizer *authorizer = NULL; /* TODO: use g_autoptr */
  GFBGraphPhoto *photo = NULL;
  GoaObject *object;
  PhotosSource *source;
  const gchar *const facebook_prefix = "facebook:";
  const gchar *identifier;
  const gchar *resource_urn;
  gsize prefix_len;

  resource_urn = photos_base_item_get_resource_urn (item);
  source = PHOTOS_SOURCE (photos_base_manager_get_object_by_id (self->src_mngr, resource_urn));
  object = photos_source_get_goa_object (source);
  authorizer = gfbgraph_goa_authorizer_new (object);

  if (!gfbgraph_authorizer_refresh_authorization (GFBGRAPH_AUTHORIZER (authorizer), cancellable, error))
    goto out;

  prefix_len = strlen (facebook_prefix);
  identifier = photos_base_item_get_identifier (item) + prefix_len;
  photo = gfbgraph_photo_new_from_id (GFBGRAPH_AUTHORIZER (authorizer), identifier, error);

 out:
  g_clear_object (&authorizer);
  return photo;
}


static gboolean
photos_facebook_item_create_thumbnail (PhotosBaseItem *item, GCancellable *cancellable, GError **error)
{
  g_autoptr (GFile) local_file = NULL;
  g_autoptr (GFile) remote_file = NULL;
  GFBGraphPhoto *photo = NULL; /* TODO: use g_autoptr */
  const GFBGraphPhotoImage *thumbnail_image;
  gboolean ret_val = FALSE;
  const gchar *uri;
  g_autofree gchar *local_dir = NULL;
  g_autofree gchar *local_path = NULL;
  gint64 height;
  gint64 width;
  guint size;

  photo = photos_facebook_get_gfbgraph_photo (item, cancellable, error);
  if (photo == NULL)
    goto out;

  size = (guint) photos_utils_get_icon_size ();
  thumbnail_image = gfbgraph_photo_get_image_near_width (photo, size);
  if (thumbnail_image == NULL)
    {
      g_set_error (error, PHOTOS_ERROR, 0, "Failed to find an image for the thumbnail");
      goto out;
    }

  remote_file = g_file_new_for_uri (thumbnail_image->source);

  local_path = photos_base_item_create_thumbnail_path (item);
  local_file = g_file_new_for_path (local_path);
  local_dir = g_path_get_dirname (local_path);
  g_mkdir_with_parents (local_dir, 0700);

  uri = photos_base_item_get_uri (item);

  height = photos_base_item_get_height (item);
  width = photos_base_item_get_width (item);

  photos_debug (PHOTOS_DEBUG_NETWORK, "Downloading %s from Facebook to %s", thumbnail_image->source, local_path);
  if (!photos_utils_file_copy_as_thumbnail (remote_file,
                                            local_file,
                                            uri,
                                            height,
                                            width,
                                            cancellable,
                                            error))
    goto out;

  ret_val = TRUE;

 out:
  g_clear_object (&photo);
  return ret_val;
}


static gchar *
photos_facebook_item_download (PhotosBaseItem *item, GCancellable *cancellable, GError **error)
{
  g_autoptr (GFile) local_file = NULL;
  g_autoptr (GFile) remote_file = NULL;
  GFBGraphPhoto *photo = NULL; /* TODO: use g_autoptr */
  const GFBGraphPhotoImage *higher_image;
  const gchar *cache_dir;
  const gchar *local_filename;
  g_autofree gchar *local_dir = NULL;
  g_autofree gchar *local_path = NULL;
  gchar *ret_val = NULL;

  cache_dir = g_get_user_cache_dir ();
  local_dir = g_build_filename (cache_dir, PACKAGE_TARNAME, "facebook", NULL);
  g_mkdir_with_parents (local_dir, 0700);

  local_filename = photos_base_item_get_filename (item);
  local_path = g_build_filename (local_dir, local_filename, NULL);
  if (g_file_test (local_path, G_FILE_TEST_EXISTS))
    goto end;

  photo = photos_facebook_get_gfbgraph_photo (item, cancellable, error);
  if (photo == NULL)
    goto out;

  higher_image = gfbgraph_photo_get_image_hires (photo);
  if (higher_image == NULL)
    {
      g_set_error (error, PHOTOS_ERROR, 0, "Failed to find a high resolution image");
      goto out;
    }

  remote_file = g_file_new_for_uri (higher_image->source);
  local_file = g_file_new_for_path (local_path);

  photos_debug (PHOTOS_DEBUG_NETWORK, "Downloading %s from Facebook to %s", higher_image->source, local_path);
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

 end:
  ret_val = g_steal_pointer (&local_path);

 out:
  g_clear_object (&photo);
  return ret_val;
}


static GtkWidget *
photos_facebook_item_get_source_widget (PhotosBaseItem *item)
{
  PhotosFacebookItem *self = PHOTOS_FACEBOOK_ITEM (item);
  GtkWidget *source_widget;
  const gchar *name;

  name = photos_utils_get_provider_name (self->src_mngr, item);
  source_widget = gtk_link_button_new_with_label ("https://www.facebook.com/", name);
  gtk_widget_set_halign (source_widget, GTK_ALIGN_START);

  return source_widget;
}


/* TODO */
static void
photos_facebook_item_open (PhotosBaseItem *item, GtkWindow *parent, guint32 timestamp)
{
  const gchar *facebook_uri;

  facebook_uri = photos_base_item_get_uri (item);

  {
    g_autoptr (GError) error = NULL;

    gtk_show_uri_on_window (parent, facebook_uri, timestamp, &error);
    if (error != NULL)
      g_warning ("Unable to show URI %s: %s", facebook_uri, error->message);
  }
}


static void
photos_facebook_item_constructed (GObject *object)
{
  PhotosFacebookItem *self = PHOTOS_FACEBOOK_ITEM (object);
  const gchar *name;

  G_OBJECT_CLASS (photos_facebook_item_parent_class)->constructed (object);

  name = photos_utils_get_provider_name (self->src_mngr, PHOTOS_BASE_ITEM (self));
  photos_base_item_set_default_app_name (PHOTOS_BASE_ITEM (self), name);
}


static void
photos_facebook_item_dispose (GObject *object)
{
  PhotosFacebookItem *self = PHOTOS_FACEBOOK_ITEM (object);

  g_clear_object (&self->src_mngr);

  G_OBJECT_CLASS (photos_facebook_item_parent_class)->dispose (object);
}


static void
photos_facebook_item_init (PhotosFacebookItem *self)
{
  GApplication *app;
  PhotosSearchContextState *state;

  app = g_application_get_default ();
  state = photos_search_context_get_state (PHOTOS_SEARCH_CONTEXT (app));

  self->src_mngr = g_object_ref (state->src_mngr);
}


static void
photos_facebook_item_class_init (PhotosFacebookItemClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  PhotosBaseItemClass *base_item_class = PHOTOS_BASE_ITEM_CLASS (class);

  base_item_class->miner_name = "org.gnome.OnlineMiners.Facebook";
  base_item_class->miner_object_path = "/org/gnome/OnlineMiners/Facebook";

  object_class->constructed = photos_facebook_item_constructed;
  object_class->dispose = photos_facebook_item_dispose;
  base_item_class->create_filename_fallback = photos_facebook_item_create_filename_fallback;
  base_item_class->create_name_fallback = photos_facebook_item_create_name_fallback;
  base_item_class->create_thumbnail = photos_facebook_item_create_thumbnail;
  base_item_class->download = photos_facebook_item_download;
  base_item_class->get_source_widget = photos_facebook_item_get_source_widget;
  base_item_class->open = photos_facebook_item_open;
}
