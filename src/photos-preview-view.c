/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2013 Red Hat, Inc.
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

#include <glib.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "gegl-gtk-view.h"
#include "photos-mode-controller.h"
#include "photos-preview-nav-buttons.h"
#include "photos-preview-view.h"


struct _PhotosPreviewViewPrivate
{
  GeglNode *node;
  GtkWidget *overlay;
  GtkWidget *view;
  PhotosModeController *mode_cntrlr;
  PhotosPreviewNavButtons *nav_buttons;
};

enum
{
  PROP_0,
  PROP_OVERLAY
};


G_DEFINE_TYPE_WITH_PRIVATE (PhotosPreviewView, photos_preview_view, GTK_TYPE_SCROLLED_WINDOW);


static void
photos_preview_view_draw_background (PhotosPreviewView *self, cairo_t *cr, GdkRectangle *rect)
{
  PhotosPreviewViewPrivate *priv = self->priv;
  GtkStyleContext *context;
  GtkStateFlags flags;

  context = gtk_widget_get_style_context (priv->view);
  flags = gtk_widget_get_state_flags (priv->view);
  gtk_style_context_save (context);
  gtk_style_context_set_state (context, flags);
  gtk_render_background (context, cr, 0, 0, rect->width, rect->height);
  gtk_style_context_restore (context);
}


static void
photos_preview_view_scale_and_align_image (PhotosPreviewView *self)
{
  PhotosPreviewViewPrivate *priv = self->priv;
  GeglRectangle bbox;
  GtkAllocation alloc;
  float delta_x;
  float delta_y;
  float scale = 1.0;

  /* Reset these properties, otherwise values from the previous node
   * will interfere with the current one.
   */
  gegl_gtk_view_set_autoscale_policy (GEGL_GTK_VIEW (priv->view), GEGL_GTK_VIEW_AUTOSCALE_DISABLED);
  gegl_gtk_view_set_scale (GEGL_GTK_VIEW (priv->view), 1.0);
  gegl_gtk_view_set_x (GEGL_GTK_VIEW (priv->view), 0.0);
  gegl_gtk_view_set_y (GEGL_GTK_VIEW (priv->view), 0.0);

  bbox = gegl_node_get_bounding_box (priv->node);
  gtk_widget_get_allocation (priv->view, &alloc);

  if (bbox.width > alloc.width || bbox.height > alloc.height)
    {
      float height_ratio;
      float max_ratio;
      float width_ratio;

      gegl_gtk_view_set_autoscale_policy (GEGL_GTK_VIEW (priv->view), GEGL_GTK_VIEW_AUTOSCALE_CONTENT);

      /* TODO: since gegl_gtk_view_get_scale is not giving the
       *       correct value of scale, we calculate it ourselves.
       */
      height_ratio = (float) bbox.height / alloc.height;
      width_ratio = (float) bbox.width / alloc.width;
      max_ratio = width_ratio >= height_ratio ? width_ratio : height_ratio;
      scale = 1.0 / max_ratio;

      bbox.width = (gint) (scale * bbox.width + 0.5);
      bbox.height = (gint) (scale * bbox.height + 0.5);
    }


  /* At this point, alloc is definitely bigger than bbox. */
  delta_x = (alloc.width - bbox.width) / 2.0;
  delta_y = (alloc.height - bbox.height) / 2.0;
  gegl_gtk_view_set_x (GEGL_GTK_VIEW (priv->view), -delta_x);
  gegl_gtk_view_set_y (GEGL_GTK_VIEW (priv->view), -delta_y);
}


static void
photos_preview_view_window_mode_changed (PhotosPreviewView *self, PhotosWindowMode mode, PhotosWindowMode old_mode)
{
  if (mode != PHOTOS_WINDOW_MODE_PREVIEW)
    photos_preview_nav_buttons_hide (self->priv->nav_buttons);
}


static void
photos_preview_view_dispose (GObject *object)
{
  PhotosPreviewView *self = PHOTOS_PREVIEW_VIEW (object);
  PhotosPreviewViewPrivate *priv = self->priv;

  g_clear_object (&priv->node);
  g_clear_object (&priv->mode_cntrlr);

  G_OBJECT_CLASS (photos_preview_view_parent_class)->dispose (object);
}


static void
photos_preview_view_constructed (GObject *object)
{
  PhotosPreviewView *self = PHOTOS_PREVIEW_VIEW (object);
  PhotosPreviewViewPrivate *priv = self->priv;

  G_OBJECT_CLASS (photos_preview_view_parent_class)->constructed (object);

  /* Add the view to the scrolled window after the default
   * adjustments have been created.
   */
  gtk_container_add (GTK_CONTAINER (self), priv->view);

  priv->nav_buttons = photos_preview_nav_buttons_new (self, GTK_OVERLAY (priv->overlay));

  gtk_widget_show_all (GTK_WIDGET (self));
}


static void
photos_preview_view_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
  PhotosPreviewView *self = PHOTOS_PREVIEW_VIEW (object);
  PhotosPreviewViewPrivate *priv = self->priv;

  switch (prop_id)
    {
    case PROP_OVERLAY:
      priv->overlay = GTK_WIDGET (g_value_dup_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
photos_preview_view_init (PhotosPreviewView *self)
{
  PhotosPreviewViewPrivate *priv;
  GtkStyleContext *context;

  self->priv = photos_preview_view_get_instance_private (self);
  priv = self->priv;

  priv->mode_cntrlr = photos_mode_controller_new ();
  g_signal_connect_swapped (priv->mode_cntrlr,
                            "window-mode-changed",
                            G_CALLBACK (photos_preview_view_window_mode_changed),
                            self);

  gtk_widget_set_hexpand (GTK_WIDGET (self), TRUE);
  gtk_widget_set_vexpand (GTK_WIDGET (self), TRUE);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (self), GTK_SHADOW_IN);
  context = gtk_widget_get_style_context (GTK_WIDGET (self));
  gtk_style_context_add_class (context, "documents-scrolledwin");

  priv->view = GTK_WIDGET (gegl_gtk_view_new ());
  gtk_widget_add_events (priv->view, GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK);
  context = gtk_widget_get_style_context (priv->view);
  gtk_style_context_add_class (context, GTK_STYLE_CLASS_VIEW);
  gtk_style_context_add_class (context, "content-view");
  g_signal_connect_swapped (priv->view,
                            "size-allocate",
                            G_CALLBACK (photos_preview_view_scale_and_align_image),
                            self);
  g_signal_connect_swapped (priv->view,
                            "draw-background",
                            G_CALLBACK (photos_preview_view_draw_background),
                            self);
}


static void
photos_preview_view_class_init (PhotosPreviewViewClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->constructed = photos_preview_view_constructed;
  object_class->dispose = photos_preview_view_dispose;
  object_class->set_property = photos_preview_view_set_property;

  g_object_class_install_property (object_class,
                                   PROP_OVERLAY,
                                   g_param_spec_object ("overlay",
                                                        "GtkOverlay object",
                                                        "The stack overlay widget",
                                                        GTK_TYPE_OVERLAY,
                                                        G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE));
}


GtkWidget *
photos_preview_view_new (GtkOverlay *overlay)
{
  return g_object_new (PHOTOS_TYPE_PREVIEW_VIEW, "overlay", overlay, NULL);
}


void
photos_preview_view_set_model (PhotosPreviewView *self, GtkTreeModel *model, GtkTreePath *current_path)
{
  photos_preview_nav_buttons_set_model (self->priv->nav_buttons, model, current_path);
}


void
photos_preview_view_set_node (PhotosPreviewView *self, GeglNode *node)
{
  PhotosPreviewViewPrivate *priv = self->priv;

  if (priv->node == node)
    goto out;

  g_clear_object (&priv->node);
  if (node == NULL)
    goto out;

  priv->node = g_object_ref (node);
  photos_preview_view_scale_and_align_image (self);

  /* Steals the reference to the GeglNode. */
  gegl_gtk_view_set_node (GEGL_GTK_VIEW (priv->view), g_object_ref (priv->node));
  gtk_widget_queue_draw (priv->view);

 out:
  photos_preview_nav_buttons_show (priv->nav_buttons);
}
