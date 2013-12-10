/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2012, 2013 Red Hat, Inc.
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

#include "photos-source.h"
#include "photos-source-manager.h"


struct _PhotosSourceManagerPrivate
{
  GoaClient *client;
};


G_DEFINE_TYPE_WITH_PRIVATE (PhotosSourceManager, photos_source_manager, PHOTOS_TYPE_BASE_MANAGER);


static void
photos_source_manager_refresh_accounts (PhotosSourceManager *self)
{
  PhotosSourceManagerPrivate *priv = self->priv;
  GHashTable *new_sources;
  GList *accounts;
  GList *l;

  accounts = goa_client_get_accounts (priv->client);
  new_sources = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);

  for (l = accounts; l != NULL; l = l->next)
    {
      GoaAccount *account;
      GoaObject *object = GOA_OBJECT (l->data);
      PhotosSource *source;
      gchar *id;

      account = goa_object_peek_account (object);
      if (account == NULL)
        continue;

      if (goa_account_get_photos_disabled (account))
        continue;

      if (goa_object_peek_photos (object) == NULL)
        continue;

      source = photos_source_new_from_goa_object (GOA_OBJECT (l->data));
      g_object_get (source, "id", &id, NULL);
      g_hash_table_insert (new_sources, id, g_object_ref (source));
      g_object_unref (source);
    }

  photos_base_manager_process_new_objects (PHOTOS_BASE_MANAGER (self), new_sources);

  g_hash_table_unref (new_sources);
  g_list_free_full (accounts, g_object_unref);
}


static GObject *
photos_source_manager_constructor (GType                  type,
                                   guint                  n_construct_params,
                                   GObjectConstructParam *construct_params)
{
  static GObject *self = NULL;

  if (self == NULL)
    {
      self = G_OBJECT_CLASS (photos_source_manager_parent_class)->constructor (type,
                                                                               n_construct_params,
                                                                               construct_params);
      g_object_add_weak_pointer (self, (gpointer) &self);
      return self;
    }

  return g_object_ref (self);
}


static void
photos_source_manager_dispose (GObject *object)
{
  PhotosSourceManager *self = PHOTOS_SOURCE_MANAGER (object);

  g_clear_object (&self->priv->client);

  G_OBJECT_CLASS (photos_source_manager_parent_class)->dispose (object);
}


static void
photos_source_manager_init (PhotosSourceManager *self)
{
  PhotosSourceManagerPrivate *priv = self->priv;
  PhotosSource *source;

  self->priv = photos_source_manager_get_instance_private (self);
  priv = self->priv;

  source = photos_source_new (PHOTOS_SOURCE_STOCK_ALL, _("All"), TRUE);
  photos_base_manager_add_object (PHOTOS_BASE_MANAGER (self), G_OBJECT (source));
  g_object_unref (source);

  source = photos_source_new (PHOTOS_SOURCE_STOCK_LOCAL, _("Local"), TRUE);
  photos_base_manager_add_object (PHOTOS_BASE_MANAGER (self), G_OBJECT (source));
  g_object_unref (source);

  priv->client = goa_client_new_sync (NULL, NULL); /* TODO: use GError */
  if (priv->client != NULL)
    {
      g_signal_connect_swapped (priv->client,
                                "account-added",
                                G_CALLBACK (photos_source_manager_refresh_accounts),
                                self);
      g_signal_connect_swapped (priv->client,
                                "account-changed",
                                G_CALLBACK (photos_source_manager_refresh_accounts),
                                self);
      g_signal_connect_swapped (priv->client,
                                "account-removed",
                                G_CALLBACK (photos_source_manager_refresh_accounts),
                                self);
    }

  photos_source_manager_refresh_accounts (self);
  photos_base_manager_set_active_object_by_id (PHOTOS_BASE_MANAGER (self), PHOTOS_SOURCE_STOCK_ALL);
}


static void
photos_source_manager_class_init (PhotosSourceManagerClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->constructor = photos_source_manager_constructor;
  object_class->dispose = photos_source_manager_dispose;
}


PhotosBaseManager *
photos_source_manager_dup_singleton (void)
{
  return g_object_new (PHOTOS_TYPE_SOURCE_MANAGER, NULL);
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
