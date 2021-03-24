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

#include <locale.h>
#include <stdlib.h>

#include <dazzle.h>
#include <glib/gi18n.h>

#include "photos-debug.h"
#include "photos-error.h"
#include "photos-online-miner-dbus.h"
#include "photos-online-miner.h"


typedef struct _PhotosOnlineMinerPrivate PhotosOnlineMinerPrivate;

struct _PhotosOnlineMinerPrivate
{
  GApplication parent_instance;
  GDBusConnection *dbus_connection;
  GError *client_error;
  GHashTable *cancellables;
  GoaClient *client;
  PhotosOnlineMinerDBus *skeleton;
  TrackerSparqlConnection *sparql_connection;
  gchar *address;
};


G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (PhotosOnlineMiner, photos_online_miner, G_TYPE_APPLICATION);


typedef struct _PhotosOnlineMinerRefreshAccountData PhotosOnlineMinerRefreshAccountData;
typedef struct _PhotosOnlineMinerRefreshDBData PhotosOnlineMinerRefreshDBData;

struct _PhotosOnlineMinerRefreshAccountData
{
  GHashTable *identifier_to_urn_old;
  GoaObject *object;
  gchar *datasource;
};

struct _PhotosOnlineMinerRefreshDBData
{
  GList *accounts_selected;
  GPtrArray *accounts_to_be_indexed;
  GPtrArray *datasources_to_be_deleted;
  GTask *cache_task;
};

enum
{
  INACTIVITY_TIMEOUT = 12000 /* ms */
};

static const GOptionEntry COMMAND_LINE_OPTIONS[] =
{
  { "address", 0, G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING, NULL, N_("D-Bus address to use"), NULL},
  { NULL }
};

static DzlTaskCache *refresh_db_cache;
static const gchar *ONLINE_MINER_PATH = "/org/gnome/Photos/OnlineMiner";


static PhotosOnlineMinerRefreshAccountData *
photos_online_miner_refresh_account_data_new (GoaObject *object, const gchar *datasource)
{
  PhotosOnlineMinerRefreshAccountData *data;

  g_return_val_if_fail (GOA_IS_OBJECT (object), NULL);
  g_return_val_if_fail (datasource != NULL && datasource[0] != '\0', NULL);

  data = g_slice_new0 (PhotosOnlineMinerRefreshAccountData);
  data->identifier_to_urn_old = g_hash_table_new_full (g_str_hash,
                                                       g_str_equal,
                                                       (GDestroyNotify) g_free,
                                                       (GDestroyNotify) g_free);
  data->object = g_object_ref (object);
  data->datasource = g_strdup (datasource);

  return data;
}


static void
photos_online_miner_refresh_account_data_free (PhotosOnlineMinerRefreshAccountData *data)
{
  g_object_unref (data->identifier_to_urn_old);
  g_object_unref (data->object);
  g_free (data->datasource);
  g_slice_free (PhotosOnlineMinerRefreshAccountData, data);
}


static PhotosOnlineMinerRefreshDBData *
photos_online_miner_refresh_db_data_new (GList *accounts_selected, GTask *cache_task)
{
  PhotosOnlineMinerRefreshDBData *data;

  g_return_val_if_fail (G_IS_TASK (cache_task), NULL);

  data = g_slice_new0 (PhotosOnlineMinerRefreshDBData);
  data->accounts_selected = g_list_copy_deep (accounts_selected, (GCopyFunc) g_object_ref, NULL);
  data->accounts_to_be_indexed = g_ptr_array_new_with_free_func (g_object_unref);
  data->datasources_to_be_deleted = g_ptr_array_new_with_free_func (g_free);
  data->cache_task = g_object_ref (cache_task);

  return data;
}


static void
photos_online_miner_refresh_db_data_free (PhotosOnlineMinerRefreshDBData *data)
{
  g_list_free_full (data->accounts_selected, g_object_unref);
  g_ptr_array_unref (data->accounts_to_be_indexed);
  g_ptr_array_unref (data->datasources_to_be_deleted);
  g_object_unref (data->cache_task);
  g_slice_free (PhotosOnlineMinerRefreshDBData, data);
}


static gboolean
photos_online_miner_authorize_authenticated_peer (PhotosOnlineMiner *self,
                                                  GIOStream *iostream,
                                                  GCredentials *credentials)
{
  g_autoptr (GCredentials) own_credentials = NULL;
  gboolean ret_val = FALSE;

  if (credentials == NULL)
    goto out;

  own_credentials = g_credentials_new ();

  {
    g_autoptr (GError) error = NULL;

    if (!g_credentials_is_same_user (credentials, own_credentials, &error))
      {
        g_warning ("Unable to authorize peer: %s", error->message);
        goto out;
      }
  }

  ret_val = TRUE;

 out:
  return ret_val;
}


static gchar *
photos_online_miner_create_datasource_from_account (GoaObject *object)
{
  GoaAccount *account;
  const gchar *id;
  gchar *ret_val = NULL;

  account = goa_object_peek_account (object);
  if (account == NULL)
    goto out;

  id = goa_account_get_id (account);
  ret_val = g_strdup_printf ("photos:goa-account:%s", id);

 out:
  return ret_val;
}


static gint
photos_online_miner_compare_account_with_datasource (gconstpointer a, gconstpointer b)
{
  GoaObject *object = GOA_OBJECT (a);
  const gchar *datasource = (const gchar *) b;
  g_autofree gchar *datasource_object = NULL;
  gint ret_val;

  datasource_object = photos_online_miner_create_datasource_from_account (object);
  ret_val = g_strcmp0 (datasource_object, datasource);
  return ret_val;
}


static void
photos_online_miner_create_datasources_update_array (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  g_autoptr (GTask) task = G_TASK (user_data);
  TrackerSparqlConnection *sparql_connection = TRACKER_SPARQL_CONNECTION (source_object);

  {
    g_autoptr (GError) error = NULL;

    if (!tracker_sparql_connection_update_array_finish (sparql_connection, res, &error))
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
photos_online_miner_create_datasources_async (PhotosOnlineMiner *self,
                                              GPtrArray *accounts,
                                              GCancellable *cancellable,
                                              GAsyncReadyCallback callback,
                                              gpointer user_data)
{
  PhotosOnlineMinerPrivate *priv;
  g_autoptr (GPtrArray) sparql_array = NULL;
  g_autoptr (GTask) task = NULL;
  guint i;

  g_return_if_fail (PHOTOS_IS_ONLINE_MINER (self));
  priv = photos_online_miner_get_instance_private (self);

  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, photos_online_miner_create_datasources_async);

  sparql_array = g_ptr_array_new_full (accounts->len, g_free);

  for (i = 0; i < accounts->len; i++)
    {
      GoaObject *object = GOA_OBJECT (accounts->pdata[i]);
      g_autofree gchar *datasource = NULL;
      g_autofree gchar *sparql = NULL;

      datasource = photos_online_miner_create_datasource_from_account (object);
      if (datasource == NULL)
        continue;

      sparql = g_strdup_printf ("INSERT OR REPLACE INTO tracker:Pictures { "
                                "  <%s> a nie:DataSource ; nao:identifier \"%s\" . "
                                "  <%s:root-element> a nie:InformationElement ; "
                                "    nie:rootElementOf <%s> ; "
                                "    nie:version \"%d\" . "
                                "}",
                                datasource,
                                PHOTOS_ONLINE_MINER_GET_CLASS (self)->identifier,
                                datasource,
                                datasource,
                                PHOTOS_ONLINE_MINER_GET_CLASS (self)->version);

      g_ptr_array_add (sparql_array, g_steal_pointer (&sparql));
    }

  tracker_sparql_connection_update_array_async (priv->sparql_connection,
                                                (gchar **) sparql_array->pdata,
                                                (gint) sparql_array->len,
                                                cancellable,
                                                photos_online_miner_create_datasources_update_array,
                                                g_object_ref (task));
}


static gboolean
photos_online_miner_create_datasources_finish (PhotosOnlineMiner *self, GAsyncResult *res, GError **error)
{
  GTask *task;

  g_return_val_if_fail (PHOTOS_IS_ONLINE_MINER (self), FALSE);

  g_return_val_if_fail (g_task_is_valid (res, self), FALSE);
  task = G_TASK (res);

  g_return_val_if_fail (g_task_get_source_tag (task) == photos_online_miner_create_datasources_async, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  return g_task_propagate_boolean (task, error);
}


static void
photos_online_miner_delete_datasources_update_array (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  g_autoptr (GTask) task = G_TASK (user_data);
  TrackerSparqlConnection *sparql_connection = TRACKER_SPARQL_CONNECTION (source_object);

  {
    g_autoptr (GError) error = NULL;

    if (!tracker_sparql_connection_update_array_finish (sparql_connection, res, &error))
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
photos_online_miner_delete_datasources_async (PhotosOnlineMiner *self,
                                              GPtrArray *datasources,
                                              GCancellable *cancellable,
                                              GAsyncReadyCallback callback,
                                              gpointer user_data)
{
  PhotosOnlineMinerPrivate *priv;
  g_autoptr (GPtrArray) sparql_array = NULL;
  g_autoptr (GTask) task = NULL;
  guint i;

  g_return_if_fail (PHOTOS_IS_ONLINE_MINER (self));
  priv = photos_online_miner_get_instance_private (self);

  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, photos_online_miner_delete_datasources_async);

  sparql_array = g_ptr_array_new_full (datasources->len, g_free);

  for (i = 0; i < datasources->len; i++)
    {
      const gchar *datasource = (gchar *) datasources->pdata[i];
      g_autofree gchar *sparql = NULL;

      sparql = g_strdup_printf ("WITH tracker:Pictures "
                                "DELETE { ?u a rds:Resource } "
                                "WHERE { ?u nie:dataSource <%s> }",
                                datasource);

      g_ptr_array_add (sparql_array, g_steal_pointer (&sparql));
    }

  tracker_sparql_connection_update_array_async (priv->sparql_connection,
                                                (gchar **) sparql_array->pdata,
                                                (gint) sparql_array->len,
                                                cancellable,
                                                photos_online_miner_delete_datasources_update_array,
                                                g_object_ref (task));
}


static gboolean
photos_online_miner_delete_datasources_finish (PhotosOnlineMiner *self, GAsyncResult *res, GError **error)
{
  GTask *task;

  g_return_val_if_fail (PHOTOS_IS_ONLINE_MINER (self), FALSE);

  g_return_val_if_fail (g_task_is_valid (res, self), FALSE);
  task = G_TASK (res);

  g_return_val_if_fail (g_task_get_source_tag (task) == photos_online_miner_delete_datasources_async, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  return g_task_propagate_boolean (task, error);
}


static void
photos_online_miner_refresh_account_refresh_account (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosOnlineMiner *self = PHOTOS_ONLINE_MINER (source_object);
  g_autoptr (GTask) task = G_TASK (user_data);

  {
    g_autoptr (GError) error = NULL;

    if (!PHOTOS_ONLINE_MINER_GET_CLASS (self)->refresh_account_finish (self, res, &error))
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
photos_online_miner_refresh_account_cursor_next_old (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosOnlineMiner *self;
  GCancellable *cancellable;
  g_autoptr (GTask) task = G_TASK (user_data);
  PhotosOnlineMinerRefreshAccountData *data;
  TrackerSparqlCursor *cursor = TRACKER_SPARQL_CURSOR (source_object);
  gboolean success;
  const gchar *identifier;
  const gchar *urn;

  self = PHOTOS_ONLINE_MINER (g_task_get_source_object (task));
  cancellable = g_task_get_cancellable (task);
  data = (PhotosOnlineMinerRefreshAccountData *) g_task_get_task_data (task);

  {
    g_autoptr (GError) error = NULL;

    /* Note that tracker_sparql_cursor_next_finish can return FALSE even
     * without an error.
     */
    success = tracker_sparql_cursor_next_finish (cursor, res, &error);
    if (error != NULL)
      {
        g_task_return_error (task, g_steal_pointer (&error));
        goto out;
      }
  }

  if (!success)
    {
      PhotosOnlineMinerClass *class;

      class = PHOTOS_ONLINE_MINER_GET_CLASS (self);
      class->refresh_account_async (self,
                                    data->identifier_to_urn_old,
                                    data->object,
                                    data->datasource,
                                    cancellable,
                                    photos_online_miner_refresh_account_refresh_account,
                                    g_object_ref (task));

      goto out;
    }

  urn = tracker_sparql_cursor_get_string (cursor, 0, NULL);
  identifier = tracker_sparql_cursor_get_string (cursor, 1, NULL);
  g_hash_table_insert (data->identifier_to_urn_old, g_strdup (identifier), g_strdup (urn));

  tracker_sparql_cursor_next_async (cursor,
                                    cancellable,
                                    photos_online_miner_refresh_account_cursor_next_old,
                                    g_object_ref (task));

 out:
  return;
}


static void
photos_online_miner_refresh_account_query_old (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  GCancellable *cancellable;
  g_autoptr (GTask) task = G_TASK (user_data);
  TrackerSparqlConnection *sparql_connection = TRACKER_SPARQL_CONNECTION (source_object);
  g_autoptr (TrackerSparqlCursor) cursor = NULL;

  cancellable = g_task_get_cancellable (task);

  {
    g_autoptr (GError) error = NULL;

    cursor = tracker_sparql_connection_query_finish (sparql_connection, res, &error);
    if (error != NULL)
      {
        g_task_return_error (task, g_steal_pointer (&error));
        goto out;
      }
  }

  tracker_sparql_cursor_next_async (cursor,
                                    cancellable,
                                    photos_online_miner_refresh_account_cursor_next_old,
                                    g_object_ref (task));

 out:
  return;
}


static void
photos_online_miner_refresh_account_async (PhotosOnlineMiner *self,
                                           GoaObject *object,
                                           GCancellable *cancellable,
                                           GAsyncReadyCallback callback,
                                           gpointer user_data)
{
  PhotosOnlineMinerPrivate *priv;
  g_autoptr (GTask) task = NULL;
  PhotosOnlineMinerRefreshAccountData *data;
  g_autofree gchar *datasource = NULL;
  g_autofree gchar *sparql = NULL;

  g_return_if_fail (PHOTOS_IS_ONLINE_MINER (self));
  priv = photos_online_miner_get_instance_private (self);

  g_return_if_fail (GOA_IS_OBJECT (object));
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, photos_online_miner_refresh_account_async);

  datasource = photos_online_miner_create_datasource_from_account (object);
  if (datasource == NULL)
    {
      g_task_return_new_error (task, PHOTOS_ERROR, 0, "Failed to get the nie:DataSource URN");
      goto out;
    }

  data = photos_online_miner_refresh_account_data_new (object, datasource);
  g_task_set_task_data (task, data, (GDestroyNotify) photos_online_miner_refresh_account_data_free);

  sparql = g_strdup_printf ("SELECT ?urn nao:identifier(?urn) FROM tracker:Pictures WHERE {"
                            "  ?urn nie:dataSource <%s> "
                            "}",
                            datasource);

  tracker_sparql_connection_query_async (priv->sparql_connection,
                                         sparql,
                                         cancellable,
                                         photos_online_miner_refresh_account_query_old,
                                         g_object_ref (task));

 out:
  return;
}


static gboolean
photos_online_miner_refresh_account_finish (PhotosOnlineMiner *self, GAsyncResult *res, GError **error)
{
  GTask *task;

  g_return_val_if_fail (PHOTOS_IS_ONLINE_MINER (self), FALSE);

  g_return_val_if_fail (g_task_is_valid (res, self), FALSE);
  task = G_TASK (res);

  g_return_val_if_fail (g_task_get_source_tag (task) == photos_online_miner_refresh_account_async, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  return g_task_propagate_boolean (task, error);
}


static gboolean
photos_online_miner_handle_cancel (PhotosOnlineMiner *self, GDBusMethodInvocation *invocation, guint serial)
{
  PhotosOnlineMinerPrivate *priv;
  GCancellable *cancellable;
  GDBusConnection *connection;
  GDBusMethodInvocation *invocation_ongoing;
  GHashTableIter iter;
  const gchar *type_name;

  g_return_val_if_fail (PHOTOS_IS_ONLINE_MINER (self), FALSE);
  priv = photos_online_miner_get_instance_private (self);

  g_return_val_if_fail (G_IS_DBUS_METHOD_INVOCATION (invocation), FALSE);

  type_name = G_OBJECT_TYPE_NAME (self);
  photos_debug (PHOTOS_DEBUG_ONLINE_MINER, "%s handling Cancel for %u", type_name, serial);

  g_application_hold (G_APPLICATION (self));

  connection = g_dbus_method_invocation_get_connection (invocation);

  g_hash_table_iter_init (&iter, priv->cancellables);
  while (g_hash_table_iter_next (&iter, (gpointer *) &invocation_ongoing, (gpointer *) &cancellable))
    {
      GDBusConnection *connection_ongoing;
      GDBusMessage *message_ongoing;
      guint32 serial_ongoing;

      connection_ongoing = g_dbus_method_invocation_get_connection (invocation_ongoing);
      message_ongoing = g_dbus_method_invocation_get_message (invocation_ongoing);
      serial_ongoing = g_dbus_message_get_serial (message_ongoing);

      if (connection == connection_ongoing && (guint32) serial == serial_ongoing)
        {
          g_cancellable_cancel (cancellable);
          photos_online_miner_dbus_complete_cancel (priv->skeleton, invocation);
          goto out;
        }
    }

  g_dbus_method_invocation_return_error_literal (invocation, PHOTOS_ERROR, 0, "Invalid serial");

 out:
  photos_debug (PHOTOS_DEBUG_ONLINE_MINER, "%s completed Cancel", type_name);
  g_application_release (G_APPLICATION (self));
  return TRUE;
}


static void
photos_online_miner_handle_insert_shared_content_insert_shared_content (GObject *source_object,
                                                                        GAsyncResult *res,
                                                                        gpointer user_data)
{
  PhotosOnlineMiner *self = PHOTOS_ONLINE_MINER (source_object);
  PhotosOnlineMinerPrivate *priv;
  g_autoptr (GDBusMethodInvocation) invocation = G_DBUS_METHOD_INVOCATION (user_data);
  const gchar *type_name;

  priv = photos_online_miner_get_instance_private (self);

  /* { */
  /*   g_autoptr (GError) error = NULL; */

  /*   if (!photos_online_miner_google_insert_shared_content_finish (self, res, &error)) */
  /*     { */
  /*       g_dbus_method_invocation_take_error (invocation, g_steal_pointer (&error)); */
  /*       goto out; */
  /*     } */
  /* } */

  photos_online_miner_dbus_complete_insert_shared_content (priv->skeleton, invocation);

 out:
  type_name = G_OBJECT_TYPE_NAME (self);
  photos_debug (PHOTOS_DEBUG_ONLINE_MINER, "%s completed InsertSharedContent", type_name);

  g_application_release (G_APPLICATION (self));
  g_hash_table_remove (priv->cancellables, invocation);
}


static gboolean
photos_online_miner_handle_insert_shared_content (PhotosOnlineMiner *self,
                                                  GDBusMethodInvocation *invocation,
                                                  const gchar *account_id,
                                                  const gchar *shared_id,
                                                  const gchar *source_urn)
{
  PhotosOnlineMinerPrivate *priv;
  g_autoptr (GCancellable) cancellable = NULL;
  const gchar *type_name;

  g_return_val_if_fail (PHOTOS_IS_ONLINE_MINER (self), FALSE);
  priv = photos_online_miner_get_instance_private (self);

  g_return_val_if_fail (G_IS_DBUS_METHOD_INVOCATION (invocation), FALSE);
  g_return_val_if_fail (account_id != NULL && account_id[0] != '\0', FALSE);
  g_return_val_if_fail (shared_id != NULL && shared_id[0] != '\0', FALSE);
  g_return_val_if_fail (source_urn != NULL && source_urn[0] != '\0', FALSE);

  type_name = G_OBJECT_TYPE_NAME (self);
  photos_debug (PHOTOS_DEBUG_ONLINE_MINER, "%s handling InsertSharedContent", type_name);

  cancellable = g_cancellable_new ();
  g_hash_table_insert (priv->cancellables, g_object_ref (invocation), g_object_ref (cancellable));

  g_application_hold (G_APPLICATION (self));

  return TRUE;
}


static void
photos_online_miner_handle_refresh_db_task_cache_populate_refresh_account (GObject *source_object,
                                                                           GAsyncResult *res,
                                                                           gpointer user_data)
{
  PhotosOnlineMiner *self = PHOTOS_ONLINE_MINER (source_object);
  GCancellable *cancellable;
  g_autoptr (GTask) task = G_TASK (user_data);
  GoaObject *object;
  PhotosOnlineMinerRefreshDBData *data;

  cancellable = g_task_get_cancellable (task);
  data = (PhotosOnlineMinerRefreshDBData *) g_task_get_task_data (task);

  object = GOA_OBJECT (data->accounts_to_be_indexed->pdata[0]);

  {
    g_autoptr (GError) error = NULL;

    if (!photos_online_miner_refresh_account_finish (self, res, &error))
      {
        GoaAccount *account;
        g_autofree gchar *error_details = NULL;

        if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
          {
            g_task_return_error (data->cache_task, g_steal_pointer (&error));
            goto out;
          }

        account = goa_object_peek_account (object);
        if (account != NULL)
          {
            const gchar *id;

            id = goa_account_get_id (account);
            error_details = g_strdup_printf (" for %s", id);
          }

        g_warning ("Unable to update the cache%s: %s", error_details == NULL ? "" : error_details, error->message);
      }
  }

  g_ptr_array_remove_index (data->accounts_to_be_indexed, 0U);
  if (data->accounts_to_be_indexed->len == 0U)
    {
      g_task_return_pointer (data->cache_task, GINT_TO_POINTER (TRUE), NULL);
      goto out;
    }

  object = GOA_OBJECT (data->accounts_to_be_indexed->pdata[0]);
  photos_online_miner_refresh_account_async (
    self,
    object,
    cancellable,
    photos_online_miner_handle_refresh_db_task_cache_populate_refresh_account,
    g_object_ref (task));

 out:
  return;
}


static void
photos_online_miner_handle_refresh_db_task_cache_populate_create (GObject *source_object,
                                                                  GAsyncResult *res,
                                                                  gpointer user_data)
{
  PhotosOnlineMiner *self = PHOTOS_ONLINE_MINER (source_object);
  GCancellable *cancellable;
  g_autoptr (GTask) task = G_TASK (user_data);
  GoaObject *object;
  PhotosOnlineMinerRefreshDBData *data;

  cancellable = g_task_get_cancellable (task);
  data = (PhotosOnlineMinerRefreshDBData *) g_task_get_task_data (task);

  {
    g_autoptr (GError) error = NULL;

    if (!photos_online_miner_create_datasources_finish (self, res, &error))
      {
        g_task_return_error (data->cache_task, g_steal_pointer (&error));
        goto out;
      }
  }

  if (data->accounts_to_be_indexed->len == 0U)
    {
      g_task_return_pointer (data->cache_task, GINT_TO_POINTER (TRUE), NULL);
      goto out;
    }

  object = GOA_OBJECT (data->accounts_to_be_indexed->pdata[0]);
  photos_online_miner_refresh_account_async (
    self,
    object,
    cancellable,
    photos_online_miner_handle_refresh_db_task_cache_populate_refresh_account,
    g_object_ref (task));

 out:
  return;
}


static void
photos_online_miner_handle_refresh_db_task_cache_populate_delete (GObject *source_object,
                                                                  GAsyncResult *res,
                                                                  gpointer user_data)
{
  PhotosOnlineMiner *self = PHOTOS_ONLINE_MINER (source_object);
  GCancellable *cancellable;
  g_autoptr (GTask) task = G_TASK (user_data);
  PhotosOnlineMinerRefreshDBData *data;
  GList *l;

  cancellable = g_task_get_cancellable (task);
  data = (PhotosOnlineMinerRefreshDBData *) g_task_get_task_data (task);

  {
    g_autoptr (GError) error = NULL;

    if (!photos_online_miner_delete_datasources_finish (self, res, &error))
      {
        g_task_return_error (data->cache_task, g_steal_pointer (&error));
        goto out;
      }
  }

  g_assert (data->accounts_to_be_indexed->len == 0U);

  for (l = data->accounts_selected; l != NULL; l = l->next)
    {
      GoaObject *object = GOA_OBJECT (l->data);
      GoaPhotos *photos;

      photos = goa_object_peek_photos (object);
      if (photos == NULL)
        continue;

      g_ptr_array_add (data->accounts_to_be_indexed, g_object_ref (object));
    }

  photos_online_miner_create_datasources_async (self,
                                                data->accounts_to_be_indexed,
                                                cancellable,
                                                photos_online_miner_handle_refresh_db_task_cache_populate_create,
                                                g_object_ref (task));

 out:
  return;
}


static void
photos_online_miner_handle_refresh_db_task_cache_populate_cursor_next_old (GObject *source_object,
                                                                           GAsyncResult *res,
                                                                           gpointer user_data)
{
  PhotosOnlineMiner *self;
  GCancellable *cancellable;
  g_autoptr (GTask) task = G_TASK (user_data);
  TrackerSparqlCursor *cursor = TRACKER_SPARQL_CURSOR (source_object);
  PhotosOnlineMinerRefreshDBData *data;
  gboolean success;
  const gchar *datasource;
  const gchar *version_old;
  g_autofree gchar *version_current = NULL;

  self = PHOTOS_ONLINE_MINER (g_task_get_source_object (task));
  cancellable = g_task_get_cancellable (task);
  data = (PhotosOnlineMinerRefreshDBData *) g_task_get_task_data (task);

  {
    g_autoptr (GError) error = NULL;

    /* Note that tracker_sparql_cursor_next_finish can return FALSE even
     * without an error.
     */
    success = tracker_sparql_cursor_next_finish (cursor, res, &error);
    if (error != NULL)
      {
        g_task_return_error (data->cache_task, g_steal_pointer (&error));
        goto out;
      }
  }

  if (!success)
    {
      photos_online_miner_delete_datasources_async (self,
                                                    data->datasources_to_be_deleted,
                                                    cancellable,
                                                    photos_online_miner_handle_refresh_db_task_cache_populate_delete,
                                                    g_object_ref (task));
      goto out;
    }

  datasource = tracker_sparql_cursor_get_string (cursor, 0, NULL);
  version_old = tracker_sparql_cursor_get_string (cursor, 1, NULL);

  version_current = g_strdup_printf ("%u", PHOTOS_ONLINE_MINER_GET_CLASS (self)->version);

  if (g_list_find_custom (data->accounts_selected,
                          datasource,
                          photos_online_miner_compare_account_with_datasource) == NULL)
    {
      const gchar *type_name;

      type_name = G_OBJECT_TYPE_NAME (self);
      photos_debug (PHOTOS_DEBUG_ONLINE_MINER, "%s: Account not found for nie:DataSource %s", type_name, datasource);
      g_ptr_array_add (data->datasources_to_be_deleted, g_strdup (datasource));
    }
  else if (g_strcmp0 (version_old, version_current) != 0)
    {
      const gchar *type_name;

      type_name = G_OBJECT_TYPE_NAME (self);
      photos_debug (PHOTOS_DEBUG_ONLINE_MINER,
                    "%s: Version mismatch (%s, %s) for nie:DataSource %s",
                    type_name,
                    version_old,
                    version_current,
                    datasource);

      g_ptr_array_add (data->datasources_to_be_deleted, g_strdup (datasource));
    }

  tracker_sparql_cursor_next_async (cursor,
                                    cancellable,
                                    photos_online_miner_handle_refresh_db_task_cache_populate_cursor_next_old,
                                    g_object_ref (task));

 out:
  return;
}


static void
photos_online_miner_handle_refresh_db_task_cache_populate_query_old (GObject *source_object,
                                                                     GAsyncResult *res,
                                                                     gpointer user_data)
{
  PhotosOnlineMiner *self;
  PhotosOnlineMinerPrivate *priv;
  GCancellable *cancellable;
  g_autoptr (GTask) task = G_TASK (user_data);
  g_autoptr (TrackerSparqlCursor) cursor = NULL;
  PhotosOnlineMinerRefreshDBData *data;

  self = PHOTOS_ONLINE_MINER (g_task_get_source_object (task));
  priv = photos_online_miner_get_instance_private (self);

  cancellable = g_task_get_cancellable (task);
  data = (PhotosOnlineMinerRefreshDBData *) g_task_get_task_data (task);

  {
    g_autoptr (GError) error = NULL;

    cursor = tracker_sparql_connection_query_finish (priv->sparql_connection, res, &error);
    if (error != NULL)
      {
        g_task_return_error (data->cache_task, g_steal_pointer (&error));
        goto out;
      }
  }

  g_assert (data->datasources_to_be_deleted->len == 0U);

  tracker_sparql_cursor_next_async (cursor,
                                    cancellable,
                                    photos_online_miner_handle_refresh_db_task_cache_populate_cursor_next_old,
                                    g_object_ref (task));

 out:
  return;
}


static void
photos_online_miner_handle_refresh_db_task_cache_populate (DzlTaskCache *cache,
                                                           gconstpointer key,
                                                           GTask *cache_task,
                                                           gpointer user_data)
{
  PhotosOnlineMiner *self = PHOTOS_ONLINE_MINER ((gpointer) key);
  PhotosOnlineMinerPrivate *priv;
  GCancellable *cancellable;
  g_autolist (GoaObject) accounts = NULL;
  g_autolist (GoaObject) accounts_selected = NULL;
  GList *l;
  g_autoptr (GTask) task = NULL;
  PhotosOnlineMinerRefreshDBData *data;
  g_autofree gchar *sparql = NULL;

  priv = photos_online_miner_get_instance_private (self);
  cancellable = g_task_get_cancellable (cache_task);

  accounts = goa_client_get_accounts (priv->client);
  for (l = accounts; l != NULL; l = l->next)
    {
      GoaAccount *account;
      GoaObject *object = GOA_OBJECT (l->data);
      const gchar *provider_type;

      account = goa_object_peek_account (object);
      if (account == NULL)
        continue;

      provider_type = goa_account_get_provider_type (account);
      if (g_strcmp0 (provider_type, PHOTOS_ONLINE_MINER_GET_CLASS (self)->provider_type) != 0)
        continue;

      accounts_selected = g_list_prepend (accounts_selected, g_object_ref (object));
    }

  data = photos_online_miner_refresh_db_data_new (accounts_selected, cache_task);

  task = g_task_new (self, cancellable, NULL, NULL);
  g_task_set_task_data (task, data, (GDestroyNotify) photos_online_miner_refresh_db_data_free);

  sparql = g_strdup_printf ("SELECT ?datasource nie:version (?root) FROM tracker:Pictures WHERE {"
                            "  ?datasource a nie:DataSource ; nao:identifier \"%s\" ."
                            "  ?root a nie:InformationElement ; nie:rootElementOf ?datasource ."
                            "}",
                            PHOTOS_ONLINE_MINER_GET_CLASS (self)->identifier);

  tracker_sparql_connection_query_async (priv->sparql_connection,
                                         sparql,
                                         cancellable,
                                         photos_online_miner_handle_refresh_db_task_cache_populate_query_old,
                                         g_object_ref (task));
}


static void
photos_online_miner_handle_refresh_db_task_cache_get (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosOnlineMiner *self;
  PhotosOnlineMinerPrivate *priv;
  DzlTaskCache *cache = DZL_TASK_CACHE (source_object);
  g_autoptr (GDBusMethodInvocation) invocation = G_DBUS_METHOD_INVOCATION (user_data);
  const gchar *method_name;
  const gchar *type_name;

  g_assert (cache == refresh_db_cache);

  self = PHOTOS_ONLINE_MINER (g_application_get_default ());
  priv = photos_online_miner_get_instance_private (self);

  {
    g_autoptr (GError) error = NULL;

    /* Semantically, this should return a gboolean, but DzlTaskCache
     * doesn't support gboolean return values, only gpointers. It's
     * easier to rely on the GError than converting the gpointer into
     * a gboolean.
     */
    dzl_task_cache_get_finish (cache, res, &error);
    if (error != NULL)
      {
        g_dbus_method_invocation_take_error (invocation, g_steal_pointer (&error));
        goto out;
      }
  }

  photos_online_miner_dbus_complete_refresh_db (priv->skeleton, invocation);

 out:
  method_name = g_dbus_method_invocation_get_method_name (invocation);
  type_name = G_OBJECT_TYPE_NAME (self);
  photos_debug (PHOTOS_DEBUG_ONLINE_MINER, "%s completed %s", type_name, method_name);

  g_application_release (G_APPLICATION (self));
  dzl_task_cache_evict (cache, self);
  g_hash_table_remove (priv->cancellables, invocation);
}


static gboolean
photos_online_miner_handle_refresh_db (PhotosOnlineMiner *self, GDBusMethodInvocation *invocation)
{
  PhotosOnlineMinerPrivate *priv;
  g_autoptr (GCancellable) cancellable = NULL;
  const gchar *method_name;
  const gchar *type_name;

  g_return_val_if_fail (PHOTOS_IS_ONLINE_MINER (self), FALSE);
  priv = photos_online_miner_get_instance_private (self);

  g_return_val_if_fail (G_IS_DBUS_METHOD_INVOCATION (invocation), FALSE);

  method_name = g_dbus_method_invocation_get_method_name (invocation);
  type_name = G_OBJECT_TYPE_NAME (self);
  photos_debug (PHOTOS_DEBUG_ONLINE_MINER, "%s handling %s", type_name, method_name);

  if (G_UNLIKELY (priv->client == NULL))
    {
      g_dbus_method_invocation_take_error (invocation, g_error_copy (priv->client_error));
      goto out;
    }

  cancellable = g_cancellable_new ();
  g_hash_table_insert (priv->cancellables, g_object_ref (invocation), g_object_ref (cancellable));

  g_application_hold (G_APPLICATION (self));
  dzl_task_cache_get_async (refresh_db_cache,
                            self,
                            FALSE,
                            cancellable,
                            photos_online_miner_handle_refresh_db_task_cache_get,
                            g_object_ref (invocation));

 out:
  return TRUE;
}


static gboolean
photos_online_miner_dbus_register (GApplication *application,
                                   GDBusConnection *connection,
                                   const gchar *object_path,
                                   GError **error)
{
  PhotosOnlineMiner *self = PHOTOS_ONLINE_MINER (application);
  PhotosOnlineMinerPrivate *priv;
  g_autoptr (GDBusAuthObserver) observer = NULL;
  gboolean ret_val = FALSE;

  priv = photos_online_miner_get_instance_private (self);

  g_return_val_if_fail (priv->skeleton == NULL, FALSE);

  if (!G_APPLICATION_CLASS (photos_online_miner_parent_class)->dbus_register (application,
                                                                              connection,
                                                                              object_path,
                                                                              error))
    {
      goto out;
    }

  observer = g_dbus_auth_observer_new ();
  g_signal_connect_swapped (observer,
                            "authorize-authenticated-peer",
                            G_CALLBACK (photos_online_miner_authorize_authenticated_peer),
                            self);

  priv->dbus_connection = g_dbus_connection_new_for_address_sync (priv->address,
                                                                  G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT,
                                                                  observer,
                                                                  NULL,
                                                                  error);
  if (priv->dbus_connection == NULL)
    goto out;

  priv->skeleton = photos_online_miner_dbus_skeleton_new ();
  g_signal_connect_swapped (priv->skeleton, "handle-cancel", G_CALLBACK (photos_online_miner_handle_cancel), self);
  g_signal_connect_swapped (priv->skeleton,
                            "handle-insert-shared-content",
                            G_CALLBACK (photos_online_miner_handle_insert_shared_content),
                            self);
  g_signal_connect_swapped (priv->skeleton,
                            "handle-refresh-db",
                            G_CALLBACK (photos_online_miner_handle_refresh_db),
                            self);

  if (!g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (priv->skeleton),
                                         priv->dbus_connection,
                                         ONLINE_MINER_PATH,
                                         error))
    {
      g_clear_object (&priv->skeleton);
      goto out;
    }

  priv->sparql_connection = tracker_sparql_connection_bus_new (NULL, NULL, priv->dbus_connection, error);
  if (priv->sparql_connection == NULL)
    goto out;

  ret_val = TRUE;

 out:
  return ret_val;
}


static void
photos_online_miner_dbus_unregister (GApplication *application,
                                     GDBusConnection *connection,
                                     const gchar *object_path)
{
  PhotosOnlineMiner *self = PHOTOS_ONLINE_MINER (application);
  PhotosOnlineMinerPrivate *priv;

  priv = photos_online_miner_get_instance_private (self);

  if (priv->skeleton != NULL)
    {
      g_dbus_interface_skeleton_unexport_from_connection (G_DBUS_INTERFACE_SKELETON (priv->skeleton),
                                                          priv->dbus_connection);
      g_clear_object (&priv->skeleton);
    }

  G_APPLICATION_CLASS (photos_online_miner_parent_class)->dbus_unregister (application, connection, object_path);
}


static gint
photos_online_miner_handle_local_options (GApplication *application, GVariantDict *options)
{
  PhotosOnlineMiner *self = PHOTOS_ONLINE_MINER (application);
  PhotosOnlineMinerPrivate *priv;
  gint ret_val = EXIT_FAILURE;

  priv = photos_online_miner_get_instance_private (self);

  if (g_variant_dict_lookup (options, "address", "s", &priv->address))
    ret_val = -1;

  return ret_val;
}


static void
photos_online_miner_shutdown (GApplication *application)
{
  const gchar *type_name;

  type_name = G_OBJECT_TYPE_NAME (application);
  photos_debug (PHOTOS_DEBUG_ONLINE_MINER, "%s exiting", type_name);

  G_APPLICATION_CLASS (photos_online_miner_parent_class)->shutdown (application);
}


static void
photos_online_miner_startup (GApplication *application)
{
  const gchar *type_name;

  G_APPLICATION_CLASS (photos_online_miner_parent_class)->startup (application);

  type_name = G_OBJECT_TYPE_NAME (application);
  photos_debug (PHOTOS_DEBUG_ONLINE_MINER, "%s ready", type_name);
}


static void
photos_online_miner_dispose (GObject *object)
{
  PhotosOnlineMiner *self = PHOTOS_ONLINE_MINER (object);
  PhotosOnlineMinerPrivate *priv;

  priv = photos_online_miner_get_instance_private (self);

  g_assert (priv->skeleton == NULL);

  dzl_task_cache_evict (refresh_db_cache, self);

  g_clear_object (&priv->dbus_connection);
  g_clear_object (&priv->client);
  g_clear_object (&priv->sparql_connection);
  g_clear_pointer (&priv->cancellables, g_hash_table_unref);

  G_OBJECT_CLASS (photos_online_miner_parent_class)->dispose (object);
}


static void
photos_online_miner_finalize (GObject *object)
{
  PhotosOnlineMiner *self = PHOTOS_ONLINE_MINER (object);
  PhotosOnlineMinerPrivate *priv;

  priv = photos_online_miner_get_instance_private (self);

  g_clear_error (&priv->client_error);
  g_free (priv->address);

  G_OBJECT_CLASS (photos_online_miner_parent_class)->finalize (object);
}


static void
photos_online_miner_init (PhotosOnlineMiner *self)
{
  PhotosOnlineMinerPrivate *priv;

  priv = photos_online_miner_get_instance_private (self);

  setlocale (LC_ALL, "");

  bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);

  g_application_add_main_option_entries (G_APPLICATION (self), COMMAND_LINE_OPTIONS);
  g_application_set_flags (G_APPLICATION (self), G_APPLICATION_IS_SERVICE | G_APPLICATION_NON_UNIQUE);
  g_application_set_inactivity_timeout (G_APPLICATION (self), INACTIVITY_TIMEOUT);

  priv->cancellables = g_hash_table_new_full (g_direct_hash, g_direct_equal, g_object_unref, g_object_unref);
  priv->client = goa_client_new_sync (NULL, &priv->client_error);
}


static void
photos_online_miner_class_init (PhotosOnlineMinerClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GApplicationClass *application_class = G_APPLICATION_CLASS (class);

  object_class->dispose = photos_online_miner_dispose;
  object_class->finalize = photos_online_miner_finalize;
  application_class->dbus_register = photos_online_miner_dbus_register;
  application_class->dbus_unregister = photos_online_miner_dbus_unregister;
  application_class->handle_local_options = photos_online_miner_handle_local_options;
  application_class->shutdown = photos_online_miner_shutdown;
  application_class->startup = photos_online_miner_startup;

  refresh_db_cache = dzl_task_cache_new (g_direct_hash,
                                         g_direct_equal,
                                         NULL,
                                         NULL,
                                         NULL,
                                         NULL,
                                         0,
                                         photos_online_miner_handle_refresh_db_task_cache_populate,
                                         NULL,
                                         NULL);
  dzl_task_cache_set_name (refresh_db_cache, "RefreshDB cache");
}


TrackerSparqlConnection *
photos_online_miner_get_connection (PhotosOnlineMiner *self)
{
  PhotosOnlineMinerPrivate *priv;

  g_return_val_if_fail (PHOTOS_IS_ONLINE_MINER (self), NULL);
  priv = photos_online_miner_get_instance_private (self);

  return priv->sparql_connection;
}
