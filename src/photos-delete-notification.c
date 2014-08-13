/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2014 Pranav Kant
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

#include <glib/gi18n.h>

#include "photos-base-item.h"
#include "photos-delete-notification.h"
#include "photos-icons.h"
#include "photos-notification-manager.h"
#include "photos-item-manager.h"


struct _PhotosDeleteNotificationPrivate
{
  GList *items;
  GtkWidget *ntfctn_mngr;
  PhotosBaseManager *item_mngr;
  guint timeout_id;
};

enum
{
  PROP_0,
  PROP_ITEMS
};


G_DEFINE_TYPE_WITH_PRIVATE (PhotosDeleteNotification, photos_delete_notification, GTK_TYPE_GRID);


enum
{
    DELETE_TIMEOUT = 10 /* s */
};


static void
photos_delete_notification_remove_timeout (PhotosDeleteNotification *self)
{
  PhotosDeleteNotificationPrivate *priv = self->priv;

  if (priv->timeout_id != 0)
    {
      g_source_remove (priv->timeout_id);
      priv->timeout_id = 0;
    }
}


static void
photos_delete_notification_destroy (PhotosDeleteNotification *self)
{
  photos_delete_notification_remove_timeout (self);
  gtk_widget_destroy (GTK_WIDGET (self));
}


static void
photos_delete_notification_delete_items (PhotosDeleteNotification *self)
{
  PhotosDeleteNotificationPrivate *priv = self->priv;
  GList *l;

  for (l = priv->items; l != NULL; l = l->next)
    {
      PhotosBaseItem *item = PHOTOS_BASE_ITEM (l->data);
      photos_base_item_trash (item);
    }

  photos_delete_notification_destroy (self);
}


static gboolean
photos_delete_notification_timeout (gpointer user_data)
{
  PhotosDeleteNotification *self = PHOTOS_DELETE_NOTIFICATION (user_data);

  self->priv->timeout_id = 0;
  photos_delete_notification_delete_items (self);
  return G_SOURCE_REMOVE;
}


static void
photos_delete_notification_undo_clicked (PhotosDeleteNotification *self)
{
  PhotosDeleteNotificationPrivate *priv = self->priv;
  GList *l;

  for (l = priv->items; l != NULL; l = l->next)
    {
      GObject *item = G_OBJECT (l->data);
      photos_base_manager_add_object (priv->item_mngr, item);
    }

  photos_delete_notification_destroy (self);
}


static void
photos_delete_notification_constructed (GObject *object)
{
  PhotosDeleteNotification *self = PHOTOS_DELETE_NOTIFICATION (object);
  PhotosDeleteNotificationPrivate *priv = self->priv;
  GtkWidget *close;
  GtkWidget *image;
  GtkWidget *label;
  GtkWidget *undo;
  guint length;

  G_OBJECT_CLASS (photos_delete_notification_parent_class)->constructed (object);

  length = g_list_length (priv->items);
  label = gtk_label_new (ngettext ("Selected item has been deleted",
                                   "Selected items have been deleted",
                                   length));
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_container_add (GTK_CONTAINER (self), label);

  undo = gtk_button_new_with_label (_("Undo"));
  gtk_widget_set_valign (undo, GTK_ALIGN_CENTER);
  gtk_container_add (GTK_CONTAINER (self), undo);
  g_signal_connect_swapped (undo, "clicked", G_CALLBACK (photos_delete_notification_undo_clicked), self);

  image = gtk_image_new_from_icon_name (PHOTOS_ICON_WINDOW_CLOSE_SYMBOLIC, GTK_ICON_SIZE_INVALID);
  gtk_widget_set_margin_bottom (image, 2);
  gtk_widget_set_margin_top (image, 2);
  gtk_image_set_pixel_size (GTK_IMAGE (image), 16);

  close = gtk_button_new ();
  gtk_widget_set_valign (close, GTK_ALIGN_CENTER);
  gtk_button_set_focus_on_click (GTK_BUTTON (close), FALSE);
  gtk_button_set_relief (GTK_BUTTON (close), GTK_RELIEF_NONE);
  gtk_container_add (GTK_CONTAINER (close), image);
  gtk_container_add (GTK_CONTAINER (self), close);
  g_signal_connect_swapped (close, "clicked", G_CALLBACK (photos_delete_notification_delete_items), self);

  photos_notification_manager_add_notification (PHOTOS_NOTIFICATION_MANAGER (priv->ntfctn_mngr),
                                                GTK_WIDGET (self));

  priv->timeout_id = g_timeout_add_seconds_full (G_PRIORITY_DEFAULT,
                                                 DELETE_TIMEOUT,
                                                 photos_delete_notification_timeout,
                                                 g_object_ref (self),
                                                 g_object_unref);
}


static void
photos_delete_notification_dispose (GObject *object)
{
  PhotosDeleteNotification *self = PHOTOS_DELETE_NOTIFICATION (object);
  PhotosDeleteNotificationPrivate *priv = self->priv;

  if (priv->items != NULL)
    {
      g_list_free_full (priv->items, g_object_unref);
      priv->items = NULL;
    }

  g_clear_object (&priv->ntfctn_mngr);
  g_clear_object (&priv->item_mngr);

  G_OBJECT_CLASS (photos_delete_notification_parent_class)->dispose (object);
}


static void
photos_delete_notification_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
  PhotosDeleteNotification *self = PHOTOS_DELETE_NOTIFICATION (object);

  switch (prop_id)
    {
    case PROP_ITEMS:
      {
        GList *items;

        items = (GList *) g_value_get_pointer (value);
        self->priv->items = g_list_copy_deep (items, (GCopyFunc) g_object_ref, NULL);
        break;
      }

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
photos_delete_notification_init (PhotosDeleteNotification *self)
{
  PhotosDeleteNotificationPrivate *priv;

  self->priv = photos_delete_notification_get_instance_private (self);
  priv = self->priv;

  priv->ntfctn_mngr = g_object_ref_sink (photos_notification_manager_dup_singleton ());
  priv->item_mngr = photos_item_manager_dup_singleton ();
}


static void
photos_delete_notification_class_init (PhotosDeleteNotificationClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->constructed = photos_delete_notification_constructed;
  object_class->dispose = photos_delete_notification_dispose;
  object_class->set_property = photos_delete_notification_set_property;

  g_object_class_install_property (object_class,
                                   PROP_ITEMS,
                                   g_param_spec_pointer ("items",
                                                         "List of PhotosBaseItems",
                                                         "List of items that are being deleted",
                                                         G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE));
}


void
photos_delete_notification_new (GList *items)
{
  g_object_new (PHOTOS_TYPE_DELETE_NOTIFICATION,
                "column-spacing", 12,
                "items", items,
                "margin-start", 12,
                "margin-end", 12,
                "orientation", GTK_ORIENTATION_HORIZONTAL,
                NULL);
}
