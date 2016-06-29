/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2016 Red Hat, Inc.
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

#include <glib.h>
#include <gio/gio.h>

#include "photos-filterable.h"
#include "photos-search-context.h"
#include "photos-share-point.h"
#include "photos-share-point-manager.h"
#include "photos-source.h"
#include "photos-utils.h"


struct _PhotosSharePointManager
{
  PhotosBaseManager parent_instance;
  GAppInfoMonitor *app_info_monitor;
  GIOExtensionPoint *extension_point;
  GIOExtensionPoint *extension_point_online;
  PhotosBaseManager *src_mngr;
};

struct _PhotosSharePointManagerClass
{
  PhotosBaseManagerClass parent_class;
};


G_DEFINE_TYPE (PhotosSharePointManager, photos_share_point_manager, PHOTOS_TYPE_BASE_MANAGER);


static PhotosSharePoint *
photos_share_point_manager_create_share_point_online (PhotosSharePointManager *self, PhotosSource *source)
{
  GIOExtension *extension;
  GType type;
  GoaAccount *account;
  GoaObject *object;
  PhotosSharePoint *ret_val = NULL;
  const gchar *provider_type;

  object = photos_source_get_goa_object (source);
  if (object == NULL)
    goto out;

  account = goa_object_peek_account (object);
  provider_type = goa_account_get_provider_type (account);
  extension = g_io_extension_point_get_extension_by_name (self->extension_point_online, provider_type);
  if (extension == NULL)
    goto out;

  type = g_io_extension_get_type (extension);
  ret_val = PHOTOS_SHARE_POINT (g_object_new (type, "source", source, NULL));

 out:
  return ret_val;
}


static void
photos_share_point_manager_refresh_share_points (PhotosSharePointManager *self)
{
  GHashTable *new_share_points;
  GHashTable *sources;
  GHashTableIter iter;
  GList *extensions;
  GList *l;
  PhotosSource *source;

  new_share_points = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);

  extensions = g_io_extension_point_get_extensions (self->extension_point);
  for (l = extensions; l != NULL; l = l->next)
    {
      GError *error;
      GIOExtension *extension = (GIOExtension *) l->data;
      GType type;
      PhotosSharePoint *share_point;
      const gchar *id;

      type = g_io_extension_get_type (extension);
      if (g_type_is_a (type, G_TYPE_INITABLE))
        {
          error = NULL;
          share_point = PHOTOS_SHARE_POINT (g_initable_new (type, NULL, &error, NULL));
          if (share_point == NULL)
            {
              const gchar *name;

              name = g_io_extension_get_name (extension);
              g_debug ("Unable to initialize share point %s: %s", name, error->message);
              g_error_free (error);
              continue;
            }
        }
      else
        {
          share_point = PHOTOS_SHARE_POINT (g_object_new (type, NULL));
        }

      id = photos_filterable_get_id (PHOTOS_FILTERABLE (share_point));
      g_hash_table_insert (new_share_points, g_strdup (id), g_object_ref (share_point));
      g_object_unref (share_point);
    }

  sources = photos_base_manager_get_objects (self->src_mngr);

  g_hash_table_iter_init (&iter, sources);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &source))
    {
      PhotosSharePoint *share_point;
      const gchar *id;

      share_point = photos_share_point_manager_create_share_point_online (self, source);
      if (share_point == NULL)
        continue;

      id = photos_filterable_get_id (PHOTOS_FILTERABLE (share_point));
      g_hash_table_insert (new_share_points, g_strdup (id), g_object_ref (share_point));
      g_object_unref (share_point);
    }

  photos_base_manager_process_new_objects (PHOTOS_BASE_MANAGER (self), new_share_points);
  g_hash_table_unref (new_share_points);
}


static GObject *
photos_share_point_manager_constructor (GType type,
                                        guint n_construct_params,
                                        GObjectConstructParam *construct_params)
{
  static GObject *self = NULL;

  if (self == NULL)
    {
      self = G_OBJECT_CLASS (photos_share_point_manager_parent_class)->constructor (type,
                                                                                    n_construct_params,
                                                                                    construct_params);
      g_object_add_weak_pointer (self, (gpointer) &self);
      return self;
    }

  return g_object_ref (self);
}


static void
photos_share_point_manager_dispose (GObject *object)
{
  PhotosSharePointManager *self = PHOTOS_SHARE_POINT_MANAGER (object);

  g_clear_object (&self->app_info_monitor);
  g_clear_object (&self->src_mngr);

  G_OBJECT_CLASS (photos_share_point_manager_parent_class)->dispose (object);
}


static void
photos_share_point_manager_init (PhotosSharePointManager *self)
{
  GApplication *app;
  PhotosSearchContextState *state;

  app = g_application_get_default ();
  state = photos_search_context_get_state (PHOTOS_SEARCH_CONTEXT (app));

  self->app_info_monitor = g_app_info_monitor_get ();
  g_signal_connect_object (self->app_info_monitor,
                           "changed",
                           G_CALLBACK (photos_share_point_manager_refresh_share_points),
                           self,
                           G_CONNECT_SWAPPED);

  self->extension_point = g_io_extension_point_lookup (PHOTOS_SHARE_POINT_EXTENSION_POINT_NAME);
  self->extension_point_online = g_io_extension_point_lookup (PHOTOS_SHARE_POINT_ONLINE_EXTENSION_POINT_NAME);

  self->src_mngr = g_object_ref (state->src_mngr);
  g_signal_connect_object (self->src_mngr,
                           "object-added",
                           G_CALLBACK (photos_share_point_manager_refresh_share_points),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (self->src_mngr,
                           "object-removed",
                           G_CALLBACK (photos_share_point_manager_refresh_share_points),
                           self,
                           G_CONNECT_SWAPPED);

  photos_share_point_manager_refresh_share_points (self);
}


static void
photos_share_point_manager_class_init (PhotosSharePointManagerClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->constructor = photos_share_point_manager_constructor;
  object_class->dispose = photos_share_point_manager_dispose;
}


PhotosBaseManager *
photos_share_point_manager_dup_singleton (void)
{
  return g_object_new (PHOTOS_TYPE_SHARE_POINT_MANAGER, NULL);
}


gboolean
photos_share_point_manager_can_share (PhotosSharePointManager *self, PhotosBaseItem *item)
{
  GList *share_points = NULL;
  gboolean ret_val;

  g_return_val_if_fail (PHOTOS_IS_SHARE_POINT_MANAGER (self), FALSE);
  g_return_val_if_fail (PHOTOS_IS_BASE_ITEM (item), FALSE);

  share_points = photos_share_point_manager_get_for_item (self, item);
  ret_val = share_points != NULL;

  g_list_free_full (share_points, g_object_unref);
  return ret_val;
}


GList *
photos_share_point_manager_get_for_item (PhotosSharePointManager *self, PhotosBaseItem *item)
{
  GHashTable *share_points;
  GHashTableIter iter;
  GList *ret_val = NULL;
  PhotosSharePoint *share_point;
  const gchar *resource_urn;
  const gchar *share_point_id;

  g_return_val_if_fail (PHOTOS_IS_SHARE_POINT_MANAGER (self), FALSE);
  g_return_val_if_fail (PHOTOS_IS_BASE_ITEM (item), FALSE);

  if (photos_base_item_is_collection (item))
    goto out;

  resource_urn = photos_base_item_get_resource_urn (item);
  share_points = photos_base_manager_get_objects (PHOTOS_BASE_MANAGER (self));

  g_hash_table_iter_init (&iter, share_points);
  while (g_hash_table_iter_next (&iter, (gpointer *) &share_point_id, (gpointer *) &share_point))
    {
      if (g_strcmp0 (resource_urn, share_point_id) == 0)
        continue;

      ret_val = g_list_prepend (ret_val, g_object_ref (share_point));
    }

 out:
  return ret_val;
}
