/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2012 Red Hat, Inc.
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
#include <libgd/gd.h>

#include "photos-notification-manager.h"
#include "photos-utils.h"


struct _PhotosNotificationManagerPrivate
{
  GtkWidget *grid;
  GtkWidget *widget;
};


G_DEFINE_TYPE (PhotosNotificationManager, photos_notification_manager, GTK_CLUTTER_TYPE_ACTOR);


static void
photos_notification_manager_remove (PhotosNotificationManager *self)
{
  PhotosNotificationManagerPrivate *priv = self->priv;
  GList *children;

  children = gtk_container_get_children (GTK_CONTAINER (priv->grid));
  if (children == NULL)
    gtk_widget_hide (priv->widget);
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
photos_notification_manager_constructed (GObject *object)
{
  PhotosNotificationManager *self = PHOTOS_NOTIFICATION_MANAGER (object);
  PhotosNotificationManagerPrivate *priv = self->priv;
  GtkWidget *bin;

  G_OBJECT_CLASS (photos_notification_manager_parent_class)->constructed (object);

  priv->widget = gd_notification_new ();
  gd_notification_set_show_close_button (GD_NOTIFICATION (priv->widget), FALSE);
  gd_notification_set_timeout (GD_NOTIFICATION (priv->widget), -1);

  priv->grid = gtk_grid_new ();
  gtk_orientable_set_orientation (GTK_ORIENTABLE (priv->grid), GTK_ORIENTATION_HORIZONTAL);
  gtk_grid_set_column_spacing (GTK_GRID (priv->grid), 6);
  gtk_container_add (GTK_CONTAINER (priv->widget), priv->grid);
  g_signal_connect_swapped (priv->grid, "remove", G_CALLBACK (photos_notification_manager_remove), self);

  gtk_widget_show (priv->grid);

  bin = gtk_clutter_actor_get_widget (GTK_CLUTTER_ACTOR (self));
  gtk_container_add (GTK_CONTAINER (bin), priv->widget);
  photos_utils_alpha_gtk_widget (bin);
}


static void
photos_notification_manager_init (PhotosNotificationManager *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                            PHOTOS_TYPE_NOTIFICATION_MANAGER,
                                            PhotosNotificationManagerPrivate);

  clutter_actor_set_opacity (CLUTTER_ACTOR (self), 0);
  clutter_actor_set_x_align (CLUTTER_ACTOR (self), CLUTTER_ACTOR_ALIGN_CENTER);
  clutter_actor_set_y_align (CLUTTER_ACTOR (self), CLUTTER_ACTOR_ALIGN_START);
  clutter_actor_set_y_expand (CLUTTER_ACTOR (self), TRUE);
}


static void
photos_notification_manager_class_init (PhotosNotificationManagerClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->constructed = photos_notification_manager_constructed;
  object_class->constructor = photos_notification_manager_constructor;

  g_type_class_add_private (class, sizeof (PhotosNotificationManagerPrivate));
}


ClutterActor *
photos_notification_manager_new (void)
{
  return g_object_new (PHOTOS_TYPE_NOTIFICATION_MANAGER, NULL);
}


void
photos_notification_manager_add_notification (PhotosNotificationManager *self, GtkWidget *notification)
{
  PhotosNotificationManagerPrivate *priv = self->priv;

  gtk_container_add (GTK_CONTAINER (priv->grid), notification);
  gtk_widget_show_all (notification);
  gtk_widget_show (priv->widget);
  clutter_actor_set_opacity (CLUTTER_ACTOR (self), 255);
}
