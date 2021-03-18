/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2012 – 2021 Red Hat, Inc.
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

/* Based on code from:
 *   + Documents
 */


#include "config.h"

#include <glib.h>

#include "photos-indexing-notification.h"
#include "photos-notification-manager.h"
#include "photos-utils.h"


struct _PhotosNotificationManager
{
  GtkRevealer parent_instance;
  GtkWidget *grid;
  GtkWidget *indexing_ntfctn;
};


G_DEFINE_TYPE (PhotosNotificationManager, photos_notification_manager, GTK_TYPE_REVEALER);


static void
photos_notification_manager_remove (PhotosNotificationManager *self)
{
  GList *children;

  children = gtk_container_get_children (GTK_CONTAINER (self->grid));
  if (children == NULL)
    {
      gtk_widget_hide (GTK_WIDGET (self));
      gtk_revealer_set_reveal_child (GTK_REVEALER (self), FALSE);
    }
  else
    {
      g_list_free (children);
    }
}


static void
photos_notification_manager_constructed (GObject *object)
{
  PhotosNotificationManager *self = PHOTOS_NOTIFICATION_MANAGER (object);

  G_OBJECT_CLASS (photos_notification_manager_parent_class)->constructed (object);

  /* PhotosIndexingNotification takes a reference on
   * PhotosNotificationManager during construction. Hence we should
   * instantiate PhotosIndexingNotification only after we have
   * finished constructing this object.
   */
  self->indexing_ntfctn = g_object_ref_sink (photos_indexing_notification_new ());
}


static GObject *
photos_notification_manager_constructor (GType type,
                                         guint n_construct_params,
                                         GObjectConstructParam *construct_params)
{
  static GObject *self = NULL;

  if (self == NULL)
    {
      self = G_OBJECT_CLASS (photos_notification_manager_parent_class)->constructor (type,
                                                                                     n_construct_params,
                                                                                     construct_params);
      g_object_add_weak_pointer (self, (gpointer) &self);
    }

  return g_object_ref_sink (self);
}


static void
photos_notification_manager_dispose (GObject *object)
{
  PhotosNotificationManager *self = PHOTOS_NOTIFICATION_MANAGER (object);

  g_clear_object (&self->indexing_ntfctn);

  G_OBJECT_CLASS (photos_notification_manager_parent_class)->dispose (object);
}


static void
photos_notification_manager_init (PhotosNotificationManager *self)
{
  GtkStyleContext *context;
  GtkWidget *frame;

  gtk_widget_set_halign (GTK_WIDGET (self), GTK_ALIGN_CENTER);
  gtk_widget_set_valign (GTK_WIDGET (self), GTK_ALIGN_START);

  frame = gtk_frame_new (NULL);
  context = gtk_widget_get_style_context (frame);
  gtk_style_context_add_class (context, "app-notification");
  gtk_container_add (GTK_CONTAINER (self), frame);

  self->grid = gtk_grid_new ();
  gtk_orientable_set_orientation (GTK_ORIENTABLE (self->grid), GTK_ORIENTATION_VERTICAL);
  gtk_grid_set_row_spacing (GTK_GRID (self->grid), 6);
  gtk_container_add (GTK_CONTAINER (frame), self->grid);

  g_signal_connect_swapped (self->grid, "remove", G_CALLBACK (photos_notification_manager_remove), self);
}


static void
photos_notification_manager_class_init (PhotosNotificationManagerClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->constructed = photos_notification_manager_constructed;
  object_class->constructor = photos_notification_manager_constructor;
  object_class->dispose = photos_notification_manager_dispose;
}


GtkWidget *
photos_notification_manager_dup_singleton (void)
{
  return g_object_new (PHOTOS_TYPE_NOTIFICATION_MANAGER, NULL);
}


void
photos_notification_manager_add_notification (PhotosNotificationManager *self, GtkWidget *notification)
{
  gtk_container_add (GTK_CONTAINER (self->grid), notification);
  gtk_widget_show_all (GTK_WIDGET (self));
  gtk_revealer_set_reveal_child (GTK_REVEALER (self), TRUE);
}
