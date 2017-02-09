/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2017 Red Hat, Inc.
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

#include "photos-debug.h"
#include "photos-error.h"
#include "photos-quarks.h"
#include "photos-thumbnail-factory.h"
#include "photos-thumbnailer-dbus.h"
#include "photos-utils.h"


struct _PhotosThumbnailFactory
{
  GObject parent_instance;
  GCond cond_thumbnailer;
  GDBusConnection *connection;
  GDBusServer *dbus_server;
  GError *initialization_error;
  GError *thumbnailer_error;
  GMutex mutex_connection;
  GMutex mutex_thumbnailer;
  PhotosThumbnailerDBus *thumbnailer;
  gboolean is_initialized;
};

static void photos_thumbnail_factory_initable_iface_init (GInitableIface *iface);


G_DEFINE_TYPE_WITH_CODE (PhotosThumbnailFactory, photos_thumbnail_factory, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, photos_thumbnail_factory_initable_iface_init));


G_LOCK_DEFINE_STATIC (init_lock);

static const gchar *THUMBNAILER_PATH = "/org/gnome/Photos/Thumbnailer";


static gboolean
photos_thumbnail_factory_authorize_authenticated_peer (PhotosThumbnailFactory *self,
                                                       GIOStream *iostream,
                                                       GCredentials *credentials)
{
  GCredentials *own_credentials = NULL;
  GError *error;
  gboolean ret_val = FALSE;
  gchar *str = NULL;

  g_mutex_lock (&self->mutex_connection);

  str = g_credentials_to_string (credentials);
  photos_debug (PHOTOS_DEBUG_THUMBNAILER, "Received authorization request: %s", str);

  if (self->connection != NULL)
    {
      g_warning ("Unable to authorize peer: Connection exists");
      goto out;
    }

  if (credentials == NULL)
    {
      g_warning ("Unable to authorize peer: Credentials not found");
      goto out;
    }

  own_credentials = g_credentials_new ();

  error = NULL;
  if (!g_credentials_is_same_user (credentials, own_credentials, &error))
    {
      g_warning ("Unable to authorize peer: %s", error->message);
      g_error_free (error);
      goto out;
    }

  ret_val = TRUE;

 out:
  g_mutex_unlock (&self->mutex_connection);
  g_clear_object (&own_credentials);
  g_free (str);
  return ret_val;
}


static void
photos_thumbnail_factory_connection_closed (PhotosThumbnailFactory *self,
                                            gboolean remote_peer_vanished,
                                            GError *error)
{
  g_mutex_lock (&self->mutex_connection);

  if (error != NULL)
    photos_debug (PHOTOS_DEBUG_THUMBNAILER, "Lost connection to the thumbnailer: %s", error->message);
  else
    photos_debug (PHOTOS_DEBUG_THUMBNAILER, "Lost connection to the thumbnailer");

  g_signal_handlers_disconnect_by_func (self->connection, photos_thumbnail_factory_connection_closed, self);
  g_clear_object (&self->connection);

  g_mutex_unlock (&self->mutex_connection);
}


static GdkPixbuf *
photos_thumbnail_factory_get_preview (PhotosThumbnailFactory *self,
                                      GFile *file,
                                      gint size,
                                      GCancellable *cancellable,
                                      GError **error)
{
  GFileInfo *info = NULL;
  GIcon *icon;
  GInputStream *stream = NULL;
  GdkPixbuf *pixbuf = NULL;
  GdkPixbuf *ret_val = NULL;

  info = g_file_query_info (file, G_FILE_ATTRIBUTE_PREVIEW_ICON, G_FILE_QUERY_INFO_NONE, cancellable, error);
  if (info == NULL)
    goto out;

  icon = G_ICON (g_file_info_get_attribute_object (info, G_FILE_ATTRIBUTE_PREVIEW_ICON));
  if (!G_IS_LOADABLE_ICON (icon))
    {
      g_set_error (error, PHOTOS_ERROR, 0, "Preview icon is not loadable");
      goto out;
    }

  stream = g_loadable_icon_load (G_LOADABLE_ICON (icon), 0, NULL, cancellable, error);
  if (stream == NULL)
    goto out;

  pixbuf = gdk_pixbuf_new_from_stream_at_scale (stream, size, size, TRUE, cancellable, error);
  if (pixbuf == NULL)
    goto out;

  ret_val = g_object_ref (pixbuf);

 out:
  g_clear_object (&info);
  g_clear_object (&pixbuf);
  g_clear_object (&stream);
  return ret_val;
}


static gboolean
photos_thumbnail_factory_new_connection (PhotosThumbnailFactory *self, GDBusConnection *connection)
{
  GError *error;

  g_mutex_lock (&self->mutex_connection);
  g_mutex_lock (&self->mutex_thumbnailer);

  g_assert_null (self->connection);

  g_clear_error (&self->thumbnailer_error);
  g_clear_object (&self->thumbnailer);

  photos_debug (PHOTOS_DEBUG_THUMBNAILER, "Received new connection");

  self->connection = g_object_ref (connection);
  g_signal_connect_swapped (self->connection,
                            "closed",
                            G_CALLBACK (photos_thumbnail_factory_connection_closed),
                            self);

  error = NULL;
  self->thumbnailer = photos_thumbnailer_dbus_proxy_new_sync (self->connection,
                                                              G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES
                                                              | G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS,
                                                              NULL,
                                                              THUMBNAILER_PATH,
                                                              NULL,
                                                              &error);
  if (error != NULL)
    {
      self->thumbnailer_error = g_error_copy (error);
      g_error_free (error);
    }

  g_mutex_unlock (&self->mutex_connection);

  g_cond_signal (&self->cond_thumbnailer);
  g_mutex_unlock (&self->mutex_thumbnailer);

  return TRUE;
}


static GObject *
photos_thumbnail_factory_constructor (GType type, guint n_construct_params, GObjectConstructParam *construct_params)
{
  static GObject *self = NULL;

  if (self == NULL)
    {
      self = G_OBJECT_CLASS (photos_thumbnail_factory_parent_class)->constructor (type,
                                                                                  n_construct_params,
                                                                                  construct_params);
      g_object_add_weak_pointer (self, (gpointer) &self);
      return self;
    }

  return g_object_ref (self);
}


static void
photos_thumbnail_factory_dispose (GObject *object)
{
  PhotosThumbnailFactory *self = PHOTOS_THUMBNAIL_FACTORY (object);

  if (self->dbus_server != NULL)
    {
      g_dbus_server_stop (self->dbus_server);
      g_clear_object (&self->dbus_server);
    }

  g_clear_object (&self->connection);
  g_clear_object (&self->thumbnailer);

  G_OBJECT_CLASS (photos_thumbnail_factory_parent_class)->dispose (object);
}


static void
photos_thumbnail_factory_finalize (GObject *object)
{
  PhotosThumbnailFactory *self = PHOTOS_THUMBNAIL_FACTORY (object);

  g_cond_clear (&self->cond_thumbnailer);
  g_clear_error (&self->initialization_error);
  g_clear_error (&self->thumbnailer_error);
  g_mutex_clear (&self->mutex_connection);
  g_mutex_clear (&self->mutex_thumbnailer);

  G_OBJECT_CLASS (photos_thumbnail_factory_parent_class)->finalize (object);
}


static void
photos_thumbnail_factory_init (PhotosThumbnailFactory *self)
{
  g_cond_init (&self->cond_thumbnailer);
  g_mutex_init (&self->mutex_connection);
  g_mutex_init (&self->mutex_thumbnailer);
}


static void
photos_thumbnail_factory_class_init (PhotosThumbnailFactoryClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->constructor = photos_thumbnail_factory_constructor;
  object_class->dispose = photos_thumbnail_factory_dispose;
  object_class->finalize = photos_thumbnail_factory_finalize;
}


static gboolean
photos_thumbnail_factory_initable_init (GInitable *initable, GCancellable *cancellable, GError **error)
{
  PhotosThumbnailFactory *self = PHOTOS_THUMBNAIL_FACTORY (initable);
  GDBusAuthObserver *observer = NULL;
  gboolean ret_val = FALSE;
  const gchar *tmp_dir;
  gchar *address = NULL;
  gchar *guid = NULL;

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
                            G_CALLBACK (photos_thumbnail_factory_authorize_authenticated_peer),
                            self);

  self->dbus_server = g_dbus_server_new_sync (address, G_DBUS_SERVER_FLAGS_NONE,
                                              guid,
                                              observer,
                                              cancellable,
                                              &self->initialization_error);
  if (G_UNLIKELY (self->initialization_error != NULL))
    goto out;

  g_signal_connect_swapped (self->dbus_server,
                            "new-connection",
                            G_CALLBACK (photos_thumbnail_factory_new_connection),
                            self);

  g_dbus_server_start (self->dbus_server);
  ret_val = TRUE;

 out:
  self->is_initialized = TRUE;
  if (!ret_val)
    {
      g_assert_nonnull (self->initialization_error);
      g_propagate_error (error, g_error_copy (self->initialization_error));
    }

  G_UNLOCK (init_lock);

  g_clear_object (&observer);
  g_free (address);
  g_free (guid);

  return ret_val;
}


static void
photos_thumbnail_factory_initable_iface_init (GInitableIface *iface)
{
  iface->init = photos_thumbnail_factory_initable_init;
}


PhotosThumbnailFactory *
photos_thumbnail_factory_dup_singleton (GCancellable *cancellable, GError **error)
{
  return g_initable_new (PHOTOS_TYPE_THUMBNAIL_FACTORY, cancellable, error, NULL);
}


gboolean
photos_thumbnail_factory_generate_thumbnail (PhotosThumbnailFactory *self,
                                             GFile *file,
                                             const gchar *mime_type,
                                             GQuark orientation,
                                             gint64 original_height,
                                             gint64 original_width,
                                             const gchar *pipeline_uri,
                                             GCancellable *cancellable,
                                             GError **error)
{
  GError *local_error;
  GdkPixbuf *pixbuf = NULL;
  gboolean mutex_connection_unlocked = FALSE;
  gboolean ret_val = FALSE;
  gint thumbnail_size;

  g_return_val_if_fail (PHOTOS_IS_THUMBNAIL_FACTORY (self), FALSE);
  g_return_val_if_fail (G_IS_FILE (file), FALSE);
  g_return_val_if_fail (g_file_is_native (file), FALSE);
  g_return_val_if_fail (mime_type != NULL && mime_type != '\0', FALSE);
  g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  g_mutex_lock (&self->mutex_connection);

  if (orientation == 0)
    orientation = PHOTOS_ORIENTATION_TOP;

  thumbnail_size = photos_utils_get_icon_size ();

  local_error = NULL;
  pixbuf = photos_thumbnail_factory_get_preview (self, file, thumbnail_size, cancellable, &local_error);
  if (local_error != NULL)
    {
      if (g_error_matches (local_error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        goto out;
      else
        g_clear_error (&local_error);
    }

  if (pixbuf != NULL)
    {
      ret_val = TRUE;
      goto out;
    }

  if (pixbuf == NULL)
    {
      if (self->connection == NULL)
        {
          GSubprocess *subprocess;
          const gchar *address;
          gchar *thumbnailer_path;

          g_mutex_lock (&self->mutex_thumbnailer);

          g_clear_error (&self->thumbnailer_error);
          g_clear_object (&self->thumbnailer);

          g_mutex_unlock (&self->mutex_thumbnailer);

          address = g_dbus_server_get_client_address (self->dbus_server);
          thumbnailer_path = g_strconcat (PACKAGE_LIBEXEC_DIR,
                                          G_DIR_SEPARATOR_S,
                                          PACKAGE_TARNAME,
                                          "-thumbnailer",
                                          NULL);

          photos_debug (PHOTOS_DEBUG_THUMBNAILER, "Spawning “%s --address %s”", thumbnailer_path, address);

          local_error = NULL;
          subprocess = g_subprocess_new (G_SUBPROCESS_FLAGS_NONE,
                                         &local_error,
                                         thumbnailer_path,
                                         "--address",
                                         address,
                                         NULL);
          if (local_error != NULL)
            goto out;

          g_mutex_unlock (&self->mutex_connection);
          mutex_connection_unlocked = TRUE;

          g_free (thumbnailer_path);
          g_object_unref (subprocess);
        }

      photos_debug (PHOTOS_DEBUG_THUMBNAILER, "Waiting for org.gnome.Photos.Thumbnailer proxy");

      g_mutex_lock (&self->mutex_thumbnailer);
      while (self->thumbnailer == NULL && self->thumbnailer_error == NULL)
        g_cond_wait (&self->cond_thumbnailer, &self->mutex_thumbnailer);

      if (self->thumbnailer_error != NULL)
        {
          g_assert_null (self->thumbnailer);

          photos_debug (PHOTOS_DEBUG_THUMBNAILER, "Error creating org.gnome.Photos.Thumbnailer proxy");

          local_error = self->thumbnailer_error;
          self->thumbnailer_error = NULL;
        }
      else
        {
          const gchar *orientation_str;
          gchar *thumbnail_path = NULL;
          gchar *uri = NULL;

          g_assert_true (PHOTOS_IS_THUMBNAILER_DBUS (self->thumbnailer));

          uri = g_file_get_uri (file);
          orientation_str = g_quark_to_string (orientation);
          thumbnail_path = photos_utils_get_thumbnail_path_for_file (file);

          photos_debug (PHOTOS_DEBUG_THUMBNAILER, "Calling GenerateThumbnail for %s", uri);
          ret_val = photos_thumbnailer_dbus_call_generate_thumbnail_sync (self->thumbnailer,
                                                                          uri,
                                                                          mime_type,
                                                                          orientation_str,
                                                                          original_height,
                                                                          original_width,
                                                                          pipeline_uri,
                                                                          thumbnail_path,
                                                                          thumbnail_size,
                                                                          cancellable,
                                                                          &local_error);

          g_free (thumbnail_path);
          g_free (uri);
        }

      g_mutex_unlock (&self->mutex_thumbnailer);
    }

 out:
  if (!mutex_connection_unlocked)
    g_mutex_unlock (&self->mutex_connection);

  if (!ret_val)
    {
      g_assert_nonnull (local_error);
      g_propagate_error (error, local_error);
    }

  g_clear_object (&pixbuf);
  return ret_val;
}
