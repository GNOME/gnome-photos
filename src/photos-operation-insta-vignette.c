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

/* Based on code from:
 *   + noflo-image
 */


#include "config.h"

#include <math.h>

#include <babl/babl.h>
#include <gegl.h>
#include <gegl-plugin.h>

#include "photos-operation-insta-vignette.h"


struct _PhotosOperationInstaVignette
{
  GeglOperationMeta parent_instance;
  GeglNode *crop1;
  GeglNode *crop2;
  GeglNode *crop3;
  GeglNode *gradient1;
  GeglNode *gradient2;
  GeglNode *gradient3;
  GeglNode *input;
  GeglNode *output;
  GeglRectangle bbox;
};

struct _PhotosOperationInstaVignetteClass
{
  GeglOperationMetaClass parent_class;
};


G_DEFINE_TYPE (PhotosOperationInstaVignette, photos_operation_insta_vignette, GEGL_TYPE_OPERATION_META);


static void
photos_operation_insta_vignette_setup (PhotosOperationInstaVignette *self)
{
  gdouble height_half;
  gdouble width_half;
  gdouble radius;
  gdouble radius65;
  gdouble x;
  gdouble y;

  height_half = (gdouble) self->bbox.height / 2.0;
  width_half = (gdouble) self->bbox.width / 2.0;
  radius = sqrt (height_half * height_half + width_half * width_half);
  radius65 = 0.65 * radius;
  x = width_half + (gdouble) self->bbox.x;
  y = height_half + (gdouble) self->bbox.y;
  gegl_node_set (self->gradient1, "r0", radius65, "r1", radius, "x", x, "y", y, NULL);
  gegl_node_set (self->gradient2, "r0", 0.0, "r1", radius65, "x", x, "y", y, NULL);
  gegl_node_set (self->gradient3, "r0", radius65, "r1", radius, "x", x, "y", y, NULL);

  gegl_node_set (self->crop1,
                 "height", (gdouble) self->bbox.height,
                 "width", (gdouble) self->bbox.width,
                 "x", (gdouble) self->bbox.x,
                 "y", (gdouble) self->bbox.y,
                 NULL);
  gegl_node_set (self->crop2,
                 "height", (gdouble) self->bbox.height,
                 "width", (gdouble) self->bbox.width,
                 "x", (gdouble) self->bbox.x,
                 "y", (gdouble) self->bbox.y,
                 NULL);
  gegl_node_set (self->crop3,
                 "height", (gdouble) self->bbox.height,
                 "width", (gdouble) self->bbox.width,
                 "x", (gdouble) self->bbox.x,
                 "y", (gdouble) self->bbox.y,
                 NULL);
}


static void
photos_operation_insta_vignette_attach (GeglOperation *operation)
{
  PhotosOperationInstaVignette *self = PHOTOS_OPERATION_INSTA_VIGNETTE (operation);
  GeglColor *color_end;
  GeglColor *color_start;
  GeglNode *add1;
  GeglNode *add2;
  GeglNode *over;

  self->input = gegl_node_get_output_proxy (operation->node, "input");
  self->output = gegl_node_get_output_proxy (operation->node, "output");

  color_end = gegl_color_new ("rgba(0.0, 0.0, 0.0, 0.9)");
  color_start = gegl_color_new ("rgba(0.0, 0.0, 0.0, 0.0)");
  self->gradient1 = gegl_node_new_child (operation->node,
                                         "operation", "photos:radial-gradient",
                                         "color-end", color_end,
                                         "color-start", color_start,
                                         NULL);
  g_object_unref (color_end);
  g_object_unref (color_start);

  self->crop1 = gegl_node_new_child (operation->node, "operation", "gegl:crop", NULL);
  over = gegl_node_new_child (operation->node, "operation", "gegl:over", "srgb", TRUE, NULL);

  color_end = gegl_color_new ("rgba(1.0, 1.0, 1.0, 0.0)");
  color_start = gegl_color_new ("rgba(1.0, 1.0, 1.0, 0.1)");
  self->gradient2 = gegl_node_new_child (operation->node,
                                         "operation", "photos:radial-gradient",
                                         "color-end", color_end,
                                         "color-start", color_start,
                                         "ignore-abyss", TRUE,
                                         NULL);
  g_object_unref (color_end);
  g_object_unref (color_start);

  self->crop2 = gegl_node_new_child (operation->node, "operation", "gegl:crop", NULL);
  add1 = gegl_node_new_child (operation->node, "operation", "photos:add", "srgb", TRUE, NULL);

  color_end = gegl_color_new ("rgba(0.0, 0.0, 0.0, 0.0)");
  color_start = gegl_color_new ("rgba(1.0, 1.0, 1.0, 0.0)");
  self->gradient3 = gegl_node_new_child (operation->node,
                                         "operation", "photos:radial-gradient",
                                         "color-end", color_end,
                                         "color-start", color_start,
                                         "ignore-abyss", TRUE,
                                         NULL);
  g_object_unref (color_end);
  g_object_unref (color_start);

  self->crop3 = gegl_node_new_child (operation->node, "operation", "gegl:crop", NULL);
  add2 = gegl_node_new_child (operation->node, "operation", "photos:add", "srgb", TRUE, NULL);

  gegl_node_link (self->gradient1, self->crop1);
  gegl_node_connect_to (self->crop1, "output", over, "aux");

  gegl_node_link (self->gradient2, self->crop2);
  gegl_node_connect_to (self->crop2, "output", add1, "aux");
  gegl_node_link (self->gradient3, self->crop3);
  gegl_node_connect_to (self->crop3, "output", add2, "aux");

  gegl_node_link_many (self->input, over, add1, add2, self->output, NULL);

  gegl_operation_meta_watch_nodes (operation,
                                   add1,
                                   add2,
                                   over,
                                   self->crop1,
                                   self->crop2,
                                   self->crop3,
                                   self->gradient1,
                                   self->gradient2,
                                   self->gradient3,
                                   NULL);
}


static GeglNode *
photos_operation_insta_vignette_detect (GeglOperation *operation, gint x, gint y)
{
  PhotosOperationInstaVignette *self = PHOTOS_OPERATION_INSTA_VIGNETTE (operation);
  GeglRectangle bounds;

  bounds = gegl_node_get_bounding_box (self->output);
  if (x >= bounds.x && y >= bounds.y && x < bounds.x + bounds.width && y < bounds.y + bounds.height)
    return operation->node;

  return NULL;
}


static void
photos_operation_insta_vignette_prepare (GeglOperation *operation)
{
  PhotosOperationInstaVignette *self = PHOTOS_OPERATION_INSTA_VIGNETTE (operation);
  GeglRectangle bbox;

  bbox = gegl_node_get_bounding_box (self->input);
  if (self->bbox.height != bbox.height
      || self->bbox.width != bbox.width
      || self->bbox.x != bbox.x
      || self->bbox.y != bbox.y)
    {
      self->bbox = bbox;
      photos_operation_insta_vignette_setup (self);
    }
}


static void
photos_operation_insta_vignette_init (PhotosOperationInstaVignette *self)
{
}


static void
photos_operation_insta_vignette_class_init (PhotosOperationInstaVignetteClass *class)
{
  GeglOperationClass *operation_class = GEGL_OPERATION_CLASS (class);

  operation_class->opencl_support = FALSE;

  operation_class->attach = photos_operation_insta_vignette_attach;
  operation_class->detect = photos_operation_insta_vignette_detect;
  operation_class->prepare = photos_operation_insta_vignette_prepare;

  gegl_operation_class_set_keys (operation_class,
                                 "name", "photos:insta-vignette",
                                 "title", "Insta Vignette",
                                 "description", "Apply a vignette to an image",
                                 "categories", "hidden",
                                 NULL);
}
