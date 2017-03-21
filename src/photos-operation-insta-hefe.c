/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2016 – 2017 Red Hat, Inc.
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
#include <gegl.h>

#include "photos-operation-insta-hefe.h"


struct _PhotosOperationInstaHefe
{
  GeglOperationMeta parent_instance;
  GeglNode *vignette;
  GeglNode *input;
  GeglNode *output;
  GeglRectangle bbox;
};


G_DEFINE_TYPE (PhotosOperationInstaHefe, photos_operation_insta_hefe, GEGL_TYPE_OPERATION_META);


static void
photos_operation_insta_hefe_setup (PhotosOperationInstaHefe *self)
{
  gegl_node_set (self->vignette,
                 "height", (gdouble) self->bbox.height,
                 "width", (gdouble) self->bbox.width,
                 "x", (gdouble) self->bbox.x,
                 "y", (gdouble) self->bbox.y,
                 NULL);
}


static void
photos_operation_insta_hefe_attach (GeglOperation *operation)
{
  PhotosOperationInstaHefe *self = PHOTOS_OPERATION_INSTA_HEFE (operation);
  GeglNode *curve;
  GeglNode *multiply;

  self->input = gegl_node_get_output_proxy (operation->node, "input");
  self->output = gegl_node_get_output_proxy (operation->node, "output");

  curve = gegl_node_new_child (operation->node, "operation", "photos:insta-hefe-curve", NULL);
  multiply = gegl_node_new_child (operation->node, "operation", "svg:multiply", "srgb", TRUE, NULL);
  self->vignette = gegl_node_new_child (operation->node, "operation", "photos:insta-hefe-vignette", NULL);

  gegl_node_connect_to (self->vignette, "output", multiply, "aux");
  gegl_node_link_many (self->input, multiply, curve, self->output, NULL);

  gegl_operation_meta_watch_nodes (operation, curve, multiply, self->vignette, NULL);
}


static GeglNode *
photos_operation_insta_hefe_detect (GeglOperation *operation, gint x, gint y)
{
  PhotosOperationInstaHefe *self = PHOTOS_OPERATION_INSTA_HEFE (operation);
  GeglRectangle bounds;

  bounds = gegl_node_get_bounding_box (self->output);
  if (x >= bounds.x && y >= bounds.y && x < bounds.x + bounds.width && y < bounds.y + bounds.height)
    return operation->node;

  return NULL;
}


static void
photos_operation_insta_hefe_prepare (GeglOperation *operation)
{
  PhotosOperationInstaHefe *self = PHOTOS_OPERATION_INSTA_HEFE (operation);
  GeglRectangle bbox;

  bbox = gegl_node_get_bounding_box (self->input);
  if (!gegl_rectangle_equal (&self->bbox, &bbox))
    {
      self->bbox = bbox;
      photos_operation_insta_hefe_setup (self);
    }
}


static void
photos_operation_insta_hefe_init (PhotosOperationInstaHefe *self)
{
}


static void
photos_operation_insta_hefe_class_init (PhotosOperationInstaHefeClass *class)
{
  GeglOperationClass *operation_class = GEGL_OPERATION_CLASS (class);

  operation_class->opencl_support = FALSE;

  operation_class->attach = photos_operation_insta_hefe_attach;
  operation_class->detect = photos_operation_insta_hefe_detect;
  operation_class->prepare = photos_operation_insta_hefe_prepare;

  gegl_operation_class_set_keys (operation_class,
                                 "name", "photos:insta-hefe",
                                 "title", "Insta Hefe",
                                 "description", "Apply the Hefe filter to an image",
                                 "categories", "hidden",
                                 NULL);
}
