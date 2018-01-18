/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2012 – 2017 Red Hat, Inc.
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
#include "photos-indexing-notification.h"
#include "photos-notification-manager.h"


struct _PhotosIndexingNotification
{
  GtkGrid parent_instance;
  GtkWidget *ntfctn_mngr;
  GtkWidget *primary_label;
  GtkWidget *secondary_label;
  GtkWidget *spinner;
  TrackerMinerManager *manager;
  gboolean closed;
  gboolean on_display;
  guint timeout_id;
};


G_DEFINE_TYPE (PhotosIndexingNotification, photos_indexing_notification, GTK_TYPE_GRID);


enum
{
  REMOTE_MINER_TIMEOUT = 10 /* s */
};

static const gchar *MINER_FILES = "org.freedesktop.Tracker1.Miner.Files";


static void
photos_indexing_notification_remove_timeout (PhotosIndexingNotification *self)
{
  if (self->timeout_id != 0)
    {
      g_source_remove (self->timeout_id);
      self->timeout_id = 0;
    }
}


static void
photos_indexing_notification_destroy (PhotosIndexingNotification *self, gboolean closed)
{
  GtkWidget *parent;

  photos_indexing_notification_remove_timeout (self);

  self->on_display = FALSE;
  gtk_spinner_stop (GTK_SPINNER (self->spinner));

  parent = gtk_widget_get_parent (GTK_WIDGET (self));
  if (parent != NULL)
    gtk_container_remove (GTK_CONTAINER (parent), GTK_WIDGET (self));

  self->closed = closed;
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
  gtk_label_set_label (GTK_LABEL (self->primary_label), primary_text);
  gtk_label_set_label (GTK_LABEL (self->secondary_label), secondary_text);

  if (secondary_text != NULL)
    {
      gtk_widget_set_vexpand (self->primary_label, FALSE);
      gtk_widget_show (self->secondary_label);
    }
  else
    {
      gtk_widget_set_vexpand (self->primary_label, TRUE);
      gtk_widget_hide (self->secondary_label);
    }
}


static void
photos_indexing_notification_display (PhotosIndexingNotification *self,
                                      const gchar *primary_text,
                                      const gchar *secondary_text)
{
  if (self->on_display)
    return;

  if (self->closed)
    return;

  photos_notification_manager_add_notification (PHOTOS_NOTIFICATION_MANAGER (self->ntfctn_mngr),
                                                GTK_WIDGET (self));
  gtk_spinner_start (GTK_SPINNER (self->spinner));
  self->on_display = TRUE;

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
  g_autofree gchar *primary = NULL;

  self->timeout_id = 0;

  app = g_application_get_default ();
  miners_running = photos_application_get_miners_running (PHOTOS_APPLICATION (app));
  if (miners_running != NULL && miners_running->next == NULL) /* length == 1 */
    miner = GOM_MINER (miners_running->data);

  if (miner != NULL)
    display_name = gom_miner_get_display_name (miner);

  if (display_name != NULL)
    {
      /* Translators: %s refers to an online account provider, e.g.,
       * "Facebook" or "Flickr".
       */
      primary = g_strdup_printf (_("Fetching photos from %s"), display_name);
    }
  else
    primary = g_strdup (_("Fetching photos from online accounts"));

  photos_indexing_notification_display (self, primary, NULL);

  return G_SOURCE_REMOVE;
}


static void
photos_indexing_notification_check_notification (PhotosIndexingNotification *self)
{
  GApplication *app;
  GList *miners_running;
  GSList *running = NULL;
  gboolean is_indexing_local = FALSE;
  gboolean is_indexing_remote = FALSE;

  running = tracker_miner_manager_get_running (self->manager);
  if (g_slist_find_custom (running, (gconstpointer) MINER_FILES, (GCompareFunc) g_strcmp0) != NULL)
    {
      gdouble progress;

      tracker_miner_manager_get_status (self->manager, MINER_FILES, NULL, &progress, NULL);
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
      self->timeout_id = g_timeout_add_seconds (REMOTE_MINER_TIMEOUT, photos_indexing_notification_timeout, self);
    }
  else
    photos_indexing_notification_destroy (self, FALSE);

  g_slist_free_full (running, g_free);
}


static void
photos_indexing_notification_dispose (GObject *object)
{
  PhotosIndexingNotification *self = PHOTOS_INDEXING_NOTIFICATION (object);

  photos_indexing_notification_remove_timeout (self);

  g_clear_object (&self->ntfctn_mngr);
  g_clear_object (&self->manager);

  G_OBJECT_CLASS (photos_indexing_notification_parent_class)->dispose (object);
}


static void
photos_indexing_notification_init (PhotosIndexingNotification *self)
{
  GApplication *app;
  GtkStyleContext *context;
  GtkWidget *close;
  GtkWidget *image;
  GtkWidget *labels;

  app = g_application_get_default ();

  {
    g_autoptr (GError) error = NULL;

    self->manager = tracker_miner_manager_new_full (FALSE, &error);
    if (error != NULL)
      {
        g_warning ("Unable to create a TrackerMinerManager, indexing progress notification won't work: %s",
                   error->message);
        return;
      }
  }

  self->ntfctn_mngr = photos_notification_manager_dup_singleton ();

  self->spinner = gtk_spinner_new ();
  gtk_widget_set_size_request (self->spinner, 16, 16);
  gtk_container_add (GTK_CONTAINER (self), self->spinner);

  labels = gtk_grid_new ();
  gtk_orientable_set_orientation (GTK_ORIENTABLE (labels), GTK_ORIENTATION_VERTICAL);
  gtk_grid_set_row_spacing (GTK_GRID (labels), 3);
  gtk_container_add (GTK_CONTAINER (self), labels);

  self->primary_label = gtk_label_new (NULL);
  gtk_widget_set_halign (self->primary_label, GTK_ALIGN_START);
  gtk_widget_set_hexpand (self->primary_label, TRUE);
  gtk_container_add (GTK_CONTAINER (labels), self->primary_label);

  self->secondary_label = gtk_label_new (NULL);
  gtk_widget_set_halign (self->secondary_label, GTK_ALIGN_START);
  gtk_widget_set_hexpand (self->secondary_label, TRUE);
  context = gtk_widget_get_style_context (self->secondary_label);
  gtk_style_context_add_class (context, "dim-label");
  gtk_container_add (GTK_CONTAINER (labels), self->secondary_label);

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
  g_signal_connect_swapped (close, "clicked", G_CALLBACK (photos_indexing_notification_close_clicked), self);

  g_signal_connect_object (app,
                           "miners-changed",
                           G_CALLBACK (photos_indexing_notification_check_notification),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_swapped (self->manager,
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
                       "orientation", GTK_ORIENTATION_HORIZONTAL,
                       NULL);
}
