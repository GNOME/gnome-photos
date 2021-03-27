/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2013 Intel Corporation. All rights reserved.
 * Copyright © 2013 – 2021 Red Hat, Inc.
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
#include "photos-dleyna-renderer-device.h"
#include "photos-dleyna-renderer-push-host.h"
#include "photos-dlna-renderer.h"
#include "photos-filterable.h"
#include "photos-mpris-player.h"


struct _PhotosDlnaRenderer
{
  GObject parent_instance;
  DleynaRendererDevice *device;
  DleynaRendererPushHost *push_host;
  GBusType bus_type;
  GDBusProxyFlags flags;
  GHashTable *urls_to_item;
  MprisPlayer *player;
  gchar *object_path;
  gchar *well_known_name;
};

enum
{
  PROP_0,
  PROP_BUS_TYPE,
  PROP_FLAGS,
  PROP_OBJECT_PATH,
  PROP_SHARED_COUNT,
  PROP_WELL_KNOWN_NAME,
};

static void photos_dlna_renderer_async_initable_iface_init (GAsyncInitableIface *iface);


G_DEFINE_TYPE_WITH_CODE (PhotosDlnaRenderer, photos_dlna_renderer, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_ASYNC_INITABLE,
                                                photos_dlna_renderer_async_initable_iface_init));


static void
photos_dlna_renderer_dispose (GObject *object)
{
  PhotosDlnaRenderer *self = PHOTOS_DLNA_RENDERER (object);

  g_clear_object (&self->device);
  g_clear_object (&self->push_host);
  g_clear_object (&self->player);
  g_clear_pointer (&self->urls_to_item, g_hash_table_unref);

  G_OBJECT_CLASS (photos_dlna_renderer_parent_class)->dispose (object);
}


static void
photos_dlna_renderer_finalize (GObject *object)
{
  PhotosDlnaRenderer *self = PHOTOS_DLNA_RENDERER (object);

  g_free (self->well_known_name);
  g_free (self->object_path);

  G_OBJECT_CLASS (photos_dlna_renderer_parent_class)->finalize (object);
}


static void
photos_dlna_renderer_get_property (GObject *object,
                                   guint prop_id,
                                   GValue *value,
                                   GParamSpec *pspec)
{
  PhotosDlnaRenderer *self = PHOTOS_DLNA_RENDERER (object);

  switch (prop_id)
    {
    case PROP_SHARED_COUNT:
      g_value_set_uint (value, photos_dlna_renderer_get_shared_count (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
photos_dlna_renderer_set_property (GObject *object,
                                   guint prop_id,
                                   const GValue *value,
                                   GParamSpec *pspec)
{
  PhotosDlnaRenderer *self = PHOTOS_DLNA_RENDERER (object);

  switch (prop_id)
    {
    case PROP_BUS_TYPE:
      self->bus_type = g_value_get_enum (value);
      break;

    case PROP_FLAGS:
      self->flags = g_value_get_flags (value);
      break;

    case PROP_OBJECT_PATH:
      self->object_path = g_value_dup_string (value);
      break;

    case PROP_WELL_KNOWN_NAME:
      self->well_known_name = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
photos_dlna_renderer_init (PhotosDlnaRenderer *self)
{
  self->urls_to_item = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
}


static void
photos_dlna_renderer_class_init (PhotosDlnaRendererClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  gobject_class->dispose = photos_dlna_renderer_dispose;
  gobject_class->finalize = photos_dlna_renderer_finalize;
  gobject_class->get_property = photos_dlna_renderer_get_property;
  gobject_class->set_property = photos_dlna_renderer_set_property;

  g_object_class_install_property (gobject_class,
                                   PROP_BUS_TYPE,
                                   g_param_spec_enum ("bus-type",
                                                      "Bus Type",
                                                      "The bus to connect to, defaults to the session one",
                                                      G_TYPE_BUS_TYPE,
                                                      G_BUS_TYPE_SESSION,
                                                      G_PARAM_WRITABLE |
                                                      G_PARAM_CONSTRUCT_ONLY |
                                                      G_PARAM_STATIC_NAME |
                                                      G_PARAM_STATIC_BLURB |
                                                      G_PARAM_STATIC_NICK));

  g_object_class_install_property (gobject_class,
                                   PROP_FLAGS,
                                   g_param_spec_flags ("flags",
                                                       "Flags",
                                                       "Proxy flags",
                                                       G_TYPE_DBUS_PROXY_FLAGS,
                                                       G_DBUS_PROXY_FLAGS_NONE,
                                                       G_PARAM_READABLE |
                                                       G_PARAM_WRITABLE |
                                                       G_PARAM_CONSTRUCT_ONLY |
                                                       G_PARAM_STATIC_NAME |
                                                       G_PARAM_STATIC_BLURB |
                                                       G_PARAM_STATIC_NICK));

  g_object_class_install_property (gobject_class,
                                   PROP_OBJECT_PATH,
                                   g_param_spec_string ("object-path",
                                                        "Object Path",
                                                        "The object path the proxy is for",
                                                        NULL,
                                                        G_PARAM_WRITABLE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_NAME |
                                                        G_PARAM_STATIC_BLURB |
                                                        G_PARAM_STATIC_NICK));

  g_object_class_install_property (gobject_class,
                                   PROP_SHARED_COUNT,
                                   g_param_spec_uint ("shared-count",
                                                      "Shared Count",
                                                      "The number of shared items",
                                                      0, G_MAXUINT, 0,
                                                      G_PARAM_READABLE |
                                                      G_PARAM_STATIC_NAME |
                                                      G_PARAM_STATIC_BLURB |
                                                      G_PARAM_STATIC_NICK));

  g_object_class_install_property (gobject_class,
                                   PROP_WELL_KNOWN_NAME,
                                   g_param_spec_string ("well-known-name",
                                                        "Well-Known Name",
                                                        "The well-known name of the service",
                                                        NULL,
                                                        G_PARAM_READABLE |
                                                        G_PARAM_WRITABLE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_NAME |
                                                        G_PARAM_STATIC_BLURB |
                                                        G_PARAM_STATIC_NICK));
}


void
photos_dlna_renderer_new_for_bus (GBusType bus_type,
                                  GDBusProxyFlags flags,
                                  const gchar *well_known_name,
                                  const gchar *object_path,
                                  GCancellable *cancellable,
                                  GAsyncReadyCallback callback,
                                  gpointer user_data)
{
  g_async_initable_new_async (PHOTOS_TYPE_DLNA_RENDERER,
                              G_PRIORITY_DEFAULT,
                              cancellable,
                              callback,
                              user_data,
                              "bus-type", bus_type,
                              "flags", flags,
                              "object-path", object_path,
                              "well-known-name", well_known_name,
                              NULL);
}


static void
photos_dlna_renderer_device_proxy_new_cb (GObject *source_object,
                                          GAsyncResult *res,
                                          gpointer user_data)
{
  g_autoptr (GTask) init_task = G_TASK (user_data);
  PhotosDlnaRenderer *self;
  GError *error = NULL;

  self = PHOTOS_DLNA_RENDERER (g_task_get_source_object (init_task));

  self->device = dleyna_renderer_device_proxy_new_for_bus_finish (res, &error);
  if (error != NULL)
    {
      g_warning ("%s: Unable to load the RendererDevice interface: %s", G_STRFUNC, error->message);
      g_task_return_error (init_task, error);
      return;
    }

  g_task_return_boolean (init_task, TRUE);
}


static void
photos_dlna_renderer_init_async (GAsyncInitable *initable,
                                 int io_priority,
                                 GCancellable *cancellable,
                                 GAsyncReadyCallback callback,
                                 gpointer user_data)
{
  PhotosDlnaRenderer *self = PHOTOS_DLNA_RENDERER (initable);
  g_autoptr (GTask) init_task = NULL;
  GError *error = NULL;

  init_task = g_task_new (initable, cancellable, callback, user_data);
  g_task_set_priority (init_task, io_priority);

  self->push_host = dleyna_renderer_push_host_proxy_new_for_bus_sync (self->bus_type,
                                                                      G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES
                                                                      | G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS,
                                                                      self->well_known_name,
                                                                      self->object_path,
                                                                      NULL,
                                                                      &error);
  if (error != NULL)
    {
      g_warning ("%s: Unable to load the PushHost interface: %s", G_STRFUNC, error->message);
      g_task_return_error (init_task, error);
      return;
    }

  self->player = mpris_player_proxy_new_for_bus_sync (self->bus_type,
                                                      G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES
                                                      | G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS,
                                                      self->well_known_name,
                                                      self->object_path,
                                                      NULL,
                                                      &error);
  if (error != NULL)
    {
      g_warning ("%s: Unable to load the Player interface: %s", G_STRFUNC, error->message);
      g_task_return_error (init_task, error);
      return;
    }

  dleyna_renderer_device_proxy_new_for_bus (self->bus_type,
                                            G_DBUS_PROXY_FLAGS_NONE,
                                            self->well_known_name,
                                            self->object_path,
                                            cancellable,
                                            photos_dlna_renderer_device_proxy_new_cb,
                                            g_object_ref (init_task));
}


PhotosDlnaRenderer*
photos_dlna_renderer_new_for_bus_finish (GAsyncResult *result,
                                         GError **error)
{
  GObject *object;
  g_autoptr (GObject) source_object = NULL;
  GError *err = NULL;

  source_object = g_async_result_get_source_object (result);
  object = g_async_initable_new_finish (G_ASYNC_INITABLE (source_object), result, &err);
  if (err != NULL)
    {
      g_clear_object (&object);
      g_propagate_error (error, err);
      return NULL;
    }

  return PHOTOS_DLNA_RENDERER (object);
}


static void
photos_dlna_renderer_async_initable_iface_init (GAsyncInitableIface *iface)
{
  iface->init_async = photos_dlna_renderer_init_async;
}


const gchar *
photos_dlna_renderer_get_object_path (PhotosDlnaRenderer *self)
{
  return self->object_path;
}


static void
photos_dlna_renderer_share_play_cb (GObject *source_object,
                                    GAsyncResult *res,
                                    gpointer user_data)
{
  g_autoptr (GTask) task = G_TASK (user_data);
  PhotosBaseItem *item;
  GError *error = NULL;

  mpris_player_call_play_finish (MPRIS_PLAYER (source_object), res, &error);
  if (error != NULL)
    {
      g_warning ("%s: Failed to call the Play method: %s", G_STRFUNC, error->message);
      g_task_return_error (task, error);
      return;
    }

  item = g_object_get_data (G_OBJECT (task), "item");
  g_task_return_pointer (task, g_object_ref (item), g_object_unref);
}


static void
photos_dlna_renderer_share_open_uri_cb (GObject *source_object,
                                        GAsyncResult *res,
                                        gpointer user_data)
{
  g_autoptr (GTask) task = G_TASK (user_data);
  PhotosDlnaRenderer *self;
  GError *error = NULL;

  self = PHOTOS_DLNA_RENDERER (g_task_get_source_object (task));

  mpris_player_call_open_uri_finish (MPRIS_PLAYER (source_object), res, &error);
  if (error != NULL)
    {
      g_warning ("%s: Failed to call the OpenUri method: %s", G_STRFUNC, error->message);
      g_task_return_error (task, error);
      return;
    }

  /* 3) Mpris.Player.Play() */
  mpris_player_call_play (self->player,
                          g_task_get_cancellable (task),
                          photos_dlna_renderer_share_play_cb,
                          g_object_ref (task));
}


static void
photos_dlna_renderer_share_host_file_cb (GObject *source_object,
                                         GAsyncResult *res,
                                         gpointer user_data)
{
  g_autoptr (GTask) task = G_TASK (user_data);
  PhotosDlnaRenderer *self;
  PhotosBaseItem *item;
  gchar *hosted_url;
  GError *error = NULL;

  self = PHOTOS_DLNA_RENDERER (g_task_get_source_object (task));

  dleyna_renderer_push_host_call_host_file_finish (DLEYNA_RENDERER_PUSH_HOST (source_object),
                                                   &hosted_url,
                                                   res,
                                                   &error);
  if (error != NULL)
    {
      g_warning ("%s: Failed to call the HostFile method: %s", G_STRFUNC, error->message);
      g_task_return_error (task, error);
      return;
    }

  item = g_object_get_data (G_OBJECT (task), "item");
  g_hash_table_replace (self->urls_to_item, hosted_url, g_object_ref (item));
  g_object_notify (G_OBJECT (self), "shared-count");

  /* 2) Mpris.Player.OpenUri(hosted_url) */
  photos_debug (PHOTOS_DEBUG_DLNA, "%s %s", G_STRFUNC, hosted_url);
  mpris_player_call_open_uri (self->player, hosted_url,
                              g_task_get_cancellable (task),
                              photos_dlna_renderer_share_open_uri_cb,
                              g_object_ref (task));
}


static void
photos_dlna_renderer_share_download_cb (GObject *source_object,
                                        GAsyncResult *res,
                                        gpointer user_data)
{
  PhotosDlnaRenderer *self;
  GError *error;
  g_autoptr (GFile) file = NULL;
  g_autoptr (GTask) task = G_TASK (user_data);
  g_autofree gchar *filename = NULL;

  self = PHOTOS_DLNA_RENDERER (g_task_get_source_object (task));

  error = NULL;
  file = photos_base_item_download_finish (PHOTOS_BASE_ITEM (source_object), res, &error);
  if (error != NULL)
    {
      g_warning ("%s: Unable to extract the local filename for the shared item: %s", G_STRFUNC, error->message);
      g_task_return_error (task, error);
      return;
    }

  /* This will call a sequence of DBus methods to send the item to the DMR:
   * 1) DleynaRenderer.PushHost.HostFile (filename)
   *    → returns hosted_url, the HTTP URL published by the local webserver
   * 2) Mpris.Player.OpenUri(hosted_url)
   * 3) Mpris.Player.Play()
   *    → not really needed as OpenUri should automatically switch to Play to
   *      avoid races, but with dleyna-renderer v0.0.1-22-g6981acf it is still
   *      needed, see https://github.com/01org/dleyna-renderer/issues/78
   */

  /* 1) DleynaRenderer.PushHost.HostFile() */
  filename = g_file_get_path (file);
  dleyna_renderer_push_host_call_host_file (self->push_host,
                                            filename,
                                            g_task_get_cancellable (task),
                                            photos_dlna_renderer_share_host_file_cb,
                                            g_object_ref (task));
}


void
photos_dlna_renderer_share (PhotosDlnaRenderer *self,
                            PhotosBaseItem *item,
                            GCancellable *cancellable,
                            GAsyncReadyCallback callback,
                            gpointer user_data)
{
  g_autoptr (GTask) task = NULL;

  task = g_task_new (self, cancellable, callback, user_data);
  g_object_set_data_full (G_OBJECT (task), "item", g_object_ref (item), g_object_unref);

  photos_base_item_download_async (item, cancellable, photos_dlna_renderer_share_download_cb, g_object_ref (task));
}


PhotosBaseItem *
photos_dlna_renderer_share_finish (PhotosDlnaRenderer *self,
                                   GAsyncResult *res,
                                   GError **error)
{
  g_return_val_if_fail (g_task_is_valid (res, self), FALSE);

  return g_task_propagate_pointer (G_TASK (res), error);
}


static gboolean
photos_dlna_renderer_match_by_item_value (gpointer key,
                                          gpointer value,
                                          gpointer user_data)
{
  PhotosFilterable *a = PHOTOS_FILTERABLE (value);
  PhotosFilterable *b = PHOTOS_FILTERABLE (user_data);

  return g_strcmp0 (photos_filterable_get_id (a), photos_filterable_get_id (b)) == 0;
}


static void
photos_dlna_renderer_unshare_remove_file_cb (GObject *source_object,
                                             GAsyncResult *res,
                                             gpointer user_data)
{
  g_autoptr (GTask) task = G_TASK (user_data);
  PhotosDlnaRenderer *self = PHOTOS_DLNA_RENDERER (g_task_get_source_object (task));
  PhotosBaseItem *item;

  item = g_object_get_data (G_OBJECT (task), "item");
  g_hash_table_foreach_remove (self->urls_to_item, photos_dlna_renderer_match_by_item_value, item);
  g_object_notify (G_OBJECT (self), "shared-count");

  {
    g_autoptr (GError) error = NULL;

    dleyna_renderer_push_host_call_remove_file_finish (DLEYNA_RENDERER_PUSH_HOST (source_object), res, &error);
    if (error != NULL)
      {
        /* Assume that ignoring RemoveFile() errors is safe, since they
         * are likely caused by the file being already removed or the
         * DBus service having been restarted.
         */
        g_warning ("Failed to call the RemoveFile method: %s", error->message);
      }
  }

  g_task_return_boolean (task, TRUE);
}


static void
photos_dlna_renderer_unshare_download_cb (GObject *source_object,
                                          GAsyncResult *res,
                                          gpointer user_data)
{
  PhotosDlnaRenderer *self;
  GError *error;
  g_autoptr (GFile) file = NULL;
  g_autoptr (GTask) task = G_TASK (user_data);
  g_autofree gchar *filename = NULL;

  self = PHOTOS_DLNA_RENDERER (g_task_get_source_object (task));

  error = NULL;
  file = photos_base_item_download_finish (PHOTOS_BASE_ITEM (source_object), res, &error);
  if (error != NULL)
    {
      g_warning ("%s: Unable to extract the local filename for the shared item: %s", G_STRFUNC, error->message);
      g_task_return_error (task, error);
      return;
    }

  filename = g_file_get_path (file);
  dleyna_renderer_push_host_call_remove_file (self->push_host,
                                              filename,
                                              g_task_get_cancellable (task),
                                              photos_dlna_renderer_unshare_remove_file_cb,
                                              g_object_ref (task));
}


void
photos_dlna_renderer_unshare (PhotosDlnaRenderer *self,
                              PhotosBaseItem *item,
                              GCancellable *cancellable,
                              GAsyncReadyCallback callback,
                              gpointer user_data)
{
  g_autoptr (GTask) task = NULL;

  task = g_task_new (self, cancellable, callback, user_data);
  g_object_set_data_full (G_OBJECT (task), "item", g_object_ref (item), g_object_unref);

  photos_base_item_download_async (item,
                                   cancellable,
                                   photos_dlna_renderer_unshare_download_cb,
                                   g_object_ref (task));
}


gboolean
photos_dlna_renderer_unshare_finish (PhotosDlnaRenderer *self,
                                     GAsyncResult *res,
                                     GError **error)
{
  g_return_val_if_fail (g_task_is_valid (res, self), FALSE);

  return g_task_propagate_boolean (G_TASK (res), error);
}


static void
photos_dlna_renderer_unshare_all_unshare_cb (GObject *source_object,
                                             GAsyncResult *res,
                                             gpointer user_data)
{
  PhotosDlnaRenderer *self = PHOTOS_DLNA_RENDERER (source_object);
  g_autoptr (GTask) task = G_TASK (user_data);
  guint remaining;

  /* decrement the remaining count */
  remaining = GPOINTER_TO_UINT (g_task_get_task_data (task));
  g_task_set_task_data (task, GUINT_TO_POINTER (--remaining), NULL);

  {
    g_autoptr (GError) error = NULL;

    if (!photos_dlna_renderer_unshare_finish (self, res, &error))
      g_warning ("Unable to unshare item: %s", error->message);
  }

  if (remaining == 0)
    {
      g_task_return_boolean (task, TRUE);
      return;
    }
}

void
photos_dlna_renderer_unshare_all (PhotosDlnaRenderer  *self,
                                  GCancellable *cancellable,
                                  GAsyncReadyCallback callback,
                                  gpointer user_data)
{
  GList *item;
  g_autoptr (GList) items = NULL;
  g_autoptr (GTask) task = NULL;
  guint remaining;

  task = g_task_new (self, cancellable, callback, user_data);

  items = g_hash_table_get_values (self->urls_to_item);

  remaining = g_list_length (items);
  g_task_set_task_data (task, GUINT_TO_POINTER (remaining), NULL);

  for (item = items; item != NULL; item = g_list_next (item))
    photos_dlna_renderer_unshare (self, PHOTOS_BASE_ITEM (item->data), cancellable,
                                  photos_dlna_renderer_unshare_all_unshare_cb, g_object_ref (task));
}


gboolean
photos_dlna_renderer_unshare_all_finish (PhotosDlnaRenderer *self,
                                         GAsyncResult *res,
                                         GError **error)
{
  g_return_val_if_fail (g_task_is_valid (res, self), FALSE);

  return g_task_propagate_boolean (G_TASK (res), error);
}


static void
photos_dlna_renderer_device_get_icon_cb (GObject *source_object,
                                         GAsyncResult *res,
                                         gpointer user_data)
{
  g_autoptr (GTask) task = G_TASK (user_data);
  g_autoptr (GInputStream) icon_stream = NULL;
  GdkPixbuf *pixbuf;
  GtkIconSize size;
  g_autoptr (GBytes) icon_bytes = NULL;
  g_autoptr (GVariant) icon_variant = NULL;
  gint height = -1;
  gint width = -1;
  GError *error = NULL;

  /* The icon data is forced to be a GVariant since the GDBus bindings
   * assume bytestrings (type 'ay') to be nul-terminated and thus do
   * not return the length of the buffer.
   */
  dleyna_renderer_device_call_get_icon_finish (DLEYNA_RENDERER_DEVICE (source_object),
                                               &icon_variant, NULL, res, &error);
  if (error != NULL)
    {
      g_warning ("%s: Failed to call the GetIcon method: %s", G_STRFUNC, error->message);
      g_task_return_error (task, error);
      return;
    }

  /* We know that the serialization of variant containing just a byte
   * array 'ay' is the byte array itself.
   */
  icon_bytes = g_variant_get_data_as_bytes (icon_variant);

  size = (GtkIconSize) GPOINTER_TO_INT (g_task_get_task_data (task));
  gtk_icon_size_lookup (size, &width, &height);

  icon_stream = g_memory_input_stream_new_from_bytes (icon_bytes);
  pixbuf = gdk_pixbuf_new_from_stream_at_scale (icon_stream,
                                                width,
                                                height,
                                                TRUE,
                                                g_task_get_cancellable (task),
                                                &error);
  if (error != NULL)
    {
      g_warning ("%s: Failed to parse icon data: %s", G_STRFUNC, error->message);
      g_task_return_error (task, error);
      return;
    }

  g_task_return_pointer (task, pixbuf, g_object_unref);
}



const gchar *
photos_dlna_renderer_get_friendly_name (PhotosDlnaRenderer *self)
{
  return dleyna_renderer_device_get_friendly_name (self->device);
}


const gchar *
photos_dlna_renderer_get_udn (PhotosDlnaRenderer *self)
{
  return dleyna_renderer_device_get_udn (self->device);
}


void
photos_dlna_renderer_get_icon (PhotosDlnaRenderer *self,
                               const gchar *requested_mimetype,
                               const gchar *resolution,
                               GtkIconSize size,
                               GCancellable *cancellable,
                               GAsyncReadyCallback callback,
                               gpointer user_data)
{
  g_autoptr (GTask) task = NULL;

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_task_data (task, GINT_TO_POINTER (size), NULL);

  dleyna_renderer_device_call_get_icon (self->device, requested_mimetype, resolution,
                                        cancellable, photos_dlna_renderer_device_get_icon_cb,
                                        g_object_ref (task));
}


GdkPixbuf *
photos_dlna_renderer_get_icon_finish (PhotosDlnaRenderer *self,
                                      GAsyncResult *res,
                                      GError **error)
{
  g_return_val_if_fail (g_task_is_valid (res, self), NULL);

  return GDK_PIXBUF (g_task_propagate_pointer (G_TASK (res), error));
}


guint
photos_dlna_renderer_get_shared_count (PhotosDlnaRenderer *self)
{
  return g_hash_table_size (self->urls_to_item);
}
