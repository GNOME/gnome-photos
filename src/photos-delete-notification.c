/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2014 Pranav Kant
 * Copyright © 2014 – 2016 Red Hat, Inc.
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
#include <gtk/gtk.h>

#include "photos-base-item.h"
#include "photos-delete-notification.h"
#include "photos-icons.h"
#include "photos-item-manager.h"
#include "photos-notification-manager.h"
#include "photos-search-context.h"


struct _PhotosDeleteNotification
{
  GtkGrid parent_instance;
  GList *items;
  GtkWidget *ntfctn_mngr;
  PhotosBaseManager *item_mngr;
  guint timeout_id;
};

struct _PhotosDeleteNotificationClass
{
  GtkGridClass parent_class;
};

enum
{
  PROP_0,
  PROP_ITEMS
};


G_DEFINE_TYPE (PhotosDeleteNotification, photos_delete_notification, GTK_TYPE_GRID);


enum
{
  DELETE_TIMEOUT = 10 /* s */
};


static void
photos_delete_notification_remove_timeout (PhotosDeleteNotification *self)
{
  if (self->timeout_id != 0)
    {
      g_source_remove (self->timeout_id);
      self->timeout_id = 0;
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
  GList *l;

  for (l = self->items; l != NULL; l = l->next)
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

  self->timeout_id = 0;
  photos_delete_notification_delete_items (self);
  return G_SOURCE_REMOVE;
}


static void
photos_delete_notification_undo_clicked (PhotosDeleteNotification *self)
{
  GList *l;

  for (l = self->items; l != NULL; l = l->next)
    {
      PhotosBaseItem *item = PHOTOS_BASE_ITEM (l->data);
      photos_item_manager_unhide_item (PHOTOS_ITEM_MANAGER (self->item_mngr), item);
    }

  photos_delete_notification_destroy (self);
}


static void
photos_delete_notification_constructed (GObject *object)
{
  PhotosDeleteNotification *self = PHOTOS_DELETE_NOTIFICATION (object);
  gchar *msg;
  GtkWidget *close;
  GtkWidget *image;
  GtkWidget *label;
  GtkWidget *undo;
  guint length;

  G_OBJECT_CLASS (photos_delete_notification_parent_class)->constructed (object);

  length = g_list_length (self->items);
  if (length == 1)
    {
      const gchar *name;

      name = photos_base_item_get_name_with_fallback (PHOTOS_BASE_ITEM (self->items->data));
      msg = g_strdup_printf (_("“%s” deleted"), name);
    }
  else
    msg = g_strdup_printf (ngettext ("%d item deleted", "%d items deleted", length), length);

  label = gtk_label_new (msg);
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_widget_set_hexpand (label, TRUE);
  gtk_container_add (GTK_CONTAINER (self), label);
  g_free (msg);

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
  gtk_button_set_image (GTK_BUTTON (close), image);
  gtk_container_add (GTK_CONTAINER (self), close);
  g_signal_connect_swapped (close, "clicked", G_CALLBACK (photos_delete_notification_delete_items), self);

  photos_notification_manager_add_notification (PHOTOS_NOTIFICATION_MANAGER (self->ntfctn_mngr),
                                                GTK_WIDGET (self));

  self->timeout_id = g_timeout_add_seconds (DELETE_TIMEOUT, photos_delete_notification_timeout, self);
}


static void
photos_delete_notification_dispose (GObject *object)
{
  PhotosDeleteNotification *self = PHOTOS_DELETE_NOTIFICATION (object);

  photos_delete_notification_remove_timeout (self);

  g_list_free_full (self->items, g_object_unref);
  self->items = NULL;

  g_clear_object (&self->ntfctn_mngr);
  g_clear_object (&self->item_mngr);

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
        self->items = g_list_copy_deep (items, (GCopyFunc) g_object_ref, NULL);
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
  GApplication *app;
  PhotosSearchContextState *state;

  app = g_application_get_default ();
  state = photos_search_context_get_state (PHOTOS_SEARCH_CONTEXT (app));

  self->ntfctn_mngr = photos_notification_manager_dup_singleton ();
  self->item_mngr = g_object_ref (state->item_mngr);
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
  g_return_if_fail (items != NULL);

  g_object_new (PHOTOS_TYPE_DELETE_NOTIFICATION,
                "column-spacing", 12,
                "items", items,
                "orientation", GTK_ORIENTATION_HORIZONTAL,
                NULL);
}
