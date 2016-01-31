/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2015, 2016 Red Hat, Inc.
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


struct _PhotosImageView
{
  GtkDrawingArea parent_instance;
  GeglNode *node;
  gfloat x;
  gfloat x_scaled;
  gfloat y;
  gfloat y_scaled;
  gfloat zoom;
  gfloat zoom_scaled;
};

struct _PhotosImageViewClass
{
  GtkDrawingAreaClass parent_class;

  /* signals */
  void        (*draw_background)    (PhotosImageView *self, cairo_t *cr, GdkRectangle *rect);
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


G_DEFINE_TYPE (PhotosImageView, photos_image_view, GTK_TYPE_DRAWING_AREA);


static void
photos_image_view_update (PhotosImageView *self)
{
  GdkRectangle viewport;
  GeglRectangle bbox;
  float zoom_scaled = 1.0;
  gint scale_factor;
  gint viewport_height_real;
  gint viewport_width_real;

  if (self->node == NULL)
    return;

  gtk_widget_get_allocation (GTK_WIDGET (self), &viewport);

  if (viewport.width < 0 || viewport.height < 0)
    return;

  bbox = gegl_node_get_bounding_box (self->node);
  if (bbox.width < 0 || bbox.height < 0)
    return;

  scale_factor = gtk_widget_get_scale_factor (GTK_WIDGET (self));
  viewport_height_real = viewport.height * scale_factor;
  viewport_width_real = viewport.width * scale_factor;

  if (bbox.height > viewport_height_real || bbox.width > viewport_width_real)
    {
      gfloat height_ratio = bbox.height / (gfloat) viewport_height_real;
      gfloat width_ratio = bbox.width / (gfloat) viewport_width_real;
      gfloat max_ratio =  MAX (height_ratio, width_ratio);

      zoom_scaled = 1.0 / max_ratio;

      bbox.width = (gint) (zoom_scaled * bbox.width + 0.5);
      bbox.height = (gint) (zoom_scaled * bbox.height + 0.5);
      bbox.x = (gint) (zoom_scaled * bbox.x + 0.5);
      bbox.y = (gint) (zoom_scaled * bbox.y + 0.5);
    }

  self->zoom_scaled = zoom_scaled;
  self->zoom = self->zoom_scaled / (gfloat) scale_factor;

  /* At this point, viewport is definitely bigger than bbox. */
  self->x_scaled = (bbox.width - viewport_width_real) / 2.0 + bbox.x;
  self->y_scaled = (bbox.height - viewport_height_real) / 2.0 + bbox.y;

  self->x = self->x_scaled / (gfloat) scale_factor;
  self->y = self->y_scaled / (gfloat) scale_factor;
}


static void
photos_image_view_computed (PhotosImageView *self)
{
  photos_image_view_update (self);
}


static void
photos_image_view_draw_node (PhotosImageView *self, cairo_t *cr, GdkRectangle *rect)
{
  const Babl *format;
  GeglRectangle roi;
  cairo_surface_t *surface = NULL;
  guchar *buf = NULL;
  gint scale_factor;
  gint stride;
  gint64 end;
  gint64 start;

  scale_factor = gtk_widget_get_scale_factor (GTK_WIDGET (self));

  roi.x = (gint) self->x_scaled + rect->x * scale_factor;
  roi.y = (gint) self->y_scaled + rect->y * scale_factor;
  roi.width  = rect->width * scale_factor;
  roi.height = rect->height * scale_factor;

  format = babl_format ("cairo-ARGB32");
  stride = cairo_format_stride_for_width (CAIRO_FORMAT_ARGB32, roi.width);
  buf = g_malloc0 (stride * roi.height);

  start = g_get_monotonic_time ();

  gegl_node_blit (self->node,
                  (gdouble) self->zoom_scaled,
                  &roi,
                  format,
                  buf,
                  GEGL_AUTO_ROWSTRIDE,
                  GEGL_BLIT_CACHE | GEGL_BLIT_DIRTY);

  end = g_get_monotonic_time ();
  photos_debug (PHOTOS_DEBUG_GEGL, "PhotosImageView: Node Blit: %" G_GINT64_FORMAT, end - start);

  surface = cairo_image_surface_create_for_data (buf, CAIRO_FORMAT_ARGB32, roi.width, roi.height, stride);
  cairo_surface_set_device_scale (surface, (gdouble) scale_factor, (gdouble) scale_factor);
  cairo_set_source_surface (cr, surface, rect->x, rect->y);
  cairo_paint (cr);

  cairo_surface_destroy (surface);
  g_free (buf);
}


static gboolean
photos_image_view_draw (GtkWidget *widget, cairo_t *cr)
{
  PhotosImageView *self = PHOTOS_IMAGE_VIEW (widget);
  GdkRectangle rect;

  if (self->node == NULL)
    goto out;

  if (!gdk_cairo_get_clip_rectangle (cr, &rect))
    goto out;

  cairo_save (cr);
  g_signal_emit (self, signals[DRAW_BACKGROUND], 0, cr, &rect);
  cairo_restore(cr);

  cairo_save (cr);
  photos_image_view_draw_node (self, cr, &rect);
  cairo_restore (cr);

  cairo_save (cr);
  g_signal_emit (self, signals[DRAW_OVERLAY], 0, cr, &rect);
  cairo_restore(cr);

 out:
  return GDK_EVENT_PROPAGATE;
}


static void
photos_image_view_size_allocate (GtkWidget *widget, GtkAllocation *allocation)
{
  PhotosImageView *self = PHOTOS_IMAGE_VIEW (widget);

  GTK_WIDGET_CLASS (photos_image_view_parent_class)->size_allocate (widget, allocation);

  photos_image_view_update (self);
}


static void
photos_image_view_dispose (GObject *object)
{
  PhotosImageView *self = PHOTOS_IMAGE_VIEW (object);

  g_clear_object (&self->node);

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
      g_value_set_float (value, self->x);
      break;

    case PROP_Y:
      g_value_set_float (value, self->y);
      break;

    case PROP_ZOOM:
      g_value_set_float (value, self->zoom);
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
  widget_class->size_allocate = photos_image_view_size_allocate;

  g_object_class_install_property (object_class,
                                   PROP_NODE,
                                   g_param_spec_object ("node",
                                                        "GeglNode object",
                                                        "The GeglNode to render",
                                                        GEGL_TYPE_NODE,
                                                        G_PARAM_CONSTRUCT | G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_X,
                                   g_param_spec_float ("x",
                                                       "X",
                                                       "X origin",
                                                       -G_MAXFLOAT,
                                                       G_MAXFLOAT,
                                                       0.0,
                                                       G_PARAM_READABLE));

  g_object_class_install_property (object_class,
                                   PROP_Y,
                                   g_param_spec_float ("y",
                                                       "Y",
                                                       "Y origin",
                                                       -G_MAXFLOAT,
                                                       G_MAXFLOAT,
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

  signals[DRAW_BACKGROUND] = g_signal_new ("draw-background",
                                           G_TYPE_FROM_CLASS (class),
                                           G_SIGNAL_RUN_LAST,
                                           G_STRUCT_OFFSET (PhotosImageViewClass, draw_background),
                                           NULL, /* accumulator */
                                           NULL, /* accu_data */
                                           _photos_marshal_VOID__BOXED_BOXED,
                                           G_TYPE_NONE,
                                           2,
                                           CAIRO_GOBJECT_TYPE_CONTEXT,
                                           GDK_TYPE_RECTANGLE);

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
  return g_object_new (PHOTOS_TYPE_IMAGE_VIEW, NULL);
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


gfloat
photos_image_view_get_x (PhotosImageView *self)
{
  g_return_val_if_fail (PHOTOS_IS_IMAGE_VIEW (self), 0.0);
  return self->x;
}


gfloat
photos_image_view_get_y (PhotosImageView *self)
{
  g_return_val_if_fail (PHOTOS_IS_IMAGE_VIEW (self), 0.0);
  return self->y;
}


gfloat
photos_image_view_get_zoom (PhotosImageView *self)
{
  g_return_val_if_fail (PHOTOS_IS_IMAGE_VIEW (self), 0.0);
  return self->zoom;
}


void
photos_image_view_set_node (PhotosImageView *self, GeglNode *node)
{
  g_return_if_fail (PHOTOS_IS_IMAGE_VIEW (self));

  if (self->node == node)
    return;

  if (self->node != NULL)
    g_signal_handlers_disconnect_by_func (self->node, photos_image_view_computed, self);

  g_clear_object (&self->node);

  if (node != NULL)
    {
      g_object_ref (node);
      g_signal_connect_object (node, "computed", G_CALLBACK (photos_image_view_computed), self, G_CONNECT_SWAPPED);
    }

  self->node = node;
  photos_image_view_update (self);
}
