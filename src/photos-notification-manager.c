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
#include <gtk/gtk.h>

#include "photos-notification-manager.h"
#include "photos-utils.h"


struct _PhotosNotificationManagerPrivate
{
  GtkWidget *grid;
};


G_DEFINE_TYPE_WITH_PRIVATE (PhotosNotificationManager, photos_notification_manager, GD_TYPE_NOTIFICATION);


static void
photos_notification_manager_remove (PhotosNotificationManager *self)
{
  GList *children;

  children = gtk_container_get_children (GTK_CONTAINER (self->priv->grid));
  if (children == NULL)
    gtk_widget_hide (GTK_WIDGET (self));
  else
    g_list_free (children);
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
      return self;
    }

  return g_object_ref (self);
}


static void
photos_notification_manager_init (PhotosNotificationManager *self)
{
  PhotosNotificationManagerPrivate *priv;

  self->priv = photos_notification_manager_get_instance_private (self);
  priv = self->priv;

  gtk_widget_set_halign (GTK_WIDGET (self), GTK_ALIGN_CENTER);
  gtk_widget_set_valign (GTK_WIDGET (self), GTK_ALIGN_START);

  priv->grid = gtk_grid_new ();
  gtk_orientable_set_orientation (GTK_ORIENTABLE (priv->grid), GTK_ORIENTATION_VERTICAL);
  gtk_grid_set_row_spacing (GTK_GRID (priv->grid), 6);
  gtk_container_add (GTK_CONTAINER (self), priv->grid);

  g_signal_connect_swapped (priv->grid, "remove", G_CALLBACK (photos_notification_manager_remove), self);

  gtk_widget_show (priv->grid);
}


static void
photos_notification_manager_class_init (PhotosNotificationManagerClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->constructor = photos_notification_manager_constructor;
}


GtkWidget *
photos_notification_manager_dup_singleton (void)
{
  return g_object_new (PHOTOS_TYPE_NOTIFICATION_MANAGER, "show-close-button", FALSE, "timeout", -1, NULL);
}


void
photos_notification_manager_add_notification (PhotosNotificationManager *self, GtkWidget *notification)
{
  PhotosNotificationManagerPrivate *priv = self->priv;

  gtk_container_add (GTK_CONTAINER (priv->grid), notification);
  gtk_widget_show_all (notification);
  gtk_widget_show (GTK_WIDGET (self));
}
