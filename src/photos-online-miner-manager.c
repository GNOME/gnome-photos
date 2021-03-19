/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2021 Red Hat, Inc.
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


#include "config.h"

#include <dazzle.h>
#include <tracker-sparql.h>

#include "photos-debug.h"
#include "photos-online-miner-manager.h"
#include "photos-online-miner-process.h"
#include "photos-search-context.h"
#include "photos-source-manager.h"
#include "photos-tracker-queue.h"
#include "photos-utils.h"


struct _PhotosOnlineMinerManager
{
  GObject parent_instance;
  GCancellable *cancellable;
  GDBusServer *dbus_server;
  GError *initialization_error;
  GHashTable *online_miner_to_refresh_id;
  GHashTable *provider_type_to_online_miner;
  GList *online_miners_running;
  GPtrArray *endpoints;
  PhotosBaseManager *src_mngr;
  PhotosTrackerQueue *queue;
  gboolean is_initialized;
};

enum
{
  CHANGED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static void photos_online_miner_manager_initable_iface_init (GInitableIface *iface);


G_DEFINE_TYPE_WITH_CODE (PhotosOnlineMinerManager, photos_online_miner_manager, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, photos_online_miner_manager_initable_iface_init));
DZL_DEFINE_COUNTER (instances,
                    "PhotosOnlineMinerManager",
                    "Instances",
                    "Number of PhotosOnlineMinerManager instances");


enum
{
  MINER_REFRESH_TIMEOUT = 60 /* s */
};

static void photos_online_miner_manager_refresh (PhotosOnlineMinerManager *self,
                                                 PhotosOnlineMinerProcess *online_miner);

G_LOCK_DEFINE_STATIC (init_lock);


static gboolean
photos_online_miner_manager_authorize_authenticated_peer (PhotosOnlineMinerManager *self,
                                                          GIOStream *iostream,
                                                          GCredentials *credentials)
{
  GHashTableIter iter;
  PhotosOnlineMinerProcess *online_miner;
  gboolean ret_val = FALSE;

  photos_debug (PHOTOS_DEBUG_ONLINE_MINER, "PhotosOnlineMinerManager received authorization request");

  if (credentials == NULL)
    {
      g_warning ("Unable to authorize peer: Credentials not found");
      goto out;
    }

  g_hash_table_iter_init (&iter, self->provider_type_to_online_miner);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &online_miner))
    {
      if (photos_online_miner_process_matches_credentials (online_miner, credentials))
        {
          GDBusConnection *connection;

          connection = photos_online_miner_process_get_connection (online_miner);
          if (connection != NULL)
            {
              g_warning ("Unable to authorize peer: Connection exists");
              goto out;
            }

          ret_val = TRUE;
          break;
        }
    }

 out:
  return ret_val;
}


static void
photos_online_miner_manager_create_all_online_miners (PhotosOnlineMinerManager *self)
{
  GIOExtensionPoint *extension_point;
  GList *extensions;
  GList *l;

  extension_point = g_io_extension_point_lookup (PHOTOS_BASE_ITEM_EXTENSION_POINT_NAME);
  extensions = g_io_extension_point_get_extensions (extension_point);
  for (l = extensions; l != NULL; l = l->next)
    {
      GIOExtension *extension = (GIOExtension *) l->data;
      const gchar *extension_name;

      extension_name = g_io_extension_get_name (extension);
      if (g_strcmp0 (extension_name, "local") != 0)
        {
          g_autoptr (PhotosOnlineMinerProcess) online_miner = NULL;
          const gchar *address;
          gboolean key_didnt_exist;

          address = g_dbus_server_get_client_address (self->dbus_server);
          online_miner = photos_online_miner_process_new (address, extension_name);
          key_didnt_exist = g_hash_table_insert (self->provider_type_to_online_miner,
                                                 g_strdup (extension_name),
                                                 g_object_ref (online_miner));
          g_return_if_fail (key_didnt_exist);
        }
    }
}


static void
photos_online_miner_manager_create_endpoint (PhotosOnlineMinerManager *self,
                                             PhotosOnlineMinerProcess *online_miner,
                                             GDBusConnection *dbus_connection)
{
  g_autoptr (TrackerEndpointDBus) endpoint = NULL;
  TrackerSparqlConnection *sparql_connection_online;

  if (G_UNLIKELY (self->queue == NULL))
    goto out;

  sparql_connection_online = photos_tracker_queue_get_connection_online (self->queue);

  {
    g_autoptr (GError) error = NULL;

    endpoint = tracker_endpoint_dbus_new (sparql_connection_online, dbus_connection, NULL, NULL, &error);
    if (error != NULL)
      {
        const gchar *provider_type;

        provider_type = photos_online_miner_process_get_provider_type (online_miner);
        g_warning ("Unable to create TrackerEndpoint for %s: %s", provider_type, error->message);
        goto out;
      }
  }

  g_ptr_array_add (self->endpoints, g_object_ref (endpoint));

 out:
  return;
}


static void
photos_online_miner_manager_insert_shared_content (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  g_autoptr (GTask) task = G_TASK (user_data);
  PhotosOnlineMinerProcess *online_miner = PHOTOS_ONLINE_MINER_PROCESS (source_object);

  {
    g_autoptr (GError) error = NULL;

    if (!photos_online_miner_process_insert_shared_content_finish (online_miner, res, &error))
      {
        g_task_return_error (task, g_steal_pointer (&error));
        goto out;
      }
  }

  g_task_return_boolean (task, TRUE);

 out:
  return;
}


static gboolean
photos_online_miner_manager_new_connection (PhotosOnlineMinerManager *self, GDBusConnection *connection)
{
  GCredentials *credentials;
  GHashTableIter iter;
  PhotosOnlineMinerProcess *online_miner;
  gboolean ret_val = FALSE;

  photos_debug (PHOTOS_DEBUG_ONLINE_MINER, "PhotosOnlineMinerManager received new connection");

  credentials = g_dbus_connection_get_peer_credentials (connection);
  if (credentials == NULL)
    goto out;

  g_hash_table_iter_init (&iter, self->provider_type_to_online_miner);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &online_miner))
    {
      if (photos_online_miner_process_matches_credentials (online_miner, credentials))
        {
          photos_online_miner_process_set_connection (online_miner, connection);
          photos_online_miner_manager_create_endpoint (self, online_miner, connection);
          ret_val = TRUE;
          break;
        }
    }

 out:
  return ret_val;
}


static gboolean
photos_online_miner_manager_refresh_timeout (gpointer user_data)
{
  PhotosOnlineMinerManager *self;
  GTask *task = G_TASK (user_data);
  PhotosOnlineMinerProcess *online_miner;
  gboolean removed;

  self = PHOTOS_ONLINE_MINER_MANAGER (g_task_get_source_object (task));
  online_miner = PHOTOS_ONLINE_MINER_PROCESS (g_task_get_task_data (task));

  removed = g_hash_table_remove (self->online_miner_to_refresh_id, online_miner);
  g_return_val_if_fail (removed, G_SOURCE_REMOVE);

  photos_online_miner_manager_refresh (self, online_miner);
  return G_SOURCE_REMOVE;
}


static void
photos_online_miner_manager_refresh_db (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosOnlineMinerManager *self;
  GList *online_miner_link;
  g_autoptr (GTask) task = NULL;
  PhotosOnlineMinerProcess *online_miner = PHOTOS_ONLINE_MINER_PROCESS (source_object);
  gboolean online_miner_reschedule = TRUE;
  const gchar *provider_type;
  gpointer refresh_id_data;
  guint refresh_id;

  provider_type = photos_online_miner_process_get_provider_type (online_miner);
  photos_debug (PHOTOS_DEBUG_ONLINE_MINER, "PhotosOnlineMinerManager finished RefreshDB for %s", provider_type);

  {
    g_autoptr (GError) error = NULL;

    if (!photos_online_miner_process_refresh_db_finish (online_miner, res, &error))
      {
        if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
          goto out;

        g_warning ("Unable to update the cache for %s: %s", provider_type, error->message);
        online_miner_reschedule = FALSE;
      }
  }

  self = PHOTOS_ONLINE_MINER_MANAGER (user_data);

  refresh_id_data = g_hash_table_lookup (self->online_miner_to_refresh_id, online_miner);
  g_return_if_fail (refresh_id_data == NULL);

  online_miner_link = g_list_find (self->online_miners_running, online_miner);
  g_return_if_fail (online_miner_link != NULL);

  self->online_miners_running = g_list_remove_link (self->online_miners_running, online_miner_link);
  g_list_free_full (online_miner_link, g_object_unref);
  g_signal_emit (self, signals[CHANGED], 0, self->online_miners_running);

  if (!online_miner_reschedule)
    goto out;

  task = g_task_new (self, NULL, NULL, NULL);
  g_task_set_task_data (task, g_object_ref (online_miner), g_object_unref);

  refresh_id = g_timeout_add_seconds_full (G_PRIORITY_DEFAULT,
                                           MINER_REFRESH_TIMEOUT,
                                           photos_online_miner_manager_refresh_timeout,
                                           g_object_ref (task),
                                           g_object_unref);

  g_hash_table_insert (self->online_miner_to_refresh_id, g_object_ref (online_miner), GUINT_TO_POINTER (refresh_id));

  photos_debug (PHOTOS_DEBUG_ONLINE_MINER, "PhotosOnlineMinerManager added timeout for %s", provider_type);

 out:
  return;
}


static void
photos_online_miner_manager_refresh (PhotosOnlineMinerManager *self, PhotosOnlineMinerProcess *online_miner)
{
  const gchar *provider_type;
  gpointer refresh_id_data;

  if (g_getenv ("GNOME_PHOTOS_DISABLE_MINERS") != NULL)
    goto out;

  provider_type = photos_online_miner_process_get_provider_type (online_miner);

  if (g_list_find (self->online_miners_running, online_miner) != NULL)
    {
      photos_debug (PHOTOS_DEBUG_ONLINE_MINER,
                    "PhotosOnlineMinerManager skipped %s: already running",
                    provider_type);
      goto out;
    }

  refresh_id_data = g_hash_table_lookup (self->online_miner_to_refresh_id, online_miner);
  if (refresh_id_data != NULL)
    {
      guint refresh_id = GPOINTER_TO_UINT (refresh_id_data);

      g_source_remove (refresh_id);
      g_hash_table_remove (self->online_miner_to_refresh_id, online_miner);
      photos_debug (PHOTOS_DEBUG_ONLINE_MINER, "PhotosOnlineMinerManager removed timeout for %s", provider_type);
    }

  self->online_miners_running = g_list_prepend (self->online_miners_running, g_object_ref (online_miner));
  g_signal_emit (self, signals[CHANGED], 0, self->online_miners_running);

  photos_debug (PHOTOS_DEBUG_ONLINE_MINER, "PhotosOnlineMinerManager calling RefreshDB for %s", provider_type);
  photos_online_miner_process_refresh_db_async (online_miner,
                                                self->cancellable,
                                                photos_online_miner_manager_refresh_db,
                                                self);

 out:
  return;
}


static void
photos_online_miner_manager_refresh_all (PhotosOnlineMinerManager *self)
{
  GHashTableIter iter;
  PhotosOnlineMinerProcess *online_miner;
  const gchar *provider_type;

  g_hash_table_iter_init (&iter, self->provider_type_to_online_miner);
  while (g_hash_table_iter_next (&iter, (gpointer *) &provider_type, (gpointer *) &online_miner))
    {
      const gchar *provider_name;

      if (!photos_source_manager_has_provider_type (PHOTOS_SOURCE_MANAGER (self->src_mngr), provider_type))
        continue;

      provider_name = photos_online_miner_process_get_provider_name (online_miner);
      if (provider_name == NULL)
        {
          provider_name
            = photos_source_manager_get_provider_name_for_provider_type (PHOTOS_SOURCE_MANAGER (self->src_mngr),
                                                                         provider_type);
          g_return_if_fail (provider_name != NULL && provider_name[0] != '\0');

          photos_online_miner_process_set_provider_name (online_miner, provider_name);
        }

      photos_online_miner_manager_refresh (self, online_miner);
    }
}


static GObject *
photos_online_miner_manager_constructor (GType type,
                                         guint n_construct_params,
                                         GObjectConstructParam *construct_params)
{
  static GObject *self = NULL;

  if (self == NULL)
    {
      self = G_OBJECT_CLASS (photos_online_miner_manager_parent_class)->constructor (type,
                                                                                     n_construct_params,
                                                                                     construct_params);
      g_object_add_weak_pointer (self, (gpointer) &self);
      return self;
    }

  return g_object_ref (self);
}


static void
photos_online_miner_manager_dispose (GObject *object)
{
  PhotosOnlineMinerManager *self = PHOTOS_ONLINE_MINER_MANAGER (object);

  if (self->cancellable != NULL)
    {
      g_cancellable_cancel (self->cancellable);
      g_clear_object (&self->cancellable);
    }

  if (self->dbus_server != NULL)
    {
      g_dbus_server_stop (self->dbus_server);
      g_clear_object (&self->dbus_server);
    }

  if (self->online_miner_to_refresh_id != NULL)
    {
      GHashTableIter iter;
      gpointer refresh_id_data;

      g_hash_table_iter_init (&iter, self->online_miner_to_refresh_id);
      while (g_hash_table_iter_next (&iter, NULL, &refresh_id_data))
        {
          guint refresh_id = GPOINTER_TO_UINT (refresh_id_data);
          g_source_remove (refresh_id);
        }

      g_clear_pointer (&self->online_miner_to_refresh_id, g_hash_table_unref);
    }

  g_clear_object (&self->src_mngr);
  g_clear_object (&self->queue);
  g_clear_pointer (&self->provider_type_to_online_miner, g_hash_table_unref);
  g_clear_pointer (&self->online_miners_running, photos_utils_object_list_free_full);
  g_clear_pointer (&self->endpoints, g_ptr_array_unref);

  G_OBJECT_CLASS (photos_online_miner_manager_parent_class)->dispose (object);
}


static void
photos_online_miner_manager_finalize (GObject *object)
{
  PhotosOnlineMinerManager *self = PHOTOS_ONLINE_MINER_MANAGER (object);

  g_clear_error (&self->initialization_error);

  G_OBJECT_CLASS (photos_online_miner_manager_parent_class)->finalize (object);

  DZL_COUNTER_DEC (instances);
}


static void
photos_online_miner_manager_init (PhotosOnlineMinerManager *self)
{
  GApplication *app;
  PhotosSearchContextState *state;

  DZL_COUNTER_INC (instances);

  app = g_application_get_default ();
  state = photos_search_context_get_state (PHOTOS_SEARCH_CONTEXT (app));

  self->cancellable = g_cancellable_new ();
  self->online_miner_to_refresh_id = g_hash_table_new_full (g_direct_hash, g_direct_equal, g_object_unref, NULL);
  self->provider_type_to_online_miner = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
  self->endpoints = g_ptr_array_new_with_free_func (g_object_unref);

  self->src_mngr = g_object_ref (state->src_mngr);
  g_signal_connect_object (self->src_mngr,
                           "object-added",
                           G_CALLBACK (photos_online_miner_manager_refresh_all),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (self->src_mngr,
                           "object-removed",
                           G_CALLBACK (photos_online_miner_manager_refresh_all),
                           self,
                           G_CONNECT_SWAPPED);

  {
    g_autoptr (GError) error = NULL;

    self->queue = photos_tracker_queue_dup_singleton (NULL, &error);
    if (G_UNLIKELY (error != NULL))
      g_warning ("Unable to create PhotosTrackerQueue: %s", error->message);
  }
}


static void
photos_online_miner_manager_class_init (PhotosOnlineMinerManagerClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->constructor = photos_online_miner_manager_constructor;
  object_class->dispose = photos_online_miner_manager_dispose;
  object_class->finalize = photos_online_miner_manager_finalize;

  signals[CHANGED] = g_signal_new ("changed",
                                   G_TYPE_FROM_CLASS (class),
                                   G_SIGNAL_RUN_LAST,
                                   0,
                                   NULL, /* accumulator */
                                   NULL, /* accu_data */
                                   g_cclosure_marshal_VOID__POINTER,
                                   G_TYPE_NONE,
                                   1,
                                   G_TYPE_POINTER);
}


static gboolean
photos_online_miner_manager_initable_init (GInitable *initable, GCancellable *cancellable, GError **error)
{
  PhotosOnlineMinerManager *self = PHOTOS_ONLINE_MINER_MANAGER (initable);
  g_autoptr (GDBusAuthObserver) observer = NULL;
  gboolean ret_val = FALSE;
  const gchar *tmp_dir;
  g_autofree gchar *address = NULL;
  g_autofree gchar *guid = NULL;

  G_LOCK (init_lock);

  if (self->is_initialized)
    {
      if (self->dbus_server != NULL)
        ret_val = TRUE;
      else
        g_assert_nonnull (self->initialization_error);

      goto out;
    }

  g_assert_no_error (self->initialization_error);

  tmp_dir = g_get_tmp_dir ();
  address = g_strdup_printf ("unix:tmpdir=%s", tmp_dir);

  guid = g_dbus_generate_guid ();

  observer = g_dbus_auth_observer_new ();
  g_signal_connect_swapped (observer,
                            "authorize-authenticated-peer",
                            G_CALLBACK (photos_online_miner_manager_authorize_authenticated_peer),
                            self);

  self->dbus_server = g_dbus_server_new_sync (address,
                                              G_DBUS_SERVER_FLAGS_NONE,
                                              guid,
                                              observer,
                                              cancellable,
                                              &self->initialization_error);
  if (G_UNLIKELY (self->initialization_error != NULL))
    goto out;

  g_signal_connect_swapped (self->dbus_server,
                            "new-connection",
                            G_CALLBACK (photos_online_miner_manager_new_connection),
                            self);

  g_dbus_server_start (self->dbus_server);

  photos_online_miner_manager_create_all_online_miners (self);
  photos_online_miner_manager_refresh_all (self);

  ret_val = TRUE;

 out:
  self->is_initialized = TRUE;
  if (!ret_val)
    {
      g_assert_nonnull (self->initialization_error);
      g_propagate_error (error, g_error_copy (self->initialization_error));
    }

  G_UNLOCK (init_lock);

  return ret_val;
}


static void
photos_online_miner_manager_initable_iface_init (GInitableIface *iface)
{
  iface->init = photos_online_miner_manager_initable_init;
}


PhotosOnlineMinerManager *
photos_online_miner_manager_dup_singleton (GCancellable *cancellable, GError **error)
{
  g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  return g_initable_new (PHOTOS_TYPE_ONLINE_MINER_MANAGER, cancellable, error, NULL);
}


GList *
photos_online_miner_manager_get_running (PhotosOnlineMinerManager *self)
{
  g_return_val_if_fail (PHOTOS_IS_ONLINE_MINER_MANAGER (self), NULL);
  return self->online_miners_running;
}


void
photos_online_miner_manager_insert_shared_content_async (PhotosOnlineMinerManager *self,
                                                         const gchar *provider_type,
                                                         const gchar *account_id,
                                                         const gchar *shared_id,
                                                         const gchar *source_urn,
                                                         GCancellable *cancellable,
                                                         GAsyncReadyCallback callback,
                                                         gpointer user_data)
{
  g_autoptr (GTask) task = NULL;
  PhotosOnlineMinerProcess *online_miner;

  g_return_if_fail (PHOTOS_IS_ONLINE_MINER_MANAGER (self));
  g_return_if_fail (provider_type != NULL && provider_type[0] != '\0');
  g_return_if_fail (account_id != NULL && account_id[0] != '\0');
  g_return_if_fail (shared_id != NULL && shared_id[0] != '\0');
  g_return_if_fail (source_urn != NULL && source_urn[0] != '\0');
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  online_miner = PHOTOS_ONLINE_MINER_PROCESS (g_hash_table_lookup (self->provider_type_to_online_miner,
                                                                   provider_type));
  g_return_if_fail (PHOTOS_IS_ONLINE_MINER_PROCESS (online_miner));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, photos_online_miner_manager_insert_shared_content_async);

  photos_online_miner_process_insert_shared_content_async (online_miner,
                                                           account_id,
                                                           shared_id,
                                                           source_urn,
                                                           cancellable,
                                                           photos_online_miner_manager_insert_shared_content,
                                                           g_object_ref (task));
}


gboolean
photos_online_miner_manager_insert_shared_content_finish (PhotosOnlineMinerManager *self,
                                                          GAsyncResult *res,
                                                          GError **error)
{
  GTask *task;

  g_return_val_if_fail (g_task_is_valid (res, self), FALSE);
  task = G_TASK (res);

  g_return_val_if_fail (g_task_get_source_tag (task) == photos_online_miner_manager_insert_shared_content_async,
                        FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  return g_task_propagate_boolean (task, error);
}
