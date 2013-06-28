/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2012, 2013 Red Hat, Inc.
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
#include <libtracker-miner/tracker-miner.h>

#include "photos-indexing-notification.h"
#include "photos-notification-manager.h"


struct _PhotosIndexingNotificationPrivate
{
  GtkWidget *ntfctn_mngr;
  GtkWidget *spinner;
  TrackerMinerManager *manager;
  gboolean manually_closed;
  gboolean on_display;
};


G_DEFINE_TYPE_WITH_PRIVATE (PhotosIndexingNotification, photos_indexing_notification, GTK_TYPE_GRID);


static const gchar *MINER_FILES = "org.freedesktop.Tracker1.Miner.Files";


static void
photos_indexing_notification_destroy_notification (PhotosIndexingNotification *self)
{
  PhotosIndexingNotificationPrivate *priv = self->priv;
  GtkWidget *parent;

  priv->on_display = FALSE;
  gtk_spinner_stop (GTK_SPINNER (priv->spinner));

  parent = gtk_widget_get_parent (GTK_WIDGET (self));
  if (parent != NULL)
    gtk_container_remove (GTK_CONTAINER (parent), GTK_WIDGET (self));
}


static void
photos_indexing_notification_clicked (PhotosIndexingNotification *self)
{
  self->priv->manually_closed = TRUE;
  photos_indexing_notification_destroy_notification (self);
}


static void
photos_indexing_notification_display_notification (PhotosIndexingNotification *self)
{
  PhotosIndexingNotificationPrivate *priv = self->priv;

  if (priv->on_display)
    return;

  if (priv->manually_closed)
    return;

  photos_notification_manager_add_notification (PHOTOS_NOTIFICATION_MANAGER (priv->ntfctn_mngr),
                                                GTK_WIDGET (self));
  gtk_spinner_start (GTK_SPINNER (priv->spinner));
  priv->on_display = TRUE;
}


static void
photos_indexing_notification_check_notification (PhotosIndexingNotification *self)
{
  PhotosIndexingNotificationPrivate *priv = self->priv;
  GSList *running;

  running = tracker_miner_manager_get_running (priv->manager);
  if (g_slist_find_custom (running, (gconstpointer) MINER_FILES, (GCompareFunc) g_strcmp0) != NULL)
    {
      gdouble progress;

      tracker_miner_manager_get_status (priv->manager, MINER_FILES, NULL, &progress, NULL);
      if (progress < 1)
        photos_indexing_notification_display_notification (self);
      else
        {
          priv->manually_closed = FALSE;
          photos_indexing_notification_destroy_notification (self);
        }
    }

  if (running != NULL)
    g_slist_free_full (running, g_free);
}


static void
photos_indexing_notification_dispose (GObject *object)
{
  PhotosIndexingNotification *self = PHOTOS_INDEXING_NOTIFICATION (object);
  PhotosIndexingNotificationPrivate *priv = self->priv;

  g_clear_object (&priv->ntfctn_mngr);
  g_clear_object (&priv->manager);

  G_OBJECT_CLASS (photos_indexing_notification_parent_class)->dispose (object);
}


static void
photos_indexing_notification_init (PhotosIndexingNotification *self)
{
  PhotosIndexingNotificationPrivate *priv;
  GError *error;
  GtkStyleContext *context;
  GtkWidget *close;
  GtkWidget *image;
  GtkWidget *labels;
  GtkWidget *primary;
  GtkWidget *secondary;

  self->priv = photos_indexing_notification_get_instance_private (self);
  priv = self->priv;

  error = NULL;
  priv->manager = tracker_miner_manager_new_full (FALSE, &error);
  if (error != NULL)
    {
      g_warning ("Unable to create a TrackerMinerManager, indexing progress notification won't work: %s",
                 error->message);
      g_error_free (error);
      return;
    }

  priv->ntfctn_mngr = g_object_ref_sink (photos_notification_manager_dup_singleton ());

  priv->spinner = gtk_spinner_new ();
  gtk_widget_set_size_request (priv->spinner, 16, 16);
  gtk_container_add (GTK_CONTAINER (self), priv->spinner);

  labels = gtk_grid_new ();
  gtk_orientable_set_orientation (GTK_ORIENTABLE (labels), GTK_ORIENTATION_VERTICAL);
  gtk_grid_set_row_spacing (GTK_GRID (labels), 3);
  gtk_container_add (GTK_CONTAINER (self), labels);

  primary = gtk_label_new (_("Your photos are being indexed"));
  gtk_widget_set_halign (primary, GTK_ALIGN_START);
  gtk_container_add (GTK_CONTAINER (labels), primary);

  secondary = gtk_label_new (_("Some photos might not be available during this process"));
  gtk_widget_set_halign (secondary, GTK_ALIGN_START);
  context = gtk_widget_get_style_context (secondary);
  gtk_style_context_add_class (context, "dim-label");
  gtk_container_add (GTK_CONTAINER (labels), secondary);

  image = gtk_image_new_from_icon_name ("window-close-symbolic", GTK_ICON_SIZE_INVALID);
  gtk_widget_set_margin_bottom (image, 2);
  gtk_widget_set_margin_top (image, 2);
  gtk_image_set_pixel_size (GTK_IMAGE (image), 16);

  close = gtk_button_new ();
  gtk_widget_set_valign (close, GTK_ALIGN_CENTER);
  gtk_container_add (GTK_CONTAINER (close), image);
  gtk_container_add (GTK_CONTAINER (self), close);

  g_signal_connect_swapped (close, "clicked", G_CALLBACK (photos_indexing_notification_clicked), self);

  g_signal_connect_swapped (priv->manager,
                            "miner-progress",
                            G_CALLBACK (photos_indexing_notification_check_notification),
                            self);
  photos_indexing_notification_check_notification (self);
}


static void
photos_indexing_notification_class_init (PhotosIndexingNotificationClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->dispose = photos_indexing_notification_dispose;
}


GtkWidget *
photos_indexing_notification_new (void)
{
  return g_object_new (PHOTOS_TYPE_INDEXING_NOTIFICATION,
                       "column-spacing", 12,
                       "margin-left", 12,
                       "margin-right", 12,
                       "orientation", GTK_ORIENTATION_HORIZONTAL,
                       NULL);
}
