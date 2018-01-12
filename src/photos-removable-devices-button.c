/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2018 Red Hat, Inc.
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

#include <gio/gio.h>

#include "photos-base-manager.h"
#include "photos-filterable.h"
#include "photos-model-button.h"
#include "photos-search-context.h"
#include "photos-removable-device-widget.h"
#include "photos-removable-devices-button.h"


struct _PhotosRemovableDevicesButton
{
  GtkBin parent_instance;
  GtkWidget *device_button;
  GtkWidget *device_widget;
  GtkWidget *devices_button;
  GtkWidget *devices_popover;
  GtkWidget *devices_popover_grid;
  GtkWidget *stack;
  PhotosBaseManager *src_mngr;
};


G_DEFINE_TYPE (PhotosRemovableDevicesButton, photos_removable_devices_button, GTK_TYPE_BIN);


static void
photos_removable_devices_button_refresh_devices (PhotosRemovableDevicesButton *self)
{
  GList *sources = NULL;
  gboolean visible = FALSE;
  const gchar *action_id;
  const gchar *action_namespace = "app";
  guint i;
  guint n_items;

  photos_removable_device_widget_set_source (PHOTOS_REMOVABLE_DEVICE_WIDGET (self->device_widget), NULL);
  gtk_container_foreach (GTK_CONTAINER (self->devices_popover_grid), (GtkCallback) gtk_widget_destroy, NULL);

  n_items = g_list_model_get_n_items (G_LIST_MODEL (self->src_mngr));
  for (i = 0; i < n_items; i++)
    {
      GMount *mount;
      g_autoptr (PhotosSource) source = NULL;

      source = PHOTOS_SOURCE (g_list_model_get_object (G_LIST_MODEL (self->src_mngr), i));
      mount = photos_source_get_mount (source);
      if (mount == NULL)
        continue;

      sources = g_list_prepend (sources, g_object_ref (source));
    }

  if (sources == NULL)
    goto out;

  action_id = photos_base_manager_get_action_id (self->src_mngr);

  if (sources->next == NULL) /* length == 1 */
    {
      PhotosSource *source = PHOTOS_SOURCE (sources->data);
      const gchar *id;
      g_autofree gchar *action_name = NULL;

      id = photos_filterable_get_id (PHOTOS_FILTERABLE (source));
      gtk_actionable_set_action_target (GTK_ACTIONABLE (self->device_button), "s", id);
      action_name = g_strconcat (action_namespace, ".", action_id, NULL);
      gtk_actionable_set_action_name (GTK_ACTIONABLE (self->device_button), action_name);

      photos_removable_device_widget_set_source (PHOTOS_REMOVABLE_DEVICE_WIDGET (self->device_widget), source);
      gtk_widget_show_all (self->device_widget);

      gtk_stack_set_visible_child (GTK_STACK (self->stack), self->device_button);
    }
  else
    {
      GList *l;

      for (l = sources; l != NULL; l = l->next)
        {
          GtkWidget *device_button;
          GtkWidget *device_widget;
          PhotosSource *source = PHOTOS_SOURCE (l->data);
          const gchar *id;
          g_autofree gchar *action_name = NULL;

          device_button = photos_model_button_new ();
          action_name = g_strconcat (action_namespace, ".", action_id, NULL);
          gtk_actionable_set_action_name (GTK_ACTIONABLE (device_button), action_name);
          id = photos_filterable_get_id (PHOTOS_FILTERABLE (source));
          gtk_actionable_set_action_target (GTK_ACTIONABLE (device_button), "s", id);
          gtk_container_add (GTK_CONTAINER (self->devices_popover_grid), device_button);

          device_widget = photos_removable_device_widget_new (source);
          gtk_container_add (GTK_CONTAINER (device_button), device_widget);

          gtk_widget_show_all (device_button);
        }

      gtk_stack_set_visible_child (GTK_STACK (self->stack), self->devices_button);
    }

  visible = TRUE;

 out:
  gtk_widget_set_visible (self->stack, visible);
  g_list_free_full (sources, g_object_unref);
}


static void
photos_removable_devices_button_dispose (GObject *object)
{
  PhotosRemovableDevicesButton *self = PHOTOS_REMOVABLE_DEVICES_BUTTON (object);

  g_clear_object (&self->src_mngr);

  G_OBJECT_CLASS (photos_removable_devices_button_parent_class)->dispose (object);
}


static void
photos_removable_devices_button_init (PhotosRemovableDevicesButton *self)
{
  GApplication *app;
  PhotosSearchContextState *state;

  gtk_widget_init_template (GTK_WIDGET (self));

  app = g_application_get_default ();
  state = photos_search_context_get_state (PHOTOS_SEARCH_CONTEXT (app));

  self->src_mngr = g_object_ref (state->src_mngr);
  g_signal_connect_object (self->src_mngr,
                           "object-added",
                           G_CALLBACK (photos_removable_devices_button_refresh_devices),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (self->src_mngr,
                           "object-removed",
                           G_CALLBACK (photos_removable_devices_button_refresh_devices),
                           self,
                           G_CONNECT_SWAPPED);

  photos_removable_devices_button_refresh_devices (self);
}


static void
photos_removable_devices_button_class_init (PhotosRemovableDevicesButtonClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->dispose = photos_removable_devices_button_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Photos/removable-devices-button.ui");
  gtk_widget_class_bind_template_child (widget_class, PhotosRemovableDevicesButton, device_button);
  gtk_widget_class_bind_template_child (widget_class, PhotosRemovableDevicesButton, device_widget);
  gtk_widget_class_bind_template_child (widget_class, PhotosRemovableDevicesButton, devices_button);
  gtk_widget_class_bind_template_child (widget_class, PhotosRemovableDevicesButton, devices_popover);
  gtk_widget_class_bind_template_child (widget_class, PhotosRemovableDevicesButton, devices_popover_grid);
  gtk_widget_class_bind_template_child (widget_class, PhotosRemovableDevicesButton, stack);
}


GtkWidget *
photos_removable_devices_button_new (void)
{
  return g_object_new (PHOTOS_TYPE_REMOVABLE_DEVICES_BUTTON, NULL);
}
