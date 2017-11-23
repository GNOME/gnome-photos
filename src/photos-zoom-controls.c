/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2017 Red Hat, Inc.
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

#include <string.h>

#include <gio/gio.h>

#include "photos-utils.h"
#include "photos-zoom-controls.h"


struct _PhotosZoomControls
{
  GtkBin parent_instance;
  GAction *zoom_best_fit_action;
  GAction *zoom_out_action;
  GtkWidget *revealer;
  GtkWidget *zoom_in_button;
  GtkWidget *zoom_out_button;
  GtkWidget *zoom_toggle_button;
  gboolean hover;
  guint notify_enabled_id;
};

enum
{
  PROP_0,
  PROP_HOVER
};


G_DEFINE_TYPE (PhotosZoomControls, photos_zoom_controls, GTK_TYPE_BIN);


static void
photos_zoom_controls_update_buttons (PhotosZoomControls *self)
{
  GtkWidget *image;
  gboolean zoom_best_fit_enabled;
  gboolean zoom_out_enabled;
  const gchar *icon_name;

  zoom_best_fit_enabled = g_action_get_enabled (self->zoom_best_fit_action);
  zoom_out_enabled = g_action_get_enabled (self->zoom_out_action);
  g_return_if_fail (zoom_best_fit_enabled == zoom_out_enabled);

  gtk_revealer_set_reveal_child (GTK_REVEALER (self->revealer), zoom_out_enabled);

  gtk_actionable_set_action_name (GTK_ACTIONABLE (self->zoom_toggle_button), NULL);
  gtk_actionable_set_action_target_value (GTK_ACTIONABLE (self->zoom_toggle_button), NULL);

  if (zoom_out_enabled)
    {
      gtk_actionable_set_action_name (GTK_ACTIONABLE (self->zoom_toggle_button), "app.zoom-best-fit");
    }
  else
    {
      GVariant *target_value = NULL;

      target_value = photos_utils_create_zoom_target_value (1.0, PHOTOS_ZOOM_EVENT_MOUSE_CLICK);
      gtk_actionable_set_action_target_value (GTK_ACTIONABLE (self->zoom_toggle_button), target_value);
      gtk_actionable_set_action_name (GTK_ACTIONABLE (self->zoom_toggle_button), "app.zoom-in");
    }

  icon_name = zoom_out_enabled ? "zoom-fit-best-symbolic" : "zoom-in-symbolic";
  image = gtk_image_new_from_icon_name (icon_name, GTK_ICON_SIZE_INVALID);
  gtk_image_set_pixel_size (GTK_IMAGE (image), 16);
  gtk_button_set_image (GTK_BUTTON (self->zoom_toggle_button), image);
}


static gboolean
photos_zoom_controls_notify_idle (gpointer user_data)
{
  PhotosZoomControls *self = PHOTOS_ZOOM_CONTROLS (user_data);

  self->notify_enabled_id = 0;
  photos_zoom_controls_update_buttons (self);
  return G_SOURCE_REMOVE;
}


static void
photos_zoom_controls_notify_enabled (PhotosZoomControls *self)
{
  if (self->notify_enabled_id != 0)
    return;

  self->notify_enabled_id = g_idle_add (photos_zoom_controls_notify_idle, self);
}


static void
photos_zoom_controls_set_hover (PhotosZoomControls *self, gboolean hover)
{
  if (self->hover == hover)
    return;

  self->hover = hover;
  g_object_notify (G_OBJECT (self), "hover");
}


static gboolean
photos_zoom_controls_enter_notify (GtkWidget *widget, GdkEventCrossing *event)
{
  PhotosZoomControls *self = PHOTOS_ZOOM_CONTROLS (widget);

  if (event->detail != GDK_NOTIFY_INFERIOR)
    photos_zoom_controls_set_hover (self, TRUE);

  return GDK_EVENT_PROPAGATE;
}


static gboolean
photos_zoom_controls_leave_notify (GtkWidget *widget, GdkEventCrossing *event)
{
  PhotosZoomControls *self = PHOTOS_ZOOM_CONTROLS (widget);

  if (event->detail != GDK_NOTIFY_INFERIOR)
    photos_zoom_controls_set_hover (self, FALSE);

  return GDK_EVENT_PROPAGATE;
}


static void
photos_zoom_controls_realize (GtkWidget *widget)
{
  GdkVisual *visual;
  g_autoptr (GdkWindow) window = NULL;
  GdkWindow *parent_window;
  GdkWindowAttr attributes;
  GtkAllocation allocation;
  gint attributes_mask;

  gtk_widget_set_realized (widget, TRUE);

  attributes.event_mask = gtk_widget_get_events (widget);
  attributes.event_mask |= (GDK_BUTTON_PRESS_MASK
                            | GDK_BUTTON_RELEASE_MASK
                            | GDK_ENTER_NOTIFY_MASK
                            | GDK_LEAVE_NOTIFY_MASK);

  gtk_widget_get_allocation (widget, &allocation);
  attributes.x = allocation.x;
  attributes.y = allocation.y;
  attributes.width = allocation.width;
  attributes.height = allocation.height;

  attributes.wclass = GDK_INPUT_OUTPUT;

  visual = gtk_widget_get_visual (widget);
  attributes.visual = visual;

  attributes.window_type = GDK_WINDOW_CHILD;

  attributes_mask = GDK_WA_VISUAL | GDK_WA_X | GDK_WA_Y;

  parent_window = gtk_widget_get_parent_window (widget);
  window = gdk_window_new (parent_window, &attributes, attributes_mask);
  gtk_widget_set_window (widget, g_object_ref (window));
  gtk_widget_register_window (widget, window);
}


static void
photos_zoom_controls_size_allocate (GtkWidget *widget, GtkAllocation *allocation)
{
  GdkWindow *window;

  GTK_WIDGET_CLASS (photos_zoom_controls_parent_class)->size_allocate (widget, allocation);

  if (!gtk_widget_get_realized (widget))
    return;

  window = gtk_widget_get_window (widget);
  gdk_window_move_resize (window, allocation->x, allocation->y, allocation->width, allocation->height);
}


static void
photos_zoom_controls_dispose (GObject *object)
{
  PhotosZoomControls *self = PHOTOS_ZOOM_CONTROLS (object);

  if (self->notify_enabled_id != 0)
    {
      g_source_remove (self->notify_enabled_id);
      self->notify_enabled_id = 0;
    }

  G_OBJECT_CLASS (photos_zoom_controls_parent_class)->dispose (object);
}


static void
photos_zoom_controls_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
  PhotosZoomControls *self = PHOTOS_ZOOM_CONTROLS (object);

  switch (prop_id)
    {
    case PROP_HOVER:
      g_value_set_boolean (value, self->hover);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
photos_zoom_controls_init (PhotosZoomControls *self)
{
  GApplication *app;

  app = g_application_get_default ();

  gtk_widget_init_template (GTK_WIDGET (self));
  gtk_widget_set_has_window (GTK_WIDGET (self), TRUE);

  self->zoom_best_fit_action = g_action_map_lookup_action (G_ACTION_MAP (app), "zoom-best-fit");
  g_signal_connect_swapped (self->zoom_best_fit_action,
                            "notify::enabled",
                            G_CALLBACK (photos_zoom_controls_notify_enabled),
                            self);

  self->zoom_out_action = g_action_map_lookup_action (G_ACTION_MAP (app), "zoom-out");
  g_signal_connect_swapped (self->zoom_out_action,
                            "notify::enabled",
                            G_CALLBACK (photos_zoom_controls_notify_enabled),
                            self);

  photos_zoom_controls_update_buttons (self);
}


static void
photos_zoom_controls_class_init (PhotosZoomControlsClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->dispose = photos_zoom_controls_dispose;
  object_class->get_property = photos_zoom_controls_get_property;

  widget_class->enter_notify_event = photos_zoom_controls_enter_notify;
  widget_class->leave_notify_event = photos_zoom_controls_leave_notify;
  widget_class->realize = photos_zoom_controls_realize;
  widget_class->size_allocate = photos_zoom_controls_size_allocate;

  g_object_class_install_property (object_class,
                                   PROP_HOVER,
                                   g_param_spec_boolean ("hover",
                                                         "Hover",
                                                         "Whether the widget is hovered",
                                                         FALSE,
                                                         G_PARAM_EXPLICIT_NOTIFY |
                                                         G_PARAM_READABLE |
                                                         G_PARAM_STATIC_STRINGS));

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Photos/zoom-controls.ui");
  gtk_widget_class_bind_template_child (widget_class, PhotosZoomControls, revealer);
  gtk_widget_class_bind_template_child (widget_class, PhotosZoomControls, zoom_in_button);
  gtk_widget_class_bind_template_child (widget_class, PhotosZoomControls, zoom_out_button);
  gtk_widget_class_bind_template_child (widget_class, PhotosZoomControls, zoom_toggle_button);
}


GtkWidget *
photos_zoom_controls_new (void)
{
  return g_object_new (PHOTOS_TYPE_ZOOM_CONTROLS, NULL);
}


gboolean
photos_zoom_controls_get_hover (PhotosZoomControls *self)
{
  g_return_val_if_fail (PHOTOS_IS_ZOOM_CONTROLS (self), FALSE);
  return self->hover;
}
