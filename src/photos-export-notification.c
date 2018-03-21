/*
 * Photos - access, organize and share your photos on GNOME
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

#include <dazzle.h>
#include <gio/gdesktopappinfo.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "photos-base-item.h"
#include "photos-base-manager.h"
#include "photos-export-notification.h"
#include "photos-notification-manager.h"
#include "photos-utils.h"


struct _PhotosExportNotification
{
  GtkGrid parent_instance;
  GtkWidget *ntfctn_mngr;
  GError *error;
  GFile *file;
  GList *items;
  guint timeout_id;
};

enum
{
  PROP_0,
  PROP_ERROR,
  PROP_FILE,
  PROP_ITEMS
};


G_DEFINE_TYPE (PhotosExportNotification, photos_export_notification, GTK_TYPE_GRID);


enum
{
  EXPORT_TIMEOUT = 10 /* s */
};


static void
photos_export_notification_remove_timeout (PhotosExportNotification *self)
{
  if (self->timeout_id != 0)
    {
      g_source_remove (self->timeout_id);
      self->timeout_id = 0;
    }
}


static void
photos_export_notification_destroy (PhotosExportNotification *self)
{
  photos_export_notification_remove_timeout (self);
  gtk_widget_destroy (GTK_WIDGET (self));
}


static void
photos_export_notification_analyze (PhotosExportNotification *self)
{
  g_autoptr (GAppLaunchContext) ctx = NULL;
  g_autoptr (GDesktopAppInfo) analyzer = NULL;

  analyzer = g_desktop_app_info_new ("org.gnome.baobab.desktop");
  ctx = photos_utils_new_app_launch_context_from_widget (GTK_WIDGET (self));

  {
    g_autoptr (GError) error = NULL;

    if (!g_app_info_launch (G_APP_INFO (analyzer), NULL, ctx, &error))
      g_warning ("Unable to launch disk usage analyzer: %s", error->message);
  }

  photos_export_notification_destroy (self);
}


static void
photos_export_notification_close (PhotosExportNotification *self)
{
  photos_export_notification_destroy (self);
}


static void
photos_export_notification_empty_trash_call_empty_trash (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  GDBusConnection *connection = G_DBUS_CONNECTION (source_object);
  g_autoptr (GVariant) variant = NULL;

  {
    g_autoptr (GError) error = NULL;

    variant = g_dbus_connection_call_finish (connection, res, &error);
    if (error != NULL)
      {
        if (g_error_matches (error, G_DBUS_ERROR, G_DBUS_ERROR_SERVICE_UNKNOWN)
            || g_error_matches (error, G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_INTERFACE)
            || g_error_matches (error, G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_METHOD)
            || g_error_matches (error, G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_OBJECT))
          {
            g_warning ("Unable to call org.gnome.SettingsDaemon.Housekeeping:EmptyTrash: "
                       "unknown interface/method/object/service");
          }
        else
          {
            g_warning ("Unable to call org.gnome.SettingsDaemon.Housekeeping:EmptyTrash: %s", error->message);
          }
      }
  }
}


static void
photos_export_notification_empty_trash_call_empty_trash_pre_3_24 (GObject *source_object,
                                                                  GAsyncResult *res,
                                                                  gpointer user_data)
{
  GDBusConnection *connection = G_DBUS_CONNECTION (source_object);
  g_autoptr (GVariant) variant = NULL;

  {
    g_autoptr (GError) error = NULL;

    variant = g_dbus_connection_call_finish (connection, res, &error);
    if (error != NULL)
      {
        if (g_error_matches (error, G_DBUS_ERROR, G_DBUS_ERROR_SERVICE_UNKNOWN)
            || g_error_matches (error, G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_INTERFACE)
            || g_error_matches (error, G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_METHOD)
            || g_error_matches (error, G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_OBJECT))
          {
            g_dbus_connection_call (connection,
                                    "org.gnome.SettingsDaemon.Housekeeping",
                                    "/org/gnome/SettingsDaemon/Housekeeping",
                                    "org.gnome.SettingsDaemon.Housekeeping",
                                    "EmptyTrash",
                                    NULL,
                                    NULL,
                                    G_DBUS_CALL_FLAGS_NONE,
                                    -1,
                                    NULL,
                                    photos_export_notification_empty_trash_call_empty_trash,
                                    NULL);
          }
        else
          {
            g_warning ("Unable to call org.gnome.SettingsDaemon.Housekeeping:EmptyTrash (GNOME < 3.24): %s",
                       error->message);
          }
      }
  }
}


static void
photos_export_notification_empty_trash_bus_get (GObject *source_object, GAsyncResult *result, gpointer user_data)
{
  g_autoptr (PhotosExportNotification) self = PHOTOS_EXPORT_NOTIFICATION (user_data);
  g_autoptr (GDBusConnection) connection = NULL;

  {
    g_autoptr (GError) error = NULL;

    connection = g_bus_get_finish (result, &error);
    if (error != NULL)
      {
        g_warning ("Unable to connect to session bus: %s", error->message);
        goto out;
      }
  }

  g_dbus_connection_call (connection,
                          "org.gnome.SettingsDaemon",
                          "/org/gnome/SettingsDaemon/Housekeeping",
                          "org.gnome.SettingsDaemon.Housekeeping",
                          "EmptyTrash",
                          NULL,
                          NULL,
                          G_DBUS_CALL_FLAGS_NONE,
                          -1,
                          NULL,
                          photos_export_notification_empty_trash_call_empty_trash_pre_3_24,
                          NULL);

 out:
  return;
}


static void
photos_export_notification_empty_trash (PhotosExportNotification *self)
{
  g_bus_get (G_BUS_TYPE_SESSION, NULL, photos_export_notification_empty_trash_bus_get, g_object_ref (self));
  photos_export_notification_destroy (self);
}


static void
photos_export_notification_export_folder (PhotosExportNotification *self)
{
  GApplication *app;
  g_autoptr (GFile) directory = NULL;
  GtkWindow *parent;
  g_autofree gchar *directory_uri = NULL;
  guint32 time;

  g_return_if_fail (self->file != NULL);
  g_return_if_fail (self->items != NULL);

  app = g_application_get_default ();
  parent = gtk_application_get_active_window (GTK_APPLICATION (app));
  time = gtk_get_current_event_time ();

  if (self->items->next == NULL) /* length == 1 */
    {
      {
        g_autoptr (GError) error = NULL;
        g_autofree gchar *file_uri = NULL;

        if (dzl_file_manager_show (self->file, &error))
          goto out;

        file_uri = g_file_get_uri (self->file);
        g_warning ("Unable to use org.freedesktop.FileManager1 for %s: %s", file_uri, error->message);
      }

      directory = g_file_get_parent (self->file);
    }
  else
    {
      directory = g_object_ref (self->file);
    }

  directory_uri = g_file_get_uri (directory);

  {
    g_autoptr (GError) error = NULL;

    if (!gtk_show_uri_on_window (parent, directory_uri, time, &error))
      g_warning ("Failed to open uri: %s", error->message);
  }

 out:
  photos_export_notification_destroy (self);
}


static void
photos_export_notification_open (PhotosExportNotification *self)
{
  GApplication *app;
  GtkWindow *parent;
  g_autofree gchar *uri = NULL;
  guint32 time;

  g_return_if_fail (self->file != NULL);
  g_return_if_fail (self->items != NULL);
  g_return_if_fail (self->items->next == NULL);

  app = g_application_get_default ();
  parent = gtk_application_get_active_window (GTK_APPLICATION (app));
  time = gtk_get_current_event_time ();

  uri = g_file_get_uri (self->file);

  {
    g_autoptr (GError) error = NULL;

    if (!gtk_show_uri_on_window (parent, uri, time, &error))
      g_warning ("Failed to open uri: %s", error->message);
  }

  photos_export_notification_destroy (self);
}


static gboolean
photos_export_notification_timeout (gpointer user_data)
{
  PhotosExportNotification *self = PHOTOS_EXPORT_NOTIFICATION (user_data);

  self->timeout_id = 0;
  photos_export_notification_destroy (self);
  return G_SOURCE_REMOVE;
}


static void
photos_export_notification_constructed (GObject *object)
{
  PhotosExportNotification *self = PHOTOS_EXPORT_NOTIFICATION (object);
  GtkWidget *close;
  GtkWidget *image;
  GtkWidget *label;
  g_autofree gchar *msg = NULL;
  guint length;

  G_OBJECT_CLASS (photos_export_notification_parent_class)->constructed (object);

  gtk_grid_set_column_spacing (GTK_GRID (self), 12);
  gtk_orientable_set_orientation (GTK_ORIENTABLE (self), GTK_ORIENTATION_HORIZONTAL);

  length = g_list_length (self->items);

  if (length == 0)
    {
      g_assert_nonnull (self->error);

      if (g_error_matches (self->error, G_IO_ERROR, G_IO_ERROR_NO_SPACE))
        msg = g_strdup (_("Failed to export: not enough space"));
      else
        msg = g_strdup (_("Failed to export"));
    }
  else if (length == 1)
    {
      const gchar *name;

      name = photos_base_item_get_name_with_fallback (PHOTOS_BASE_ITEM (self->items->data));
      msg = g_strdup_printf (_("“%s” exported"), name);
    }
  else
    {
      msg = g_strdup_printf (ngettext ("%d item exported", "%d items exported", length), length);
    }

  label = gtk_label_new (msg);
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_widget_set_hexpand (label, TRUE);
  gtk_container_add (GTK_CONTAINER (self), label);

  if (length == 0)
    {
      g_assert_nonnull (self->error);

      if (g_error_matches (self->error, G_IO_ERROR, G_IO_ERROR_NO_SPACE))
        {
          GtkWidget *analyze;
          GtkWidget *empty_trash;

          analyze = gtk_button_new_with_label (_("Analyze"));
          gtk_widget_set_valign (analyze, GTK_ALIGN_CENTER);
          gtk_container_add (GTK_CONTAINER (self), analyze);
          g_signal_connect_swapped (analyze, "clicked", G_CALLBACK (photos_export_notification_analyze), self);

          empty_trash = gtk_button_new_with_label (_("Empty Trash"));
          gtk_widget_set_valign (empty_trash, GTK_ALIGN_CENTER);
          gtk_container_add (GTK_CONTAINER (self), empty_trash);
          g_signal_connect_swapped (empty_trash,
                                    "clicked",
                                    G_CALLBACK (photos_export_notification_empty_trash),
                                    self);
        }
    }
  else
    {
      GtkWidget *export_folder;

      if (length == 1)
        {
          GtkWidget *open;

          open = gtk_button_new_with_label (_("Open"));
          gtk_widget_set_valign (open, GTK_ALIGN_CENTER);
          gtk_widget_set_halign (open, GTK_ALIGN_CENTER);
          gtk_container_add (GTK_CONTAINER (self), open);
          g_signal_connect_swapped (open, "clicked", G_CALLBACK (photos_export_notification_open), self);
        }

      /* Translators: this is the label of the button to open the
       * folder where the item was exported.
       */
      export_folder = gtk_button_new_with_label (_("Export Folder"));
      gtk_widget_set_valign (export_folder, GTK_ALIGN_CENTER);
      gtk_container_add (GTK_CONTAINER (self), export_folder);
      g_signal_connect_swapped (export_folder,
                                "clicked",
                                G_CALLBACK (photos_export_notification_export_folder),
                                self);
    }

  image = gtk_image_new_from_icon_name ("window-close-symbolic", GTK_ICON_SIZE_INVALID);
  gtk_widget_set_margin_bottom (image, 2);
  gtk_widget_set_margin_top (image, 2);
  gtk_image_set_pixel_size (GTK_IMAGE (image), 16);

  close = gtk_button_new ();
  gtk_widget_set_valign (close, GTK_ALIGN_CENTER);
  gtk_widget_set_focus_on_click (close, FALSE);
  gtk_button_set_relief (GTK_BUTTON (close), GTK_RELIEF_NONE);
  gtk_button_set_image (GTK_BUTTON (close), image);
  gtk_container_add (GTK_CONTAINER (self), close);
  g_signal_connect_swapped (close, "clicked", G_CALLBACK (photos_export_notification_close), self);

  photos_notification_manager_add_notification (PHOTOS_NOTIFICATION_MANAGER (self->ntfctn_mngr), GTK_WIDGET (self));

  self->timeout_id = g_timeout_add_seconds (EXPORT_TIMEOUT, photos_export_notification_timeout, self);
}


static void
photos_export_notification_dispose (GObject *object)
{
  PhotosExportNotification *self = PHOTOS_EXPORT_NOTIFICATION (object);

  photos_export_notification_remove_timeout (self);

  if (self->items != NULL)
    {
      g_list_free_full (self->items, g_object_unref);
      self->items = NULL;
    }

  g_clear_object (&self->file);
  g_clear_object (&self->ntfctn_mngr);

  G_OBJECT_CLASS (photos_export_notification_parent_class)->dispose (object);
}


static void
photos_export_notification_finalize (GObject *object)
{
  PhotosExportNotification *self = PHOTOS_EXPORT_NOTIFICATION (object);

  g_clear_error (&self->error);

  G_OBJECT_CLASS (photos_export_notification_parent_class)->finalize (object);
}


static void
photos_export_notification_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
  PhotosExportNotification *self = PHOTOS_EXPORT_NOTIFICATION (object);

  switch (prop_id)
    {
    case PROP_ERROR:
      self->error = (GError *) g_value_dup_boxed (value);
      break;

    case PROP_FILE:
      self->file = G_FILE (g_value_dup_object (value));
      break;

    case PROP_ITEMS:
      {
        GList *items;

        items = (GList *) g_value_get_pointer (value);
        self->items = g_list_copy_deep (items, (GCopyFunc) g_object_ref, NULL);
        break;
      }

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
photos_export_notification_init (PhotosExportNotification *self)
{
  self->ntfctn_mngr = photos_notification_manager_dup_singleton ();
}


static void
photos_export_notification_class_init (PhotosExportNotificationClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->constructed = photos_export_notification_constructed;
  object_class->dispose = photos_export_notification_dispose;
  object_class->finalize = photos_export_notification_finalize;
  object_class->set_property = photos_export_notification_set_property;

  g_object_class_install_property (object_class,
                                   PROP_ERROR,
                                   g_param_spec_boxed ("error",
                                                       "Error",
                                                       "Error thrown during export",
                                                       G_TYPE_ERROR,
                                                       G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE));

  g_object_class_install_property (object_class,
                                   PROP_FILE,
                                   g_param_spec_object ("file",
                                                        "File",
                                                        "A GFile representing the exported file or directory",
                                                        G_TYPE_FILE,
                                                        G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE));

  g_object_class_install_property (object_class,
                                   PROP_ITEMS,
                                   g_param_spec_pointer ("items",
                                                         "List of PhotosBaseItems",
                                                         "List of items that were exported",
                                                         G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE));
}


void
photos_export_notification_new (GList *items, GFile *file)
{
  g_return_if_fail (G_IS_FILE (file));
  g_return_if_fail (items != NULL);

  g_object_new (PHOTOS_TYPE_EXPORT_NOTIFICATION, "file", file, "items", items, NULL);
}


void
photos_export_notification_new_with_error (GError *error)
{
  g_return_if_fail (error != NULL);
  g_object_new (PHOTOS_TYPE_EXPORT_NOTIFICATION, "error", error, NULL);
}
