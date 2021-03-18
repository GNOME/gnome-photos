/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2016 Rafael Fonseca
 * Copyright © 2016 – 2021 Red Hat, Inc.
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

#include <glib/gi18n.h>

#include "photos-done-notification.h"
#include "photos-filterable.h"
#include "photos-notification-manager.h"
#include "photos-search-context.h"


struct _PhotosDoneNotification
{
  GtkGrid parent_instance;
  GAction *edit_revert_action;
  PhotosBaseItem *item;
  GtkWidget *ntfctn_mngr;
  guint timeout_id;
};

enum
{
  PROP_0,
  PROP_ITEM
};


G_DEFINE_TYPE (PhotosDoneNotification, photos_done_notification, GTK_TYPE_GRID);


enum
{
  DONE_TIMEOUT = 10 /* s */
};


static void
photos_done_notification_remove_timeout (PhotosDoneNotification *self)
{
  if (self->timeout_id != 0)
    {
      g_source_remove (self->timeout_id);
      self->timeout_id = 0;
    }
}


static void
photos_done_notification_destroy (PhotosDoneNotification *self)
{
  g_simple_action_set_enabled (G_SIMPLE_ACTION (self->edit_revert_action), FALSE);
  photos_done_notification_remove_timeout (self);
  gtk_widget_destroy (GTK_WIDGET (self));
}


static gboolean
photos_done_notification_timeout (gpointer user_data)
{
  PhotosDoneNotification *self = PHOTOS_DONE_NOTIFICATION (user_data);

  self->timeout_id = 0;
  photos_done_notification_destroy (self);
  return G_SOURCE_REMOVE;
}


static void
photos_done_notification_undo_clicked (PhotosDoneNotification *self)
{
  photos_done_notification_destroy (self);
}


static void
photos_done_notification_constructed (GObject *object)
{
  PhotosDoneNotification *self = PHOTOS_DONE_NOTIFICATION (object);
  GtkWidget *close;
  GtkWidget *image;
  GtkWidget *label;
  GtkWidget *undo;
  const gchar *id;
  const gchar *name;
  g_autofree gchar *msg = NULL;

  G_OBJECT_CLASS (photos_done_notification_parent_class)->constructed (object);

  gtk_grid_set_column_spacing (GTK_GRID (self), 12);
  gtk_orientable_set_orientation (GTK_ORIENTABLE (self), GTK_ORIENTATION_HORIZONTAL);

  name = photos_base_item_get_name_with_fallback (PHOTOS_BASE_ITEM (self->item));
  msg = g_strdup_printf (_("“%s” edited"), name);

  label = gtk_label_new (msg);
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_widget_set_hexpand (label, TRUE);
  gtk_container_add (GTK_CONTAINER (self), label);

  undo = gtk_button_new_with_label (_("Undo"));
  gtk_widget_set_valign (undo, GTK_ALIGN_CENTER);
  gtk_actionable_set_action_name (GTK_ACTIONABLE (undo), "app.edit-revert");
  id = photos_filterable_get_id (PHOTOS_FILTERABLE (self->item));
  gtk_actionable_set_action_target (GTK_ACTIONABLE (undo), "s", id);
  gtk_container_add (GTK_CONTAINER (self), undo);
  /* GtkButton will activate the GAction in a signal handler with the
   * 'after' flag set. We need to ensure that we don't self-destruct
   * before that.
   */
  g_signal_connect_object (undo,
                           "clicked",
                           G_CALLBACK (photos_done_notification_undo_clicked),
                           self,
                           G_CONNECT_AFTER | G_CONNECT_SWAPPED);

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
  g_signal_connect_swapped (close, "clicked", G_CALLBACK (photos_done_notification_destroy), self);

  photos_notification_manager_add_notification (PHOTOS_NOTIFICATION_MANAGER (self->ntfctn_mngr),
                                                GTK_WIDGET (self));
  g_simple_action_set_enabled (G_SIMPLE_ACTION (self->edit_revert_action), TRUE);

  self->timeout_id = g_timeout_add_seconds (DONE_TIMEOUT, photos_done_notification_timeout, self);
}


static void
photos_done_notification_dispose (GObject *object)
{
  PhotosDoneNotification *self = PHOTOS_DONE_NOTIFICATION (object);

  g_simple_action_set_enabled (G_SIMPLE_ACTION (self->edit_revert_action), FALSE);
  photos_done_notification_remove_timeout (self);

  g_clear_object (&self->item);
  g_clear_object (&self->ntfctn_mngr);

  G_OBJECT_CLASS (photos_done_notification_parent_class)->dispose (object);
}


static void
photos_done_notification_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
  PhotosDoneNotification *self = PHOTOS_DONE_NOTIFICATION (object);

  switch (prop_id)
    {
      case PROP_ITEM:
        self->item = PHOTOS_BASE_ITEM (g_value_dup_object (value));
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}


static void
photos_done_notification_init (PhotosDoneNotification *self)
{
  GApplication *app;

  app = g_application_get_default ();
  self->edit_revert_action = g_action_map_lookup_action (G_ACTION_MAP (app), "edit-revert");

  self->ntfctn_mngr = photos_notification_manager_dup_singleton ();
}


static void
photos_done_notification_class_init (PhotosDoneNotificationClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->constructed = photos_done_notification_constructed;
  object_class->dispose = photos_done_notification_dispose;
  object_class->set_property = photos_done_notification_set_property;

  g_object_class_install_property (object_class,
                                   PROP_ITEM,
                                   g_param_spec_object ("item",
                                                        "PhotosBaseItem instance",
                                                        "The edited PhotosBaseItem",
                                                        PHOTOS_TYPE_BASE_ITEM,
                                                        G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE));
}


void
photos_done_notification_new (PhotosBaseItem *item)
{
  g_return_if_fail (PHOTOS_IS_BASE_ITEM (item));
  g_object_new (PHOTOS_TYPE_DONE_NOTIFICATION, "item", item, NULL);
}
