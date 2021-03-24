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

#include <gdata/gdata.h>
#include <goa/goa.h>
#include <tracker-sparql.h>

#include "photos-debug.h"
#include "photos-error.h"
#include "photos-online-miner-google.h"


struct _PhotosOnlineMinerGoogle
{
  PhotosOnlineMiner parent_instance;
};


G_DEFINE_TYPE (PhotosOnlineMinerGoogle, photos_online_miner_google, PHOTOS_TYPE_ONLINE_MINER);


typedef struct _PhotosOnlineMinerGoogleProcessAlbumData PhotosOnlineMinerGoogleProcessAlbumData;
typedef struct _PhotosOnlineMinerGoogleRefreshAccountData PhotosOnlineMinerGoogleRefreshAccountData;

struct _PhotosOnlineMinerGoogleProcessAlbumData
{
  GHashTable *identifier_to_urn_old;
  GDataPicasaWebAlbum *album;
  GDataPicasaWebService *service;
  gchar *datasource;
};

struct _PhotosOnlineMinerGoogleRefreshAccountData
{
  GList *current_album_link;
  GHashTable *identifier_to_urn_old;
  GDataFeed *feed;
  GDataPicasaWebService *service;
  GoaObject *object;
  gchar *datasource;
};


static PhotosOnlineMinerGoogleProcessAlbumData *
photos_online_miner_google_process_album_data_new (GHashTable *identifier_to_urn_old,
                                                   GDataPicasaWebAlbum *album,
                                                   GDataPicasaWebService *service,
                                                   const gchar *datasource)
{
  PhotosOnlineMinerGoogleProcessAlbumData *data;

  g_return_val_if_fail (GDATA_IS_PICASAWEB_ALBUM (album), NULL);
  g_return_val_if_fail (GDATA_IS_PICASAWEB_SERVICE (service), NULL);
  g_return_val_if_fail (datasource != NULL && datasource[0] != '\0', NULL);

  data = g_slice_new0 (PhotosOnlineMinerGoogleProcessAlbumData);
  data->identifier_to_urn_old = g_hash_table_ref (identifier_to_urn_old);
  data->album = g_object_ref (album);
  data->service = g_object_ref (service);
  data->datasource = g_strdup (datasource);

  return data;
}


static void
photos_online_miner_google_process_album_data_free (PhotosOnlineMinerGoogleProcessAlbumData *data)
{
  g_object_unref (data->identifier_to_urn_old);
  g_object_unref (data->album);
  g_object_unref (data->service);
  g_free (data->datasource);
  g_slice_free (PhotosOnlineMinerGoogleProcessAlbumData, data);
}


static PhotosOnlineMinerGoogleRefreshAccountData *
photos_online_miner_google_refresh_account_data_new (GHashTable *identifier_to_urn_old,
                                                     GDataPicasaWebService *service,
                                                     GoaObject *object,
                                                     const gchar *datasource)
{
  PhotosOnlineMinerGoogleRefreshAccountData *data;

  g_return_val_if_fail (GOA_IS_OBJECT (object), NULL);
  g_return_val_if_fail (GDATA_IS_PICASAWEB_SERVICE (service), NULL);
  g_return_val_if_fail (datasource != NULL && datasource[0] != '\0', NULL);

  data = g_slice_new0 (PhotosOnlineMinerGoogleRefreshAccountData);
  data->identifier_to_urn_old = g_hash_table_ref (identifier_to_urn_old);
  data->service = g_object_ref (service);
  data->object = g_object_ref (object);
  data->datasource = g_strdup (datasource);

  return data;
}


static void
photos_online_miner_google_refresh_account_data_free (PhotosOnlineMinerGoogleRefreshAccountData *data)
{
  g_clear_object (&data->feed);
  g_object_unref (data->identifier_to_urn_old);
  g_object_unref (data->service);
  g_object_unref (data->object);
  g_free (data->datasource);
  g_slice_free (PhotosOnlineMinerGoogleRefreshAccountData, data);
}


static gboolean
photos_online_miner_google_process_album (PhotosOnlineMinerGoogle *self,
                                          GDataPicasaWebService *service,
                                          GDataPicasaWebAlbum *album,
                                          GHashTable *identifier_to_urn_old,
                                          const gchar *datasource,
                                          GCancellable *cancellable,
                                          GError **error)
{
  g_return_val_if_fail (PHOTOS_IS_ONLINE_MINER_GOOGLE (self), FALSE);
  g_return_val_if_fail (GDATA_IS_PICASAWEB_SERVICE (service), FALSE);
  g_return_val_if_fail (GDATA_IS_PICASAWEB_ALBUM (album), FALSE);
  g_return_val_if_fail (datasource != NULL && datasource[0] != '\0', FALSE);
  g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  return TRUE;
}


static void
photos_online_miner_google_process_album_in_thread_func (GTask *task,
                                                         gpointer source_object,
                                                         gpointer task_data,
                                                         GCancellable *cancellable)
{
  PhotosOnlineMinerGoogle *self = PHOTOS_ONLINE_MINER_GOOGLE (source_object);
  PhotosOnlineMinerGoogleProcessAlbumData *data = (PhotosOnlineMinerGoogleProcessAlbumData *) task_data;

  {
    g_autoptr (GError) error = NULL;

    if (!photos_online_miner_google_process_album (self,
                                                   data->service,
                                                   data->album,
                                                   data->identifier_to_urn_old,
                                                   data->datasource,
                                                   cancellable,
                                                   &error))
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
photos_online_miner_google_process_album_async (PhotosOnlineMinerGoogle *self,
                                                GDataPicasaWebService *service,
                                                GDataPicasaWebAlbum *album,
                                                GHashTable *identifier_to_urn_old,
                                                const gchar *datasource,
                                                GCancellable *cancellable,
                                                GAsyncReadyCallback callback,
                                                gpointer user_data)
{
  g_autoptr (GTask) task = NULL;
  PhotosOnlineMinerGoogleProcessAlbumData *data;

  g_return_if_fail (PHOTOS_IS_ONLINE_MINER_GOOGLE (self));
  g_return_if_fail (GDATA_IS_PICASAWEB_SERVICE (service));
  g_return_if_fail (GDATA_IS_PICASAWEB_ALBUM (album));
  g_return_if_fail (datasource != NULL && datasource[0] != '\0');
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  data = photos_online_miner_google_process_album_data_new (identifier_to_urn_old, album, service, datasource);

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, photos_online_miner_google_process_album_async);
  g_task_set_task_data (task, data, (GDestroyNotify) photos_online_miner_google_process_album_data_free);

  g_task_run_in_thread (task, photos_online_miner_google_process_album_in_thread_func);
}


static gboolean
photos_online_miner_google_process_album_finish (PhotosOnlineMinerGoogle *self, GAsyncResult *res, GError **error)
{
  GTask *task;

  g_return_val_if_fail (PHOTOS_IS_ONLINE_MINER_GOOGLE (self), FALSE);

  g_return_val_if_fail (g_task_is_valid (res, self), FALSE);
  task = G_TASK (res);

  g_return_val_if_fail (g_task_get_source_tag (task) == photos_online_miner_google_process_album_async, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  return g_task_propagate_boolean (task, error);
}


static void
photos_online_miner_google_refresh_account_process_album (GObject *source_object,
                                                          GAsyncResult *res,
                                                          gpointer user_data)
{
  PhotosOnlineMinerGoogle *self = PHOTOS_ONLINE_MINER_GOOGLE (source_object);
  GCancellable *cancellable;
  g_autoptr (GTask) task = G_TASK (user_data);
  GDataPicasaWebAlbum *album;
  PhotosOnlineMinerGoogleRefreshAccountData *data;

  cancellable = g_task_get_cancellable (task);
  data = (PhotosOnlineMinerGoogleRefreshAccountData *) g_task_get_task_data (task);

  album = GDATA_PICASAWEB_ALBUM (data->current_album_link->data);

  {
    g_autoptr (GError) error = NULL;

    if (!photos_online_miner_google_process_album_finish (self, res, &error))
      {
        if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
          g_warning ("Unable to process album: %s", error->message);
      }
  }

  data->current_album_link = data->current_album_link->next;
  if (data->current_album_link == NULL)
    {
      g_task_return_boolean (task, TRUE);
      goto out;
    }

  album = GDATA_PICASAWEB_ALBUM (data->current_album_link->data);

  photos_online_miner_google_process_album_async (self,
                                                  data->service,
                                                  album,
                                                  data->identifier_to_urn_old,
                                                  data->datasource,
                                                  cancellable,
                                                  photos_online_miner_google_refresh_account_process_album,
                                                  g_object_ref (task));

 out:
  return;
}


static void
photos_online_miner_google_refresh_account_query_all_albums (GObject *source_object,
                                                             GAsyncResult *res,
                                                             gpointer user_data)
{
  PhotosOnlineMinerGoogle *self;
  GCancellable *cancellable;
  g_autoptr (GTask) task = G_TASK (user_data);
  g_autoptr (GDataFeed) feed = NULL;
  GDataPicasaWebAlbum *album;
  GDataPicasaWebService *service = GDATA_PICASAWEB_SERVICE (source_object);
  PhotosOnlineMinerGoogleRefreshAccountData *data;

  self = PHOTOS_ONLINE_MINER_GOOGLE (g_task_get_source_object (task));
  cancellable = g_task_get_cancellable (task);
  data = (PhotosOnlineMinerGoogleRefreshAccountData *) g_task_get_task_data (task);

  {
    g_autoptr (GError) error = NULL;

    feed = gdata_service_query_finish (GDATA_SERVICE (service), res, &error);
    if (error != NULL)
      {
        g_task_return_error (task, g_steal_pointer (&error));
        goto out;
      }
  }

  g_assert (data->feed == NULL);
  g_assert (GDATA_IS_FEED (feed));
  data->feed = g_object_ref (feed);

  data->current_album_link = gdata_feed_get_entries (data->feed);
  if (data->current_album_link == NULL)
    {
      g_task_return_boolean (task, TRUE);
      goto out;
    }

  album = GDATA_PICASAWEB_ALBUM (data->current_album_link->data);

  photos_online_miner_google_process_album_async (self,
                                                  service,
                                                  album,
                                                  data->identifier_to_urn_old,
                                                  data->datasource,
                                                  cancellable,
                                                  photos_online_miner_google_refresh_account_process_album,
                                                  g_object_ref (task));

 out:
  return;
}


static void
photos_online_miner_google_refresh_account_async (PhotosOnlineMiner *online_miner,
                                                  GHashTable *identifier_to_urn_old,
                                                  GoaObject *object,
                                                  const gchar *datasource,
                                                  GCancellable *cancellable,
                                                  GAsyncReadyCallback callback,
                                                  gpointer user_data)
{
  PhotosOnlineMinerGoogle *self = PHOTOS_ONLINE_MINER_GOOGLE (online_miner);
  g_autoptr (GTask) task = NULL;
  g_autoptr (GDataGoaAuthorizer) authorizer = NULL;
  g_autoptr (GDataPicasaWebService) service = NULL;
  PhotosOnlineMinerGoogleRefreshAccountData *data;

  authorizer = gdata_goa_authorizer_new (object);
  service = gdata_picasaweb_service_new (GDATA_AUTHORIZER (authorizer));
  data = photos_online_miner_google_refresh_account_data_new (identifier_to_urn_old, service, object, datasource);

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, photos_online_miner_google_refresh_account_async);
  g_task_set_task_data (task, data, (GDestroyNotify) photos_online_miner_google_refresh_account_data_free);

  gdata_picasaweb_service_query_all_albums_async (service,
                                                  NULL,
                                                  NULL,
                                                  cancellable,
                                                  NULL,
                                                  NULL,
                                                  NULL,
                                                  photos_online_miner_google_refresh_account_query_all_albums,
                                                  g_object_ref (task));
}


static gboolean
photos_online_miner_google_refresh_account_finish (PhotosOnlineMiner *online_miner,
                                                   GAsyncResult *res,
                                                   GError **error)
{
  PhotosOnlineMinerGoogle *self = PHOTOS_ONLINE_MINER_GOOGLE (online_miner);
  GTask *task;

  g_return_val_if_fail (g_task_is_valid (res, self), FALSE);
  task = G_TASK (res);

  g_return_val_if_fail (g_task_get_source_tag (task) == photos_online_miner_google_refresh_account_async, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  return g_task_propagate_boolean (task, error);
}


static void
photos_online_miner_google_dispose (GObject *object)
{
  PhotosOnlineMinerGoogle *self = PHOTOS_ONLINE_MINER_GOOGLE (object);

  G_OBJECT_CLASS (photos_online_miner_google_parent_class)->dispose (object);
}


static void
photos_online_miner_google_finalize (GObject *object)
{
  PhotosOnlineMinerGoogle *self = PHOTOS_ONLINE_MINER_GOOGLE (object);

  G_OBJECT_CLASS (photos_online_miner_google_parent_class)->finalize (object);
}


static void
photos_online_miner_google_init (PhotosOnlineMinerGoogle *self)
{
}


static void
photos_online_miner_google_class_init (PhotosOnlineMinerGoogleClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  PhotosOnlineMinerClass *online_miner_class = PHOTOS_ONLINE_MINER_CLASS (class);

  online_miner_class->identifier = "photos:google:miner:02797020-eab2-456b-ae6e-053994d803a6";
  online_miner_class->provider_type = "google";
  online_miner_class->version = 0U;

  object_class->dispose = photos_online_miner_google_dispose;
  object_class->finalize = photos_online_miner_google_finalize;
  online_miner_class->refresh_account_async = photos_online_miner_google_refresh_account_async;
  online_miner_class->refresh_account_finish = photos_online_miner_google_refresh_account_finish;
}


GApplication *
photos_online_miner_google_new (void)
{
  return g_object_new (PHOTOS_TYPE_ONLINE_MINER_GOOGLE, NULL);
}
