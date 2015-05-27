/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2015 Red Hat, Inc.
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

#include <math.h>

#include <babl/babl.h>
#include <gegl.h>
#include <gegl-plugin.h>

#include "photos-enums.h"
#include "photos-operation-insta-common.h"
#include "photos-operation-insta-filter.h"


struct _PhotosOperationInstaFilter
{
  GeglOperationMeta parent_instance;
  GeglNode *input;
  GeglNode *output;
  GList *nodes;
  PhotosOperationInstaPreset preset;
};

struct _PhotosOperationInstaFilterClass
{
  GeglOperationMetaClass parent_class;
};

enum
{
  PROP_0,
  PROP_PRESET
};


G_DEFINE_TYPE (PhotosOperationInstaFilter, photos_operation_insta_filter, GEGL_TYPE_OPERATION_META);


static void
photos_operation_insta_filter_setup (PhotosOperationInstaFilter *self)
{
  GeglOperation *operation = GEGL_OPERATION (self);
  GeglNode *node;
  GList *l;

  g_list_free_full (self->nodes, g_object_unref);
  self->nodes = NULL;

  switch (self->preset)
    {
    case PHOTOS_OPERATION_INSTA_PRESET_NONE:
      node = gegl_node_new_child (operation->node,
                                  "operation", "gegl:nop",
                                  NULL);
      self->nodes = g_list_prepend (self->nodes, node);
      break;

    case PHOTOS_OPERATION_INSTA_PRESET_1977:
      node = gegl_node_new_child (operation->node,
                                  "operation", "photos:insta-curve",
                                  "preset", self->preset,
                                  NULL);
      self->nodes = g_list_prepend (self->nodes, node);
      break;

    case PHOTOS_OPERATION_INSTA_PRESET_BRANNAN:
      node = gegl_node_new_child (operation->node,
                                  "operation", "photos:insta-curve",
                                  "preset", self->preset,
                                  NULL);
      self->nodes = g_list_prepend (self->nodes, node);
      break;

    case PHOTOS_OPERATION_INSTA_PRESET_GOTHAM:
      node = gegl_node_new_child (operation->node,
                                  "operation", "photos:insta-curve",
                                  "preset", self->preset,
                                  NULL);
      self->nodes = g_list_prepend (self->nodes, node);

      node = gegl_node_new_child (operation->node,
                                  "operation", "gegl:gray",
                                  NULL);
      self->nodes = g_list_prepend (self->nodes, node);
      break;

    case PHOTOS_OPERATION_INSTA_PRESET_GRAY:
      node = gegl_node_new_child (operation->node,
                                  "operation", "gegl:gray",
                                  NULL);
      self->nodes = g_list_prepend (self->nodes, node);
      break;

    case PHOTOS_OPERATION_INSTA_PRESET_NASHVILLE:
      node = gegl_node_new_child (operation->node,
                                  "operation", "photos:insta-curve",
                                  "preset", self->preset,
                                  NULL);
      self->nodes = g_list_prepend (self->nodes, node);
      break;

    default:
      break;
    }

  node = GEGL_NODE (self->nodes->data);
  gegl_node_link (self->input, node);

  for (l = self->nodes; l != NULL && l->next != NULL; l = l->next)
    {
      GeglNode *sink = GEGL_NODE (l->next->data);
      GeglNode *source = GEGL_NODE (l->data);

      gegl_node_link (source, sink);
      gegl_operation_meta_watch_node (operation, source);
    }

  node = GEGL_NODE (l->data);
  gegl_node_link (node, self->output);
  gegl_operation_meta_watch_node (operation, node);
}


static void
photos_operation_insta_filter_set_preset (PhotosOperationInstaFilter *self, PhotosOperationInstaPreset preset)
{
  if (self->preset == preset)
    return;

  self->preset = preset;
  if (self->input != NULL)
    photos_operation_insta_filter_setup (self);
}


static void
photos_operation_insta_filter_attach (GeglOperation *operation)
{
  PhotosOperationInstaFilter *self = PHOTOS_OPERATION_INSTA_FILTER (operation);

  self->input = gegl_node_get_output_proxy (operation->node, "input");
  self->output = gegl_node_get_output_proxy (operation->node, "output");
  photos_operation_insta_filter_setup (self);
}


static GeglNode *
photos_operation_insta_filter_detect (GeglOperation *operation, gint x, gint y)
{
  PhotosOperationInstaFilter *self = PHOTOS_OPERATION_INSTA_FILTER (operation);
  GeglRectangle bounds;

  bounds = gegl_node_get_bounding_box (self->output);
  if (x >= bounds.x && y >= bounds.y && x  < bounds.x + bounds.width && y  < bounds.y + bounds.height)
    return operation->node;

  return NULL;
}


static void
photos_operation_insta_filter_prepare (GeglOperation *operation)
{
  const Babl *format;
  const Babl *format_float;
  const Babl *format_u8;
  const Babl *input_format;
  const Babl *type;
  const Babl *type_u8;

  input_format = gegl_operation_get_source_format (operation, "input");
  type = babl_format_get_type (input_format, 0);

  format_float = babl_format ("R'G'B' float");
  format_u8 = babl_format ("R'G'B' u8");
  type_u8 = babl_type ("u8");

  if (type == type_u8)
    format = format_u8;
  else
    format = format_float;

  gegl_operation_set_format (operation, "input", format);
  gegl_operation_set_format (operation, "output", format);
}


static void
photos_operation_insta_filter_finalize (GObject *object)
{
  PhotosOperationInstaFilter *self = PHOTOS_OPERATION_INSTA_FILTER (object);

  g_list_free (self->nodes);

  G_OBJECT_CLASS (photos_operation_insta_filter_parent_class)->finalize (object);
}


static void
photos_operation_insta_filter_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
  PhotosOperationInstaFilter *self = PHOTOS_OPERATION_INSTA_FILTER (object);

  switch (prop_id)
    {
    case PROP_PRESET:
      g_value_set_enum (value, (gint) self->preset);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
photos_operation_insta_filter_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
  PhotosOperationInstaFilter *self = PHOTOS_OPERATION_INSTA_FILTER (object);
  PhotosOperationInstaPreset preset;

  switch (prop_id)
    {
    case PROP_PRESET:
      preset = (PhotosOperationInstaPreset) g_value_get_enum (value);
      photos_operation_insta_filter_set_preset (self, preset);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
photos_operation_insta_filter_init (PhotosOperationInstaFilter *self)
{
}


static void
photos_operation_insta_filter_class_init (PhotosOperationInstaFilterClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GeglOperationClass *operation_class = GEGL_OPERATION_CLASS (class);

  operation_class->opencl_support = FALSE;

  object_class->finalize = photos_operation_insta_filter_finalize;
  object_class->get_property = photos_operation_insta_filter_get_property;
  object_class->set_property = photos_operation_insta_filter_set_property;
  operation_class->attach = photos_operation_insta_filter_attach;
  operation_class->detect = photos_operation_insta_filter_detect;
  operation_class->prepare = photos_operation_insta_filter_prepare;

  g_object_class_install_property (object_class,
                                   PROP_PRESET,
                                   g_param_spec_enum ("preset",
                                                      "PhotosOperationInstaPreset enum",
                                                      "Which filter to apply",
                                                      PHOTOS_TYPE_OPERATION_INSTA_PRESET,
                                                      PHOTOS_OPERATION_INSTA_PRESET_NONE,
                                                      G_PARAM_CONSTRUCT | G_PARAM_READWRITE));

  gegl_operation_class_set_keys (operation_class,
                                 "name", "photos:insta-filter",
                                 "title", "Insta Filter",
                                 "description", "Apply a preset filter to an image",
                                 NULL);
}
