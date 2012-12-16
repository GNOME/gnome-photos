/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2012 Red Hat, Inc.
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


G_DEFINE_TYPE (PhotosSourceManager, photos_source_manager, PHOTOS_TYPE_BASE_MANAGER);


static void
photos_source_manager_client_account_added (GoaClient *client, GoaObject *object, gpointer user_data)
{
  PhotosSourceManager *self = PHOTOS_SOURCE_MANAGER (user_data);
  PhotosSource *source;

  source = photos_source_new_from_goa_object (object);
  photos_base_manager_add_object (PHOTOS_BASE_MANAGER (self), G_OBJECT (source));
  g_object_unref (source);
}


static void
photos_source_manager_client_account_removed (GoaClient *client, GoaObject *object, gpointer user_data)
{
  PhotosSourceManager *self = PHOTOS_SOURCE_MANAGER (user_data);
  GoaAccount *account;
  const gchar *id;

  account = goa_object_peek_account (object);
  id = goa_account_get_id (account);
  photos_base_manager_remove_object_by_id (PHOTOS_BASE_MANAGER (self), id);
}


static void
photos_source_manager_client_account_changed (GoaClient *client, GoaObject *object, gpointer user_data)
{
  PhotosSourceManager *self = PHOTOS_SOURCE_MANAGER (user_data);
  photos_source_manager_client_account_removed (client, object, user_data);
  photos_source_manager_client_account_added (client, object, user_data);
}


static void
photos_source_manager_refresh_accounts (PhotosSourceManager *self)
{
  PhotosSourceManagerPrivate *priv = self->priv;
  GList *accounts;
  GList *l;

  accounts = goa_client_get_accounts (priv->client);
  for (l = accounts; l != NULL; l = l->next)
    {
      PhotosSource *source;

      if (goa_object_peek_account (GOA_OBJECT (l->data)) == NULL)
        continue;

      /* TODO: uncomment when we start supporting online providers */
      /* source = photos_source_new_from_goa_object (GOA_OBJECT (l->data)); */
      /* photos_base_manager_add_object (PHOTOS_BASE_MANAGER (self), G_OBJECT (source)); */
      /* g_object_unref (source); */
    }

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

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, PHOTOS_TYPE_SOURCE_MANAGER, PhotosSourceManagerPrivate);
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
      g_signal_connect (priv->client,
                        "account-added",
                        G_CALLBACK (photos_source_manager_client_account_added),
                        self);
      g_signal_connect (priv->client,
                        "account-changed",
                        G_CALLBACK (photos_source_manager_client_account_changed),
                        self);
      g_signal_connect (priv->client,
                        "account-removed",
                        G_CALLBACK (photos_source_manager_client_account_removed),
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

  g_type_class_add_private (class, sizeof (PhotosSourceManagerPrivate));
}


PhotosBaseManager *
photos_source_manager_new (void)
{
  return g_object_new (PHOTOS_TYPE_SOURCE_MANAGER, NULL);
}
