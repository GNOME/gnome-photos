/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2017 – 2021 Red Hat, Inc.
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
#include "photos-quarks.h"
#include "photos-thumbnail-factory.h"
#include "photos-thumbnailer-dbus.h"
#include "photos-utils.h"


struct _PhotosThumbnailFactory
{
  GObject parent_instance;
  GCond cond;
  GDBusConnection *connection;
  GDBusServer *dbus_server;
  GError *initialization_error;
  GError *thumbnailer_error;
  GMutex mutex;
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
  g_autoptr (GCredentials) own_credentials = NULL;
  gboolean ret_val = FALSE;
  g_autofree gchar *str = NULL;

  g_mutex_lock (&self->mutex);

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
  g_mutex_unlock (&self->mutex);
  return ret_val;
}


static void
photos_thumbnail_factory_connection_closed (PhotosThumbnailFactory *self,
                                            gboolean remote_peer_vanished,
                                            GError *error)
{
  g_mutex_lock (&self->mutex);

  if (error != NULL)
    photos_debug (PHOTOS_DEBUG_THUMBNAILER, "Lost connection to the thumbnailer: %s", error->message);
  else
    photos_debug (PHOTOS_DEBUG_THUMBNAILER, "Lost connection to the thumbnailer");

  g_signal_handlers_disconnect_by_func (self->connection, photos_thumbnail_factory_connection_closed, self);
  g_clear_object (&self->connection);

  g_clear_error (&self->thumbnailer_error);
  g_clear_object (&self->thumbnailer);

  g_mutex_unlock (&self->mutex);
}


static gboolean
photos_thumbnail_factory_new_connection (PhotosThumbnailFactory *self, GDBusConnection *connection)
{
  g_mutex_lock (&self->mutex);

  g_assert_null (self->connection);

  g_assert (self->thumbnailer_error == NULL);
  g_assert (self->thumbnailer == NULL);

  photos_debug (PHOTOS_DEBUG_THUMBNAILER, "Received new connection");

  self->connection = g_object_ref (connection);
  g_signal_connect_swapped (self->connection,
                            "closed",
                            G_CALLBACK (photos_thumbnail_factory_connection_closed),
                            self);

  self->thumbnailer = photos_thumbnailer_dbus_proxy_new_sync (self->connection,
                                                              G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES
                                                              | G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS,
                                                              NULL,
                                                              THUMBNAILER_PATH,
                                                              NULL,
                                                              &self->thumbnailer_error);

  g_cond_signal (&self->cond);
  g_mutex_unlock (&self->mutex);

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

  g_cond_clear (&self->cond);
  g_clear_error (&self->initialization_error);
  g_clear_error (&self->thumbnailer_error);
  g_mutex_clear (&self->mutex);

  G_OBJECT_CLASS (photos_thumbnail_factory_parent_class)->finalize (object);
}


static void
photos_thumbnail_factory_init (PhotosThumbnailFactory *self)
{
  g_cond_init (&self->cond);
  g_mutex_init (&self->mutex);
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
                            G_CALLBACK (photos_thumbnail_factory_authorize_authenticated_peer),
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
  g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  return g_initable_new (PHOTOS_TYPE_THUMBNAIL_FACTORY, cancellable, error, NULL);
}


gboolean
photos_thumbnail_factory_generate_thumbnail (PhotosThumbnailFactory *self,
                                             GFile *file,
                                             const gchar *mime_type,
                                             GQuark orientation,
                                             gint64 original_height,
                                             gint64 original_width,
                                             const gchar *const *pipeline_uris,
                                             const gchar *thumbnail_path,
                                             GCancellable *cancellable,
                                             GError **error)
{
  GError *local_error = NULL;
  gboolean ret_val = FALSE;

  g_return_val_if_fail (PHOTOS_IS_THUMBNAIL_FACTORY (self), FALSE);
  g_return_val_if_fail (G_IS_FILE (file), FALSE);
  g_return_val_if_fail (mime_type != NULL && mime_type[0] != '\0', FALSE);
  g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  g_mutex_lock (&self->mutex);

  if (orientation == 0)
    orientation = PHOTOS_ORIENTATION_TOP;

  if (self->connection == NULL)
    {
      g_autoptr (GSubprocess) subprocess = NULL;
      const gchar *address;
      g_autofree gchar *thumbnailer_path = NULL;

      g_assert (self->thumbnailer_error == NULL);
      g_assert (self->thumbnailer == NULL);

      address = g_dbus_server_get_client_address (self->dbus_server);
      thumbnailer_path = g_strconcat (PACKAGE_LIBEXEC_DIR,
                                      G_DIR_SEPARATOR_S,
                                      PACKAGE_TARNAME,
                                      "-thumbnailer",
                                      NULL);

      photos_debug (PHOTOS_DEBUG_THUMBNAILER, "Spawning “%s --address %s”", thumbnailer_path, address);

      {
        g_autoptr (GError) subprocess_error = NULL;

        subprocess = g_subprocess_new (G_SUBPROCESS_FLAGS_NONE,
                                       &subprocess_error,
                                       thumbnailer_path,
                                       "--address",
                                       address,
                                       NULL);
        if (subprocess_error != NULL)
          {
            local_error = g_steal_pointer (&subprocess_error);
            goto out;
          }
      }

      photos_debug (PHOTOS_DEBUG_THUMBNAILER, "Waiting for org.gnome.Photos.Thumbnailer proxy");

      while (self->connection == NULL)
        g_cond_wait (&self->cond, &self->mutex);
    }

  if (self->thumbnailer_error != NULL)
    {
      g_assert_null (self->thumbnailer);

      photos_debug (PHOTOS_DEBUG_THUMBNAILER, "Error creating org.gnome.Photos.Thumbnailer proxy");
      local_error = g_steal_pointer (&self->thumbnailer_error);
    }
  else
    {
      GVariant *pipeline_uris_variant;
      const gchar *orientation_str;
      g_autofree gchar *uri = NULL;
      gint thumbnail_size;
      gssize n_pipeline_uris;

      g_assert_true (PHOTOS_IS_THUMBNAILER_DBUS (self->thumbnailer));

      uri = g_file_get_uri (file);
      orientation_str = g_quark_to_string (orientation);

      n_pipeline_uris = pipeline_uris == NULL ? 0 : -1;
      pipeline_uris_variant = g_variant_new_strv (pipeline_uris, n_pipeline_uris);

      thumbnail_size = photos_utils_get_icon_size ();

      photos_debug (PHOTOS_DEBUG_THUMBNAILER, "Calling GenerateThumbnail for %s", uri);
      if (!photos_thumbnailer_dbus_call_generate_thumbnail_sync (self->thumbnailer,
                                                                 uri,
                                                                 mime_type,
                                                                 orientation_str,
                                                                 original_height,
                                                                 original_width,
                                                                 pipeline_uris_variant,
                                                                 thumbnail_path,
                                                                 thumbnail_size,
                                                                 cancellable,
                                                                 &local_error))
        {
          if (g_error_matches (local_error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
            {
              guint32 serial;

              serial = g_dbus_connection_get_last_serial (self->connection);
              photos_thumbnailer_dbus_call_cancel_sync (self->thumbnailer, (guint) serial, NULL, NULL);
            }
        }
      else
        {
          ret_val = TRUE;
        }
    }

 out:
  g_mutex_unlock (&self->mutex);

  if (!ret_val)
    {
      g_assert_nonnull (local_error);
      g_propagate_error (error, local_error);
    }

  return ret_val;
}
