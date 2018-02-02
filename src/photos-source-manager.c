/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2012 – 2017 Red Hat, Inc.
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

#include <gio/gio.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <goa/goa.h>

#include "photos-application.h"
#include "photos-filterable.h"
#include "photos-query.h"
#include "photos-source.h"
#include "photos-source-manager.h"


struct _PhotosSourceManager
{
  PhotosBaseManager parent_instance;
  GCancellable *cancellable;
  GHashTable *sources_notified;
  GoaClient *client;
};

enum
{
  NOTIFICATION_HIDE,
  NOTIFICATION_SHOW,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };


G_DEFINE_TYPE (PhotosSourceManager, photos_source_manager, PHOTOS_TYPE_BASE_MANAGER);


static gchar *
photos_source_manager_get_filter (PhotosBaseManager *mngr, gint flags)
{
  GApplication *app;
  GObject *source;
  const gchar *id;
  gchar *filter;

  app = g_application_get_default ();
  if (photos_application_get_empty_results (PHOTOS_APPLICATION (app)))
    {
      filter = g_strdup ("(false)");
      goto out;
    }

  if (flags & PHOTOS_QUERY_FLAGS_SEARCH)
    source = photos_base_manager_get_active_object (mngr);
  else
    source = photos_base_manager_get_object_by_id (mngr, PHOTOS_SOURCE_STOCK_ALL);

  id = photos_filterable_get_id (PHOTOS_FILTERABLE (source));
  if (g_strcmp0 (id, PHOTOS_SOURCE_STOCK_ALL) == 0)
    filter = photos_base_manager_get_all_filter (mngr);
  else
    filter = photos_filterable_get_filter (PHOTOS_FILTERABLE (source));

 out:
  return filter;
}


static void
photos_source_manager_remove_object_by_id (PhotosBaseManager *mngr, const gchar *id)
{
  PhotosSourceManager *self = PHOTOS_SOURCE_MANAGER (mngr);
  PhotosSource *source;

  source = PHOTOS_SOURCE (g_hash_table_lookup (self->sources_notified, id));
  if (source != NULL)
    {
      gboolean removed;

      g_signal_emit (self, signals[NOTIFICATION_HIDE], 0, source);
      removed = g_hash_table_remove (self->sources_notified, id);
      g_assert_true (removed);
    }

  PHOTOS_BASE_MANAGER_CLASS (photos_source_manager_parent_class)->remove_object_by_id (mngr, id);
}


static void
photos_source_manager_refresh_accounts (PhotosSourceManager *self)
{
  GApplication *app;
  g_autoptr (GHashTable) new_sources = NULL;
  GList *accounts = NULL;
  GList *l;
  PhotosSource *active_source;
  const gchar *active_id;
  guint i;
  guint n_items;

  app = g_application_get_default ();
  if (photos_application_get_empty_results (PHOTOS_APPLICATION (app)))
    goto out;

  accounts = goa_client_get_accounts (self->client);
  new_sources = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);

  for (l = accounts; l != NULL; l = l->next)
    {
      GoaAccount *account;
      GoaObject *object = GOA_OBJECT (l->data);
      g_autoptr (PhotosSource) source = NULL;
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
    }

  active_source = PHOTOS_SOURCE (photos_base_manager_get_active_object (PHOTOS_BASE_MANAGER (self)));
  active_id = photos_filterable_get_id (PHOTOS_FILTERABLE (active_source));
  if (!photos_filterable_get_builtin (PHOTOS_FILTERABLE (active_source))
      && !g_hash_table_contains (new_sources, active_id))
    {
      photos_base_manager_set_active_object_by_id (PHOTOS_BASE_MANAGER (self), PHOTOS_SOURCE_STOCK_ALL);
    }

  photos_base_manager_process_new_objects (PHOTOS_BASE_MANAGER (self), new_sources);

  n_items = g_list_model_get_n_items (G_LIST_MODEL (self));
  for (i = 0; i < n_items; i++)
    {
      GoaAccount *account;
      GoaObject *object;
      g_autoptr (PhotosSource) source = NULL;
      gboolean attention_needed;
      gboolean source_notified;
      const gchar *id;

      source = PHOTOS_SOURCE (g_list_model_get_object (G_LIST_MODEL (self), i));
      object = photos_source_get_goa_object (source);
      if (object == NULL)
        continue;

      account = goa_object_peek_account (object);
      if (account == NULL)
        continue;

      attention_needed = goa_account_get_attention_needed (account);
      id = photos_filterable_get_id (PHOTOS_FILTERABLE (source));
      source_notified = g_hash_table_contains (self->sources_notified, id);

      if (!attention_needed && source_notified)
        {
          gboolean removed;

          g_signal_emit (self, signals[NOTIFICATION_HIDE], 0, source);
          removed = g_hash_table_remove (self->sources_notified, id);
          g_assert_true (removed);
        }
      else if (attention_needed && !source_notified)
        {
          gboolean inserted;

          g_signal_emit (self, signals[NOTIFICATION_SHOW], 0, source);
          inserted = g_hash_table_insert (self->sources_notified, g_strdup (id), g_object_ref (source));
          g_assert_true (inserted);
        }
    }

 out:
  g_list_free_full (accounts, g_object_unref);
}


static void
photos_source_manager_goa_client (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosSourceManager *self;
  GoaClient *client;

  {
    g_autoptr (GError) error = NULL;

    client = goa_client_new_finish (res, &error);
    if (error != NULL)
      {
        if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
          g_warning ("Unable to create GoaClient: %s", error->message);

        return;
      }
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
  g_clear_pointer (&self->sources_notified, (GDestroyNotify) g_hash_table_unref);

  G_OBJECT_CLASS (photos_source_manager_parent_class)->dispose (object);
}


static void
photos_source_manager_init (PhotosSourceManager *self)
{
  g_autoptr (PhotosSource) source_all = NULL;
  g_autoptr (PhotosSource) source_local = NULL;

  source_all = photos_source_new (PHOTOS_SOURCE_STOCK_ALL, _("All"), TRUE);
  photos_base_manager_add_object (PHOTOS_BASE_MANAGER (self), G_OBJECT (source_all));

  source_local = photos_source_new (PHOTOS_SOURCE_STOCK_LOCAL, _("Local"), TRUE);
  photos_base_manager_add_object (PHOTOS_BASE_MANAGER (self), G_OBJECT (source_local));

  self->cancellable = g_cancellable_new ();
  self->sources_notified = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
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
  base_manager_class->remove_object_by_id = photos_source_manager_remove_object_by_id;

  signals[NOTIFICATION_HIDE] = g_signal_new ("notification-hide",
                                             G_TYPE_FROM_CLASS (class),
                                             G_SIGNAL_RUN_LAST,
                                             0,
                                             NULL, /* accumulator */
                                             NULL, /* accu_data */
                                             g_cclosure_marshal_VOID__OBJECT,
                                             G_TYPE_NONE,
                                             1,
                                             PHOTOS_TYPE_SOURCE);

  signals[NOTIFICATION_SHOW] = g_signal_new ("notification-show",
                                             G_TYPE_FROM_CLASS (class),
                                             G_SIGNAL_RUN_LAST,
                                             0,
                                             NULL, /* accumulator */
                                             NULL, /* accu_data */
                                             g_cclosure_marshal_VOID__OBJECT,
                                             G_TYPE_NONE,
                                             1,
                                             PHOTOS_TYPE_SOURCE);
}


PhotosBaseManager *
photos_source_manager_new (void)
{
  return g_object_new (PHOTOS_TYPE_SOURCE_MANAGER, "action-id", "search-source", "title", _("Sources"), NULL);
}


GList *
photos_source_manager_get_for_provider_type (PhotosSourceManager *self, const gchar *provider_type)
{
  GList *items = NULL;
  guint i;
  guint n_items;

  n_items = g_list_model_get_n_items (G_LIST_MODEL (self));
  for (i = 0; i < n_items; i++)
    {
      GoaObject *object;
      g_autoptr (PhotosSource) source = NULL;

      source = PHOTOS_SOURCE (g_list_model_get_object (G_LIST_MODEL (self), i));
      object = photos_source_get_goa_object (source);
      if (object != NULL)
        {
          GoaAccount *account;

          account = goa_object_peek_account (object);
          if (g_strcmp0 (goa_account_get_provider_type (account), provider_type) == 0)
            items = g_list_prepend (items, g_object_ref (source));
        }
    }

  return items;
}


gboolean
photos_source_manager_has_online_sources (PhotosSourceManager *self)
{
  gboolean ret_val = FALSE;
  guint i;
  guint n_items;

  n_items = g_list_model_get_n_items (G_LIST_MODEL (self));
  for (i = 0; i < n_items; i++)
    {
      GoaObject *object;
      g_autoptr (PhotosSource) source = NULL;

      source = PHOTOS_SOURCE (g_list_model_get_object (G_LIST_MODEL (self), i));
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
