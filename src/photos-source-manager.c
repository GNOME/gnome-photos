/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2012, 2013, 2014, 2015, 2016 Red Hat, Inc.
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
#include <glib/gi18n.h>
#include <goa/goa.h>

#include "photos-filterable.h"
#include "photos-query.h"
#include "photos-source.h"
#include "photos-source-manager.h"


struct _PhotosSourceManager
{
  PhotosBaseManager parent_instance;
  GCancellable *cancellable;
  GoaClient *client;
};

struct _PhotosSourceManagerClass
{
  PhotosBaseManagerClass parent_class;
};


G_DEFINE_TYPE (PhotosSourceManager, photos_source_manager, PHOTOS_TYPE_BASE_MANAGER);


static gchar *
photos_source_manager_get_filter (PhotosBaseManager *mngr, gint flags)
{
  GObject *source;
  const gchar *id;
  gchar *filter;

  if (flags & PHOTOS_QUERY_FLAGS_SEARCH)
    source = photos_base_manager_get_active_object (mngr);
  else
    source = photos_base_manager_get_object_by_id (mngr, PHOTOS_SOURCE_STOCK_ALL);

  id = photos_filterable_get_id (PHOTOS_FILTERABLE (source));
  if (g_strcmp0 (id, PHOTOS_SOURCE_STOCK_ALL) == 0)
    filter = photos_base_manager_get_all_filter (mngr);
  else
    filter = photos_filterable_get_filter (PHOTOS_FILTERABLE (source));

  return filter;
}


static void
photos_source_manager_refresh_accounts (PhotosSourceManager *self)
{
  GHashTable *new_sources;
  GList *accounts;
  GList *l;

  accounts = goa_client_get_accounts (self->client);
  new_sources = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);

  for (l = accounts; l != NULL; l = l->next)
    {
      GoaAccount *account;
      GoaObject *object = GOA_OBJECT (l->data);
      PhotosSource *source;
      const gchar *id;

      account = goa_object_peek_account (object);
      if (account == NULL)
        continue;

      if (goa_account_get_photos_disabled (account))
        continue;

      if (goa_object_peek_photos (object) == NULL)
        continue;

      source = photos_source_new_from_goa_object (GOA_OBJECT (l->data));
      id = photos_filterable_get_id (PHOTOS_FILTERABLE (source));
      g_hash_table_insert (new_sources, g_strdup (id), g_object_ref (source));
      g_object_unref (source);
    }

  photos_base_manager_process_new_objects (PHOTOS_BASE_MANAGER (self), new_sources);

  g_hash_table_unref (new_sources);
  g_list_free_full (accounts, g_object_unref);
}


static void
photos_source_manager_goa_client (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosSourceManager *self;
  GError *error;
  GoaClient *client;

  error = NULL;
  client = goa_client_new_finish (res, &error);
  if (error != NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Unable to create GoaClient: %s", error->message);

      g_error_free (error);
      return;
    }

  self = PHOTOS_SOURCE_MANAGER (user_data);

  self->client = client;
  g_signal_connect_swapped (self->client,
                            "account-added",
                            G_CALLBACK (photos_source_manager_refresh_accounts),
                            self);
  g_signal_connect_swapped (self->client,
                            "account-changed",
                            G_CALLBACK (photos_source_manager_refresh_accounts),
                            self);
  g_signal_connect_swapped (self->client,
                            "account-removed",
                            G_CALLBACK (photos_source_manager_refresh_accounts),
                            self);

  photos_source_manager_refresh_accounts (self);
}


static void
photos_source_manager_dispose (GObject *object)
{
  PhotosSourceManager *self = PHOTOS_SOURCE_MANAGER (object);

  if (self->cancellable != NULL)
    g_cancellable_cancel (self->cancellable);

  g_clear_object (&self->cancellable);
  g_clear_object (&self->client);

  G_OBJECT_CLASS (photos_source_manager_parent_class)->dispose (object);
}


static void
photos_source_manager_init (PhotosSourceManager *self)
{
  PhotosSource *source;

  source = photos_source_new (PHOTOS_SOURCE_STOCK_ALL, _("All"), TRUE);
  photos_base_manager_add_object (PHOTOS_BASE_MANAGER (self), G_OBJECT (source));
  g_object_unref (source);

  source = photos_source_new (PHOTOS_SOURCE_STOCK_LOCAL, _("Local"), TRUE);
  photos_base_manager_add_object (PHOTOS_BASE_MANAGER (self), G_OBJECT (source));
  g_object_unref (source);

  self->cancellable = g_cancellable_new ();
  goa_client_new (self->cancellable, photos_source_manager_goa_client, self);

  photos_base_manager_set_active_object_by_id (PHOTOS_BASE_MANAGER (self), PHOTOS_SOURCE_STOCK_ALL);
}


static void
photos_source_manager_class_init (PhotosSourceManagerClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  PhotosBaseManagerClass *base_manager_class = PHOTOS_BASE_MANAGER_CLASS (class);

  object_class->dispose = photos_source_manager_dispose;
  base_manager_class->get_filter = photos_source_manager_get_filter;
}


PhotosBaseManager *
photos_source_manager_new (void)
{
  return g_object_new (PHOTOS_TYPE_SOURCE_MANAGER, "action-id", "search-source", "title", _("Sources"), NULL);
}


GList *
photos_source_manager_get_for_provider_type (PhotosSourceManager *self, const gchar *provider_type)
{
  GHashTable *sources;
  GHashTableIter iter;
  GList *items = NULL;
  PhotosSource *source;

  sources = photos_base_manager_get_objects (PHOTOS_BASE_MANAGER (self));
  g_hash_table_iter_init (&iter, sources);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &source))
    {
      GoaAccount *account;
      GoaObject *object;

      object = photos_source_get_goa_object (source);
      if (object == NULL)
        continue;

      account = goa_object_peek_account (object);
      if (g_strcmp0 (goa_account_get_provider_type (account), provider_type) == 0)
        items = g_list_prepend (items, g_object_ref (source));
    }

  return items;
}


gboolean
photos_source_manager_has_online_sources (PhotosSourceManager *self)
{
  GHashTable *sources;
  GHashTableIter iter;
  PhotosSource *source;
  gboolean ret_val = FALSE;

  sources = photos_base_manager_get_objects (PHOTOS_BASE_MANAGER (self));
  g_hash_table_iter_init (&iter, sources);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &source))
    {
      GoaObject *object;

      object = photos_source_get_goa_object (source);
      if (object != NULL)
        {
          ret_val = TRUE;
          break;
        }
    }

  return ret_val;
}


gboolean
photos_source_manager_has_provider_type (PhotosSourceManager *self, const gchar *provider_type)
{
  GList *items;
  gboolean ret_val = FALSE;

  items = photos_source_manager_get_for_provider_type (self, provider_type);
  if (items != NULL)
    ret_val = TRUE;

  g_list_free_full (items, g_object_unref);
  return ret_val;
}
