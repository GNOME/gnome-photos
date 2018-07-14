/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2013 – 2017 Red Hat, Inc.
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
#include <glib/gi18n.h>
#include <goa/goa.h>
#include <grilo.h>

#include "photos-base-manager.h"
#include "photos-debug.h"
#include "photos-error.h"
#include "photos-flickr-item.h"
#include "photos-search-context.h"
#include "photos-source.h"
#include "photos-utils.h"


struct _PhotosFlickrItem
{
  PhotosBaseItem parent_instance;
  PhotosBaseManager *src_mngr;
};


G_DEFINE_TYPE_WITH_CODE (PhotosFlickrItem, photos_flickr_item, PHOTOS_TYPE_BASE_ITEM,
                         photos_utils_ensure_extension_points ();
                         g_io_extension_point_implement (PHOTOS_BASE_ITEM_EXTENSION_POINT_NAME,
                                                         g_define_type_id,
                                                         "flickr",
                                                         0));


typedef struct _PhotosFlickrItemSyncData PhotosFlickrItemSyncData;

struct _PhotosFlickrItemSyncData
{
  GError **error;
  GMainLoop *loop;
  gboolean op_res;
};


static gchar *
photos_flickr_item_create_filename_fallback (PhotosBaseItem *item)
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
photos_flickr_item_create_name_fallback (PhotosBaseItem *item)
{
  PhotosFlickrItem *self = PHOTOS_FLICKR_ITEM (item);
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


static GrlOperationOptions *
photos_flickr_item_get_grl_options (GrlSource *source)
{
  GrlCaps *caps;
  GrlOperationOptions *options;

  caps = grl_source_get_caps (source, GRL_OP_RESOLVE);
  options = grl_operation_options_new (caps);
  return options;
}


static void
photos_flickr_item_source_resolve (GrlSource *source,
                                   guint operation_id,
                                   GrlMedia *media,
                                   gpointer user_data,
                                   const GError *error)
{
  PhotosFlickrItemSyncData *data = (PhotosFlickrItemSyncData *) user_data;

  if (error != NULL)
    {
      if (data->error != NULL)
        *(data->error) = g_error_copy (error);
      data->op_res = FALSE;
    }
  else
    data->op_res = TRUE;

  g_main_loop_quit (data->loop);
}


static gboolean
photos_flickr_item_create_thumbnail (PhotosBaseItem *item, GCancellable *cancellable, GError **error)
{
  PhotosFlickrItemSyncData data;
  g_autoptr (GFile) local_file = NULL;
  g_autoptr (GFile) remote_file = NULL;
  g_autoptr (GList) keys = NULL;
  g_autoptr (GMainContext) context = NULL;
  g_autoptr (GrlMedia) media = NULL;
  g_autoptr (GrlOperationOptions) options = NULL;
  GrlRegistry *registry;
  GrlSource *source;
  gboolean ret_val = FALSE;
  const gchar *const flickr_prefix = "flickr:";
  const gchar *const resource_prefix = "gd:goa-account:";
  const gchar *flickr_id;
  const gchar *goa_id;
  const gchar *identifier;
  const gchar *resource_urn;
  const gchar *thumbnail_uri;
  const gchar *uri;
  g_autofree gchar *grilo_id = NULL;
  g_autofree gchar *local_dir = NULL;
  g_autofree gchar *local_path = NULL;
  gint64 height;
  gint64 width;
  gsize prefix_len;

  data.error = error;
  data.loop = NULL;

  prefix_len = strlen (flickr_prefix);
  identifier = photos_base_item_get_identifier (item);
  if (strlen (identifier) <= prefix_len || !g_str_has_prefix (identifier, flickr_prefix))
    {
      g_set_error (error,
                   PHOTOS_ERROR,
                   0,
                   "Invalid nao:identifier for Flickr item %s",
                   identifier);
      goto out;
    }
  flickr_id = identifier + prefix_len;

  prefix_len = strlen (resource_prefix);
  resource_urn = photos_base_item_get_resource_urn (item);
  if (strlen (resource_urn) <= prefix_len || !g_str_has_prefix (resource_urn, resource_prefix))
    {
      g_set_error (error,
                   PHOTOS_ERROR,
                   0,
                   "Invalid nie:dataSource for Flickr item %s",
                   resource_urn);
      goto out;
    }
  goa_id = resource_urn + prefix_len;

  grilo_id = g_strdup_printf ("grl-flickr-%s", goa_id);
  registry = grl_registry_get_default ();
  source = grl_registry_lookup_source (registry, grilo_id);
  if (source == NULL)
    {
      g_set_error (error,
                   PHOTOS_ERROR,
                   0,
                   "Failed to find a GrlSource for %s",
                   grilo_id);
      goto out;
    }

  media = grl_media_new ();
  grl_media_set_id (media, flickr_id);

  keys = grl_metadata_key_list_new (GRL_METADATA_KEY_THUMBNAIL, GRL_METADATA_KEY_INVALID);
  options = photos_flickr_item_get_grl_options (source);

  context = g_main_context_new ();
  g_main_context_push_thread_default (context);
  data.loop = g_main_loop_new (context, FALSE);

  grl_source_resolve (source, media, keys, options, photos_flickr_item_source_resolve, &data);
  g_main_loop_run (data.loop);
  g_main_context_pop_thread_default (context);

  if (!data.op_res)
    goto out;

  thumbnail_uri = grl_media_get_thumbnail (media);
  remote_file = g_file_new_for_uri (thumbnail_uri);

  local_path = photos_base_item_create_thumbnail_path (item);
  local_file = g_file_new_for_path (local_path);
  local_dir = g_path_get_dirname (local_path);
  g_mkdir_with_parents (local_dir, 0700);

  uri = photos_base_item_get_uri (item);

  height = photos_base_item_get_height (item);
  width = photos_base_item_get_width (item);

  photos_debug (PHOTOS_DEBUG_NETWORK, "Downloading %s from Flickr to %s", thumbnail_uri, local_path);
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
  if (data.loop != NULL)
    g_main_loop_unref (data.loop);
  return ret_val;
}


static gchar *
photos_flickr_item_download (PhotosBaseItem *item, GCancellable *cancellable, GError **error)
{
  g_autoptr (GFile) local_file = NULL;
  g_autoptr (GFile) remote_file = NULL;
  const gchar *cache_dir;
  const gchar *local_filename;
  const gchar *uri;
  g_autofree gchar *local_dir = NULL;
  g_autofree gchar *local_path = NULL;
  gchar *ret_val = NULL;

  uri = photos_base_item_get_uri (item);
  remote_file = g_file_new_for_uri (uri);
  cache_dir = g_get_user_cache_dir ();

  local_dir = g_build_filename (cache_dir, PACKAGE_TARNAME, "flickr", NULL);
  g_mkdir_with_parents (local_dir, 0700);

  local_filename = photos_base_item_get_filename (item);
  local_path = g_build_filename (local_dir, local_filename, NULL);
  local_file = g_file_new_for_path (local_path);

  if (!g_file_test (local_path, G_FILE_TEST_EXISTS))
    {
      photos_debug (PHOTOS_DEBUG_NETWORK, "Downloading %s from Flickr to %s", uri, local_path);
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

  ret_val = g_steal_pointer (&local_path);

 out:
  return ret_val;
}


static GtkWidget *
photos_flickr_item_get_source_widget (PhotosBaseItem *item)
{
  PhotosFlickrItem *self = PHOTOS_FLICKR_ITEM (item);
  GtkWidget *source_widget;
  const gchar *name;

  name = photos_utils_get_provider_name (self->src_mngr, item);
  source_widget = gtk_link_button_new_with_label ("https://www.flickr.com/", name);
  gtk_widget_set_halign (source_widget, GTK_ALIGN_START);

  return source_widget;
}


static void
photos_flickr_item_open (PhotosBaseItem *item, GtkWindow *parent, guint32 timestamp)
{
  PhotosFlickrItem *self = PHOTOS_FLICKR_ITEM (item);
  GoaAccount *account;
  GoaObject *object;
  PhotosSource *source;
  const gchar *identifier;
  const gchar *identity;
  const gchar *resource_urn;
  g_autofree gchar *flickr_uri = NULL;

  identifier = photos_base_item_get_identifier (item) + strlen ("flickr:");

  resource_urn = photos_base_item_get_resource_urn (item);
  source = PHOTOS_SOURCE (photos_base_manager_get_object_by_id (self->src_mngr, resource_urn));
  object = photos_source_get_goa_object (source);
  account = goa_object_peek_account (object);
  identity = goa_account_get_identity (account);

  flickr_uri = g_strdup_printf ("https://www.flickr.com/photos/%s/%s", identity, identifier);

  {
    g_autoptr (GError) error = NULL;

    if (!gtk_show_uri_on_window (parent, flickr_uri, timestamp, &error))
      g_warning ("Unable to show URI %s: %s", flickr_uri, error->message);
  }
}


static void
photos_flickr_item_constructed (GObject *object)
{
  PhotosFlickrItem *self = PHOTOS_FLICKR_ITEM (object);
  const gchar *name;

  G_OBJECT_CLASS (photos_flickr_item_parent_class)->constructed (object);

  name = photos_utils_get_provider_name (self->src_mngr, PHOTOS_BASE_ITEM (self));
  photos_base_item_set_default_app_name (PHOTOS_BASE_ITEM (self), name);
}


static void
photos_flickr_item_dispose (GObject *object)
{
  PhotosFlickrItem *self = PHOTOS_FLICKR_ITEM (object);

  g_clear_object (&self->src_mngr);

  G_OBJECT_CLASS (photos_flickr_item_parent_class)->dispose (object);
}


static void
photos_flickr_item_init (PhotosFlickrItem *self)
{
  GApplication *app;
  PhotosSearchContextState *state;

  app = g_application_get_default ();
  state = photos_search_context_get_state (PHOTOS_SEARCH_CONTEXT (app));

  self->src_mngr = g_object_ref (state->src_mngr);
}


static void
photos_flickr_item_class_init (PhotosFlickrItemClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  PhotosBaseItemClass *base_item_class = PHOTOS_BASE_ITEM_CLASS (class);

  base_item_class->miner_name = "org.gnome.OnlineMiners.Flickr";
  base_item_class->miner_object_path = "/org/gnome/OnlineMiners/Flickr";

  object_class->constructed = photos_flickr_item_constructed;
  object_class->dispose = photos_flickr_item_dispose;
  base_item_class->create_filename_fallback = photos_flickr_item_create_filename_fallback;
  base_item_class->create_name_fallback = photos_flickr_item_create_name_fallback;
  base_item_class->create_thumbnail = photos_flickr_item_create_thumbnail;
  base_item_class->download = photos_flickr_item_download;
  base_item_class->get_source_widget = photos_flickr_item_get_source_widget;
  base_item_class->open = photos_flickr_item_open;
}
