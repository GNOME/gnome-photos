/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2014 Red Hat, Inc.
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

/* Based on code from:
 *   + Documents
 */


#include "config.h"

#include <glib.h>
#include <glib/gi18n.h>

#include "photos-icons.h"
#include "photos-print-notification.h"
#include "photos-notification-manager.h"


struct _PhotosPrintNotificationPrivate
{
  GtkPrintOperation *print_op;
  GtkWidget *ntfctn_mngr;
  GtkWidget *spinner;
  GtkWidget *status_label;
  GtkWidget *stop_button;
};

enum
{
  PROP_0,
  PROP_PRINT_OP
};


G_DEFINE_TYPE_WITH_PRIVATE (PhotosPrintNotification, photos_print_notification, GTK_TYPE_GRID);


static void
photos_print_notification_begin_print (PhotosPrintNotification *self)
{
  PhotosPrintNotificationPrivate *priv = self->priv;

  photos_notification_manager_add_notification (PHOTOS_NOTIFICATION_MANAGER (priv->ntfctn_mngr),
                                                GTK_WIDGET (self));
  gtk_spinner_start (GTK_SPINNER (priv->spinner));
}


static void
photos_print_notification_status_changed (PhotosPrintNotification *self)
{
  PhotosPrintNotificationPrivate *priv = self->priv;
  const gchar *status_str;
  gchar *job_name = NULL;
  gchar *status = NULL;

  status_str = gtk_print_operation_get_status_string (priv->print_op);
  g_object_get (priv->print_op, "job-name", &job_name, NULL);
  status = g_strdup_printf (_("Printing “%s”: %s"), job_name, status_str);
  gtk_label_set_text (GTK_LABEL (priv->status_label), status);

  if (gtk_print_operation_is_finished (priv->print_op))
    gtk_widget_destroy (GTK_WIDGET (self));

  g_free (job_name);
  g_free (status);
}


static void
photos_print_notification_stop_clicked (PhotosPrintNotification *self)
{
  gtk_print_operation_cancel (self->priv->print_op);
  gtk_widget_destroy (GTK_WIDGET (self));
}


static void
photos_print_notification_constructed (GObject *object)
{
  PhotosPrintNotification *self = PHOTOS_PRINT_NOTIFICATION (object);
  PhotosPrintNotificationPrivate *priv = self->priv;

  G_OBJECT_CLASS (photos_print_notification_parent_class)->constructed (object);

  g_signal_connect_object (priv->print_op,
                           "begin-print",
                           G_CALLBACK (photos_print_notification_begin_print),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (priv->print_op,
                           "status-changed",
                           G_CALLBACK (photos_print_notification_status_changed),
                           self,
                           G_CONNECT_SWAPPED);
}


static void
photos_print_notification_dispose (GObject *object)
{
  PhotosPrintNotification *self = PHOTOS_PRINT_NOTIFICATION (object);
  PhotosPrintNotificationPrivate *priv = self->priv;

  g_clear_object (&priv->print_op);
  g_clear_object (&priv->ntfctn_mngr);

  G_OBJECT_CLASS (photos_print_notification_parent_class)->dispose (object);
}


static void
photos_print_notification_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
  PhotosPrintNotification *self = PHOTOS_PRINT_NOTIFICATION (object);

  switch (prop_id)
    {
    case PROP_PRINT_OP:
      self->priv->print_op = GTK_PRINT_OPERATION (g_value_dup_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
photos_print_notification_init (PhotosPrintNotification *self)
{
  PhotosPrintNotificationPrivate *priv;
  GtkWidget *image;

  self->priv = photos_print_notification_get_instance_private (self);
  priv = self->priv;

  priv->ntfctn_mngr = g_object_ref_sink (photos_notification_manager_dup_singleton ());

  priv->spinner = gtk_spinner_new ();
  gtk_widget_set_size_request (priv->spinner, 16, 16);
  gtk_container_add (GTK_CONTAINER (self), priv->spinner);

  priv->status_label = gtk_label_new (NULL);
  gtk_widget_set_halign (priv->status_label, GTK_ALIGN_START);
  gtk_container_add (GTK_CONTAINER (self), priv->status_label);

  image = gtk_image_new_from_icon_name (PHOTOS_ICON_PROCESS_STOP_SYMBOLIC, GTK_ICON_SIZE_INVALID);
  gtk_widget_set_margin_bottom (image, 2);
  gtk_widget_set_margin_top (image, 2);
  gtk_image_set_pixel_size (GTK_IMAGE (image), 16);

  priv->stop_button = gtk_button_new ();
  gtk_widget_set_valign (priv->stop_button, GTK_ALIGN_CENTER);
  gtk_container_add (GTK_CONTAINER (priv->stop_button), image);
  gtk_container_add (GTK_CONTAINER (self), priv->stop_button);
  g_signal_connect_swapped (priv->stop_button,
                            "clicked",
                            G_CALLBACK (photos_print_notification_stop_clicked),
                            self);
}


static void
photos_print_notification_class_init (PhotosPrintNotificationClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->constructed = photos_print_notification_constructed;
  object_class->dispose = photos_print_notification_dispose;
  object_class->set_property = photos_print_notification_set_property;

  g_object_class_install_property (object_class,
                                   PROP_PRINT_OP,
                                   g_param_spec_object ("print-op",
                                                        "GtkPrintOperation object",
                                                        "The print operation in progress",
                                                        GTK_TYPE_PRINT_OPERATION,
                                                        G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE));
}


void
photos_print_notification_new (GtkPrintOperation *print_op)
{
  return g_object_new (PHOTOS_TYPE_PRINT_NOTIFICATION,
                       "column-spacing", 12,
                       "margin-start", 12,
                       "margin-end", 12,
                       "orientation", GTK_ORIENTATION_HORIZONTAL,
                       "print-op", print_op,
                       NULL);
}
