/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2015 – 2016 Red Hat, Inc.
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

#include <babl/babl.h>
#include <cairo-gobject.h>
#include <glib.h>

#include "photos-debug.h"
#include "photos-image-view.h"
#include "photos-marshalers.h"
#include "photos-gegl-image.h"


struct _PhotosImageView
{
  GtkImageView parent_instance;
  GeglNode *node;
  PhotosGeglImage *image;
};

struct _PhotosImageViewClass
{
  GtkImageViewClass parent_class;

  /* signals */
  void        (*draw_overlay)       (PhotosImageView *self, cairo_t *cr, GdkRectangle *rect);
};

enum
{
  PROP_0,
  PROP_NODE,
  PROP_X,
  PROP_Y,
  PROP_ZOOM
};

enum
{
  DRAW_BACKGROUND,
  DRAW_OVERLAY,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (PhotosImageView, photos_image_view, GTK_TYPE_IMAGE_VIEW);

static gboolean
photos_image_view_draw (GtkWidget *widget, cairo_t *cr)
{
  PhotosImageView *self = PHOTOS_IMAGE_VIEW (widget);
  GdkRectangle rect;

  if (self->node == NULL)
    goto out;

  if (!gdk_cairo_get_clip_rectangle (cr, &rect))
    goto out;

  GTK_WIDGET_CLASS (photos_image_view_parent_class)->draw (widget, cr);

  cairo_save (cr);
  g_signal_emit (self, signals[DRAW_OVERLAY], 0, cr, &rect);
  cairo_restore(cr);

 out:
  return GDK_EVENT_PROPAGATE;
}

static void
photos_image_view_dispose (GObject *object)
{
  PhotosImageView *self = PHOTOS_IMAGE_VIEW (object);

  g_clear_object (&self->node);
  g_clear_object (&self->image);

  G_OBJECT_CLASS (photos_image_view_parent_class)->dispose (object);
}


static void
photos_image_view_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
  PhotosImageView *self = PHOTOS_IMAGE_VIEW (object);

  switch (prop_id)
    {
    case PROP_NODE:
      g_value_set_object (value, self->node);
      break;

    case PROP_X:
      g_value_set_double (value, 0);
      break;

    case PROP_Y:
      g_value_set_double (value, 0);
      break;

    case PROP_ZOOM:
      g_value_set_float (value, gtk_image_view_get_scale (GTK_IMAGE_VIEW (self)));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
photos_image_view_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
  PhotosImageView *self = PHOTOS_IMAGE_VIEW (object);

  switch (prop_id)
    {
    case PROP_NODE:
      {
        GeglNode *node;

        node = GEGL_NODE (g_value_get_object (value));
        photos_image_view_set_node (self, node);
        break;
      }

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
view_scale_changed_cb (GObject *source, GParamSpec *spec, gpointer user_data)
{
  PhotosImageView *view = PHOTOS_IMAGE_VIEW (source);
  double scale = gtk_image_view_get_scale (GTK_IMAGE_VIEW (source));

  photos_gegl_image_set_view_scale (view->image, scale);
}

static void
photos_image_view_init (PhotosImageView *self)
{
  GtkStyleContext *context;

  gtk_widget_add_events (GTK_WIDGET (self),
                         GDK_BUTTON_PRESS_MASK
                         | GDK_BUTTON_RELEASE_MASK
                         | GDK_POINTER_MOTION_MASK);

  context = gtk_widget_get_style_context (GTK_WIDGET (self));
  gtk_style_context_add_class (context, GTK_STYLE_CLASS_VIEW);
  gtk_style_context_add_class (context, "content-view");

  g_signal_connect (G_OBJECT (self), "notify::scale", G_CALLBACK (view_scale_changed_cb), NULL);
}


static void
photos_image_view_class_init (PhotosImageViewClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->dispose = photos_image_view_dispose;
  object_class->get_property = photos_image_view_get_property;
  object_class->set_property = photos_image_view_set_property;
  widget_class->draw = photos_image_view_draw;

  g_object_class_install_property (object_class,
                                   PROP_NODE,
                                   g_param_spec_object ("node",
                                                        "GeglNode object",
                                                        "The GeglNode to render",
                                                        GEGL_TYPE_NODE,
                                                        G_PARAM_CONSTRUCT | G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_X,
                                   g_param_spec_double ("x",
                                                        "X",
                                                        "X origin",
                                                        -G_MAXDOUBLE,
                                                        G_MAXDOUBLE,
                                                        0.0,
                                                        G_PARAM_READABLE));

  g_object_class_install_property (object_class,
                                   PROP_Y,
                                   g_param_spec_double ("y",
                                                        "Y",
                                                        "Y origin",
                                                        -G_MAXDOUBLE,
                                                        G_MAXDOUBLE,
                                                        0.0,
                                                        G_PARAM_READABLE));

  g_object_class_install_property (object_class,
                                   PROP_ZOOM,
                                   g_param_spec_float ("zoom",
                                                       "Zoom",
                                                       "Zoom factor",
                                                       0.0f,
                                                       100.0f,
                                                       1.0f,
                                                       G_PARAM_READABLE));

  signals[DRAW_OVERLAY] = g_signal_new ("draw-overlay",
                                        G_TYPE_FROM_CLASS (class),
                                        G_SIGNAL_RUN_LAST,
                                        G_STRUCT_OFFSET (PhotosImageViewClass, draw_overlay),
                                        NULL, /* accumulator */
                                        NULL, /* accu_data */
                                        _photos_marshal_VOID__BOXED_BOXED,
                                        G_TYPE_NONE,
                                        2,
                                        CAIRO_GOBJECT_TYPE_CONTEXT,
                                        GDK_TYPE_RECTANGLE);
}


GtkWidget *
photos_image_view_new (void)
{
  return g_object_new (PHOTOS_TYPE_IMAGE_VIEW,
                       "fit-allocation", TRUE,
                       /*"zoomable", FALSE,*/
                       "rotatable", FALSE,
                       NULL);
}


GtkWidget *
photos_image_view_new_from_node (GeglNode *node)
{
  g_return_val_if_fail (node == NULL || GEGL_IS_NODE (node), NULL);
  return g_object_new (PHOTOS_TYPE_IMAGE_VIEW, "node", node, NULL);
}


GeglNode *
photos_image_view_get_node (PhotosImageView *self)
{
  g_return_val_if_fail (PHOTOS_IS_IMAGE_VIEW (self), NULL);
  return self->node;
}


double
photos_image_view_get_x (PhotosImageView *self)
{
  double x;
  int widget_width;
  double image_width;

  g_return_val_if_fail (PHOTOS_IS_IMAGE_VIEW (self), 0.0);

  widget_width = gtk_widget_get_allocated_width (GTK_WIDGET (self));

  image_width = gtk_abstract_image_get_width (GTK_ABSTRACT_IMAGE (self->image)) *
                gtk_image_view_get_scale (GTK_IMAGE_VIEW (self));


  x = (widget_width - image_width) / 2.0;

  return x;
}


double
photos_image_view_get_y (PhotosImageView *self)
{
  double y;
  int widget_height;
  double image_height;

  g_return_val_if_fail (PHOTOS_IS_IMAGE_VIEW (self), 0.0);

  widget_height = gtk_widget_get_allocated_height (GTK_WIDGET (self));

  image_height = gtk_abstract_image_get_height (GTK_ABSTRACT_IMAGE (self->image)) *
                gtk_image_view_get_scale (GTK_IMAGE_VIEW (self));


  y = (widget_height - image_height) / 2.0;

  return y;
}


double
photos_image_view_get_zoom (PhotosImageView *self)
{
  g_return_val_if_fail (PHOTOS_IS_IMAGE_VIEW (self), 0.0);

  return gtk_image_view_get_scale (GTK_IMAGE_VIEW (self));
}


void
photos_image_view_set_node (PhotosImageView *self, GeglNode *node)
{
  int scale_factor;

  g_return_if_fail (PHOTOS_IS_IMAGE_VIEW (self));

  if (self->node == node)
    return;

  g_clear_object (&self->node);

  if (node != NULL)
    g_object_ref (node);

  scale_factor = gtk_widget_get_scale_factor (GTK_WIDGET (self));

  self->node = node;
  self->image = photos_gegl_image_new (node, scale_factor);
  gtk_image_view_set_abstract_image (GTK_IMAGE_VIEW (self), GTK_ABSTRACT_IMAGE (self->image));
}
