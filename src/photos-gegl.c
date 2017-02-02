/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2013 – 2017 Red Hat, Inc.
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

#include "photos-debug.h"
#include "photos-gegl.h"
#include "photos-operation-insta-curve.h"
#include "photos-operation-insta-filter.h"
#include "photos-operation-insta-hefe.h"
#include "photos-operation-insta-hefe-curve.h"
#include "photos-operation-insta-hefe-vignette.h"
#include "photos-operation-jpg-guess-sizes.h"
#include "photos-operation-png-guess-sizes.h"
#include "photos-operation-saturation.h"
#include "photos-quarks.h"


static const struct
{
  const gchar *input_format;
  const gchar *output_format;
} REQUIRED_BABL_FISHES[] =
{
  { "R'G'B' u8", "cairo-ARGB32" },
  { "R'G'B' u8", "YA float" }
};


static GeglBuffer *
photos_gegl_buffer_zoom (GeglBuffer *buffer, gdouble zoom, GCancellable *cancellable, GError **error)
{
  GeglBuffer *ret_val = NULL;
  GeglNode *buffer_sink;
  GeglNode *buffer_source;
  GeglNode *graph;
  GeglNode *scale;

  graph = gegl_node_new ();
  buffer_source = gegl_node_new_child (graph, "operation", "gegl:buffer-source", "buffer", buffer, NULL);
  scale = gegl_node_new_child (graph, "operation", "gegl:scale-ratio", "x", zoom, "y", zoom, NULL);
  buffer_sink = gegl_node_new_child (graph, "operation", "gegl:buffer-sink", "buffer", &ret_val, NULL);
  gegl_node_link_many (buffer_source, scale, buffer_sink, NULL);
  gegl_node_process (buffer_sink);

  g_object_unref (graph);
  return ret_val;
}


static void
photos_gegl_buffer_zoom_in_thread_func (GTask *task,
                                        gpointer source_object,
                                        gpointer task_data,
                                        GCancellable *cancellable)
{
  GeglBuffer *buffer = GEGL_BUFFER (source_object);
  GeglBuffer *result = NULL;
  GError *error;
  const gchar *zoom_str = (const gchar *) task_data;
  gchar *endptr;
  gdouble zoom;

  zoom = g_ascii_strtod (zoom_str, &endptr);
  g_assert (*endptr == '\0');

  error = NULL;
  result = photos_gegl_buffer_zoom (buffer, zoom, cancellable, &error);
  if (error != NULL)
    {
      g_task_return_error (task, error);
      goto out;
    }

  g_task_return_pointer (task, g_object_ref (result), g_object_unref);

 out:
  g_clear_object (&result);
}


void
photos_gegl_buffer_zoom_async (GeglBuffer *buffer,
                               gdouble zoom,
                               GCancellable *cancellable,
                               GAsyncReadyCallback callback,
                               gpointer user_data)
{
  GTask *task;
  gchar zoom_str[G_ASCII_DTOSTR_BUF_SIZE];

  g_return_if_fail (GEGL_IS_BUFFER (buffer));
  g_return_if_fail (zoom > 0.0);

  task = g_task_new (buffer, cancellable, callback, user_data);
  g_task_set_source_tag (task, photos_gegl_buffer_zoom_async);

  if (GEGL_FLOAT_EQUAL ((gfloat) zoom, 1.0))
    {
      g_task_return_pointer (task, g_object_ref (buffer), g_object_unref);
      goto out;
    }

  g_ascii_dtostr (zoom_str, G_N_ELEMENTS (zoom_str), zoom);
  g_task_set_task_data (task, g_strdup (zoom_str), g_free);

  g_task_run_in_thread (task, photos_gegl_buffer_zoom_in_thread_func);

 out:
  g_object_unref (task);
}


GeglBuffer *
photos_gegl_buffer_zoom_finish (GeglBuffer *buffer, GAsyncResult *res, GError **error)
{
  GTask *task = G_TASK (res);

  g_return_val_if_fail (g_task_is_valid (res, buffer), NULL);
  g_return_val_if_fail (g_task_get_source_tag (task) == photos_gegl_buffer_zoom_async, NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  return g_task_propagate_pointer (task, error);
}


GeglNode *
photos_gegl_create_orientation_node (GeglNode *parent, GQuark orientation)
{
  GeglNode *ret_val = NULL;
  double degrees = 1.0;

  if (orientation == PHOTOS_ORIENTATION_TOP)
    goto out;

  if (orientation == PHOTOS_ORIENTATION_BOTTOM)
    degrees = -180.0;
  else if (orientation == PHOTOS_ORIENTATION_LEFT)
    degrees = -270.0;
  else if (orientation == PHOTOS_ORIENTATION_RIGHT)
    degrees = -90.0;

  if (degrees < 0.0)
    ret_val = gegl_node_new_child (parent, "operation", "gegl:rotate-on-center", "degrees", degrees, NULL);

 out:
  if (ret_val == NULL)
    ret_val = gegl_node_new_child (parent, "operation", "gegl:nop", NULL);

  return ret_val;
}


GdkPixbuf *
photos_gegl_create_pixbuf_from_node (GeglNode *node)
{
  GdkPixbuf *pixbuf = NULL;
  GeglNode *graph;
  GeglNode *save_pixbuf;

  graph = gegl_node_get_parent (node);
  save_pixbuf = gegl_node_new_child (graph,
                                     "operation", "gegl:save-pixbuf",
                                     "pixbuf", &pixbuf,
                                     NULL);
  gegl_node_link_many (node, save_pixbuf, NULL);
  gegl_node_process (save_pixbuf);
  g_object_unref (save_pixbuf);

  return pixbuf;
}


GeglBuffer *
photos_gegl_dup_buffer_from_node (GeglNode *node, const Babl *format)
{
  GeglBuffer *buffer;
  GeglRectangle bbox;
  gint64 end;
  gint64 start;

  g_return_val_if_fail (GEGL_IS_NODE (node), NULL);

  bbox = gegl_node_get_bounding_box (node);
  buffer = gegl_buffer_new (&bbox, format);

  start = g_get_monotonic_time ();

  gegl_node_blit_buffer (node, buffer, &bbox, 0, GEGL_ABYSS_NONE);

  end = g_get_monotonic_time ();
  photos_debug (PHOTOS_DEBUG_GEGL, "GEGL: Dup Buffer from Node: %" G_GINT64_FORMAT, end - start);

  return buffer;
}


void
photos_gegl_ensure_builtins (void)
{
  static gsize once_init_value = 0;

  if (g_once_init_enter (&once_init_value))
    {
      g_type_ensure (PHOTOS_TYPE_OPERATION_INSTA_CURVE);
      g_type_ensure (PHOTOS_TYPE_OPERATION_INSTA_FILTER);
      g_type_ensure (PHOTOS_TYPE_OPERATION_INSTA_HEFE);
      g_type_ensure (PHOTOS_TYPE_OPERATION_INSTA_HEFE_CURVE);
      g_type_ensure (PHOTOS_TYPE_OPERATION_INSTA_HEFE_VIGNETTE);
      g_type_ensure (PHOTOS_TYPE_OPERATION_JPG_GUESS_SIZES);
      g_type_ensure (PHOTOS_TYPE_OPERATION_PNG_GUESS_SIZES);
      g_type_ensure (PHOTOS_TYPE_OPERATION_SATURATION);

      g_once_init_leave (&once_init_value, 1);
    }
}


GeglBuffer *
photos_gegl_get_buffer_from_node (GeglNode *node, const Babl *format)
{
  GeglBuffer *buffer = NULL;
  GeglNode *buffer_sink;
  GeglNode *graph;
  gint64 end;
  gint64 start;

  graph = gegl_node_get_parent (node);
  buffer_sink = gegl_node_new_child (graph,
                                     "operation", "gegl:buffer-sink",
                                     "buffer", &buffer,
                                     "format", format,
                                     NULL);
  gegl_node_link (node, buffer_sink);

  start = g_get_monotonic_time ();

  gegl_node_process (buffer_sink);

  end = g_get_monotonic_time ();
  photos_debug (PHOTOS_DEBUG_GEGL, "GEGL: Get Buffer from Node: %" G_GINT64_FORMAT, end - start);

  g_object_unref (buffer_sink);

  return buffer;
}


void
photos_gegl_init_fishes (void)
{
  gint64 end;
  gint64 start;
  guint i;

  start = g_get_monotonic_time ();

  for (i = 0; i < G_N_ELEMENTS (REQUIRED_BABL_FISHES); i++)
    {
      const Babl *input_format;
      const Babl *output_format;

      input_format = babl_format (REQUIRED_BABL_FISHES[i].input_format);
      output_format = babl_format (REQUIRED_BABL_FISHES[i].output_format);
      babl_fish (input_format, output_format);
    }

  end = g_get_monotonic_time ();
  photos_debug (PHOTOS_DEBUG_GEGL, "GEGL: Init Fishes: %" G_GINT64_FORMAT, end - start);
}


static gboolean
photos_gegl_processor_process_idle (gpointer user_data)
{
  GTask *task = G_TASK (user_data);
  GeglProcessor *processor;

  processor = GEGL_PROCESSOR (g_task_get_source_object (task));

  if (g_task_return_error_if_cancelled (task))
    goto done;

  if (gegl_processor_work (processor, NULL))
    return G_SOURCE_CONTINUE;

  g_task_return_boolean (task, TRUE);

 done:
  return G_SOURCE_REMOVE;
}


void
photos_gegl_processor_process_async (GeglProcessor *processor,
                                     GCancellable *cancellable,
                                     GAsyncReadyCallback callback,
                                     gpointer user_data)
{
  GTask *task;

  g_return_if_fail (GEGL_IS_PROCESSOR (processor));
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (processor, cancellable, callback, user_data);
  g_task_set_source_tag (task, photos_gegl_processor_process_async);

  g_idle_add_full (G_PRIORITY_DEFAULT_IDLE,
                   photos_gegl_processor_process_idle,
                   g_object_ref (task),
                   g_object_unref);

  g_object_unref (task);
}


gboolean
photos_gegl_processor_process_finish (GeglProcessor *processor, GAsyncResult *res, GError **error)
{
  GTask *task = G_TASK (res);

  g_return_val_if_fail (GEGL_IS_PROCESSOR (processor), FALSE);
  g_return_val_if_fail (g_task_is_valid (res, processor), FALSE);
  g_return_val_if_fail (g_task_get_source_tag (task) == photos_gegl_processor_process_async, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  return g_task_propagate_boolean (task, error);
}


void
photos_gegl_remove_children_from_node (GeglNode *node)
{
  GeglNode *input;
  GeglNode *last;
  GeglNode *output;
  GeglOperation *operation;

  operation = gegl_node_get_gegl_operation (node);
  g_return_if_fail (operation == NULL);

  input = gegl_node_get_input_proxy (node, "input");
  output = gegl_node_get_output_proxy (node, "output");
  last = gegl_node_get_producer (output, "input", NULL);

  while (last != NULL && last != input)
    {
      GeglNode *last2;

      last2 = gegl_node_get_producer (last, "input", NULL);
      gegl_node_remove_child (node, last);
      last = last2;
    }

  gegl_node_link (input, output);
}
