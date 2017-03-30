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

#include <babl/babl.h>
#include <cairo.h>
#include <cairo-gobject.h>
#include <glib.h>

#include "photos-debug.h"
#include "photos-gegl.h"
#include "photos-image-view.h"
#include "photos-marshalers.h"
#include "photos-utils.h"


struct _PhotosImageView
{
  GtkDrawingArea parent_instance;
  GeglBuffer *buffer;
  GeglNode *node;
  GeglRectangle bbox_zoomed_old;
  GtkAdjustment *hadjustment;
  GtkAdjustment *vadjustment;
  GtkAllocation allocation_scaled_old;
  GtkScrollablePolicy hscroll_policy;
  GtkScrollablePolicy vscroll_policy;
  cairo_region_t *bbox_region;
  cairo_region_t *region;
  gboolean best_fit;
  gdouble height;
  gdouble width;
  gdouble x;
  gdouble x_scaled;
  gdouble y;
  gdouble y_scaled;
  gdouble zoom;
  gdouble zoom_scaled;
};

enum
{
  PROP_0,
  PROP_BEST_FIT,
  PROP_HADJUSTMENT,
  PROP_HSCROLL_POLICY,
  PROP_NODE,
  PROP_VADJUSTMENT,
  PROP_VSCROLL_POLICY,
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


G_DEFINE_TYPE_WITH_CODE (PhotosImageView, photos_image_view, GTK_TYPE_DRAWING_AREA,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_SCROLLABLE, NULL));


static void photos_image_view_computed (PhotosImageView *self, GeglRectangle *rect);


static void
photos_image_view_update_buffer (PhotosImageView *self)
{
  const Babl *format;
  GeglBuffer *buffer;

  g_signal_handlers_block_by_func (self->node, photos_image_view_computed, self);

  format = babl_format ("cairo-ARGB32");
  buffer = photos_gegl_dup_buffer_from_node (self->node, format);
  g_set_object (&self->buffer, buffer);

  g_signal_handlers_unblock_by_func (self->node, photos_image_view_computed, self);
  g_object_unref (buffer);
}


static void
photos_image_view_update_region (PhotosImageView *self)
{
  GeglRectangle bbox;

  g_clear_pointer (&self->bbox_region, (GDestroyNotify) cairo_region_destroy);
  g_clear_pointer (&self->region, (GDestroyNotify) cairo_region_destroy);

  bbox = gegl_node_get_bounding_box (self->node);
  self->bbox_region = cairo_region_create_rectangle ((cairo_rectangle_int_t *) &bbox);
  self->region = cairo_region_create ();

  photos_debug (PHOTOS_DEBUG_GEGL,
                "PhotosImageView: Node (%p) Region: %d, %d, %d×%d",
                self->node,
                bbox.x,
                bbox.y,
                bbox.width,
                bbox.height);
}


static void
photos_image_view_adjustment_value_changed (GtkAdjustment *adjustment, gpointer user_data)
{
  PhotosImageView *self = PHOTOS_IMAGE_VIEW (user_data);
  gdouble value;
  gint scale_factor;

  g_return_if_fail (GTK_IS_ADJUSTMENT (adjustment));
  g_return_if_fail (self->hadjustment == adjustment || self->vadjustment == adjustment);

  if (self->node == NULL)
    return;

  if (!gtk_widget_get_realized (GTK_WIDGET (self)))
    return;

  if (self->best_fit)
    return;

  scale_factor = gtk_widget_get_scale_factor (GTK_WIDGET (self));

  if (self->hadjustment == adjustment)
    {
      value = gtk_adjustment_get_value (self->hadjustment);
      self->x = value;
      self->x_scaled = self->x * (gdouble) scale_factor;
    }
  else
    {
      value = gtk_adjustment_get_value (self->vadjustment);
      self->y = value;
      self->y_scaled = self->y * (gdouble) scale_factor;
    }

  gtk_widget_queue_draw (GTK_WIDGET (self));
}


static void
photos_image_view_set_hadjustment_values (PhotosImageView *self)
{
  GtkAllocation allocation;
  gdouble page_increment;
  gdouble page_size;
  gdouble step_increment;
  gdouble upper;
  gdouble value;

  gtk_widget_get_allocation (GTK_WIDGET (self), &allocation);

  page_size = (gdouble) allocation.width;
  page_increment = page_size * 0.9;
  step_increment = page_size * 0.1;

  upper = MAX ((gdouble) allocation.width, self->width);
  g_return_if_fail (upper - page_size >= 0.0);

  value = self->x;

  gtk_adjustment_configure (self->hadjustment,
                            value,
                            0.0,
                            upper,
                            step_increment,
                            page_increment,
                            page_size);
}


static void
photos_image_view_set_vadjustment_values (PhotosImageView *self)
{
  GtkAllocation allocation;
  gdouble page_increment;
  gdouble page_size;
  gdouble step_increment;
  gdouble upper;
  gdouble value;

  gtk_widget_get_allocation (GTK_WIDGET (self), &allocation);

  page_size = (gdouble) allocation.height;
  page_increment = page_size * 0.9;
  step_increment = page_size * 0.1;

  upper = MAX ((gdouble) allocation.height, self->height);
  g_return_if_fail (upper - page_size >= 0.0);

  value = self->y;

  gtk_adjustment_configure (self->vadjustment,
                            value,
                            0.0,
                            upper,
                            step_increment,
                            page_increment,
                            page_size);
}


static void
photos_image_view_update (PhotosImageView *self)
{
  GeglRectangle bbox;
  GeglRectangle bbox_zoomed;
  GtkAllocation allocation;
  gint scale_factor;
  gint viewport_height_real;
  gint viewport_width_real;

  g_object_freeze_notify (G_OBJECT (self));

  if (self->node == NULL)
    goto out;

  g_assert_true (GEGL_IS_BUFFER (self->buffer));

  gtk_widget_get_allocation (GTK_WIDGET (self), &allocation);

  if (allocation.width <= 0 || allocation.height <= 0)
    goto out;

  bbox = *gegl_buffer_get_extent (self->buffer);
  if (bbox.width < 0 || bbox.height < 0)
    goto out;

  scale_factor = gtk_widget_get_scale_factor (GTK_WIDGET (self));
  viewport_height_real = allocation.height * scale_factor;
  viewport_width_real = allocation.width * scale_factor;

  if (self->best_fit)
    {
      gdouble zoom_scaled = 1.0;

      if (bbox.height > viewport_height_real || bbox.width > viewport_width_real)
        {
          gdouble height_ratio = bbox.height / (gdouble) viewport_height_real;
          gdouble width_ratio = bbox.width / (gdouble) viewport_width_real;
          gdouble max_ratio =  MAX (height_ratio, width_ratio);

          zoom_scaled = 1.0 / max_ratio;
        }

      bbox_zoomed.width = (gint) (zoom_scaled * bbox.width + 0.5);
      bbox_zoomed.height = (gint) (zoom_scaled * bbox.height + 0.5);
      bbox_zoomed.x = (gint) (zoom_scaled * bbox.x + 0.5);
      bbox_zoomed.y = (gint) (zoom_scaled * bbox.y + 0.5);

      self->zoom_scaled = zoom_scaled;
      self->zoom = self->zoom_scaled / (gdouble) scale_factor;

      self->x_scaled = (bbox_zoomed.width - viewport_width_real) / 2.0;
      self->y_scaled = (bbox_zoomed.height - viewport_height_real) / 2.0;

      g_object_notify (G_OBJECT (self), "zoom");
    }
  else
    {
      gdouble ratio_old;

      bbox_zoomed.width = (gint) (self->zoom_scaled * bbox.width + 0.5);
      bbox_zoomed.height = (gint) (self->zoom_scaled * bbox.height + 0.5);
      bbox_zoomed.x = (gint) (self->zoom_scaled * bbox.x + 0.5);
      bbox_zoomed.y = (gint) (self->zoom_scaled * bbox.y + 0.5);

      if (bbox_zoomed.width > viewport_width_real)
        {
          ratio_old = (self->x_scaled
                       + self->allocation_scaled_old.width / 2.0
                       - (gdouble) self->bbox_zoomed_old.x)
                      / (gdouble) self->bbox_zoomed_old.width;
          self->x_scaled = ratio_old * (gdouble) bbox_zoomed.width - (viewport_width_real / 2.0);
          self->x_scaled = CLAMP (self->x_scaled, 0.0, (gdouble) bbox_zoomed.width - viewport_width_real);
        }
      else
        {
          self->x_scaled = (bbox_zoomed.width - viewport_width_real) / 2.0;
        }

      if (bbox_zoomed.height > viewport_height_real)
        {
          ratio_old = (self->y_scaled
                       + self->allocation_scaled_old.height / 2.0
                       - (gdouble) self->bbox_zoomed_old.y)
                      / (gdouble) self->bbox_zoomed_old.height;
          self->y_scaled = ratio_old * (gdouble) bbox_zoomed.height - (viewport_height_real / 2.0);
          self->y_scaled = CLAMP (self->y_scaled, 0.0, (gdouble) bbox_zoomed.height - viewport_height_real);
        }
      else
        {
          self->y_scaled = (bbox_zoomed.height - viewport_height_real) / 2.0;
        }
    }

  self->x_scaled += (gdouble) bbox_zoomed.x;
  self->y_scaled += (gdouble) bbox_zoomed.y;

  self->x = self->x_scaled / (gdouble) scale_factor;
  self->y = self->y_scaled / (gdouble) scale_factor;

  self->height = (gdouble) bbox_zoomed.height / (gdouble) scale_factor;
  self->width = (gdouble) bbox_zoomed.width / (gdouble) scale_factor;

  g_signal_handlers_block_by_func (self->hadjustment, photos_image_view_adjustment_value_changed, self);
  g_signal_handlers_block_by_func (self->vadjustment, photos_image_view_adjustment_value_changed, self);

  photos_image_view_set_hadjustment_values (self);
  photos_image_view_set_vadjustment_values (self);

  g_signal_handlers_unblock_by_func (self->hadjustment, photos_image_view_adjustment_value_changed, self);
  g_signal_handlers_unblock_by_func (self->vadjustment, photos_image_view_adjustment_value_changed, self);

  self->bbox_zoomed_old = bbox_zoomed;

 out:
  g_object_thaw_notify (G_OBJECT (self));
}


static void
photos_image_view_computed (PhotosImageView *self, GeglRectangle *rect)
{
  cairo_status_t status;

  g_return_if_fail (PHOTOS_IS_IMAGE_VIEW (self));
  g_return_if_fail (GEGL_IS_BUFFER (self->buffer));
  g_return_if_fail (GEGL_IS_NODE (self->node));
  g_return_if_fail (self->bbox_region != NULL);
  g_return_if_fail (self->region != NULL);

  photos_debug (PHOTOS_DEBUG_GEGL,
                "PhotosImageView: Node (%p) Computed: %d, %d, %d×%d",
                self->node,
                rect->x,
                rect->y,
                rect->width,
                rect->height);

  status = cairo_region_union_rectangle (self->region, (cairo_rectangle_int_t *) rect);
  g_return_if_fail (status == CAIRO_STATUS_SUCCESS);

  if (!cairo_region_equal (self->bbox_region, self->region))
    return;

  photos_debug (PHOTOS_DEBUG_GEGL, "PhotosImageView: Node (%p) Computing Completed", self->node);

  photos_image_view_update_buffer (self);
  photos_image_view_update (self);
  gtk_widget_queue_draw (GTK_WIDGET (self));
}


static void
photos_image_view_invalidated (PhotosImageView *self)
{
  g_return_if_fail (PHOTOS_IS_IMAGE_VIEW (self));
  g_return_if_fail (GEGL_IS_NODE (self->node));

  photos_debug (PHOTOS_DEBUG_GEGL, "PhotosImageView: Node (%p) Invalidated", self->node);
  photos_image_view_update_region (self);
}


static void
photos_image_view_draw_node (PhotosImageView *self, cairo_t *cr, GdkRectangle *rect)
{
  const Babl *format;
  GeglRectangle roi;
  cairo_surface_t *surface = NULL;
  guchar *buf = NULL;
  gint bpp;
  gint scale_factor;
  gint stride;
  gint64 end;
  gint64 start;

  g_return_if_fail (GEGL_IS_BUFFER (self->buffer));
  g_return_if_fail (self->zoom > 0.0);
  g_return_if_fail (self->zoom_scaled > 0.0);

  scale_factor = gtk_widget_get_scale_factor (GTK_WIDGET (self));

  roi.x = (gint) self->x_scaled + rect->x * scale_factor;
  roi.y = (gint) self->y_scaled + rect->y * scale_factor;
  roi.width  = rect->width * scale_factor;
  roi.height = rect->height * scale_factor;

  format = babl_format ("cairo-ARGB32");
  stride = cairo_format_stride_for_width (CAIRO_FORMAT_ARGB32, roi.width);
  buf = g_malloc0_n (stride, roi.height);

  start = g_get_monotonic_time ();

  bpp = babl_format_get_bytes_per_pixel (format);
  stride = bpp * roi.width;
  gegl_buffer_get (self->buffer,
                   &roi,
                   self->zoom_scaled,
                   format,
                   buf,
                   stride,
                   GEGL_ABYSS_NONE);

  end = g_get_monotonic_time ();
  photos_debug (PHOTOS_DEBUG_GEGL,
                "PhotosImageView: Node Blit: %d, %d, %d×%d, %" G_GINT64_FORMAT,
                rect->x,
                rect->y,
                rect->width,
                rect->height,
                end - start);

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
  gint scale_factor;

  GTK_WIDGET_CLASS (photos_image_view_parent_class)->size_allocate (widget, allocation);

  photos_image_view_update (self);

  scale_factor = gtk_widget_get_scale_factor (GTK_WIDGET (self));
  self->allocation_scaled_old.height = allocation->height * scale_factor;
  self->allocation_scaled_old.width = allocation->width * scale_factor;
}


static void
photos_image_view_set_hadjustment (PhotosImageView *self, GtkAdjustment *hadjustment)
{
  if (hadjustment != NULL && self->hadjustment == hadjustment)
    return;

  if (self->hadjustment != NULL)
    g_signal_handlers_disconnect_by_func (self->hadjustment, photos_image_view_adjustment_value_changed, self);

  g_clear_object (&self->hadjustment);

  if (hadjustment == NULL)
    hadjustment = gtk_adjustment_new (0.0, 0.0, 0.0, 0.0, 0.0, 0.0);

  g_signal_connect_object (hadjustment,
                           "value-changed",
                           G_CALLBACK (photos_image_view_adjustment_value_changed),
                           self,
                           0);

  self->hadjustment = g_object_ref_sink (hadjustment);
  photos_image_view_set_hadjustment_values (self);

  g_object_notify (G_OBJECT (self), "hadjustment");
}


static void
photos_image_view_set_hscroll_policy (PhotosImageView *self, GtkScrollablePolicy hscroll_policy)
{
  if (self->hscroll_policy == hscroll_policy)
    return;

  self->hscroll_policy = hscroll_policy;
  gtk_widget_queue_resize (GTK_WIDGET (self));
  g_object_notify (G_OBJECT (self), "hscroll-policy");
}


static void
photos_image_view_set_vadjustment (PhotosImageView *self, GtkAdjustment *vadjustment)
{
  if (vadjustment != NULL && self->vadjustment == vadjustment)
    return;

  if (self->vadjustment != NULL)
    g_signal_handlers_disconnect_by_func (self->vadjustment, photos_image_view_adjustment_value_changed, self);

  g_clear_object (&self->vadjustment);

  if (vadjustment == NULL)
    vadjustment = gtk_adjustment_new (0.0, 0.0, 0.0, 0.0, 0.0, 0.0);

  g_signal_connect_object (vadjustment,
                           "value-changed",
                           G_CALLBACK (photos_image_view_adjustment_value_changed),
                           self,
                           0);

  self->vadjustment = g_object_ref_sink (vadjustment);
  photos_image_view_set_vadjustment_values (self);

  g_object_notify (G_OBJECT (self), "vadjustment");
}


static void
photos_image_view_set_vscroll_policy (PhotosImageView *self, GtkScrollablePolicy vscroll_policy)
{
  if (self->vscroll_policy == vscroll_policy)
    return;

  self->vscroll_policy = vscroll_policy;
  gtk_widget_queue_resize (GTK_WIDGET (self));
  g_object_notify (G_OBJECT (self), "vscroll-policy");
}


static void
photos_image_view_dispose (GObject *object)
{
  PhotosImageView *self = PHOTOS_IMAGE_VIEW (object);

  g_clear_object (&self->buffer);
  g_clear_object (&self->node);
  g_clear_object (&self->hadjustment);
  g_clear_object (&self->vadjustment);

  G_OBJECT_CLASS (photos_image_view_parent_class)->dispose (object);
}


static void
photos_image_view_finalize (GObject *object)
{
  PhotosImageView *self = PHOTOS_IMAGE_VIEW (object);

  g_clear_pointer (&self->bbox_region, (GDestroyNotify) cairo_region_destroy);
  g_clear_pointer (&self->region, (GDestroyNotify) cairo_region_destroy);

  G_OBJECT_CLASS (photos_image_view_parent_class)->finalize (object);
}


static void
photos_image_view_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
  PhotosImageView *self = PHOTOS_IMAGE_VIEW (object);

  switch (prop_id)
    {
    case PROP_BEST_FIT:
      g_value_set_boolean (value, self->best_fit);
      break;

    case PROP_HADJUSTMENT:
      g_value_set_object (value, self->hadjustment);
      break;

    case PROP_HSCROLL_POLICY:
      g_value_set_enum (value, self->hscroll_policy);
      break;

    case PROP_NODE:
      g_value_set_object (value, self->node);
      break;

    case PROP_VADJUSTMENT:
      g_value_set_object (value, self->vadjustment);
      break;

    case PROP_VSCROLL_POLICY:
      g_value_set_enum (value, self->vscroll_policy);
      break;

    case PROP_X:
      g_value_set_double (value, self->x);
      break;

    case PROP_Y:
      g_value_set_double (value, self->y);
      break;

    case PROP_ZOOM:
      g_value_set_double (value, self->zoom);
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
    case PROP_BEST_FIT:
      {
        gboolean best_fit;

        best_fit = g_value_get_boolean (value);
        photos_image_view_set_best_fit (self, best_fit);
        break;
      }

    case PROP_HADJUSTMENT:
      {
        GtkAdjustment *hadjustment;

        hadjustment = GTK_ADJUSTMENT (g_value_get_object (value));
        photos_image_view_set_hadjustment (self, hadjustment);
        break;
      }

    case PROP_HSCROLL_POLICY:
      {
        GtkScrollablePolicy hscroll_policy;

        hscroll_policy = (GtkScrollablePolicy) g_value_get_enum (value);
        photos_image_view_set_hscroll_policy (self, hscroll_policy);
        break;
      }

    case PROP_NODE:
      {
        GeglNode *node;

        node = GEGL_NODE (g_value_get_object (value));
        photos_image_view_set_node (self, node);
        break;
      }

    case PROP_VADJUSTMENT:
      {
        GtkAdjustment *vadjustment;

        vadjustment = GTK_ADJUSTMENT (g_value_get_object (value));
        photos_image_view_set_vadjustment (self, vadjustment);
        break;
      }

    case PROP_VSCROLL_POLICY:
      {
        GtkScrollablePolicy vscroll_policy;

        vscroll_policy = (GtkScrollablePolicy) g_value_get_enum (value);
        photos_image_view_set_vscroll_policy (self, vscroll_policy);
        break;
      }

    case PROP_ZOOM:
      {
        gdouble zoom;

        zoom = g_value_get_double (value);
        photos_image_view_set_zoom (self, zoom);
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
                         | GDK_POINTER_MOTION_MASK
                         | GDK_SCROLL_MASK
                         | GDK_SMOOTH_SCROLL_MASK);

  context = gtk_widget_get_style_context (GTK_WIDGET (self));
  gtk_style_context_add_class (context, GTK_STYLE_CLASS_VIEW);
  gtk_style_context_add_class (context, "content-view");

  self->best_fit = TRUE;
}


static void
photos_image_view_class_init (PhotosImageViewClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->dispose = photos_image_view_dispose;
  object_class->finalize = photos_image_view_finalize;
  object_class->get_property = photos_image_view_get_property;
  object_class->set_property = photos_image_view_set_property;
  widget_class->draw = photos_image_view_draw;
  widget_class->size_allocate = photos_image_view_size_allocate;

  g_object_class_install_property (object_class,
                                   PROP_BEST_FIT,
                                   g_param_spec_boolean ("best-fit",
                                                         "Best Fit",
                                                         "Zoom and center the GeglNode to fit the widget's size",
                                                         TRUE,
                                                         G_PARAM_CONSTRUCT
                                                         | G_PARAM_EXPLICIT_NOTIFY
                                                         | G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_NODE,
                                   g_param_spec_object ("node",
                                                        "GeglNode object",
                                                        "The GeglNode to render",
                                                        GEGL_TYPE_NODE,
                                                        G_PARAM_CONSTRUCT
                                                        | G_PARAM_EXPLICIT_NOTIFY
                                                        | G_PARAM_READWRITE));

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
                                   g_param_spec_double ("zoom",
                                                        "Zoom",
                                                        "Zoom factor",
                                                        G_MINDOUBLE,
                                                        G_MAXDOUBLE,
                                                        1.0,
                                                        G_PARAM_EXPLICIT_NOTIFY | G_PARAM_READWRITE));

  signals[DRAW_BACKGROUND] = g_signal_new ("draw-background",
                                           G_TYPE_FROM_CLASS (class),
                                           G_SIGNAL_RUN_LAST,
                                           0,
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
                                        0,
                                        NULL, /* accumulator */
                                        NULL, /* accu_data */
                                        _photos_marshal_VOID__BOXED_BOXED,
                                        G_TYPE_NONE,
                                        2,
                                        CAIRO_GOBJECT_TYPE_CONTEXT,
                                        GDK_TYPE_RECTANGLE);

  g_object_class_override_property (object_class, PROP_HADJUSTMENT, "hadjustment");
  g_object_class_override_property (object_class, PROP_HSCROLL_POLICY, "hscroll-policy");
  g_object_class_override_property (object_class, PROP_VADJUSTMENT, "vadjustment");
  g_object_class_override_property (object_class, PROP_VSCROLL_POLICY, "vscroll-policy");
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


gboolean
photos_image_view_get_best_fit (PhotosImageView *self)
{
  g_return_val_if_fail (PHOTOS_IS_IMAGE_VIEW (self), FALSE);
  return self->best_fit;
}


GeglNode *
photos_image_view_get_node (PhotosImageView *self)
{
  g_return_val_if_fail (PHOTOS_IS_IMAGE_VIEW (self), NULL);
  return self->node;
}


gdouble
photos_image_view_get_x (PhotosImageView *self)
{
  g_return_val_if_fail (PHOTOS_IS_IMAGE_VIEW (self), 0.0);
  return self->x;
}


gdouble
photos_image_view_get_y (PhotosImageView *self)
{
  g_return_val_if_fail (PHOTOS_IS_IMAGE_VIEW (self), 0.0);
  return self->y;
}


gdouble
photos_image_view_get_zoom (PhotosImageView *self)
{
  g_return_val_if_fail (PHOTOS_IS_IMAGE_VIEW (self), 0.0);
  return self->zoom;
}


void
photos_image_view_set_best_fit (PhotosImageView *self, gboolean best_fit)
{
  g_return_if_fail (PHOTOS_IS_IMAGE_VIEW (self));

  if (self->best_fit == best_fit)
    return;

  self->best_fit = best_fit;

  if (self->best_fit)
    {
      self->zoom = 0.0;
      self->zoom_scaled = 0.0;
      gtk_widget_queue_resize (GTK_WIDGET (self));
      g_object_notify (G_OBJECT (self), "zoom");
    }

  g_object_notify (G_OBJECT (self), "best-fit");
}


void
photos_image_view_set_node (PhotosImageView *self, GeglNode *node)
{
  g_return_if_fail (PHOTOS_IS_IMAGE_VIEW (self));
  g_return_if_fail (node == NULL || GEGL_IS_NODE (node));

  if (self->node == node)
    return;

  if (self->node != NULL)
    {
      g_signal_handlers_disconnect_by_func (self->node, photos_image_view_computed, self);
      g_signal_handlers_disconnect_by_func (self->node, photos_image_view_invalidated, self);
    }

  self->allocation_scaled_old.height = 0;
  self->allocation_scaled_old.width = 0;
  self->best_fit = TRUE;
  self->height = 0.0;
  self->width = 0.0;
  self->x = 0.0;
  self->x_scaled = 0.0;
  self->y = 0.0;
  self->y_scaled = 0.0;
  self->zoom = 0.0;
  self->zoom_scaled = 0.0;
  g_clear_object (&self->buffer);
  g_clear_object (&self->node);
  g_clear_pointer (&self->bbox_region, (GDestroyNotify) cairo_region_destroy);
  g_clear_pointer (&self->region, (GDestroyNotify) cairo_region_destroy);

  if (node != NULL)
    {
      self->node = g_object_ref (node);

      photos_image_view_update_region (self);
      photos_image_view_update_buffer (self);

      g_signal_connect_object (node, "computed", G_CALLBACK (photos_image_view_computed), self, G_CONNECT_SWAPPED);
      g_signal_connect_object (node,
                               "invalidated",
                               G_CALLBACK (photos_image_view_invalidated),
                               self,
                               G_CONNECT_SWAPPED);
    }

  gtk_widget_queue_resize (GTK_WIDGET (self));

  g_object_notify (G_OBJECT (self), "best-fit");
  g_object_notify (G_OBJECT (self), "node");
  g_object_notify (G_OBJECT (self), "zoom");
}


void
photos_image_view_set_zoom (PhotosImageView *self, gdouble zoom)
{
  gint scale_factor;

  g_return_if_fail (PHOTOS_IS_IMAGE_VIEW (self));

  if (photos_utils_equal_double (self->zoom, zoom))
    return;

  self->best_fit = FALSE;
  self->zoom = zoom;

  scale_factor = gtk_widget_get_scale_factor (GTK_WIDGET (self));
  self->zoom_scaled = self->zoom * scale_factor;

  gtk_widget_queue_resize (GTK_WIDGET (self));

  g_object_notify (G_OBJECT (self), "best-fit");
  g_object_notify (G_OBJECT (self), "zoom");
}
