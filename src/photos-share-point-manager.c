/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2016 – 2021 Red Hat, Inc.
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
  g_autoptr (GHashTable) new_share_points = NULL;
  GList *extensions;
  GList *l;
  guint i;
  guint n_items;

  new_share_points = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);

  extensions = g_io_extension_point_get_extensions (self->extension_point);
  for (l = extensions; l != NULL; l = l->next)
    {
      GIOExtension *extension = (GIOExtension *) l->data;
      GType type;
      g_autoptr (PhotosSharePoint) share_point = NULL;
      const gchar *id;

      type = g_io_extension_get_type (extension);
      if (g_type_is_a (type, G_TYPE_INITABLE))
        {
          {
            g_autoptr (GError) error = NULL;

            share_point = PHOTOS_SHARE_POINT (g_initable_new (type, NULL, &error, NULL));
            if (share_point == NULL)
              {
                const gchar *name;

                name = g_io_extension_get_name (extension);
                g_debug ("Unable to initialize share point %s: %s", name, error->message);
                continue;
              }
          }
        }
      else
        {
          share_point = PHOTOS_SHARE_POINT (g_object_new (type, NULL));
        }

      id = photos_filterable_get_id (PHOTOS_FILTERABLE (share_point));
      g_hash_table_insert (new_share_points, g_strdup (id), g_object_ref (share_point));
    }

  n_items = g_list_model_get_n_items (G_LIST_MODEL (self->src_mngr));
  for (i = 0; i < n_items; i++)
    {
      g_autoptr (PhotosSharePoint) share_point = NULL;
      g_autoptr (PhotosSource) source = NULL;

      source = PHOTOS_SOURCE (g_list_model_get_object (G_LIST_MODEL (self->src_mngr), i));
      share_point = photos_share_point_manager_create_share_point_online (self, source);
      if (share_point != NULL)
        {
          const gchar *id;

          id = photos_filterable_get_id (PHOTOS_FILTERABLE (share_point));
          g_hash_table_insert (new_share_points, g_strdup (id), g_object_ref (share_point));
        }
    }

  photos_base_manager_process_new_objects (PHOTOS_BASE_MANAGER (self), new_share_points);
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
  g_autolist (PhotosSharePoint) share_points = NULL;
  gboolean ret_val;

  g_return_val_if_fail (PHOTOS_IS_SHARE_POINT_MANAGER (self), FALSE);
  g_return_val_if_fail (PHOTOS_IS_BASE_ITEM (item), FALSE);

  share_points = photos_share_point_manager_get_for_item (self, item);
  ret_val = share_points != NULL;
  return ret_val;
}


GList *
photos_share_point_manager_get_for_item (PhotosSharePointManager *self, PhotosBaseItem *item)
{
  GList *ret_val = NULL;
  const gchar *resource_urn;
  guint i;
  guint n_items;

  g_return_val_if_fail (PHOTOS_IS_SHARE_POINT_MANAGER (self), FALSE);
  g_return_val_if_fail (PHOTOS_IS_BASE_ITEM (item), FALSE);

  if (photos_base_item_is_collection (item))
    goto out;

  resource_urn = photos_base_item_get_resource_urn (item);

  n_items = g_list_model_get_n_items (G_LIST_MODEL (self));
  for (i = 0; i < n_items; i++)
    {
      g_autoptr (PhotosSharePoint) share_point = NULL;
      const gchar *share_point_id;

      share_point = PHOTOS_SHARE_POINT (g_list_model_get_object (G_LIST_MODEL (self), i));
      share_point_id = photos_filterable_get_id (PHOTOS_FILTERABLE (share_point));

      if (g_strcmp0 (resource_urn, share_point_id) != 0)
        ret_val = g_list_prepend (ret_val, g_object_ref (share_point));
    }

 out:
  return ret_val;
}
