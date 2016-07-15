/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2016 Umang Jain
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


#include "config.h"

#include <gio/gio.h>
#include <gdata/gdata.h>
#include <glib/gi18n.h>

#include "photos-base-item.h"
#include "photos-share-point-google.h"
#include "photos-source.h"
#include "photos-utils.h"


struct _PhotosSharePointGoogle
{
  PhotosSharePointOnline parent_instance;
  GDataGoaAuthorizer *authorizer;
  GDataPicasaWebService *service;
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
  GDataUploadStream *stream;
  PhotosBaseItem *item;
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
  g_clear_object (&data->stream);
  g_clear_object (&data->item);
  g_slice_free (PhotosSharePointGoogleShareData, data);
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
photos_share_point_google_share_save_to_stream (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosSharePointGoogle *self;
  GError *error;
  GTask *task = G_TASK (user_data);
  GDataPicasaWebFile *file_entry = NULL;
  PhotosBaseItem *item = PHOTOS_BASE_ITEM (source_object);
  PhotosSharePointGoogleShareData *data;

  self = PHOTOS_SHARE_POINT_GOOGLE (g_task_get_source_object (task));
  data = (PhotosSharePointGoogleShareData *) g_task_get_task_data (task);

  error = NULL;
  if (!photos_base_item_save_to_stream_finish (item, res, &error))
    {
      g_task_return_error (task, error);
      goto out;
    }

  error = NULL;
  file_entry = gdata_picasaweb_service_finish_file_upload (GDATA_PICASAWEB_SERVICE (self->service),
                                                           data->stream,
                                                           &error);
  if (error != NULL)
    {
      g_task_return_error (task, error);
      goto out;
    }

  g_task_return_boolean (task, TRUE);

 out:
  g_clear_object (&file_entry);
  g_object_unref (task);
}


static void
photos_share_point_google_share_refresh_authorization (GObject *source_object,
                                                       GAsyncResult *res,
                                                       gpointer user_data)
{
  PhotosSharePointGoogle *self;
  GCancellable *cancellable;
  GError *error;
  GTask *task = G_TASK (user_data);
  GDataAuthorizer *authorizer = GDATA_AUTHORIZER (source_object);
  GDataPicasaWebFile *file_entry = NULL;
  GDataUploadStream *stream = NULL;
  PhotosSharePointGoogleShareData *data;
  const gchar *filename;
  const gchar *mime_type;
  const gchar *name;

  self = PHOTOS_SHARE_POINT_GOOGLE (g_task_get_source_object (task));
  cancellable = g_task_get_cancellable (task);
  data = (PhotosSharePointGoogleShareData *) g_task_get_task_data (task);

  error = NULL;
  if (!gdata_authorizer_refresh_authorization_finish (authorizer, res, &error))
    {
      g_task_return_error (task, error);
      goto out;
    }

  file_entry = gdata_picasaweb_file_new (NULL);
  name = photos_base_item_get_name_with_fallback (data->item);
  gdata_entry_set_title (GDATA_ENTRY (file_entry), name);

  filename = photos_base_item_get_filename (data->item);
  mime_type = photos_base_item_get_mime_type (data->item);

  error = NULL;
  stream = gdata_picasaweb_service_upload_file (self->service,
                                                NULL,
                                                file_entry,
                                                filename,
                                                mime_type,
                                                cancellable,
                                                &error);
  if (error != NULL)
    {
      g_task_return_error (task, error);
      goto out;
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
  g_clear_object (&file_entry);
  g_clear_object (&stream);
  g_object_unref (task);
}


static void
photos_share_point_google_share_async (PhotosSharePoint *share_point,
                                       PhotosBaseItem *item,
                                       GCancellable *cancellable,
                                       GAsyncReadyCallback callback,
                                       gpointer user_data)
{
  PhotosSharePointGoogle *self = PHOTOS_SHARE_POINT_GOOGLE (share_point);
  GTask *task;
  PhotosSharePointGoogleShareData *data;

  data = photos_share_point_google_share_data_new (item);

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, photos_share_point_google_share_async);
  g_task_set_task_data (task, data, (GDestroyNotify) photos_share_point_google_share_data_free);

  gdata_authorizer_refresh_authorization_async (GDATA_AUTHORIZER (self->authorizer),
                                                cancellable,
                                                photos_share_point_google_share_refresh_authorization,
                                                g_object_ref (task));

  g_object_unref (task);
}


static gboolean
photos_share_point_google_share_finish (PhotosSharePoint *share_point, GAsyncResult *res, GError **error)
{
  PhotosSharePointGoogle *self = PHOTOS_SHARE_POINT_GOOGLE (share_point);
  GTask *task;

  g_return_val_if_fail (g_task_is_valid (res, self), FALSE);
  task = G_TASK (res);

  g_return_val_if_fail (g_task_get_source_tag (task) == photos_share_point_google_share_async, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  return g_task_propagate_boolean (task, error);
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
photos_share_point_google_init (PhotosSharePointGoogle *self)
{
}


static void
photos_share_point_google_class_init (PhotosSharePointGoogleClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  PhotosSharePointClass *share_point_class = PHOTOS_SHARE_POINT_CLASS (class);

  object_class->constructed = photos_share_point_google_constructed;
  object_class->dispose = photos_share_point_google_dispose;
  share_point_class->parse_error = photos_share_point_google_parse_error;
  share_point_class->share_async = photos_share_point_google_share_async;
  share_point_class->share_finish = photos_share_point_google_share_finish;
}
