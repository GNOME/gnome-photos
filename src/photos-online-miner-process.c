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

#include "photos-debug.h"
#include "photos-error.h"
#include "photos-online-miner-dbus.h"
#include "photos-online-miner-process.h"
#include "photos-utils.h"


struct _PhotosOnlineMinerProcess
{
  GObject parent_instance;
  GCancellable *cancellable;
  GDBusConnection *connection;
  GError *online_miner_error;
  GList *pending_async_calls;
  GSubprocess *subprocess;
  PhotosOnlineMinerDBus *online_miner;
  gchar *address;
  gchar *provider_name;
  gchar *provider_type;
};

enum
{
  PROP_0,
  PROP_ADDRESS,
  PROP_PROVIDER_TYPE
};


G_DEFINE_TYPE (PhotosOnlineMinerProcess, photos_online_miner_process, G_TYPE_OBJECT);


typedef struct _PhotosOnlineMinerProcessInsertSharedContentData PhotosOnlineMinerProcessInsertSharedContentData;

struct _PhotosOnlineMinerProcessInsertSharedContentData
{
  gchar *account_id;
  gchar *shared_id;
  gchar *source_urn;
};

static const gchar *ONLINE_MINER_PATH = "/org/gnome/Photos/OnlineMiner";


static PhotosOnlineMinerProcessInsertSharedContentData *
photos_online_miner_process_insert_shared_content_data_new (const gchar *account_id,
                                                            const gchar *shared_id,
                                                            const gchar *source_urn)
{
  PhotosOnlineMinerProcessInsertSharedContentData *data;

  data = g_slice_new0 (PhotosOnlineMinerProcessInsertSharedContentData);
  data->account_id = g_strdup (account_id);
  data->shared_id = g_strdup (shared_id);
  data->source_urn = g_strdup (source_urn);
  return data;
}


static void
photos_online_miner_process_insert_shared_content_data_free (PhotosOnlineMinerProcessInsertSharedContentData *data)
{
  g_free (data->account_id);
  g_free (data->shared_id);
  g_free (data->source_urn);
  g_slice_free (PhotosOnlineMinerProcessInsertSharedContentData, data);
}


static void
photos_online_miner_process_connection_closed (PhotosOnlineMinerProcess *self,
                                               gboolean remote_peer_vanished,
                                               GError *error)
{
  if (error != NULL)
    {
      photos_debug (PHOTOS_DEBUG_ONLINE_MINER,
                    "PhotosOnlineMinerProcess lost connection for %s: %s",
                    self->provider_type,
                    error->message);
    }
  else
    {
      photos_debug (PHOTOS_DEBUG_ONLINE_MINER,
                    "PhotosOnlineMinerProcess lost connection for %s",
                    self->provider_type);
    }

  g_signal_handlers_disconnect_by_func (self->connection, photos_online_miner_process_connection_closed, self);
  g_clear_object (&self->connection);
  g_clear_object (&self->subprocess);

  g_clear_error (&self->online_miner_error);
  g_clear_object (&self->online_miner);
}


static void
photos_online_miner_process_insert_shared_content (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  g_autoptr (GTask) task = G_TASK (user_data);
  PhotosOnlineMinerDBus *online_miner = PHOTOS_ONLINE_MINER_DBUS (source_object);

  {
    g_autoptr (GError) error = NULL;

    if (!photos_online_miner_dbus_call_insert_shared_content_finish (online_miner, res, &error))
      {
        g_task_return_error (task, g_steal_pointer (&error));
        goto out;
      }
  }

  g_task_return_boolean (task, TRUE);

 out:
  return;
}


static void
photos_online_miner_process_refresh_db (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  g_autoptr (GTask) task = G_TASK (user_data);
  PhotosOnlineMinerDBus *online_miner = PHOTOS_ONLINE_MINER_DBUS (source_object);

  {
    g_autoptr (GError) error = NULL;

    if (!photos_online_miner_dbus_call_refresh_db_finish (online_miner, res, &error))
      {
        g_task_return_error (task, g_steal_pointer (&error));
        goto out;
      }
  }

  g_task_return_boolean (task, TRUE);

 out:
  return;
}


static void
photos_online_miner_process_continue_async_call (PhotosOnlineMinerProcess *self, GTask *task)
{
  GCancellable *cancellable;
  gpointer source_tag;

  g_return_if_fail (self->online_miner_error == NULL);
  g_return_if_fail (PHOTOS_IS_ONLINE_MINER_DBUS (self->online_miner));

  cancellable = g_task_get_cancellable (task);
  source_tag = g_task_get_source_tag (task);

  if (source_tag == photos_online_miner_process_insert_shared_content_async)
    {
      PhotosOnlineMinerProcessInsertSharedContentData *data;

      data = (PhotosOnlineMinerProcessInsertSharedContentData *) g_task_get_task_data (task);
      photos_online_miner_dbus_call_insert_shared_content (self->online_miner,
                                                           data->account_id,
                                                           data->shared_id,
                                                           data->source_urn,
                                                           cancellable,
                                                           photos_online_miner_process_insert_shared_content,
                                                           g_object_ref (task));
    }
  else if (source_tag == photos_online_miner_process_refresh_db_async)
    {
      photos_online_miner_dbus_call_refresh_db (self->online_miner,
                                                cancellable,
                                                photos_online_miner_process_refresh_db,
                                                g_object_ref (task));
    }
  else
    {
      g_assert_not_reached ();
    }
}


static void
photos_online_miner_process_wait_check (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosOnlineMinerProcess *self;
  GList *l;
  GSubprocess *subprocess = G_SUBPROCESS (source_object);

  {
    g_autoptr (GError) error = NULL;

    if (!g_subprocess_wait_check_finish (subprocess, res, &error))
      {
        if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
          goto out;

        g_warning ("Unable to successfully terminate online miner: %s", error->message);
      }
  }

  self = PHOTOS_ONLINE_MINER_PROCESS (user_data);
  photos_debug (PHOTOS_DEBUG_ONLINE_MINER, "PhotosOnlineMinerProcess for %s terminated", self->provider_type);

  for (l = self->pending_async_calls; l != NULL; l = l->next)
    {
      GTask *task = G_TASK (l->data);

      g_task_return_new_error (task,
                               PHOTOS_ERROR,
                               0,
                               "Online miner for %s terminated unexpectedly before connecting",
                               self->provider_type);
    }

  g_clear_pointer (&self->pending_async_calls, photos_utils_object_list_free_full);

 out:
  return;
}


static gboolean
photos_online_miner_process_spawn (PhotosOnlineMinerProcess *self, GError **error)
{
  gboolean ret_val = FALSE;
  g_autofree gchar *online_miner_path = NULL;

  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);
  g_return_val_if_fail (self->connection == NULL, FALSE);
  g_return_val_if_fail (self->online_miner_error == NULL, FALSE);
  g_return_val_if_fail (self->online_miner == NULL, FALSE);

  g_clear_object (&self->subprocess);

  online_miner_path = g_strconcat (PACKAGE_LIBEXEC_DIR,
                                   G_DIR_SEPARATOR_S,
                                   PACKAGE_TARNAME,
                                   "-online-miner-",
                                   self->provider_type,
                                   NULL);

  photos_debug (PHOTOS_DEBUG_ONLINE_MINER,
                "PhotosOnlineMinerProcess spawning “%s --address %s”",
                online_miner_path,
                self->address);

  self->subprocess = g_subprocess_new (G_SUBPROCESS_FLAGS_NONE,
                                       error,
                                       online_miner_path,
                                       "--address",
                                       self->address,
                                       NULL);
  if (self->subprocess == NULL)
    goto out;

  g_subprocess_wait_check_async (self->subprocess, self->cancellable, photos_online_miner_process_wait_check, self);
  ret_val = TRUE;

 out:
  return ret_val;
}


static void
photos_online_miner_process_dispose (GObject *object)
{
  PhotosOnlineMinerProcess *self = PHOTOS_ONLINE_MINER_PROCESS (object);

  if (self->cancellable != NULL)
    {
      g_cancellable_cancel (self->cancellable);
      g_clear_object (&self->cancellable);
    }

  g_clear_object (&self->connection);
  g_clear_object (&self->subprocess);
  g_clear_object (&self->online_miner);
  g_clear_pointer (&self->pending_async_calls, photos_utils_object_list_free_full);

  G_OBJECT_CLASS (photos_online_miner_process_parent_class)->dispose (object);
}


static void
photos_online_miner_process_finalize (GObject *object)
{
  PhotosOnlineMinerProcess *self = PHOTOS_ONLINE_MINER_PROCESS (object);

  g_clear_error (&self->online_miner_error);
  g_free (self->address);
  g_free (self->provider_name);
  g_free (self->provider_type);

  G_OBJECT_CLASS (photos_online_miner_process_parent_class)->finalize (object);
}


static void
photos_online_miner_process_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
  PhotosOnlineMinerProcess *self = PHOTOS_ONLINE_MINER_PROCESS (object);

  switch (prop_id)
    {
    case PROP_PROVIDER_TYPE:
      g_value_set_string (value, self->provider_type);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
photos_online_miner_process_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
  PhotosOnlineMinerProcess *self = PHOTOS_ONLINE_MINER_PROCESS (object);

  switch (prop_id)
    {
    case PROP_ADDRESS:
      self->address = g_value_dup_string (value);
      break;

    case PROP_PROVIDER_TYPE:
      self->provider_type = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
photos_online_miner_process_init (PhotosOnlineMinerProcess *self)
{
  self->cancellable = g_cancellable_new ();
}


static void
photos_online_miner_process_class_init (PhotosOnlineMinerProcessClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->dispose = photos_online_miner_process_dispose;
  object_class->finalize = photos_online_miner_process_finalize;
  object_class->get_property = photos_online_miner_process_get_property;
  object_class->set_property = photos_online_miner_process_set_property;

  g_object_class_install_property (object_class,
                                   PROP_ADDRESS,
                                   g_param_spec_string ("address",
                                                        "Address",
                                                        "D-Bus address for clients",
                                                        NULL,
                                                        G_PARAM_CONSTRUCT_ONLY
                                                        | G_PARAM_STATIC_STRINGS
                                                        | G_PARAM_WRITABLE));

  g_object_class_install_property (object_class,
                                   PROP_PROVIDER_TYPE,
                                   g_param_spec_string ("provider-type",
                                                        "Provider type",
                                                        "A GOA provider type",
                                                        NULL,
                                                        G_PARAM_CONSTRUCT_ONLY
                                                        | G_PARAM_READWRITE
                                                        | G_PARAM_STATIC_STRINGS));
}


PhotosOnlineMinerProcess *
photos_online_miner_process_new (const gchar *address, const gchar *provider_type)
{
  g_return_val_if_fail (address != NULL && address[0] != '\0', NULL);
  g_return_val_if_fail (provider_type != NULL && provider_type[0] != '\0', NULL);

  return g_object_new (PHOTOS_TYPE_ONLINE_MINER_PROCESS, "address", address, "provider-type", provider_type, NULL);
}


GDBusConnection *
photos_online_miner_process_get_connection (PhotosOnlineMinerProcess *self)
{
  g_return_val_if_fail (PHOTOS_IS_ONLINE_MINER_PROCESS (self), NULL);
  return self->connection;
}


const gchar *
photos_online_miner_process_get_provider_name (PhotosOnlineMinerProcess *self)
{
  g_return_val_if_fail (PHOTOS_IS_ONLINE_MINER_PROCESS (self), NULL);
  return self->provider_name;
}


const gchar *
photos_online_miner_process_get_provider_type (PhotosOnlineMinerProcess *self)
{
  g_return_val_if_fail (PHOTOS_IS_ONLINE_MINER_PROCESS (self), NULL);
  return self->provider_type;
}


gboolean
photos_online_miner_process_matches_credentials (PhotosOnlineMinerProcess *self, GCredentials *credentials)
{
  gboolean ret_val = FALSE;
  const gchar *identifier;
  g_autofree gchar *pid_str = NULL;
  pid_t pid = -1;

  g_return_val_if_fail (PHOTOS_IS_ONLINE_MINER_PROCESS (self), FALSE);
  g_return_val_if_fail (G_IS_CREDENTIALS (credentials), FALSE);

  if (self->subprocess == NULL)
    goto out;

  identifier = g_subprocess_get_identifier (self->subprocess);
  if (identifier == NULL)
    goto out;

  {
    g_autoptr (GError) error = NULL;

    pid = g_credentials_get_unix_pid (credentials, &error);
    if (error != NULL)
      {
        g_warning ("Unable to get UNIX PID from GCredentials: %s", error->message);
        goto out;
      }
  }

  if (pid == -1)
    goto out;

  pid_str = g_strdup_printf ("%d", (gint) pid);
  if (g_strcmp0 (identifier, pid_str) == 0)
    {
      ret_val = TRUE;
      goto out;
    }

 out:
  return ret_val;
}


void
photos_online_miner_process_insert_shared_content_async (PhotosOnlineMinerProcess *self,
                                                         const gchar *account_id,
                                                         const gchar *shared_id,
                                                         const gchar *source_urn,
                                                         GCancellable *cancellable,
                                                         GAsyncReadyCallback callback,
                                                         gpointer user_data)
{
  g_autoptr (GTask) task = NULL;
  PhotosOnlineMinerProcessInsertSharedContentData *data;

  g_return_if_fail (PHOTOS_IS_ONLINE_MINER_PROCESS (self));
  g_return_if_fail (account_id != NULL && account_id[0] != '\0');
  g_return_if_fail (shared_id != NULL && shared_id[0] != '\0');
  g_return_if_fail (source_urn != NULL && source_urn[0] != '\0');
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  data = photos_online_miner_process_insert_shared_content_data_new (account_id, shared_id, source_urn);

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, photos_online_miner_process_insert_shared_content_async);
  g_task_set_task_data (task, data, (GDestroyNotify) photos_online_miner_process_insert_shared_content_data_free);

  if (self->connection == NULL)
    {
      {
        g_autoptr (GError) error = NULL;

        if (!photos_online_miner_process_spawn (self, &error))
          {
            g_task_return_error (task, g_steal_pointer (&error));
            goto out;
          }
      }

      self->pending_async_calls = g_list_prepend (self->pending_async_calls, g_object_ref (task));
    }
  else
    {
      if (self->online_miner_error != NULL)
        g_task_return_error (task, g_error_copy (self->online_miner_error));
      else
        photos_online_miner_process_continue_async_call (self, task);
    }

 out:
  return;
}


gboolean
photos_online_miner_process_insert_shared_content_finish (PhotosOnlineMinerProcess *self,
                                                          GAsyncResult *res,
                                                          GError **error)
{
  GTask *task;

  g_return_val_if_fail (g_task_is_valid (res, self), FALSE);
  task = G_TASK (res);

  g_return_val_if_fail (g_task_get_source_tag (task) == photos_online_miner_process_insert_shared_content_async,
                        FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  return g_task_propagate_boolean (task, error);
}


void
photos_online_miner_process_refresh_db_async (PhotosOnlineMinerProcess *self,
                                              GCancellable *cancellable,
                                              GAsyncReadyCallback callback,
                                              gpointer user_data)
{
  g_autoptr (GTask) task = NULL;

  g_return_if_fail (PHOTOS_IS_ONLINE_MINER_PROCESS (self));
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, photos_online_miner_process_refresh_db_async);

  if (self->connection == NULL)
    {
      {
        g_autoptr (GError) error = NULL;

        if (!photos_online_miner_process_spawn (self, &error))
          {
            g_task_return_error (task, g_steal_pointer (&error));
            goto out;
          }
      }

      self->pending_async_calls = g_list_prepend (self->pending_async_calls, g_object_ref (task));
    }
  else
    {
      if (self->online_miner_error != NULL)
        g_task_return_error (task, g_error_copy (self->online_miner_error));
      else
        photos_online_miner_process_continue_async_call (self, task);
    }

 out:
  return;
}


gboolean
photos_online_miner_process_refresh_db_finish (PhotosOnlineMinerProcess *self, GAsyncResult *res, GError **error)
{
  GTask *task;

  g_return_val_if_fail (g_task_is_valid (res, self), FALSE);
  task = G_TASK (res);

  g_return_val_if_fail (g_task_get_source_tag (task) == photos_online_miner_process_refresh_db_async, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  return g_task_propagate_boolean (task, error);
}


void
photos_online_miner_process_set_connection (PhotosOnlineMinerProcess *self, GDBusConnection *connection)
{
  GList *l;

  g_return_if_fail (PHOTOS_IS_ONLINE_MINER_PROCESS (self));
  g_return_if_fail (G_IS_DBUS_CONNECTION (connection));
  g_return_if_fail (self->connection == NULL);
  g_return_if_fail (self->online_miner_error == NULL);
  g_return_if_fail (self->online_miner == NULL);

  photos_debug (PHOTOS_DEBUG_ONLINE_MINER,
                "PhotosOnlineMinerProcess received new connection for %s",
                self->provider_type);

  self->connection = g_object_ref (connection);
  g_signal_connect_object (self->connection,
                           "closed",
                           G_CALLBACK (photos_online_miner_process_connection_closed),
                           self,
                           G_CONNECT_SWAPPED);

  self->online_miner = photos_online_miner_dbus_proxy_new_sync (self->connection,
                                                                G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES
                                                                | G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS,
                                                                NULL,
                                                                ONLINE_MINER_PATH,
                                                                NULL,
                                                                &self->online_miner_error);

  for (l = self->pending_async_calls; l != NULL; l = l->next)
    {
      GTask *task = G_TASK (l->data);

      if (self->online_miner_error != NULL)
        g_task_return_error (task, g_error_copy (self->online_miner_error));
      else
        photos_online_miner_process_continue_async_call (self, task);
    }

  g_clear_pointer (&self->pending_async_calls, photos_utils_object_list_free_full);
}


void
photos_online_miner_process_set_provider_name (PhotosOnlineMinerProcess *self, const gchar *provider_name)
{
  g_return_if_fail (PHOTOS_IS_ONLINE_MINER_PROCESS (self));
  g_return_if_fail (provider_name != NULL && provider_name[0] != '\0');

  g_free (self->provider_name);
  self->provider_name = g_strdup (provider_name);
}
