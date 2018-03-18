/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2015 – 2017 Red Hat, Inc.
 * Copyright © 2016 Umang Jain
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

#include <string.h>

#include <glib.h>
#include <dazzle.h>

#include "photos-debug.h"
#include "photos-gegl.h"
#include "photos-operation-insta-common.h"
#include "photos-pipeline.h"


struct _PhotosPipeline
{
  GObject parent_instance;
  GHashTable *hash;
  GStrv uris;
  GeglNode *graph;
  gchar *snapshot;
};

enum
{
  PROP_0,
  PROP_PARENT,
  PROP_URIS
};

static void photos_pipeline_async_initable_iface_init (GAsyncInitableIface *iface);


G_DEFINE_TYPE_EXTENDED (PhotosPipeline, photos_pipeline, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (G_TYPE_ASYNC_INITABLE, photos_pipeline_async_initable_iface_init));
DZL_DEFINE_COUNTER (instances, "PhotosPipeline", "Instances", "Number of PhotosPipeline instances")


static const gchar *OPERATIONS[] =
{
  "gegl:crop",
  "gegl:noise-reduction",
  "gegl:shadows-highlights",
  "photos:saturation",
  "photos:insta-filter"
};


static void
photos_pipeline_link_nodes (GeglNode *input, GeglNode *output, GSList *nodes)
{
  GSList *l;
  GeglNode *node;

  if (nodes == NULL)
    {
      gegl_node_link (input, output);
      return;
    }

  node = GEGL_NODE (nodes->data);
  gegl_node_link (input, node);

  for (l = nodes; l != NULL && l->next != NULL; l = l->next)
    {
      GeglNode *sink = GEGL_NODE (l->next->data);
      GeglNode *source = GEGL_NODE (l->data);
      gegl_node_link (source, sink);
    }

  node = GEGL_NODE (l->data);
  gegl_node_link (node, output);
}


static void
photos_pipeline_reset (PhotosPipeline *self)
{
  GSList *nodes = NULL;
  GeglNode *input;
  GeglNode *last;
  GeglNode *output;
  guint i;

  input = gegl_node_get_input_proxy (self->graph, "input");
  output = gegl_node_get_output_proxy (self->graph, "output");
  last = gegl_node_get_producer (output, "input", NULL);
  g_return_if_fail (last == input);

  for (i = 0; i < G_N_ELEMENTS (OPERATIONS); i++)
    {
      GeglNode *node;

      node = gegl_node_new_child (self->graph, "operation", OPERATIONS[i], NULL);
      gegl_node_set_passthrough (node, TRUE);
      g_hash_table_insert (self->hash, g_strdup (OPERATIONS[i]), g_object_ref (node));
      nodes = g_slist_prepend (nodes, g_object_ref (node));
    }

  nodes = g_slist_reverse (nodes);
  photos_pipeline_link_nodes (input, output, nodes);

  g_slist_free_full (nodes, g_object_unref);
}


static gboolean
photos_pipeline_create_graph_from_xml (PhotosPipeline *self, const gchar *contents)
{
  g_autoptr (GeglNode) graph = NULL;
  GeglNode *input;
  GeglNode *output;
  g_autoptr (GSList) children = NULL;
  GSList *l;
  gboolean ret_val = FALSE;

  /* HACK: This graph is busted. eg., the input and output proxies
   * point to the same GeglNode. I can't imagine this to be
   * anything else other than a GEGL bug.
   *
   * Therefore, we are going to re-construct a proper graph
   * ourselves.
   */
  graph = gegl_node_new_from_xml (contents, "/");
  if (graph == NULL)
    goto out;

  g_hash_table_remove_all (self->hash);
  photos_gegl_remove_children_from_node (self->graph);

  input = gegl_node_get_input_proxy (self->graph, "input");
  output = gegl_node_get_output_proxy (self->graph, "output");

  children = gegl_node_get_children (graph);
  for (l = children; l != NULL; l = l->next)
    {
      GeglNode *node = GEGL_NODE (l->data);
      const gchar *operation;
      const gchar *operation_compat;

      g_object_ref (node);
      gegl_node_remove_child (graph, node);
      gegl_node_add_child (self->graph, node);
      g_object_unref (node);

      operation = gegl_node_get_operation (node);
      g_hash_table_insert (self->hash, g_strdup (operation), g_object_ref (node));

      operation_compat = gegl_operation_get_key (operation, "compat-name");
      if (operation_compat != NULL)
        g_hash_table_insert (self->hash, g_strdup (operation_compat), g_object_ref (node));
    }

  photos_pipeline_link_nodes (input, output, children);

  ret_val = TRUE;

 out:
  return ret_val;
}


static void
photos_pipeline_save_replace_contents (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  g_autoptr (GTask) task = G_TASK (user_data);
  GFile *file = G_FILE (source_object);

  {
    g_autoptr (GError) error = NULL;

    if (!g_file_replace_contents_finish (file, res, NULL, &error))
      {
        g_task_return_error (task, g_steal_pointer (&error));
        goto out;
      }
  }

  g_task_return_boolean (task, TRUE);

 out:
  return;
}


static void
photos_pipeline_constructed (GObject *object)
{
  PhotosPipeline *self = PHOTOS_PIPELINE (object);
  GeglNode *input;
  GeglNode *output;

  G_OBJECT_CLASS (photos_pipeline_parent_class)->constructed (object);

  input = gegl_node_get_input_proxy (self->graph, "input");
  output = gegl_node_get_output_proxy (self->graph, "output");
  gegl_node_link (input, output);
}


static void
photos_pipeline_dispose (GObject *object)
{
  PhotosPipeline *self = PHOTOS_PIPELINE (object);

  /* We must drop all references to the child nodes before destroying
   * the graph. The other option would be to ensure that the
   * GeglProcessor is destroyed before its Pipeline, but since that is
   * harder to enforce, let's do this instead.
   *
   * See: https://bugzilla.gnome.org/show_bug.cgi?id=759995
   */
  g_clear_pointer (&self->hash, (GDestroyNotify) g_hash_table_unref);

  g_clear_object (&self->graph);

  G_OBJECT_CLASS (photos_pipeline_parent_class)->dispose (object);
}


static void
photos_pipeline_finalize (GObject *object)
{
  PhotosPipeline *self = PHOTOS_PIPELINE (object);

  g_strfreev (self->uris);
  g_free (self->snapshot);

  G_OBJECT_CLASS (photos_pipeline_parent_class)->finalize (object);

  DZL_COUNTER_DEC (instances);
}


static void
photos_pipeline_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
  PhotosPipeline *self = PHOTOS_PIPELINE (object);

  switch (prop_id)
    {
    case PROP_PARENT:
      {
        GeglNode *parent;

        parent = GEGL_NODE (g_value_get_object (value));
        photos_pipeline_set_parent (self, parent);
        break;
      }

    case PROP_URIS:
      self->uris = (GStrv) g_value_dup_boxed (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
photos_pipeline_init (PhotosPipeline *self)
{
  DZL_COUNTER_INC (instances);

  self->hash = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
  self->graph = gegl_node_new ();
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
                                                        G_PARAM_CONSTRUCT | G_PARAM_WRITABLE));

  g_object_class_install_property (object_class,
                                   PROP_URIS,
                                   g_param_spec_boxed ("uris",
                                                       "URIs",
                                                       "Possible locations from which to load this pipeline, and"
                                                       "afterwards it will be saved to the first non-NULL URI.",
                                                       G_TYPE_STRV,
                                                       G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE));
}


static void
photos_pipeline_async_initable_init_load_contents (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  g_autoptr (GTask) task = G_TASK (user_data);
  PhotosPipeline *self;
  GCancellable *cancellable;
  GFile *file = G_FILE (source_object);
  g_autofree gchar *contents = NULL;
  const gchar *uri;

  self = PHOTOS_PIPELINE (g_task_get_source_object (task));
  cancellable = g_task_get_cancellable (task);
  uri = (const gchar *) g_task_get_task_data (task);

  {
    g_autoptr (GError) error = NULL;

    if (!g_file_load_contents_finish (file, res, &contents, NULL, NULL, &error))
      {
        if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND))
          {
            g_autoptr (GFile) file_next = NULL;
            guint i;

            for (i = 0; self->uris[i] != NULL; i++)
              {
                if (g_strcmp0 (self->uris[i], uri) == 0)
                  break;
              }

            g_assert_nonnull (self->uris[i]);

            i++;
            if (self->uris[i] == NULL)
              goto carry_on;

            g_task_set_task_data (task, g_strdup (self->uris[i]), g_free);

            file_next = g_file_new_for_uri (self->uris[i]);
            g_file_load_contents_async (file_next,
                                        cancellable,
                                        photos_pipeline_async_initable_init_load_contents,
                                        g_object_ref (task));

            goto out;
          }
        else
          {
            g_task_return_error (task, g_steal_pointer (&error));
            goto out;
          }
      }
  }

  if (!(photos_pipeline_create_graph_from_xml (self, contents)))
    g_warning ("Unable to deserialize from %s", uri);

 carry_on:
  g_task_return_boolean (task, TRUE);

 out:
  return;
}


static void
photos_pipeline_async_initable_init_async (GAsyncInitable *initable,
                                           gint io_priority,
                                           GCancellable *cancellable,
                                           GAsyncReadyCallback callback,
                                           gpointer user_data)
{
  PhotosPipeline *self = PHOTOS_PIPELINE (initable);
  g_autoptr (GFile) file = NULL;
  g_autoptr (GTask) task = NULL;

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, photos_pipeline_async_initable_init_async);

  if (self->uris == NULL || self->uris[0] == NULL || self->uris[0][0] == '\0')
    {
      g_task_return_boolean (task, TRUE);
      goto out;
    }

  g_task_set_task_data (task, g_strdup (self->uris[0]), g_free);

  file = g_file_new_for_uri (self->uris[0]);
  g_file_load_contents_async (file,
                              cancellable,
                              photos_pipeline_async_initable_init_load_contents,
                              g_object_ref (task));

 out:
  return;
}


static gboolean
photos_pipeline_async_initable_init_finish (GAsyncInitable *initable, GAsyncResult *res, GError **error)
{
  PhotosPipeline *self = PHOTOS_PIPELINE (initable);
  GTask *task = G_TASK (res);

  g_return_val_if_fail (g_task_is_valid (res, self), FALSE);
  g_return_val_if_fail (g_task_get_source_tag (task) == photos_pipeline_async_initable_init_async, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  return g_task_propagate_boolean (task, error);
}


static void
photos_pipeline_async_initable_iface_init (GAsyncInitableIface *iface)
{
  iface->init_async = photos_pipeline_async_initable_init_async;
  iface->init_finish = photos_pipeline_async_initable_init_finish;
}


void
photos_pipeline_new_async (GeglNode *parent,
                           const gchar *const *uris,
                           GCancellable *cancellable,
                           GAsyncReadyCallback callback,
                           gpointer user_data)
{
  g_return_if_fail (parent == NULL || GEGL_IS_NODE (parent));
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  g_async_initable_new_async (PHOTOS_TYPE_PIPELINE,
                              G_PRIORITY_DEFAULT,
                              cancellable,
                              callback,
                              user_data,
                              "parent", parent,
                              "uris", uris,
                              NULL);
}


PhotosPipeline *
photos_pipeline_new_finish (GAsyncResult *res, GError **error)
{
  GObject *ret_val;
  g_autoptr (GObject) source_object = NULL;

  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  source_object = g_async_result_get_source_object (res);
  ret_val = g_async_initable_new_finish (G_ASYNC_INITABLE (source_object), res, error);
  return PHOTOS_PIPELINE (ret_val);
}


void
photos_pipeline_add_valist (PhotosPipeline *self,
                            const gchar *operation,
                            const gchar *first_property_name,
                            va_list ap)
{
  GeglNode *input;
  GeglNode *last;
  GeglNode *node;
  GeglNode *output;
  g_autofree gchar *xml = NULL;

  g_return_if_fail (PHOTOS_IS_PIPELINE (self));
  g_return_if_fail (operation != NULL && operation[0] != '\0');

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
  else
    {
      gegl_node_set_passthrough (node, FALSE);
    }

  gegl_node_set_valist (node, first_property_name, ap);

  xml = gegl_node_to_xml_full (self->graph, self->graph, "/");
  photos_debug (PHOTOS_DEBUG_GEGL, "Pipeline: %s", xml);
}


gboolean
photos_pipeline_get (PhotosPipeline *self, const gchar *operation, const gchar *first_property_name, ...)
{
  gboolean ret_val;
  va_list ap;

  g_return_val_if_fail (PHOTOS_IS_PIPELINE (self), FALSE);
  g_return_val_if_fail (operation != NULL && operation[0] != '\0', FALSE);

  va_start (ap, first_property_name);
  ret_val = photos_pipeline_get_valist (self, operation, first_property_name, ap);
  va_end (ap);

  return ret_val;
}


gboolean
photos_pipeline_get_valist (PhotosPipeline *self,
                            const gchar *operation,
                            const gchar *first_property_name,
                            va_list ap)
{
  GeglNode *node;
  gboolean ret_val = FALSE;

  g_return_val_if_fail (PHOTOS_IS_PIPELINE (self), FALSE);
  g_return_val_if_fail (operation != NULL && operation[0] != '\0', FALSE);

  node = GEGL_NODE (g_hash_table_lookup (self->hash, operation));
  if (node == NULL)
    goto out;

  if (gegl_node_get_passthrough (node))
    goto out;

  gegl_node_get_valist (node, first_property_name, ap);
  ret_val = TRUE;

 out:
  return ret_val;
}


GeglNode *
photos_pipeline_get_graph (PhotosPipeline *self)
{
  g_return_val_if_fail (PHOTOS_IS_PIPELINE (self), NULL);
  return self->graph;
}


GeglNode *
photos_pipeline_get_output (PhotosPipeline *self)
{
  GeglNode *output;

  g_return_val_if_fail (PHOTOS_IS_PIPELINE (self), NULL);

  output = gegl_node_get_output_proxy (self->graph, "output");
  return output;
}


gboolean
photos_pipeline_is_edited (PhotosPipeline *self)
{
  g_autoptr (GSList) children = NULL;
  GSList *l;
  guint n_operations = 0;

  g_return_val_if_fail (PHOTOS_IS_PIPELINE (self), FALSE);

  children = gegl_node_get_children (self->graph);
  if (children == NULL)
    goto out;

  for (l = children; l != NULL && n_operations == 0; l = l->next)
    {
      GeglNode *node = GEGL_NODE (l->data);
      const char *operation;

      if (gegl_node_get_passthrough (node))
        continue;

      operation = gegl_node_get_operation (node);

      if (g_strcmp0 (operation, "gegl:nop") == 0)
        {
          continue;
        }
      else if (g_strcmp0 (operation, "photos:magic-filter") == 0)
        {
          gint preset;

          gegl_node_get (node, "preset", &preset, NULL);
          if (preset == PHOTOS_OPERATION_INSTA_PRESET_NONE)
            continue;
        }

      n_operations++;
    }

 out:
  return n_operations > 0;
}


GeglProcessor *
photos_pipeline_new_processor (PhotosPipeline *self)
{
  GeglProcessor *processor;

  g_return_val_if_fail (PHOTOS_IS_PIPELINE (self), NULL);

  processor = gegl_node_new_processor (self->graph, NULL);
  return processor;
}


void
photos_pipeline_save_async (PhotosPipeline *self,
                            GCancellable *cancellable,
                            GAsyncReadyCallback callback,
                            gpointer user_data)
{
  g_autoptr (GFile) file = NULL;
  g_autoptr (GTask) task = NULL;
  gchar *xml = NULL;
  gsize len;

  g_return_if_fail (PHOTOS_IS_PIPELINE (self));
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  xml = gegl_node_to_xml_full (self->graph, self->graph, "/");
  g_return_if_fail (xml != NULL);

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, photos_pipeline_save_async);

  /* We need to keep 'xml' alive until g_file_replace_contents_async
   * returns.
   */
  g_task_set_task_data (task, xml, g_free);

  file = g_file_new_for_uri (self->uris[0]);
  len = strlen (xml);
  g_file_replace_contents_async (file,
                                 xml,
                                 len,
                                 NULL,
                                 FALSE,
                                 G_FILE_CREATE_REPLACE_DESTINATION,
                                 cancellable,
                                 photos_pipeline_save_replace_contents,
                                 g_object_ref (task));
}


gboolean
photos_pipeline_save_finish (PhotosPipeline *self, GAsyncResult *res, GError **error)
{
  GTask *task = G_TASK (res);

  g_return_val_if_fail (g_task_is_valid (res, self), FALSE);
  g_return_val_if_fail (g_task_get_source_tag (task) == photos_pipeline_save_async, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  return g_task_propagate_boolean (task, error);
}


gboolean
photos_pipeline_remove (PhotosPipeline *self, const gchar *operation)
{
  GeglNode *node;
  gboolean ret_val = FALSE;
  g_autofree gchar *xml = NULL;

  g_return_val_if_fail (PHOTOS_IS_PIPELINE (self), FALSE);
  g_return_val_if_fail (operation != NULL && operation[0] != '\0', FALSE);

  node = GEGL_NODE (g_hash_table_lookup (self->hash, operation));
  if (node == NULL)
    goto out;

  if (gegl_node_get_passthrough (node))
    goto out;

  gegl_node_set_passthrough (node, TRUE);

  xml = gegl_node_to_xml_full (self->graph, self->graph, "/");
  photos_debug (PHOTOS_DEBUG_GEGL, "Pipeline: %s", xml);

  ret_val = TRUE;

 out:
  return ret_val;
}


void
photos_pipeline_revert (PhotosPipeline *self)
{
  g_autofree gchar *xml = NULL;

  g_return_if_fail (PHOTOS_IS_PIPELINE (self));
  g_return_if_fail (self->snapshot != NULL);

  if (!photos_pipeline_create_graph_from_xml (self, self->snapshot))
    g_warning ("Unable to revert to: %s", self->snapshot);

  g_clear_pointer (&self->snapshot, g_free);

  xml = gegl_node_to_xml_full (self->graph, self->graph, "/");
  photos_debug (PHOTOS_DEBUG_GEGL, "Pipeline: %s", xml);
}


void
photos_pipeline_revert_to_original (PhotosPipeline *self)
{
  const gchar *empty_xml = "<?xml version='1.0' encoding='UTF-8'?><gegl></gegl>";
  g_autofree gchar *xml = NULL;

  g_return_if_fail (PHOTOS_IS_PIPELINE (self));

  if (!photos_pipeline_create_graph_from_xml (self, empty_xml))
    g_warning ("Unable to revert to original");

  g_clear_pointer (&self->snapshot, g_free);

  xml = gegl_node_to_xml_full (self->graph, self->graph, "/");
  photos_debug (PHOTOS_DEBUG_GEGL, "Pipeline: %s", xml);
}


void
photos_pipeline_set_parent (PhotosPipeline *self, GeglNode *parent)
{
  GeglNode *old_parent;

  g_return_if_fail (PHOTOS_IS_PIPELINE (self));
  g_return_if_fail (parent == NULL || GEGL_IS_NODE (parent));

  old_parent = gegl_node_get_parent (self->graph);
  if (parent == old_parent)
    return;

  if (old_parent != NULL)
    gegl_node_remove_child (old_parent, self->graph);

  if (parent != NULL)
    gegl_node_add_child (parent, self->graph);
}


void
photos_pipeline_snapshot (PhotosPipeline *self)
{
  g_return_if_fail (PHOTOS_IS_PIPELINE (self));

  g_free (self->snapshot);
  self->snapshot = gegl_node_to_xml_full (self->graph, self->graph, "/");
  photos_debug (PHOTOS_DEBUG_GEGL, "Snapshot: %s", self->snapshot);
}
