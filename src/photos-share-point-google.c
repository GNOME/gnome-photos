/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2016 – 2021 Red Hat, Inc.
 * Copyright © 2016 Umang Jain
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

#include <gio/gio.h>
#include <gdata/gdata.h>
#include <glib/gi18n.h>

#include "photos-application.h"
#include "photos-base-item.h"
#include "photos-error.h"
#include "photos-filterable.h"
#include "photos-item-manager.h"
#include "photos-search-context.h"
#include "photos-share-point-google.h"
#include "photos-source.h"
#include "photos-utils.h"


struct _PhotosSharePointGoogle
{
  PhotosSharePointOnline parent_instance;
  GDataGoaAuthorizer *authorizer;
  GDataPicasaWebService *service;
  PhotosBaseManager *item_mngr;
};


G_DEFINE_TYPE_WITH_CODE (PhotosSharePointGoogle, photos_share_point_google, PHOTOS_TYPE_SHARE_POINT_ONLINE,
                         photos_utils_ensure_extension_points ();
                         g_io_extension_point_implement (PHOTOS_SHARE_POINT_ONLINE_EXTENSION_POINT_NAME,
                                                         g_define_type_id,
                                                         "google",
                                                         0));


typedef struct _PhotosSharePointGoogleShareData PhotosSharePointGoogleShareData;

struct _PhotosSharePointGoogleShareData
{
  GDataPicasaWebFile *file_entry;
  GDataUploadStream *stream;
  PhotosBaseItem *item;
  gchar *item_id_after_changes;
  guint pending_async_calls;
};


static PhotosSharePointGoogleShareData *
photos_share_point_google_share_data_new (PhotosBaseItem *item)
{
  PhotosSharePointGoogleShareData *data;

  data = g_slice_new0 (PhotosSharePointGoogleShareData);
  data->item = g_object_ref (item);

  return data;
}


static void
photos_share_point_google_share_data_free (PhotosSharePointGoogleShareData *data)
{
  g_clear_object (&data->file_entry);
  g_clear_object (&data->stream);
  g_clear_object (&data->item);
  g_free (data->item_id_after_changes);
  g_slice_free (PhotosSharePointGoogleShareData, data);
}


static gboolean
photos_share_point_google_needs_notification (PhotosSharePoint *share_point)
{
  return TRUE;
}


static gchar *
photos_share_point_google_parse_error (PhotosSharePoint *self, GError *error)
{
  gchar *msg;

  if (g_error_matches (error, GDATA_SERVICE_ERROR, GDATA_SERVICE_ERROR_AUTHENTICATION_REQUIRED))
    msg = g_strdup (_("Failed to upload photo: Service not authorized"));
  else
    msg = g_strdup (_("Failed to upload photo"));

  return msg;
}


static void
photos_share_point_google_share_insert_shared_content (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  g_autoptr (GTask) task = G_TASK (user_data);
  GomMiner *miner = GOM_MINER (source_object);

  {
    g_autoptr (GError) error = NULL;

    if (!gom_miner_call_insert_shared_content_finish (miner, res, &error))
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
photos_share_point_google_share_metadata_add_shared_second (PhotosSharePointGoogle *self, GTask *task)
{
  GApplication *app;
  GCancellable *cancellable;
  GoaAccount *account;
  GoaObject *object;
  GomMiner *miner;
  PhotosSource *source;
  PhotosSharePointGoogleShareData *data;
  const gchar *account_id;
  const gchar *file_entry_id;
  const gchar *provider_type;

  cancellable = g_task_get_cancellable (task);
  data = (PhotosSharePointGoogleShareData *) g_task_get_task_data (task);

  app = g_application_get_default ();

  source = photos_share_point_online_get_source (PHOTOS_SHARE_POINT_ONLINE (self));
  object = photos_source_get_goa_object (source);
  account = goa_object_peek_account (object);
  provider_type = goa_account_get_provider_type (account);

  miner = photos_application_get_miner (PHOTOS_APPLICATION (app), provider_type);
  if (miner == NULL)
    {
      g_task_return_new_error (task, PHOTOS_ERROR, 0, "Unable to find the %s miner", provider_type);
      goto out;
    }

  account_id = goa_account_get_id (account);
  file_entry_id = gdata_entry_get_id (GDATA_ENTRY (data->file_entry));

  gom_miner_call_insert_shared_content (miner,
                                        account_id,
                                        file_entry_id,
                                        "photos",
                                        data->item_id_after_changes,
                                        cancellable,
                                        photos_share_point_google_share_insert_shared_content,
                                        g_object_ref (task));

 out:
  g_object_unref (task);
}


static void
photos_share_point_google_share_metadata_add_shared (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosSharePointGoogle *self;
  g_autoptr (GTask) task = G_TASK (user_data);
  PhotosBaseItem *item = PHOTOS_BASE_ITEM (source_object);
  PhotosSharePointGoogleShareData *data;

  self = PHOTOS_SHARE_POINT_GOOGLE (g_task_get_source_object (task));
  data = (PhotosSharePointGoogleShareData *) g_task_get_task_data (task);

  {
    g_autoptr (GError) error = NULL;

    if (!photos_base_item_metadata_add_shared_finish (item, res, &error))
      {
        if (g_task_get_completed (task))
          g_warning ("Unable to add shared metadata: %s", error->message);
        else
          g_task_return_error (task, g_steal_pointer (&error));

        goto out;
      }
  }

  data->pending_async_calls--;
  if (data->pending_async_calls == 0)
    photos_share_point_google_share_metadata_add_shared_second (self, g_object_ref (task));

 out:
  return;
}


static void
photos_share_point_google_share_wait_for_changes (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosSharePointGoogle *self;
  g_autoptr (GTask) task = G_TASK (user_data);
  PhotosItemManager *item_mngr = PHOTOS_ITEM_MANAGER (source_object);
  PhotosSharePointGoogleShareData *data;
  g_autofree gchar *item_id_after_changes = NULL;

  self = PHOTOS_SHARE_POINT_GOOGLE (g_task_get_source_object (task));
  data = (PhotosSharePointGoogleShareData *) g_task_get_task_data (task);

  {
    g_autoptr (GError) error = NULL;

    item_id_after_changes = photos_item_manager_wait_for_changes_finish (item_mngr, res, &error);
    if (error != NULL)
      {
        if (g_task_get_completed (task))
          g_warning ("Unable to detect changes: %s", error->message);
        else
          g_task_return_error (task, g_steal_pointer (&error));

        goto out;
      }
  }

  g_assert_null (data->item_id_after_changes);
  data->item_id_after_changes = g_steal_pointer (&item_id_after_changes);

  data->pending_async_calls--;
  if (data->pending_async_calls == 0)
    photos_share_point_google_share_metadata_add_shared_second (self, g_object_ref (task));

 out:
  return;
}


static void
photos_share_point_google_share_save_to_stream (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosSharePointGoogle *self;
  GCancellable *cancellable;
  g_autoptr (GTask) task = G_TASK (user_data);
  g_autoptr (GDataPicasaWebFile) file_entry = NULL;
  GoaAccount *account;
  GoaObject *object;
  PhotosBaseItem *item = PHOTOS_BASE_ITEM (source_object);
  PhotosSource *source;
  PhotosSharePointGoogleShareData *data;
  const gchar *account_identity;
  const gchar *file_entry_id;
  const gchar *provider_type;
  g_autofree gchar *shared_identifier = NULL;

  self = PHOTOS_SHARE_POINT_GOOGLE (g_task_get_source_object (task));
  cancellable = g_task_get_cancellable (task);
  data = (PhotosSharePointGoogleShareData *) g_task_get_task_data (task);

  {
    g_autoptr (GError) error = NULL;

    if (!photos_base_item_save_to_stream_finish (item, res, &error))
      {
        g_task_return_error (task, g_steal_pointer (&error));
        goto out;
      }
  }

  {
    g_autoptr (GError) error = NULL;

    file_entry = gdata_picasaweb_service_finish_file_upload (GDATA_PICASAWEB_SERVICE (self->service),
                                                             data->stream,
                                                             &error);
    if (error != NULL)
      {
        g_task_return_error (task, g_steal_pointer (&error));
        goto out;
      }
  }

  g_assert_null (data->file_entry);
  data->file_entry = g_object_ref (file_entry);

  g_assert_true (PHOTOS_IS_ITEM_MANAGER (self->item_mngr));

  /* Tracker might detect the atomic update done by GExiv2 as a 'delete'
   * followed by 'create'. If that happens the URN will change. We
   * have to deal with that by waiting for TrackerChangeMonitor to
   * emit changes-pending for the BaseItem that we are interested in,
   * and using the new URN, if any.
   *
   * See https://bugzilla.gnome.org/show_bug.cgi?id=771042
   */
  photos_item_manager_wait_for_changes_async (PHOTOS_ITEM_MANAGER (self->item_mngr),
                                              data->item,
                                              cancellable,
                                              photos_share_point_google_share_wait_for_changes,
                                              g_object_ref (task));
  data->pending_async_calls++;

  source = photos_share_point_online_get_source (PHOTOS_SHARE_POINT_ONLINE (self));
  object = photos_source_get_goa_object (source);
  account = goa_object_peek_account (object);
  account_identity = goa_account_get_identity (account);
  provider_type = goa_account_get_provider_type (account);

  file_entry_id = gdata_entry_get_id (GDATA_ENTRY (file_entry));
  shared_identifier = g_strconcat ("google:picasaweb:", file_entry_id, NULL);

  photos_base_item_metadata_add_shared_async (data->item,
                                              provider_type,
                                              account_identity,
                                              shared_identifier,
                                              cancellable,
                                              photos_share_point_google_share_metadata_add_shared,
                                              g_object_ref (task));
  data->pending_async_calls++;

 out:
  return;
}


static void
photos_share_point_google_share_refresh_authorization (GObject *source_object,
                                                       GAsyncResult *res,
                                                       gpointer user_data)
{
  PhotosSharePointGoogle *self;
  GCancellable *cancellable;
  g_autoptr (GTask) task = G_TASK (user_data);
  GDataAuthorizer *authorizer = GDATA_AUTHORIZER (source_object);
  g_autoptr (GDataPicasaWebFile) file_entry = NULL;
  g_autoptr (GDataUploadStream) stream = NULL;
  PhotosSharePointGoogleShareData *data;
  const gchar *filename;
  const gchar *mime_type;
  const gchar *name;

  self = PHOTOS_SHARE_POINT_GOOGLE (g_task_get_source_object (task));
  cancellable = g_task_get_cancellable (task);
  data = (PhotosSharePointGoogleShareData *) g_task_get_task_data (task);

  {
    g_autoptr (GError) error = NULL;

    if (!gdata_authorizer_refresh_authorization_finish (authorizer, res, &error))
      {
        g_task_return_error (task, g_steal_pointer (&error));
        goto out;
      }
  }

  file_entry = gdata_picasaweb_file_new (NULL);
  name = photos_base_item_get_name_with_fallback (data->item);
  gdata_entry_set_title (GDATA_ENTRY (file_entry), name);

  filename = photos_base_item_get_filename (data->item);
  mime_type = photos_base_item_get_mime_type (data->item);

  {
    g_autoptr (GError) error = NULL;

    stream = gdata_picasaweb_service_upload_file (self->service,
                                                  NULL,
                                                  file_entry,
                                                  filename,
                                                  mime_type,
                                                  cancellable,
                                                  &error);
    if (error != NULL)
      {
        g_task_return_error (task, g_steal_pointer (&error));
        goto out;
      }
  }

  g_assert_null (data->stream);
  data->stream = g_object_ref (stream);

  photos_base_item_save_to_stream_async (data->item,
                                         G_OUTPUT_STREAM (stream),
                                         1.0,
                                         cancellable,
                                         photos_share_point_google_share_save_to_stream,
                                         g_object_ref (task));

 out:
  return;
}


static void
photos_share_point_google_share_async (PhotosSharePoint *share_point,
                                       PhotosBaseItem *item,
                                       GCancellable *cancellable,
                                       GAsyncReadyCallback callback,
                                       gpointer user_data)
{
  PhotosSharePointGoogle *self = PHOTOS_SHARE_POINT_GOOGLE (share_point);
  g_autoptr (GTask) task = NULL;
  PhotosSharePointGoogleShareData *data;

  data = photos_share_point_google_share_data_new (item);

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, photos_share_point_google_share_async);
  g_task_set_task_data (task, data, (GDestroyNotify) photos_share_point_google_share_data_free);

  gdata_authorizer_refresh_authorization_async (GDATA_AUTHORIZER (self->authorizer),
                                                cancellable,
                                                photos_share_point_google_share_refresh_authorization,
                                                g_object_ref (task));
}


static gboolean
photos_share_point_google_share_finish (PhotosSharePoint *share_point,
                                        GAsyncResult *res,
                                        gchar **out_uri,
                                        GError **error)
{
  PhotosSharePointGoogle *self = PHOTOS_SHARE_POINT_GOOGLE (share_point);
  GTask *task;
  gboolean ret_val = FALSE;

  g_return_val_if_fail (g_task_is_valid (res, self), FALSE);
  task = G_TASK (res);

  g_return_val_if_fail (g_task_get_source_tag (task) == photos_share_point_google_share_async, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  if (!g_task_propagate_boolean (task, error))
    goto out;

  ret_val = TRUE;

  if (out_uri != NULL)
    *out_uri = g_strdup ("https://photos.google.com/");

 out:
  return ret_val;
}


static void
photos_share_point_google_constructed (GObject *object)
{
  PhotosSharePointGoogle *self = PHOTOS_SHARE_POINT_GOOGLE (object);
  PhotosSource *source;
  GoaObject *goa_object;

  G_OBJECT_CLASS (photos_share_point_google_parent_class)->constructed (object);

  source = photos_share_point_online_get_source (PHOTOS_SHARE_POINT_ONLINE (self));
  goa_object = photos_source_get_goa_object (source);
  g_assert (GOA_IS_OBJECT (goa_object));

  self->authorizer = gdata_goa_authorizer_new (goa_object);
  self->service = gdata_picasaweb_service_new (GDATA_AUTHORIZER (self->authorizer));
}


static void
photos_share_point_google_dispose (GObject *object)
{
  PhotosSharePointGoogle *self = PHOTOS_SHARE_POINT_GOOGLE (object);

  g_clear_object (&self->authorizer);
  g_clear_object (&self->service);

  G_OBJECT_CLASS (photos_share_point_google_parent_class)->dispose (object);
}


static void
photos_share_point_google_finalize (GObject *object)
{
  PhotosSharePointGoogle *self = PHOTOS_SHARE_POINT_GOOGLE (object);

  if (self->item_mngr != NULL)
    g_object_remove_weak_pointer (G_OBJECT (self->item_mngr), (gpointer *) &self->item_mngr);

  G_OBJECT_CLASS (photos_share_point_google_parent_class)->finalize (object);
}


static void
photos_share_point_google_init (PhotosSharePointGoogle *self)
{
  GApplication *app;
  PhotosSearchContextState *state;

  app = g_application_get_default ();
  state = photos_search_context_get_state (PHOTOS_SEARCH_CONTEXT (app));

  self->item_mngr = state->item_mngr;
  g_object_add_weak_pointer (G_OBJECT (self->item_mngr), (gpointer *) &self->item_mngr);
}


static void
photos_share_point_google_class_init (PhotosSharePointGoogleClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  PhotosSharePointClass *share_point_class = PHOTOS_SHARE_POINT_CLASS (class);

  object_class->constructed = photos_share_point_google_constructed;
  object_class->dispose = photos_share_point_google_dispose;
  object_class->finalize = photos_share_point_google_finalize;
  share_point_class->needs_notification = photos_share_point_google_needs_notification;
  share_point_class->parse_error = photos_share_point_google_parse_error;
  share_point_class->share_async = photos_share_point_google_share_async;
  share_point_class->share_finish = photos_share_point_google_share_finish;
}
