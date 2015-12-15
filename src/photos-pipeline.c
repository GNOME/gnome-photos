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

#include <glib.h>

#include "photos-debug.h"
#include "photos-operation-insta-common.h"
#include "photos-pipeline.h"


struct _PhotosPipeline
{
  GObject parent_instance;
  GeglNode *parent;
  GHashTable *hash;
  GQueue *history;
  GeglNode *graph;
};

struct _PhotosPipelineClass
{
  GObjectClass parent_class;
};

enum
{
  PROP_0,
  PROP_PARENT,
};


G_DEFINE_TYPE (PhotosPipeline, photos_pipeline, G_TYPE_OBJECT);


static gchar *
photos_pipeline_to_xml (PhotosPipeline *self)
{
  GeglNode *input;
  GeglNode *last;
  GeglNode *output;
  gchar *ret_val;

  /* PhotosPipeline can be connected to a gegl:buffer-source, in which
   * case we will get a WARNING about trying to serialize the
   * GeglBuffer. We work around that.
   */

  input = gegl_node_get_input_proxy (self->graph, "input");
  output = gegl_node_get_output_proxy (self->graph, "output");

  last = gegl_node_get_producer (input, "input", NULL);
  if (last != NULL)
    gegl_node_disconnect (input, "input");

  ret_val = gegl_node_to_xml (output, "/");

  if (last != NULL)
    gegl_node_link (last, input);

  return ret_val;
}

static void
photos_pipeline_constructed (GObject *object)
{
  PhotosPipeline *self = PHOTOS_PIPELINE (object);
  GeglNode *input;
  GeglNode *output;

  G_OBJECT_CLASS (photos_pipeline_parent_class)->constructed (object);

  self->graph = gegl_node_new ();
  gegl_node_add_child (self->parent, self->graph);
  input = gegl_node_get_input_proxy (self->graph, "input");
  output = gegl_node_get_output_proxy (self->graph, "output");
  gegl_node_link (input, output);

  g_clear_object (&self->parent); /* We will not need it any more */
}


static void
photos_pipeline_dispose (GObject *object)
{
  PhotosPipeline *self = PHOTOS_PIPELINE (object);

  g_clear_object (&self->graph);
  g_clear_object (&self->parent);
  g_clear_pointer (&self->hash, (GDestroyNotify) g_hash_table_unref);

  G_OBJECT_CLASS (photos_pipeline_parent_class)->dispose (object);
}


static void
photos_pipeline_finalize (GObject *object)
{
  PhotosPipeline *self = PHOTOS_PIPELINE (object);

  g_queue_free (self->history);

  G_OBJECT_CLASS (photos_pipeline_parent_class)->finalize (object);
}


static void
photos_pipeline_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
  PhotosPipeline *self = PHOTOS_PIPELINE (object);

  switch (prop_id)
    {
    case PROP_PARENT:
      self->parent = GEGL_NODE (g_value_dup_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
photos_pipeline_init (PhotosPipeline *self)
{
  self->hash = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
  self->history = g_queue_new ();
}


static void
photos_pipeline_class_init (PhotosPipelineClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->constructed = photos_pipeline_constructed;
  object_class->dispose = photos_pipeline_dispose;
  object_class->finalize = photos_pipeline_finalize;
  object_class->set_property = photos_pipeline_set_property;

  g_object_class_install_property (object_class,
                                   PROP_PARENT,
                                   g_param_spec_object ("parent",
                                                        "GeglNode object",
                                                        "A GeglNode representing the parent graph",
                                                        GEGL_TYPE_NODE,
                                                        G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE));
}


PhotosPipeline *
photos_pipeline_new (GeglNode *parent)
{
  return g_object_new (PHOTOS_TYPE_PIPELINE, "parent", parent, NULL);
}


void
photos_pipeline_add (PhotosPipeline *self, const gchar *operation, const gchar *first_property_name, va_list ap)
{
  GeglNode *input;
  GeglNode *last;
  GeglNode *node;
  GeglNode *output;
  gchar *xml = NULL;

  input = gegl_node_get_input_proxy (self->graph, "input");
  output = gegl_node_get_output_proxy (self->graph, "output");
  last = gegl_node_get_producer (output, "input", NULL);
  if (last == input)
    photos_pipeline_reset (self);

  node = GEGL_NODE (g_hash_table_lookup (self->hash, operation));
  if (node == NULL)
    {
      last = gegl_node_get_producer (output, "input", NULL);
      node = gegl_node_new_child (self->graph, "operation", operation, NULL);
      gegl_node_disconnect (output, "input");
      gegl_node_link_many (last, node, output, NULL);
      g_hash_table_insert (self->hash, g_strdup (operation), g_object_ref (node));
    }

  gegl_node_set_valist (node, first_property_name, ap);

  xml = photos_pipeline_to_xml (self);
  photos_debug (PHOTOS_DEBUG_GEGL, "Pipeline: %s", xml);

  /* We want to remove the nodes from the graph too. */
  g_queue_free_full (self->history, g_object_unref);
  self->history = g_queue_new ();

  g_free (xml);
}


gboolean
photos_pipeline_get (PhotosPipeline *self, const gchar *operation, const gchar *first_property_name, va_list ap)
{
  GeglNode *node;
  gboolean ret_val = FALSE;

  node = GEGL_NODE (g_hash_table_lookup (self->hash, operation));
  if (node == NULL)
    goto out;

  gegl_node_get_valist (node, first_property_name, ap);
  ret_val = TRUE;

 out:
  return ret_val;
}


GeglNode *
photos_pipeline_get_graph (PhotosPipeline *self)
{
  return self->graph;
}


GeglNode *
photos_pipeline_get_output (PhotosPipeline *self)
{
  GeglNode *output;

  output = gegl_node_get_output_proxy (self->graph, "output");
  return output;
}


GeglProcessor *
photos_pipeline_new_processor (PhotosPipeline *self)
{
  GeglNode *output;
  GeglProcessor *processor;

  output = gegl_node_get_output_proxy (self->graph, "output");
  processor = gegl_node_new_processor (output, NULL);
  return processor;
}


void
photos_pipeline_reset (PhotosPipeline *self)
{
  GeglNode *input;
  GeglNode *node;
  GeglNode *output;

  while (photos_pipeline_undo (self))
    ;

  input = gegl_node_get_input_proxy (self->graph, "input");
  output = gegl_node_get_output_proxy (self->graph, "output");
  node = gegl_node_new_child (self->graph,
                              "operation", "photos:insta-filter",
                              "preset", PHOTOS_OPERATION_INSTA_PRESET_NONE,
                              NULL);
  gegl_node_link_many (input, node, output, NULL);
  g_hash_table_insert (self->hash, g_strdup ("photos:insta-filter"), g_object_ref (node));
}


gboolean
photos_pipeline_undo (PhotosPipeline *self)
{
  GeglNode *input;
  GeglNode *last;
  GeglNode *last2;
  GeglNode *output;
  gboolean ret_val = FALSE;
  gchar *operation = NULL;
  gchar *xml = NULL;

  input = gegl_node_get_input_proxy (self->graph, "input");
  output = gegl_node_get_output_proxy (self->graph, "output");
  last = gegl_node_get_producer (output, "input", NULL);
  if (last == input)
    goto out;

  gegl_node_get (last, "operation", &operation, NULL);
  g_hash_table_remove (self->hash, operation);
  g_queue_push_head (self->history, last);

  last2 = gegl_node_get_producer (last, "input", NULL);
  gegl_node_disconnect (output, "input");
  gegl_node_disconnect (last, "input");
  gegl_node_link (last2, output);

  xml = photos_pipeline_to_xml (self);
  photos_debug (PHOTOS_DEBUG_GEGL, "Pipeline: %s", xml);

  ret_val = TRUE;

 out:
  g_free (xml);
  g_free (operation);
  return ret_val;
}
