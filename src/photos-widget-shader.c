/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2015 – 2017 Red Hat, Inc.
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

#include <cairo.h>

#include "photos-widget-shader.h"


struct _PhotosWidgetShader
{
  GInitiallyUnowned parent_instance;
  GtkWidget *widget;
  cairo_surface_t *surface;
  gboolean active;
};

enum
{
  PROP_0,
  PROP_ACTIVE,
  PROP_WIDGET
};


G_DEFINE_TYPE (PhotosWidgetShader, photos_widget_shader, G_TYPE_INITIALLY_UNOWNED);


static gboolean
photos_widget_shader_draw (PhotosWidgetShader *self, cairo_t *cr)
{
  if (!self->active)
    goto out;

  cairo_save (cr);
  cairo_set_source_surface (cr, self->surface, 0.0, 0.0);
  cairo_paint (cr);
  cairo_restore (cr);

 out:
  return GDK_EVENT_PROPAGATE;
}


static void
photos_widget_shader_surface_create (PhotosWidgetShader *self)
{
  GdkRectangle allocation;
  GdkWindow *window;
  cairo_t *cr = NULL;

  g_clear_pointer (&self->surface, (GDestroyNotify) cairo_surface_destroy);

  if (self->widget == NULL)
    goto out;

  window = gtk_widget_get_window (self->widget);
  if (window == NULL)
    goto out;

  gtk_widget_get_allocation (self->widget, &allocation);
  self->surface = gdk_window_create_similar_surface (window,
                                                     CAIRO_CONTENT_COLOR_ALPHA,
                                                     allocation.width,
                                                     allocation.height);

  cr = cairo_create (self->surface);
  cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
  cairo_set_source_rgba (cr, 0.0, 0.0, 0.0, 0.5);
  cairo_paint (cr);

 out:
  g_clear_pointer (&cr, (GDestroyNotify) cairo_destroy);
}


static void
photos_widget_shader_size_allocate (PhotosWidgetShader *self)
{
  photos_widget_shader_surface_create (self);
}


static void
photos_widget_shader_constructed (GObject *object)
{
  PhotosWidgetShader *self = PHOTOS_WIDGET_SHADER (object);

  G_OBJECT_CLASS (photos_widget_shader_parent_class)->constructed (object);

  photos_widget_shader_surface_create (self);

  g_signal_connect_object (self->widget,
                           "draw",
                           G_CALLBACK (photos_widget_shader_draw),
                           self,
                           G_CONNECT_AFTER | G_CONNECT_SWAPPED);
  g_signal_connect_object (self->widget,
                           "size-allocate",
                           G_CALLBACK (photos_widget_shader_size_allocate),
                           self,
                           G_CONNECT_SWAPPED);

  g_object_ref_sink (self);
}


static void
photos_widget_shader_finalize (GObject *object)
{
  PhotosWidgetShader *self = PHOTOS_WIDGET_SHADER (object);

  g_assert_null (self->widget);
  g_clear_pointer (&self->surface, (GDestroyNotify) cairo_surface_destroy);

  G_OBJECT_CLASS (photos_widget_shader_parent_class)->finalize (object);
}


static void
photos_widget_shader_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
  PhotosWidgetShader *self = PHOTOS_WIDGET_SHADER (object);

  switch (prop_id)
    {
    case PROP_ACTIVE:
      {
        gboolean active;

        active = g_value_get_boolean (value);
        photos_widget_shader_set_active (self, active);
        break;
      }

    case PROP_WIDGET:
      self->widget = GTK_WIDGET (g_value_get_object (value));
      g_object_add_weak_pointer (G_OBJECT (self->widget), (gpointer *) &self->widget);
      g_object_weak_ref (G_OBJECT (self->widget), (GWeakNotify) g_object_unref, self);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
photos_widget_shader_init (PhotosWidgetShader *self)
{
}


static void
photos_widget_shader_class_init (PhotosWidgetShaderClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->constructed = photos_widget_shader_constructed;
  object_class->finalize = photos_widget_shader_finalize;
  object_class->set_property = photos_widget_shader_set_property;

  g_object_class_install_property (object_class,
                                   PROP_ACTIVE,
                                   g_param_spec_boolean ("active",
                                                         "Active",
                                                         "Whether the widget shader is active or not",
                                                         FALSE,
                                                         G_PARAM_WRITABLE));

  g_object_class_install_property (object_class,
                                   PROP_WIDGET,
                                   g_param_spec_object ("widget",
                                                        "Widget",
                                                        "The target widget that is going to be shaded",
                                                        GTK_TYPE_WIDGET,
                                                        G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE));
}


PhotosWidgetShader *
photos_widget_shader_new (GtkWidget *widget)
{
  g_return_val_if_fail (widget != NULL && GTK_IS_WIDGET (widget), NULL);
  return g_object_new (PHOTOS_TYPE_WIDGET_SHADER, "widget", widget, NULL);
}


void
photos_widget_shader_set_active (PhotosWidgetShader *self, gboolean active)
{
  if (self->active == active)
    return;

  self->active = active;
  g_object_notify (G_OBJECT (self), "active");

  if (self->widget == NULL)
    return;

  gtk_widget_queue_draw (self->widget);
}
