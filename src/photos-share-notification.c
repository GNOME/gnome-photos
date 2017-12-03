/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2017 Red Hat, Inc.
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
#include <glib/gi18n.h>

#include "photos-icons.h"
#include "photos-notification-manager.h"
#include "photos-share-notification.h"


struct _PhotosShareNotification
{
  GtkGrid parent_instance;
  GError *error;
  GtkWidget *ntfctn_mngr;
  PhotosBaseItem *item;
  PhotosSharePoint *share_point;
  gchar *uri;
  guint timeout_id;
};

enum
{
  PROP_0,
  PROP_ERROR,
  PROP_ITEM,
  PROP_SHARE_POINT,
  PROP_URI
};


G_DEFINE_TYPE (PhotosShareNotification, photos_share_notification, GTK_TYPE_GRID);


enum
{
  SHARE_TIMEOUT = 10 /* s */
};


static void
photos_share_notification_remove_timeout (PhotosShareNotification *self)
{
  if (self->timeout_id != 0)
    {
      g_source_remove (self->timeout_id);
      self->timeout_id = 0;
    }
}


static void
photos_share_notification_destroy (PhotosShareNotification *self)
{
  photos_share_notification_remove_timeout (self);
  gtk_widget_destroy (GTK_WIDGET (self));
}


static void
photos_share_notification_close (PhotosShareNotification *self)
{
  photos_share_notification_destroy (self);
}


static void
photos_share_notification_open (PhotosShareNotification *self)
{
  GApplication *app;
  GtkWindow *parent;
  guint32 time;

  g_return_if_fail (PHOTOS_IS_SHARE_NOTIFICATION (self));
  g_return_if_fail (photos_share_point_needs_notification (self->share_point));
  g_return_if_fail (PHOTOS_IS_BASE_ITEM (self->item));
  g_return_if_fail (self->uri != NULL && self->uri[0] != '\0');

  app = g_application_get_default ();
  parent = gtk_application_get_active_window (GTK_APPLICATION (app));
  time = gtk_get_current_event_time ();

  {
    g_autoptr (GError) error = NULL;

    if (!gtk_show_uri_on_window (parent, self->uri, time, &error))
      g_warning ("Failed to open uri: %s", error->message);
  }

  photos_share_notification_destroy (self);
}


static gboolean
photos_share_notification_timeout (gpointer user_data)
{
  PhotosShareNotification *self = PHOTOS_SHARE_NOTIFICATION (user_data);

  self->timeout_id = 0;
  photos_share_notification_destroy (self);
  return G_SOURCE_REMOVE;
}

static void
photos_share_notification_constructed (GObject *object)
{
  PhotosShareNotification *self = PHOTOS_SHARE_NOTIFICATION (object);
  GtkWidget *close;
  GtkWidget *image;
  GtkWidget *label;
  g_autofree gchar *msg = NULL;

  G_OBJECT_CLASS (photos_share_notification_parent_class)->constructed (object);

  gtk_grid_set_column_spacing (GTK_GRID (self), 12);
  gtk_orientable_set_orientation (GTK_ORIENTABLE (self), GTK_ORIENTATION_HORIZONTAL);

  if (self->item == NULL)
    {
      g_assert_nonnull (self->error);
      msg = photos_share_point_parse_error (self->share_point, self->error);
    }
  else
    {
      const gchar *name;

      g_assert_true (photos_share_point_needs_notification (self->share_point));
      name = photos_base_item_get_name_with_fallback (PHOTOS_BASE_ITEM (self->item));
      msg = g_strdup_printf (_("“%s” shared"), name);
    }

  label = gtk_label_new (msg);
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_widget_set_hexpand (label, TRUE);
  gtk_container_add (GTK_CONTAINER (self), label);

  if (self->item != NULL && self->uri != NULL && self->uri[0] != '\0')
    {
      GtkWidget *open;
      const gchar *name;
      g_autofree gchar *app_label = NULL;

      g_assert_true (photos_share_point_needs_notification (self->share_point));

      name = photos_share_point_get_name (self->share_point);
      app_label = g_strdup_printf (_("Open with %s"), name);

      open = gtk_button_new_with_label (app_label);
      gtk_widget_set_valign (open, GTK_ALIGN_CENTER);
      gtk_widget_set_halign (open, GTK_ALIGN_CENTER);
      gtk_container_add (GTK_CONTAINER (self), open);
      g_signal_connect_swapped (open, "clicked", G_CALLBACK (photos_share_notification_open), self);
    }

  image = gtk_image_new_from_icon_name (PHOTOS_ICON_WINDOW_CLOSE_SYMBOLIC, GTK_ICON_SIZE_INVALID);
  gtk_widget_set_margin_bottom (image, 2);
  gtk_widget_set_margin_top (image, 2);
  gtk_image_set_pixel_size (GTK_IMAGE (image), 16);

  close = gtk_button_new ();
  gtk_widget_set_valign (close, GTK_ALIGN_CENTER);
  gtk_widget_set_focus_on_click (close, FALSE);
  gtk_button_set_relief (GTK_BUTTON (close), GTK_RELIEF_NONE);
  gtk_button_set_image (GTK_BUTTON (close), image);
  gtk_container_add (GTK_CONTAINER (self), close);
  g_signal_connect_swapped (close, "clicked", G_CALLBACK (photos_share_notification_close), self);

  photos_notification_manager_add_notification (PHOTOS_NOTIFICATION_MANAGER (self->ntfctn_mngr), GTK_WIDGET (self));

  self->timeout_id = g_timeout_add_seconds (SHARE_TIMEOUT, photos_share_notification_timeout, self);
}


static void
photos_share_notification_dispose (GObject *object)
{
  PhotosShareNotification *self = PHOTOS_SHARE_NOTIFICATION (object);

  photos_share_notification_remove_timeout (self);

  g_clear_object (&self->item);
  g_clear_object (&self->ntfctn_mngr);

  G_OBJECT_CLASS (photos_share_notification_parent_class)->dispose (object);
}


static void
photos_share_notification_finalize (GObject *object)
{
  PhotosShareNotification *self = PHOTOS_SHARE_NOTIFICATION (object);

  g_clear_error (&self->error);
  g_free (self->uri);

  G_OBJECT_CLASS (photos_share_notification_parent_class)->finalize (object);
}


static void
photos_share_notification_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
  PhotosShareNotification *self = PHOTOS_SHARE_NOTIFICATION (object);

  switch (prop_id)
    {
    case PROP_ERROR:
      self->error = (GError *) g_value_dup_boxed (value);
      break;

    case PROP_ITEM:
      self->item = PHOTOS_BASE_ITEM (g_value_dup_object (value));
      break;

    case PROP_SHARE_POINT:
      self->share_point = PHOTOS_SHARE_POINT (g_value_dup_object (value));
      break;

    case PROP_URI:
      self->uri = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
photos_share_notification_init (PhotosShareNotification *self)
{
  self->ntfctn_mngr = photos_notification_manager_dup_singleton ();
}


static void
photos_share_notification_class_init (PhotosShareNotificationClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->constructed = photos_share_notification_constructed;
  object_class->dispose = photos_share_notification_dispose;
  object_class->finalize = photos_share_notification_finalize;
  object_class->set_property = photos_share_notification_set_property;

  g_object_class_install_property (object_class,
                                   PROP_ERROR,
                                   g_param_spec_boxed ("error",
                                                       "Error",
                                                       "Error thrown during share",
                                                       G_TYPE_ERROR,
                                                       G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE));

  g_object_class_install_property (object_class,
                                   PROP_ITEM,
                                   g_param_spec_object ("item",
                                                        "Item",
                                                        "The original item that was shared",
                                                        PHOTOS_TYPE_BASE_ITEM,
                                                        G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE));

  g_object_class_install_property (object_class,
                                   PROP_SHARE_POINT,
                                   g_param_spec_object ("share-point",
                                                        "Share point object",
                                                        "Share point under use",
                                                        PHOTOS_TYPE_SHARE_POINT,
                                                        G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE));

  g_object_class_install_property (object_class,
                                   PROP_URI,
                                   g_param_spec_string ("uri",
                                                        "URI",
                                                        "The URI at which the shared item can be accessed",
                                                        NULL,
                                                        G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE));
}


void
photos_share_notification_new (PhotosSharePoint *share_point, PhotosBaseItem *item, const gchar *uri)
{
  g_return_if_fail (PHOTOS_IS_SHARE_POINT (share_point));
  g_return_if_fail (photos_share_point_needs_notification (share_point));
  g_return_if_fail (PHOTOS_IS_BASE_ITEM (item));

  g_object_new (PHOTOS_TYPE_SHARE_NOTIFICATION, "share-point", share_point, "item", item, "uri", uri, NULL);
}


void
photos_share_notification_new_with_error (PhotosSharePoint *share_point, GError *error)
{
  g_return_if_fail (PHOTOS_IS_SHARE_POINT (share_point));
  g_return_if_fail (error != NULL);

  g_object_new (PHOTOS_TYPE_SHARE_NOTIFICATION, "share-point", share_point, "error", error, NULL);
}
