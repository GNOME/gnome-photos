/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2012, 2013, 2014 Red Hat, Inc.
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

#include <gio/gio.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <libtracker-control/tracker-control.h>

#include "photos-application.h"
#include "photos-gom-miner.h"
#include "photos-icons.h"
#include "photos-indexing-notification.h"
#include "photos-notification-manager.h"


struct _PhotosIndexingNotificationPrivate
{
  GtkWidget *ntfctn_mngr;
  GtkWidget *primary_label;
  GtkWidget *secondary_label;
  GtkWidget *spinner;
  TrackerMinerManager *manager;
  gboolean closed;
  gboolean on_display;
  guint timeout_id;
};


G_DEFINE_TYPE_WITH_PRIVATE (PhotosIndexingNotification, photos_indexing_notification, GTK_TYPE_GRID);


enum
{
  REMOTE_MINER_TIMEOUT = 10 /* s */
};

static const gchar *MINER_FILES = "org.freedesktop.Tracker1.Miner.Files";


static void
photos_indexing_notification_remove_timeout (PhotosIndexingNotification *self)
{
  PhotosIndexingNotificationPrivate *priv = self->priv;

  if (priv->timeout_id != 0)
    {
      g_source_remove (priv->timeout_id);
      priv->timeout_id = 0;
    }
}


static void
photos_indexing_notification_destroy (PhotosIndexingNotification *self, gboolean closed)
{
  PhotosIndexingNotificationPrivate *priv = self->priv;
  GtkWidget *parent;

  photos_indexing_notification_remove_timeout (self);

  priv->on_display = FALSE;
  gtk_spinner_stop (GTK_SPINNER (priv->spinner));

  parent = gtk_widget_get_parent (GTK_WIDGET (self));
  if (parent != NULL)
    gtk_container_remove (GTK_CONTAINER (parent), GTK_WIDGET (self));

  priv->closed = closed;
}


static void
photos_indexing_notification_close_clicked (PhotosIndexingNotification *self)
{
  photos_indexing_notification_destroy (self, TRUE);
}


static void
photos_indexing_notification_update (PhotosIndexingNotification *self,
                                     const gchar *primary_text,
                                     const gchar *secondary_text)
{
  PhotosIndexingNotificationPrivate *priv = self->priv;

  gtk_label_set_label (GTK_LABEL (priv->primary_label), primary_text);
  gtk_label_set_label (GTK_LABEL (priv->secondary_label), secondary_text);

  if (secondary_text != NULL)
    {
      gtk_widget_set_vexpand (priv->primary_label, FALSE);
      gtk_widget_show (priv->secondary_label);
    }
  else
    {
      gtk_widget_set_vexpand (priv->primary_label, TRUE);
      gtk_widget_hide (priv->secondary_label);
    }
}


static void
photos_indexing_notification_display (PhotosIndexingNotification *self,
                                      const gchar *primary_text,
                                      const gchar *secondary_text)
{
  PhotosIndexingNotificationPrivate *priv = self->priv;

  if (priv->on_display)
    return;

  if (priv->closed)
    return;

  photos_notification_manager_add_notification (PHOTOS_NOTIFICATION_MANAGER (priv->ntfctn_mngr),
                                                GTK_WIDGET (self));
  gtk_spinner_start (GTK_SPINNER (priv->spinner));
  priv->on_display = TRUE;

  photos_indexing_notification_update (self, primary_text, secondary_text);
}


static gboolean
photos_indexing_notification_timeout (gpointer user_data)
{
  PhotosIndexingNotification *self = PHOTOS_INDEXING_NOTIFICATION (user_data);
  GApplication *app;
  GList *miners_running;
  GomMiner *miner = NULL;
  const gchar *display_name = NULL;
  gchar *primary = NULL;

  self->priv->timeout_id = 0;

  app = g_application_get_default ();
  miners_running = photos_application_get_miners_running (PHOTOS_APPLICATION (app));
  if (miners_running != NULL && miners_running->next == NULL) /* length == 1 */
    miner = GOM_MINER (miners_running->data);

  if (miner != NULL)
    display_name = gom_miner_get_display_name (miner);

  if (display_name != NULL)
    {
      /* Translators: %s refers to an online account provider, e.g.,
       * "Facebook" or "Flickr.
       */
      primary = g_strdup_printf (_("Fetching photos from %s"), display_name);
    }
  else
    primary = g_strdup (_("Fetching photos from online accounts"));

  photos_indexing_notification_display (self, primary, NULL);
  g_free (primary);

  return G_SOURCE_REMOVE;
}


static void
photos_indexing_notification_check_notification (PhotosIndexingNotification *self)
{
  PhotosIndexingNotificationPrivate *priv = self->priv;
  GApplication *app;
  GList *miners_running;
  GSList *running;
  gboolean is_indexing_local = FALSE;
  gboolean is_indexing_remote = FALSE;

  running = tracker_miner_manager_get_running (priv->manager);
  if (g_slist_find_custom (running, (gconstpointer) MINER_FILES, (GCompareFunc) g_strcmp0) != NULL)
    {
      gdouble progress;

      tracker_miner_manager_get_status (priv->manager, MINER_FILES, NULL, &progress, NULL);
      if (progress < 1)
        is_indexing_local = TRUE;
    }

  app = g_application_get_default ();
  miners_running = photos_application_get_miners_running (PHOTOS_APPLICATION (app));
  if (miners_running != NULL) /* length > 0 */
    is_indexing_remote = TRUE;

  if (is_indexing_local)
    {
      photos_indexing_notification_display (self,
                                            _("Your photos are being indexed"),
                                            _("Some photos might not be available during this process"));
    }
  else if (is_indexing_remote)
    {
      photos_indexing_notification_remove_timeout (self);
      priv->timeout_id = g_timeout_add_seconds_full (G_PRIORITY_DEFAULT,
                                                     REMOTE_MINER_TIMEOUT,
                                                     photos_indexing_notification_timeout,
                                                     g_object_ref (self),
                                                     g_object_unref);
    }
  else
    photos_indexing_notification_destroy (self, FALSE);

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
  GApplication *app;
  GError *error;
  GtkStyleContext *context;
  GtkWidget *close;
  GtkWidget *image;
  GtkWidget *labels;

  self->priv = photos_indexing_notification_get_instance_private (self);
  priv = self->priv;

  app = g_application_get_default ();

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

  priv->primary_label = gtk_label_new (NULL);
  gtk_widget_set_halign (priv->primary_label, GTK_ALIGN_START);
  gtk_container_add (GTK_CONTAINER (labels), priv->primary_label);

  priv->secondary_label = gtk_label_new (NULL);
  gtk_widget_set_halign (priv->secondary_label, GTK_ALIGN_START);
  context = gtk_widget_get_style_context (priv->secondary_label);
  gtk_style_context_add_class (context, "dim-label");
  gtk_container_add (GTK_CONTAINER (labels), priv->secondary_label);

  image = gtk_image_new_from_icon_name (PHOTOS_ICON_WINDOW_CLOSE_SYMBOLIC, GTK_ICON_SIZE_INVALID);
  gtk_widget_set_margin_bottom (image, 2);
  gtk_widget_set_margin_top (image, 2);
  gtk_image_set_pixel_size (GTK_IMAGE (image), 16);

  close = gtk_button_new ();
  gtk_widget_set_valign (close, GTK_ALIGN_CENTER);
  gtk_button_set_image (GTK_BUTTON (close), image);
  gtk_container_add (GTK_CONTAINER (self), close);
  g_signal_connect_swapped (close, "clicked", G_CALLBACK (photos_indexing_notification_close_clicked), self);

  g_signal_connect_swapped (app,
                            "miners-changed",
                            G_CALLBACK (photos_indexing_notification_check_notification),
                            self);

  g_signal_connect_swapped (priv->manager,
                            "miner-progress",
                            G_CALLBACK (photos_indexing_notification_check_notification),
                            self);
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
                       "margin-start", 12,
                       "margin-end", 12,
                       "orientation", GTK_ORIENTATION_HORIZONTAL,
                       NULL);
}
