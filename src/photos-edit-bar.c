/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2015 Red Hat, Inc.
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

#include "photos-edit-bar.h"


struct _PhotosEditBar
{
  GtkBox parent_instance;
  GtkWidget *button_area;
  gboolean hover;
};

struct _PhotosEditBarClass
{
  GtkBoxClass parent_class;
};

enum
{
  PROP_0,
  PROP_HOVER
};


G_DEFINE_TYPE (PhotosEditBar, photos_edit_bar, GTK_TYPE_BOX);


static void
photos_edit_bar_set_hover (PhotosEditBar *self, gboolean hover)
{
  if (self->hover == hover)
    return;

  self->hover = hover;
  g_object_notify (G_OBJECT (self), "hover");
}


static gboolean
photos_edit_bar_draw (GtkWidget *widget, cairo_t *cr)
{
  GtkStyleContext *context;
  gint height;
  gint width;

  context = gtk_widget_get_style_context (widget);
  height = gtk_widget_get_allocated_height (widget);
  width = gtk_widget_get_allocated_width (widget);
  gtk_render_background (context, cr, 0, 0, width, height);
  gtk_render_frame (context, cr, 0, 0, width, height);

  return GTK_WIDGET_CLASS (photos_edit_bar_parent_class)->draw (widget, cr);
}


static gboolean
photos_edit_bar_enter_notify_event (GtkWidget *widget, GdkEventCrossing *event)
{
  PhotosEditBar *self = PHOTOS_EDIT_BAR (widget);

  if (event->detail != GDK_NOTIFY_INFERIOR)
    photos_edit_bar_set_hover (self, TRUE);

  return GDK_EVENT_PROPAGATE;
}


static gboolean
photos_edit_bar_leave_notify_event (GtkWidget *widget, GdkEventCrossing *event)
{
  PhotosEditBar *self = PHOTOS_EDIT_BAR (widget);

  if (event->detail != GDK_NOTIFY_INFERIOR)
    photos_edit_bar_set_hover (self, FALSE);

  return GDK_EVENT_PROPAGATE;
}


static void
photos_edit_bar_realize (GtkWidget *widget)
{
  GdkWindow *parent_window;
  GdkWindow *window;
  GdkWindowAttr attributes;
  GtkAllocation allocation;
  gint attributes_mask;

  gtk_widget_get_allocation (widget, &allocation);

  gtk_widget_set_realized (widget, TRUE);

  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.x = allocation.x;
  attributes.y = allocation.y;
  attributes.width = allocation.width;
  attributes.height = allocation.height;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.event_mask = gtk_widget_get_events (widget);
  attributes.event_mask |= (GDK_BUTTON_PRESS_MASK
                            | GDK_BUTTON_RELEASE_MASK
                            | GDK_TOUCH_MASK
                            | GDK_ENTER_NOTIFY_MASK
                            | GDK_LEAVE_NOTIFY_MASK);

  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL;

  parent_window = gtk_widget_get_parent_window (widget);
  window = gdk_window_new (parent_window, &attributes, attributes_mask);
  gtk_widget_set_window (widget, window);
  gtk_widget_register_window (widget, window);

  gtk_style_context_set_background (gtk_widget_get_style_context (widget), window);
}


static void
photos_edit_bar_size_allocate (GtkWidget *widget, GtkAllocation *allocation)
{
  GTK_WIDGET_CLASS (photos_edit_bar_parent_class)->size_allocate (widget, allocation);

  if (gtk_widget_get_realized (widget))
    {
      GdkWindow *window;

      window = gtk_widget_get_window (widget);
      gdk_window_move_resize (window, allocation->x, allocation->y, allocation->width, allocation->height);
    }
}


static void
photos_edit_bar_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
  PhotosEditBar *self = PHOTOS_EDIT_BAR (object);

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
photos_edit_bar_init (PhotosEditBar *self)
{
  GtkStyleContext *context;

  gtk_widget_set_has_window (GTK_WIDGET (self), TRUE);

  context = gtk_widget_get_style_context (GTK_WIDGET (self));
  gtk_style_context_add_class (context, GTK_STYLE_CLASS_TOOLBAR);

  self->button_area = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_set_hexpand (GTK_WIDGET (self->button_area), TRUE);
  gtk_container_set_border_width (GTK_CONTAINER (self->button_area), 10);
  gtk_box_set_spacing (GTK_BOX (self->button_area), 10);
  gtk_container_add (GTK_CONTAINER (self), self->button_area);
  gtk_widget_show (self->button_area);
}


static void
photos_edit_bar_class_init (PhotosEditBarClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->get_property = photos_edit_bar_get_property;
  widget_class->draw = photos_edit_bar_draw;
  widget_class->enter_notify_event = photos_edit_bar_enter_notify_event;
  widget_class->leave_notify_event = photos_edit_bar_leave_notify_event;
  widget_class->realize = photos_edit_bar_realize;
  widget_class->size_allocate = photos_edit_bar_size_allocate;

  g_object_class_install_property (object_class,
                                   PROP_HOVER,
                                   g_param_spec_boolean ("hover",
                                                         "Hover",
                                                         "Whether the widget is hovered",
                                                         FALSE,
                                                         G_PARAM_READABLE));
}


GtkWidget *
photos_edit_bar_new (void)
{
  return g_object_new (PHOTOS_TYPE_EDIT_BAR, "orientation", GTK_ORIENTATION_HORIZONTAL, NULL);
}


GtkBox *
photos_edit_bar_get_button_area (PhotosEditBar *self)
{
  return GTK_BOX (self->button_area);
}


gboolean
photos_edit_bar_get_hover (PhotosEditBar *self)
{
  return self->hover;
}
