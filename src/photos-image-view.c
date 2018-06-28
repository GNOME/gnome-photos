/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2015 – 2018 Red Hat, Inc.
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

#include <babl/babl.h>
#include <cairo.h>
#include <cairo-gobject.h>
#include <dazzle.h>
#include <glib.h>

#include "photos-debug.h"
#include "photos-gegl.h"
#include "photos-image-view.h"
#include "photos-image-view-helper.h"
#include "photos-marshalers.h"
#include "photos-utils.h"


struct _PhotosImageView
{
  GtkDrawingArea parent_instance;
  DzlAnimation *zoom_animation;
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
  gdouble zoom_visible;
  gdouble zoom_visible_scaled;
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


static const DzlAnimationMode ZOOM_ANIMATION_MODE = DZL_ANIMATION_EASE_OUT_CUBIC;
static const guint ZOOM_ANIMATION_DURATION = 250; /* ms */


static void photos_image_view_computed (PhotosImageView *self, GeglRectangle *rect);


static gboolean
photos_image_view_has_allocation_and_extent (PhotosImageView *self)
{
  GeglRectangle bbox;
  GtkAllocation allocation;
  gboolean ret_val = FALSE;

  if (self->buffer == NULL)
    goto out;

  gtk_widget_get_allocation (GTK_WIDGET (self), &allocation);
  if (allocation.width <= 0 || allocation.height <= 0)
    goto out;

  bbox = *gegl_buffer_get_extent (self->buffer);
  if (bbox.width < 0 || bbox.height < 0)
    goto out;

  ret_val = TRUE;

 out:
  return ret_val;
}


static void
photos_image_view_calculate_best_fit_zoom (PhotosImageView *self, gdouble *out_zoom, gdouble *out_zoom_scaled)
{
  GeglRectangle bbox;
  GtkAllocation allocation;
  gdouble zoom;
  gdouble zoom_scaled = 1.0;
  gint allocation_height_scaled;
  gint allocation_width_scaled;
  gint scale_factor;

  if (!photos_image_view_has_allocation_and_extent (self))
    goto out;

  gtk_widget_get_allocation (GTK_WIDGET (self), &allocation);
  bbox = *gegl_buffer_get_extent (self->buffer);

  scale_factor = gtk_widget_get_scale_factor (GTK_WIDGET (self));
  allocation_height_scaled = allocation.height * scale_factor;
  allocation_width_scaled = allocation.width * scale_factor;

  if (bbox.height > allocation_height_scaled || bbox.width > allocation_width_scaled)
    {
      gdouble height_ratio = bbox.height / (gdouble) allocation_height_scaled;
      gdouble width_ratio = bbox.width / (gdouble) allocation_width_scaled;
      gdouble max_ratio =  MAX (height_ratio, width_ratio);

      zoom_scaled = 1.0 / max_ratio;
    }

 out:
  zoom = zoom_scaled / (gdouble) scale_factor;

  if (out_zoom != NULL)
    *out_zoom = zoom;

  if (out_zoom_scaled != NULL)
    *out_zoom_scaled = zoom_scaled;
}


static gboolean
photos_image_view_needs_zoom_animation (PhotosImageView *self)
{
  gboolean ret_val = FALSE;

  if (!photos_image_view_has_allocation_and_extent (self))
    goto out;

  ret_val = TRUE;

 out:
  return ret_val;
}


static void
photos_image_view_notify_zoom (GObject *object, GParamSpec *pspec, gpointer user_data)
{
  PhotosImageView *self = PHOTOS_IMAGE_VIEW (user_data);
  PhotosImageViewHelper *helper = PHOTOS_IMAGE_VIEW_HELPER (object);
  gint scale_factor;

  g_return_if_fail (DZL_IS_ANIMATION (self->zoom_animation));

  self->zoom_visible = photos_image_view_helper_get_zoom (helper);

  scale_factor = gtk_widget_get_scale_factor (GTK_WIDGET (self));
  self->zoom_visible_scaled = self->zoom_visible * scale_factor;

  gtk_widget_queue_resize (GTK_WIDGET (self));
}


static void
photos_image_view_start_zoom_animation (PhotosImageView *self)
{
  GdkFrameClock *frame_clock;
  g_autoptr (PhotosImageViewHelper) helper = NULL;

  g_return_if_fail (photos_image_view_needs_zoom_animation (self));
  g_return_if_fail (self->zoom > 0.0);
  g_return_if_fail (self->zoom_animation == NULL);
  g_return_if_fail (self->zoom_visible > 0.0);

  helper = photos_image_view_helper_new ();
  photos_image_view_helper_set_zoom (helper, self->zoom_visible);
  g_signal_connect (helper, "notify::zoom", G_CALLBACK (photos_image_view_notify_zoom), self);

  frame_clock = gtk_widget_get_frame_clock (GTK_WIDGET (self));

  self->zoom_animation = dzl_object_animate_full (g_object_ref (helper),
                                                  ZOOM_ANIMATION_MODE,
                                                  ZOOM_ANIMATION_DURATION,
                                                  frame_clock,
                                                  g_object_unref,
                                                  helper,
                                                  "zoom", self->zoom,
                                                  NULL);
  g_object_add_weak_pointer (G_OBJECT (self->zoom_animation), (gpointer *) &self->zoom_animation);
}


static void
photos_image_view_update_buffer (PhotosImageView *self)
{
  const Babl *format;
  g_autoptr (GeglBuffer) buffer = NULL;

  g_signal_handlers_block_by_func (self->node, photos_image_view_computed, self);

  format = babl_format ("cairo-ARGB32");
  buffer = photos_gegl_dup_buffer_from_node (self->node, format);
  g_set_object (&self->buffer, buffer);

  g_signal_handlers_unblock_by_func (self->node, photos_image_view_computed, self);
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
  gint allocation_height_scaled;
  gint allocation_width_scaled;
  gint scale_factor;

  g_object_freeze_notify (G_OBJECT (self));

  if (self->node == NULL)
    goto out;

  if (!photos_image_view_has_allocation_and_extent (self))
    goto out;

  if (self->best_fit)
    {
      gdouble zoom;
      gdouble zoom_scaled;

      photos_image_view_calculate_best_fit_zoom (self, &zoom, &zoom_scaled);

      if (!photos_utils_equal_double (self->zoom, zoom))
        {
          self->zoom = zoom;
          g_object_notify (G_OBJECT (self), "zoom");

          if (self->zoom_animation != NULL)
            {
              dzl_animation_stop (self->zoom_animation);
              photos_image_view_start_zoom_animation (self);
              goto out;
            }

          self->zoom_visible = self->zoom;
          self->zoom_visible_scaled = zoom_scaled;
        }
    }

  gtk_widget_get_allocation (GTK_WIDGET (self), &allocation);
  bbox = *gegl_buffer_get_extent (self->buffer);
  scale_factor = gtk_widget_get_scale_factor (GTK_WIDGET (self));

  allocation_height_scaled = allocation.height * scale_factor;
  allocation_width_scaled = allocation.width * scale_factor;

  if (self->best_fit && self->zoom_animation == NULL)
    {
      bbox_zoomed.width = (gint) (self->zoom_visible_scaled * bbox.width + 0.5);
      bbox_zoomed.height = (gint) (self->zoom_visible_scaled * bbox.height + 0.5);
      bbox_zoomed.x = (gint) (self->zoom_visible_scaled * bbox.x + 0.5);
      bbox_zoomed.y = (gint) (self->zoom_visible_scaled * bbox.y + 0.5);

      self->x_scaled = (bbox_zoomed.width - allocation_width_scaled) / 2.0;
      self->y_scaled = (bbox_zoomed.height - allocation_height_scaled) / 2.0;
    }
  else
    {
      gdouble ratio_old;

      bbox_zoomed.width = (gint) (self->zoom_visible_scaled * bbox.width + 0.5);
      bbox_zoomed.height = (gint) (self->zoom_visible_scaled * bbox.height + 0.5);
      bbox_zoomed.x = (gint) (self->zoom_visible_scaled * bbox.x + 0.5);
      bbox_zoomed.y = (gint) (self->zoom_visible_scaled * bbox.y + 0.5);

      if (bbox_zoomed.width > allocation_width_scaled)
        {
          ratio_old = (self->x_scaled
                       + self->allocation_scaled_old.width / 2.0
                       - (gdouble) self->bbox_zoomed_old.x)
                      / (gdouble) self->bbox_zoomed_old.width;
          self->x_scaled = ratio_old * (gdouble) bbox_zoomed.width - (allocation_width_scaled / 2.0);
          self->x_scaled = CLAMP (self->x_scaled, 0.0, (gdouble) bbox_zoomed.width - allocation_width_scaled);
        }
      else
        {
          self->x_scaled = (bbox_zoomed.width - allocation_width_scaled) / 2.0;
        }

      if (bbox_zoomed.height > allocation_height_scaled)
        {
          ratio_old = (self->y_scaled
                       + self->allocation_scaled_old.height / 2.0
                       - (gdouble) self->bbox_zoomed_old.y)
                      / (gdouble) self->bbox_zoomed_old.height;
          self->y_scaled = ratio_old * (gdouble) bbox_zoomed.height - (allocation_height_scaled / 2.0);
          self->y_scaled = CLAMP (self->y_scaled, 0.0, (gdouble) bbox_zoomed.height - allocation_height_scaled);
        }
      else
        {
          self->y_scaled = (bbox_zoomed.height - allocation_height_scaled) / 2.0;
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
  g_autofree guchar *buf = NULL;
  gint scale_factor;
  gint stride;
  gint64 end;
  gint64 start;

  g_return_if_fail (GEGL_IS_BUFFER (self->buffer));
  g_return_if_fail (self->zoom_visible > 0.0);
  g_return_if_fail (self->zoom_visible_scaled > 0.0);

  scale_factor = gtk_widget_get_scale_factor (GTK_WIDGET (self));

  roi.x = (gint) self->x_scaled + rect->x * scale_factor;
  roi.y = (gint) self->y_scaled + rect->y * scale_factor;
  roi.width  = rect->width * scale_factor;
  roi.height = rect->height * scale_factor;

  format = babl_format ("cairo-ARGB32");
  stride = cairo_format_stride_for_width (CAIRO_FORMAT_ARGB32, roi.width);
  buf = g_malloc0_n (stride, roi.height);

  start = g_get_monotonic_time ();

  gegl_buffer_get (self->buffer,
                   &roi,
                   self->zoom_visible_scaled,
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

  if (self->zoom_animation != NULL)
    {
      dzl_animation_stop (self->zoom_animation);
      g_assert_null (self->zoom_animation);
    }

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
        photos_image_view_set_best_fit (self, best_fit, TRUE);
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
        photos_image_view_set_zoom (self, zoom, TRUE);
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
  self->zoom = 1.0;
  self->zoom_visible = 1.0;
  self->zoom_visible_scaled = 1.0;
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
photos_image_view_set_best_fit (PhotosImageView *self, gboolean best_fit, gboolean enable_animation)
{
  g_return_if_fail (PHOTOS_IS_IMAGE_VIEW (self));

  if (self->best_fit == best_fit)
    return;

  self->best_fit = best_fit;

  if (self->best_fit)
    {
      gdouble zoom_scaled;

      if (self->zoom_animation != NULL)
        dzl_animation_stop (self->zoom_animation);

      photos_image_view_calculate_best_fit_zoom (self, &self->zoom, &zoom_scaled);

      if (enable_animation && photos_image_view_needs_zoom_animation (self))
        {
          photos_image_view_start_zoom_animation (self);
        }
      else
        {
          self->zoom_visible = self->zoom;
          self->zoom_visible_scaled = zoom_scaled;
          gtk_widget_queue_resize (GTK_WIDGET (self));
        }

      g_object_notify (G_OBJECT (self), "zoom");
    }

  g_object_notify (G_OBJECT (self), "best-fit");
}


void
photos_image_view_set_node (PhotosImageView *self, GeglNode *node)
{
  gdouble zoom_scaled;

  g_return_if_fail (PHOTOS_IS_IMAGE_VIEW (self));
  g_return_if_fail (node == NULL || GEGL_IS_NODE (node));

  if (self->node == node)
    return;

  if (self->zoom_animation != NULL)
    dzl_animation_stop (self->zoom_animation);

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
  self->zoom = 1.0;
  self->zoom_visible = 1.0;
  self->zoom_visible_scaled = 1.0;
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

  photos_image_view_calculate_best_fit_zoom (self, &self->zoom, &zoom_scaled);
  self->zoom_visible = self->zoom;
  self->zoom_visible_scaled = zoom_scaled;

  gtk_widget_queue_resize (GTK_WIDGET (self));

  g_object_notify (G_OBJECT (self), "best-fit");
  g_object_notify (G_OBJECT (self), "node");
  g_object_notify (G_OBJECT (self), "zoom");
}


void
photos_image_view_set_zoom (PhotosImageView *self, gdouble zoom, gboolean enable_animation)
{
  g_return_if_fail (PHOTOS_IS_IMAGE_VIEW (self));
  g_return_if_fail (zoom > 0.0);

  if (photos_utils_equal_double (self->zoom, zoom))
    return;

  if (self->zoom_animation != NULL)
    dzl_animation_stop (self->zoom_animation);

  self->best_fit = FALSE;
  self->zoom = zoom;

  if (enable_animation && photos_image_view_needs_zoom_animation (self))
    {
      photos_image_view_start_zoom_animation (self);
    }
  else
    {
      gint scale_factor;

      self->zoom_visible = self->zoom;

      scale_factor = gtk_widget_get_scale_factor (GTK_WIDGET (self));
      self->zoom_visible_scaled = self->zoom_visible * scale_factor;

      gtk_widget_queue_resize (GTK_WIDGET (self));
    }

  g_object_notify (G_OBJECT (self), "best-fit");
  g_object_notify (G_OBJECT (self), "zoom");
}
