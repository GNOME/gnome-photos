/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2013, 2014 Red Hat, Inc.
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
#include <glib/gi18n.h>
#include <goa/goa.h>
#include <grilo.h>
#include <libgnome-desktop/gnome-desktop-thumbnail.h>

#include "photos-base-manager.h"
#include "photos-flickr-item.h"
#include "photos-search-context.h"
#include "photos-source.h"
#include "photos-utils.h"


struct _PhotosFlickrItemPrivate
{
  PhotosBaseManager *src_mngr;
};


G_DEFINE_TYPE_WITH_PRIVATE (PhotosFlickrItem, photos_flickr_item, PHOTOS_TYPE_BASE_ITEM);


typedef struct _PhotosFlickrItemSyncData PhotosFlickrItemSyncData;

struct _PhotosFlickrItemSyncData
{
  GError **error;
  GMainLoop *loop;
  gboolean op_res;
};


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
  GFile *local_file = NULL;
  GFile *remote_file = NULL;
  GList *keys = NULL;
  GMainContext *context = NULL;
  GrlMedia *media = NULL;
  GrlOperationOptions *options = NULL;
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
  gchar *grilo_id = NULL;
  gchar *local_dir = NULL;
  gchar *local_path = NULL;
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

  uri = photos_base_item_get_uri (item);
  local_path = gnome_desktop_thumbnail_path_for_uri (uri, GNOME_DESKTOP_THUMBNAIL_SIZE_LARGE);
  local_file = g_file_new_for_path (local_path);

  local_dir = g_path_get_dirname (local_path);
  g_mkdir_with_parents (local_dir, 0700);

  g_debug ("Downloading %s from Flickr to %s", uri, local_path);
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
  g_free (grilo_id);
  g_free (local_dir);
  g_free (local_path);
  g_list_free (keys);
  g_clear_object (&local_file);
  g_clear_object (&remote_file);
  g_clear_object (&options);
  g_clear_object (&media);
  if (context != NULL)
    g_main_context_unref (context);
  if (data.loop != NULL)
    g_main_loop_unref (data.loop);
  return ret_val;
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

  identifier = photos_base_item_get_identifier (item) + strlen ("flickr:");

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
  GApplication *app;
  PhotosSearchContextState *state;

  self->priv = photos_flickr_item_get_instance_private (self);
  priv = self->priv;

  app = g_application_get_default ();
  state = photos_search_context_get_state (PHOTOS_SEARCH_CONTEXT (app));

  priv->src_mngr = g_object_ref (state->src_mngr);
}


static void
photos_flickr_item_class_init (PhotosFlickrItemClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  PhotosBaseItemClass *base_item_class = PHOTOS_BASE_ITEM_CLASS (class);

  object_class->constructed = photos_flickr_item_constructed;
  object_class->dispose = photos_flickr_item_dispose;
  base_item_class->create_thumbnail = photos_flickr_item_create_thumbnail;
  base_item_class->download = photos_flickr_item_download;
  base_item_class->get_source_name = photos_flickr_item_get_source_name;
  base_item_class->open = photos_flickr_item_open;
}


PhotosBaseItem *
photos_flickr_item_new (TrackerSparqlCursor *cursor)
{
  return g_object_new (PHOTOS_TYPE_FLICKR_ITEM,
                       "cursor", cursor,
                       "failed-thumbnailing", FALSE,
                       NULL);
}
