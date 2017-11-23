/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2017 Red Hat, Inc.
 * Copyright © 2017 Umang Jain
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
#include <glib/gi18n.h>

#include "photos-source-notification.h"
#include "photos-utils.h"


struct _PhotosSourceNotification
{
  GtkGrid parent_instance;
  PhotosSource *source;
};

enum
{
  PROP_0,
  PROP_SOURCE
};

enum
{
  CLOSED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };


G_DEFINE_TYPE (PhotosSourceNotification, photos_source_notification, GTK_TYPE_GRID);


static void
photos_source_notification_close (PhotosSourceNotification *self)
{
  g_signal_emit (self, signals[CLOSED], 0);
}


static void
photos_source_notification_settings_clicked (PhotosSourceNotification *self)
{
  g_autoptr (GAppInfo) app = NULL;
  g_autoptr (GAppLaunchContext) ctx = NULL;
  GoaAccount *account;
  GoaObject *object;
  const gchar *id;
  g_autofree gchar *command_line = NULL;

  object = photos_source_get_goa_object (self->source);
  g_return_if_fail (GOA_IS_OBJECT (object));

  account = goa_object_peek_account (object);
  id = goa_account_get_id (account);
  command_line = g_strconcat ("gnome-control-center online-accounts ", id, NULL);

  {
    g_autoptr (GError) error = NULL;

    app = g_app_info_create_from_commandline (command_line, NULL, G_APP_INFO_CREATE_NONE, &error);
    if (error != NULL)
      {
        g_warning ("Unable to launch gnome-control-center: %s", error->message);
        goto out;
      }
  }

  ctx = photos_utils_new_app_launch_context_from_widget (GTK_WIDGET (self));

  {
    g_autoptr (GError) error = NULL;

    g_app_info_launch (app, NULL, ctx, &error);
    if (error != NULL)
      {
        g_warning ("Unable to launch gnome-control-center: %s", error->message);
        goto out;
      }
  }

 out:
  return;
}


static void
photos_source_notification_constructed (GObject *object)
{
  PhotosSourceNotification *self = PHOTOS_SOURCE_NOTIFICATION (object);
  GtkWidget *close;
  GtkWidget *image;
  GtkWidget *label;
  GtkWidget *settings;
  const gchar *name;
  g_autofree gchar *msg = NULL;

  G_OBJECT_CLASS (photos_source_notification_parent_class)->constructed (object);

  gtk_grid_set_column_spacing (GTK_GRID (self), 12);
  gtk_orientable_set_orientation (GTK_ORIENTABLE (self), GTK_ORIENTATION_HORIZONTAL);

  name = photos_source_get_name (self->source);
  /* Translators: %s refers to an online account provider, e.g.,
   * "Facebook" or "Flickr".
   */
  msg = g_strdup_printf (_("Your %s credentials have expired"), name);

  label = gtk_label_new (msg);
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_widget_set_hexpand (label, TRUE);
  gtk_container_add (GTK_CONTAINER (self), label);

  settings = gtk_button_new_with_label (_("Settings"));
  gtk_widget_set_valign (settings, GTK_ALIGN_CENTER);
  gtk_container_add (GTK_CONTAINER (self), settings);
  g_signal_connect_swapped (settings, "clicked", G_CALLBACK (photos_source_notification_settings_clicked), self);

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
  g_signal_connect_swapped (close, "clicked", G_CALLBACK (photos_source_notification_close), self);
}


static void
photos_source_notification_dispose (GObject *object)
{
  PhotosSourceNotification *self = PHOTOS_SOURCE_NOTIFICATION (object);

  g_clear_object (&self->source);

  G_OBJECT_CLASS (photos_source_notification_parent_class)->dispose (object);
}


static void
photos_source_notification_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
  PhotosSourceNotification *self = PHOTOS_SOURCE_NOTIFICATION (object);

  switch (prop_id)
    {
    case PROP_SOURCE:
      g_value_set_object (value, self->source);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
photos_source_notification_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
  PhotosSourceNotification *self = PHOTOS_SOURCE_NOTIFICATION (object);

  switch (prop_id)
    {
    case PROP_SOURCE:
      self->source = PHOTOS_SOURCE (g_value_dup_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
photos_source_notification_init (PhotosSourceNotification *self)
{
}


static void
photos_source_notification_class_init (PhotosSourceNotificationClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->constructed = photos_source_notification_constructed;
  object_class->dispose = photos_source_notification_dispose;
  object_class->get_property = photos_source_notification_get_property;
  object_class->set_property = photos_source_notification_set_property;

  g_object_class_install_property (object_class,
                                   PROP_SOURCE,
                                   g_param_spec_object ("source",
                                                        "PhotosSource instance",
                                                        "The PhotosSource corresponding to this notification",
                                                        PHOTOS_TYPE_SOURCE,
                                                        G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE));

  signals[CLOSED] = g_signal_new ("closed",
                                  G_TYPE_FROM_CLASS (class),
                                  G_SIGNAL_RUN_LAST,
                                  0,
                                  NULL, /* accumulator */
                                  NULL, /* accu_data */
                                  g_cclosure_marshal_VOID__VOID,
                                  G_TYPE_NONE,
                                  0);
}


GtkWidget *
photos_source_notification_new (PhotosSource *source)
{
  GoaAccount *account;
  GoaObject *object;
  gboolean attention_needed;

  g_return_val_if_fail (PHOTOS_IS_SOURCE (source), NULL);

  object = photos_source_get_goa_object (source);
  g_return_val_if_fail (GOA_IS_OBJECT (object), NULL);

  account = goa_object_peek_account (object);
  g_return_val_if_fail (GOA_IS_ACCOUNT (account), NULL);

  attention_needed = goa_account_get_attention_needed (account);
  g_return_val_if_fail (attention_needed, NULL);

  return g_object_new (PHOTOS_TYPE_SOURCE_NOTIFICATION, "source", source, NULL);
}


PhotosSource *
photos_source_notification_get_source (PhotosSourceNotification *self)
{
  g_return_val_if_fail (PHOTOS_IS_SOURCE_NOTIFICATION (self), NULL);
  return self->source;
}
