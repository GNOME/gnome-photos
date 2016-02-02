/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2016 Red Hat, Inc.
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

#include "egg-animation.h"
#include "photos-image-container.h"
#include "photos-image-view.h"
#include "photos-utils.h"


struct _PhotosImageContainer
{
  GtkBin parent_instance;
  GeglNode *crop;
  GeglNode *buffer_source;
  GeglNode *graph;
  GeglNode *graph_anim;
  GeglNode *node;
  GeglRectangle bbox;
  GtkWidget *stack;
  GtkWidget *view;
  GtkWidget *view_anim;
  cairo_region_t *bbox_region;
  cairo_region_t *region;
  guint computed_id;
};

struct _PhotosImageContainerClass
{
  GtkBinClass parent_class;
};

enum
{
  PROP_0,
  PROP_NODE,
  PROP_VIEW
};


G_DEFINE_TYPE (PhotosImageContainer, photos_image_container, GTK_TYPE_BIN);


static void photos_image_container_computed (PhotosImageContainer *self, GeglRectangle *rect);


static void
photos_image_container_invalidated (PhotosImageContainer *self, GeglRectangle *rect)
{
  GeglRectangle bbox;

  g_clear_pointer (&self->bbox_region, (GDestroyNotify) cairo_region_destroy);
  g_clear_pointer (&self->region, (GDestroyNotify) cairo_region_destroy);

  self->region = cairo_region_create ();
  self->bbox_region = cairo_region_create_rectangle ((cairo_rectangle_int_t *) &bbox);
}


static void
photos_image_container_notify_transition_running (PhotosImageContainer *self)
{
  g_signal_handlers_disconnect_by_func (self->stack,
                                        photos_image_container_notify_transition_running,
                                        self->view_anim);

  if (!gtk_stack_get_transition_running (GTK_STACK (self->stack)))
    g_clear_pointer (&self->view_anim, (GDestroyNotify) gtk_widget_destroy);
}


static void
photos_image_container_crossfade (PhotosImageContainer *self)
{
  const Babl *format;
  GeglBuffer *buffer;
  GeglBuffer *buffer_orig;

  g_clear_object (&self->graph_anim);
  self->graph_anim = gegl_node_new ();

  g_object_ref (self->buffer_source);
  gegl_node_remove_child (self->graph, self->buffer_source);
  gegl_node_add_child (self->graph_anim, self->buffer_source);
  g_object_unref (self->buffer_source);

  self->view_anim = photos_image_view_new_from_node (self->buffer_source);
  gtk_container_add (GTK_CONTAINER (self->stack), self->view_anim);
  gtk_widget_show (self->view_anim);
  gtk_stack_set_visible_child (GTK_STACK (self->stack), self->view_anim);

  g_clear_object (&self->graph);
  g_signal_handlers_block_by_func (self->node, photos_image_container_computed, self);

  format = babl_format ("cairo-ARGB32");
  buffer_orig = photos_utils_create_buffer_from_node (self->node, format);
  buffer = gegl_buffer_dup (buffer_orig);
  self->graph = gegl_node_new ();
  self->buffer_source = gegl_node_new_child (self->graph,
                                             "operation", "gegl:buffer-source",
                                             "buffer", buffer,
                                             NULL);
  gegl_node_process (self->buffer_source);
  photos_image_view_set_node (PHOTOS_IMAGE_VIEW (self->view), self->buffer_source);
  gtk_stack_set_visible_child_full (GTK_STACK (self->stack), "view", GTK_STACK_TRANSITION_TYPE_CROSSFADE);
  g_signal_connect_swapped (self->stack,
                            "notify::transition-running",
                            G_CALLBACK (photos_image_container_notify_transition_running),
                            self);

  g_signal_handlers_unblock_by_func (self->node, photos_image_container_computed, self);

  g_object_unref (buffer);
  g_object_unref (buffer_orig);
}


static void
photos_image_container_notify_animation (PhotosImageContainer *self)
{
  g_message ("photos_image_container_notify_animation");
  gtk_stack_set_visible_child_name (GTK_STACK (self->stack), "view");
  g_clear_pointer (&self->view_anim, (GDestroyNotify) gtk_widget_destroy);
  gtk_widget_queue_draw (GTK_WIDGET (self->view));
}


static void
photos_image_container_rubber_band_tick (PhotosImageContainer *self)
{
  g_message ("photos_image_container_rubber_band_tick: %p", self->crop);
  gegl_node_process (self->crop);
  photos_image_view_set_node (PHOTOS_IMAGE_VIEW (self->view_anim), self->crop);
  gtk_widget_queue_draw (GTK_WIDGET (self->view));
}


static void
photos_image_container_rubber_band_contract (PhotosImageContainer *self)
{
  const Babl *format;
  EggAnimation *animation;
  GdkFrameClock *frame_clock;
  GeglBuffer *buffer;
  GeglBuffer *buffer_orig;
  GeglOperation *op;
  GeglRectangle bbox;
  guint transition_duration;

  g_clear_object (&self->graph_anim);
  self->graph_anim = gegl_node_new ();

  g_object_ref (self->buffer_source);
  gegl_node_remove_child (self->graph, self->buffer_source);
  gegl_node_add_child (self->graph_anim, self->buffer_source);
  g_object_unref (self->buffer_source);

  self->crop = gegl_node_new_child (self->graph_anim,
                                    "operation", "gegl:crop",
                                    "height", (gdouble) self->bbox.height,
                                    "width", (gdouble) self->bbox.width,
                                    "x", (gdouble) self->bbox.x,
                                    "y", (gdouble) self->bbox.y,
                                    NULL);

  gegl_node_link (self->buffer_source, self->crop);
  gegl_node_process (self->crop);

  self->view_anim = photos_image_view_new_from_node (self->crop);
  gtk_container_add (GTK_CONTAINER (self->stack), self->view_anim);
  gtk_widget_show (self->view_anim);
  gtk_stack_set_visible_child (GTK_STACK (self->stack), self->view_anim);

  g_clear_object (&self->graph);
  g_signal_handlers_block_by_func (self->node, photos_image_container_computed, self);

  format = babl_format ("cairo-ARGB32");
  buffer_orig = photos_utils_create_buffer_from_node (self->node, format);
  buffer = gegl_buffer_dup (buffer_orig);
  self->graph = gegl_node_new ();
  self->buffer_source = gegl_node_new_child (self->graph,
                                             "operation", "gegl:buffer-source",
                                             "buffer", buffer,
                                             NULL);
  gegl_node_process (self->buffer_source);
  photos_image_view_set_node (PHOTOS_IMAGE_VIEW (self->view), self->buffer_source);

  g_signal_handlers_unblock_by_func (self->node, photos_image_container_computed, self);

  bbox = gegl_node_get_bounding_box (self->node);
  g_message ("photos_image_container_rubber_band_contract: %d %d %d %d",
             bbox.x, bbox.y, bbox.width, bbox.height);

  op = gegl_node_get_gegl_operation (self->crop);
  frame_clock = gtk_widget_get_frame_clock (self->stack);
  transition_duration = gtk_stack_get_transition_duration (GTK_STACK (self->stack));
  animation = egg_object_animate_full (op,
                                       EGG_ANIMATION_EASE_OUT_CUBIC,
                                       transition_duration,
                                       frame_clock,
                                       NULL,
                                       self,
                                       "height", (gdouble) bbox.height,
                                       "width", (gdouble) bbox.width,
                                       "x", (gdouble) bbox.x,
                                       "y", (gdouble) bbox.y,
                                       NULL);

  g_signal_connect_swapped (animation, "tick", G_CALLBACK (photos_image_container_rubber_band_tick), self);
  g_object_weak_ref (G_OBJECT (animation), (GWeakNotify) photos_image_container_notify_animation, self);

  self->bbox = bbox;

  g_object_unref (buffer);
  g_object_unref (buffer_orig);
}


static void
photos_image_container_rubber_band_expand (PhotosImageContainer *self)
{
  const Babl *format;
  EggAnimation *animation;
  GdkFrameClock *frame_clock;
  GeglBuffer *buffer;
  GeglBuffer *buffer_orig;
  GeglOperation *op;
  GeglRectangle bbox;
  guint transition_duration;

  g_clear_object (&self->graph);
  g_clear_object (&self->graph_anim);
  g_signal_handlers_block_by_func (self->node, photos_image_container_computed, self);

  format = babl_format ("cairo-ARGB32");
  buffer_orig = photos_utils_create_buffer_from_node (self->node, format);
  buffer = gegl_buffer_dup (buffer_orig);

  self->graph_anim = gegl_node_new ();
  self->buffer_source = gegl_node_new_child (self->graph_anim,
                                             "operation", "gegl:buffer-source",
                                             "buffer", buffer,
                                             NULL);
  self->crop = gegl_node_new_child (self->graph_anim,
                                    "operation", "gegl:crop",
                                    "height", (gdouble) self->bbox.height,
                                    "width", (gdouble) self->bbox.width,
                                    "x", (gdouble) self->bbox.x,
                                    "y", (gdouble) self->bbox.y,
                                    NULL);
  gegl_node_link (self->buffer_source, self->crop);
  gegl_node_process (self->crop);

  self->view_anim = photos_image_view_new_from_node (self->crop);
  gtk_container_add (GTK_CONTAINER (self->stack), self->view_anim);
  gtk_widget_show (self->view_anim);
  gtk_stack_set_visible_child (GTK_STACK (self->stack), self->view_anim);

  self->graph = gegl_node_new ();
  self->buffer_source = gegl_node_new_child (self->graph,
                                             "operation", "gegl:buffer-source",
                                             "buffer", buffer,
                                             NULL);
  gegl_node_process (self->buffer_source);
  photos_image_view_set_node (PHOTOS_IMAGE_VIEW (self->view), self->buffer_source);

  g_signal_handlers_unblock_by_func (self->node, photos_image_container_computed, self);

  bbox = gegl_node_get_bounding_box (self->node);
  g_message ("photos_image_container_rubber_band_expand: %d %d %d %d", bbox.x, bbox.y, bbox.width, bbox.height);

  op = gegl_node_get_gegl_operation (self->crop);
  frame_clock = gtk_widget_get_frame_clock (self->stack);
  transition_duration = gtk_stack_get_transition_duration (GTK_STACK (self->stack)) * 10;
  animation = egg_object_animate_full (op,
                                       EGG_ANIMATION_EASE_OUT_CUBIC,
                                       transition_duration,
                                       frame_clock,
                                       NULL,
                                       self,
                                       "height", (gdouble) bbox.height,
                                       "width", (gdouble) bbox.width,
                                       "x", (gdouble) bbox.x,
                                       "y", (gdouble) bbox.y,
                                       NULL);

  g_signal_connect_swapped (animation, "tick", G_CALLBACK (photos_image_container_rubber_band_tick), self);
  g_object_weak_ref (G_OBJECT (animation), (GWeakNotify) photos_image_container_notify_animation, self);

  self->bbox = bbox;

  g_object_unref (buffer);
  g_object_unref (buffer_orig);
}


static void
photos_image_container_rubber_band (PhotosImageContainer *self)
{
  GeglRectangle bbox;

  bbox = gegl_node_get_bounding_box (self->node);
  if (gegl_rectangle_contains (&self->bbox, &bbox))
    photos_image_container_rubber_band_contract (self);
  else
    photos_image_container_rubber_band_expand (self);
}


static gboolean
photos_image_container_computed_idle (gpointer user_data)
{
  PhotosImageContainer *self = PHOTOS_IMAGE_CONTAINER (user_data);
  GeglRectangle bbox;

  self->computed_id = 0;

  g_return_val_if_fail (self->buffer_source != NULL, G_SOURCE_REMOVE);
  g_return_val_if_fail (self->node != NULL, G_SOURCE_REMOVE);

  g_message ("photos_image_computed_idle: %p", self->node);

  bbox = gegl_node_get_bounding_box (self->node);
  if (gegl_rectangle_equal (&self->bbox, &bbox))
    photos_image_container_crossfade (self);
  else
    photos_image_container_rubber_band (self);

  return G_SOURCE_REMOVE;
}


static void
photos_image_container_computed (PhotosImageContainer *self, GeglRectangle *rect)
{
  cairo_status_t status;

  if (self->computed_id != 0)
    return;

  status = cairo_region_union_rectangle (self->region, (cairo_rectangle_int_t *) rect);
  g_assert_cmpint (status, ==, CAIRO_STATUS_SUCCESS);

  if (cairo_region_equal (self->bbox_region, self->region))
    self->computed_id = g_idle_add_full (G_PRIORITY_HIGH_IDLE, photos_image_container_computed_idle, self, NULL);
}


static void
photos_image_container_dispose (GObject *object)
{
  PhotosImageContainer *self = PHOTOS_IMAGE_CONTAINER (object);

  if (self->computed_id != 0)
    {
      g_source_remove (self->computed_id);
      self->computed_id = 0;
    }

  g_clear_object (&self->graph);
  g_clear_object (&self->graph_anim);
  g_clear_object (&self->node);

  G_OBJECT_CLASS (photos_image_container_parent_class)->dispose (object);
}


static void
photos_image_container_finalize (GObject *object)
{
  PhotosImageContainer *self = PHOTOS_IMAGE_CONTAINER (object);

  g_clear_pointer (&self->bbox_region, (GDestroyNotify) cairo_region_destroy);
  g_clear_pointer (&self->region, (GDestroyNotify) cairo_region_destroy);

  G_OBJECT_CLASS (photos_image_container_parent_class)->finalize (object);
}


static void
photos_image_container_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
  PhotosImageContainer *self = PHOTOS_IMAGE_CONTAINER (object);

  switch (prop_id)
    {
    case PROP_NODE:
      g_value_set_object (value, self->node);
      break;

    case PROP_VIEW:
      g_value_set_object (value, self->view);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
photos_image_container_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
  PhotosImageContainer *self = PHOTOS_IMAGE_CONTAINER (object);

  switch (prop_id)
    {
    case PROP_NODE:
      {
        GeglNode *node;

        node = GEGL_NODE (g_value_get_object (value));
        photos_image_container_set_node (self, node);
        break;
      }

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
photos_image_container_init (PhotosImageContainer *self)
{
  GtkStyleContext *context;
  GtkWidget *sw;

  self->stack = gtk_stack_new ();
  gtk_widget_set_hexpand (self->stack, TRUE);
  gtk_widget_set_vexpand (self->stack, TRUE);
  gtk_container_add (GTK_CONTAINER (self), self->stack);

  sw = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw), GTK_SHADOW_IN);
  context = gtk_widget_get_style_context (sw);
  gtk_style_context_add_class (context, "documents-scrolledwin");
  gtk_stack_add_named (GTK_STACK (self->stack), sw, "view");

  self->view = photos_image_view_new ();
  gtk_container_add (GTK_CONTAINER (sw), self->view);

  gtk_widget_show_all (sw);
  gtk_stack_set_visible_child (GTK_STACK (self->stack), sw);
}


static void
photos_image_container_class_init (PhotosImageContainerClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->dispose = photos_image_container_dispose;
  object_class->finalize = photos_image_container_finalize;
  object_class->get_property = photos_image_container_get_property;
  object_class->set_property = photos_image_container_set_property;

  g_object_class_install_property (object_class,
                                   PROP_NODE,
                                   g_param_spec_object ("node",
                                                        "GeglNode object",
                                                        "The GeglNode to render",
                                                        GEGL_TYPE_NODE,
                                                        G_PARAM_CONSTRUCT | G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_VIEW,
                                   g_param_spec_object ("view",
                                                        "GtkWidget object",
                                                        "The view inside this container",
                                                        GTK_TYPE_WIDGET,
                                                        G_PARAM_READABLE));
}


GtkWidget *
photos_image_container_new (void)
{
  return g_object_new (PHOTOS_TYPE_IMAGE_CONTAINER, NULL);
}


GeglNode *
photos_image_container_get_node (PhotosImageContainer *self)
{
  g_return_val_if_fail (PHOTOS_IS_IMAGE_CONTAINER (self), NULL);
  return self->node;
}


GtkWidget *
photos_image_container_get_view (PhotosImageContainer *self)
{
  g_return_val_if_fail (PHOTOS_IS_IMAGE_CONTAINER (self), NULL);
  return self->view;
}


void
photos_image_container_set_node (PhotosImageContainer *self, GeglNode *node)
{
  g_return_if_fail (PHOTOS_IS_IMAGE_CONTAINER (self));

  if (self->node == node)
    return;

  if (self->node != NULL)
    {
      g_signal_handlers_disconnect_by_func (self->node, photos_image_container_computed, self);
      g_signal_handlers_disconnect_by_func (self->node, photos_image_container_invalidated, self);
    }

  gegl_rectangle_set (&self->bbox, 0, 0, 0, 0);
  self->buffer_source = NULL;

  g_clear_object (&self->graph);
  g_clear_object (&self->node);
  g_clear_pointer (&self->bbox_region, (GDestroyNotify) cairo_region_destroy);
  g_clear_pointer (&self->region, (GDestroyNotify) cairo_region_destroy);

  if (node != NULL)
    {
      const Babl *format;
      GeglBuffer *buffer;
      GeglBuffer *buffer_orig;

      g_object_ref (node);

      self->bbox = gegl_node_get_bounding_box (node);
      self->bbox_region = cairo_region_create_rectangle ((cairo_rectangle_int_t *) &self->bbox);
      self->region = cairo_region_create ();

      format = babl_format ("cairo-ARGB32");
      buffer_orig = photos_utils_create_buffer_from_node (node, format);
      buffer = gegl_buffer_dup (buffer_orig);
      self->graph = gegl_node_new ();
      self->buffer_source = gegl_node_new_child (self->graph,
                                                 "operation", "gegl:buffer-source",
                                                 "buffer", buffer,
                                                 NULL);
      gegl_node_process (self->buffer_source);

      g_signal_connect_object (node,
                               "computed",
                               G_CALLBACK (photos_image_container_computed),
                               self,
                               G_CONNECT_SWAPPED);
      g_signal_connect_object (node,
                               "invalidated",
                               G_CALLBACK (photos_image_container_invalidated),
                               self,
                               G_CONNECT_SWAPPED);

      g_object_unref (buffer);
      g_object_unref (buffer_orig);
    }

  self->node = node;
  photos_image_view_set_node (PHOTOS_IMAGE_VIEW (self->view), self->buffer_source);
  gtk_widget_queue_draw (GTK_WIDGET (self));
}
