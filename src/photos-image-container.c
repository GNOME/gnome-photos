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

#include "photos-image-container.h"
#include "photos-image-view.h"
#include "photos-utils.h"


struct _PhotosImageContainer
{
  GtkBin parent_instance;
  GeglNode *buffer_source;
  GeglNode *graph;
  GeglNode *node;
  GtkWidget *stack;
  GtkWidget *view;
  cairo_region_t *bbox_region;
  cairo_region_t *region;
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
photos_image_container_notify_transition_running (GtkStack *stack, GParamSpec *pspec, gpointer user_data)
{
  GtkWidget *view = GTK_WIDGET (user_data);

  if (!gtk_stack_get_transition_running (stack))
    gtk_widget_destroy (view);

  g_signal_handlers_disconnect_by_func (stack, photos_image_container_notify_transition_running, view);
}


static gboolean
photos_image_container_computed_idle (gpointer user_data)
{
  PhotosImageContainer *self = PHOTOS_IMAGE_CONTAINER (user_data);
  const Babl *format;
  GeglBuffer *buffer;
  GeglBuffer *buffer_orig;
  GtkWidget *view;

  g_return_val_if_fail (self->buffer_source != NULL, G_SOURCE_REMOVE);

  view = photos_image_view_new ();
  photos_image_view_set_node (PHOTOS_IMAGE_VIEW (view), self->buffer_source);
  gtk_container_add (GTK_CONTAINER (self->stack), view);
  gtk_widget_show (view);
  gtk_stack_set_visible_child (GTK_STACK (self->stack), view);

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
  g_signal_connect (self->stack,
                    "notify::transition-running",
                    G_CALLBACK (photos_image_container_notify_transition_running),
                    view);

  gtk_widget_queue_draw (GTK_WIDGET (self));
  g_signal_handlers_unblock_by_func (self->node, photos_image_container_computed, self);

  g_object_unref (buffer);
  g_object_unref (buffer_orig);
  return G_SOURCE_REMOVE;
}


static void
photos_image_container_computed (PhotosImageContainer *self, GeglRectangle *rect)
{
  cairo_status_t status;

  status = cairo_region_union_rectangle (self->region, (cairo_rectangle_int_t *) rect);
  g_assert_cmpint (status, ==, CAIRO_STATUS_SUCCESS);

  if (cairo_region_equal (self->bbox_region, self->region))
    g_idle_add (photos_image_container_computed_idle, self);
}


static void
photos_image_container_dispose (GObject *object)
{
  PhotosImageContainer *self = PHOTOS_IMAGE_CONTAINER (object);

  g_clear_object (&self->graph);
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
    g_signal_handlers_disconnect_by_func (self->node, photos_image_container_computed, self);

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
      GeglRectangle bbox;

      g_object_ref (node);

      bbox = gegl_node_get_bounding_box (node);
      self->bbox_region = cairo_region_create_rectangle ((cairo_rectangle_int_t *) &bbox);
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

      g_object_unref (buffer);
      g_object_unref (buffer_orig);
    }

  self->node = node;
  photos_image_view_set_node (PHOTOS_IMAGE_VIEW (self->view), self->buffer_source);
  gtk_widget_queue_draw (GTK_WIDGET (self));
}
