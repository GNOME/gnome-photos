/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2013 – 2017 Red Hat, Inc.
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

/* Based on code from:
 *   + GIMP
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
#include "photos-operation-svg-multiply.h"
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

static const gchar *REQUIRED_GEGL_OPS[] =
{
  "gegl:buffer-sink",
  "gegl:buffer-source",
  "gegl:crop",
  "gegl:exposure",
  "gegl:gray",
  "gegl:load",
  "gegl:noise-reduction",
  "gegl:nop",
  "gegl:scale-ratio",
  "gegl:shadows-highlights",
  "gegl:unsharp-mask",

  /* Used by gegl:load */
  "gegl:jpg-load",
  "gegl:png-load",
  "gegl:raw-load",
  "gegl:text"
};


static void
photos_gegl_buffer_apply_orientation_flip_in_place (guchar *buf, gint bpp, gint n_pixels)
{
  gint i;

  for (i = 0; i < n_pixels / 2; i++)
    {
      gint j;
      guchar *pixel_left = buf + i * bpp;
      guchar *pixel_right = buf + (n_pixels - 1 - i) * bpp;

      for (j = 0; j < bpp; j++)
        {
          guchar tmp = pixel_left[j];

          pixel_left[j] = pixel_right[j];
          pixel_right[j] = tmp;
        }
    }
}


GeglBuffer *
photos_gegl_buffer_apply_orientation (GeglBuffer *buffer_original, GQuark orientation)
{
  const Babl *format;
  g_autoptr (GeglBuffer) buffer_oriented = NULL;
  GeglBuffer *ret_val = NULL;
  GeglRectangle bbox_oriented;
  GeglRectangle bbox_original;
  gint bpp;

  g_return_val_if_fail (GEGL_IS_BUFFER (buffer_original), NULL);
  g_return_val_if_fail (orientation == PHOTOS_ORIENTATION_BOTTOM
                        || orientation == PHOTOS_ORIENTATION_BOTTOM_MIRROR
                        || orientation == PHOTOS_ORIENTATION_LEFT
                        || orientation == PHOTOS_ORIENTATION_LEFT_MIRROR
                        || orientation == PHOTOS_ORIENTATION_RIGHT
                        || orientation == PHOTOS_ORIENTATION_RIGHT_MIRROR
                        || orientation == PHOTOS_ORIENTATION_TOP
                        || orientation == PHOTOS_ORIENTATION_TOP_MIRROR,
                        NULL);

  if (orientation == PHOTOS_ORIENTATION_TOP)
    {
      ret_val = g_object_ref (buffer_original);
      goto out;
    }

  bbox_original = *gegl_buffer_get_extent (buffer_original);

  if (orientation == PHOTOS_ORIENTATION_BOTTOM || orientation == PHOTOS_ORIENTATION_BOTTOM_MIRROR)
    {
      /* angle = 180 degrees */
      /* angle = 180 degrees, axis = vertical; or, axis = horizontal */
      bbox_oriented.height = bbox_original.height;
      bbox_oriented.width = bbox_original.width;
      bbox_oriented.x = bbox_original.x;
      bbox_oriented.y = bbox_original.y;
    }
  else if (orientation == PHOTOS_ORIENTATION_LEFT || orientation == PHOTOS_ORIENTATION_LEFT_MIRROR)
    {
      /* angle = -270 or 90 degrees counterclockwise */
      /* angle = -270 or 90 degrees counterclockwise, axis = horizontal */
      bbox_oriented.height = bbox_original.width;
      bbox_oriented.width = bbox_original.height;
      bbox_oriented.x = bbox_original.x;
      bbox_oriented.y = bbox_original.y;
    }
  else if (orientation == PHOTOS_ORIENTATION_RIGHT || orientation == PHOTOS_ORIENTATION_RIGHT_MIRROR)
    {
      /* angle = -90 or 270 degrees counterclockwise */
      /* angle = -90 or 270 degrees counterclockwise, axis = horizontal */
      bbox_oriented.height = bbox_original.width;
      bbox_oriented.width = bbox_original.height;
      bbox_oriented.x = bbox_original.x;
      bbox_oriented.y = bbox_original.y;
    }
  else if (orientation == PHOTOS_ORIENTATION_TOP_MIRROR)
    {
      /* axis = vertical */
      bbox_oriented.height = bbox_original.height;
      bbox_oriented.width = bbox_original.width;
      bbox_oriented.x = bbox_original.x;
      bbox_oriented.y = bbox_original.y;
    }
  else
    {
      g_return_val_if_reached (NULL);
    }

  format = gegl_buffer_get_format (buffer_original);
  bpp = babl_format_get_bytes_per_pixel (format);
  buffer_oriented = gegl_buffer_new (&bbox_oriented, format);

  if (orientation == PHOTOS_ORIENTATION_BOTTOM || orientation == PHOTOS_ORIENTATION_BOTTOM_MIRROR)
    {
      GeglRectangle bbox_destination;
      GeglRectangle bbox_source;

      /* angle = 180 degrees */
      /* angle = 180 degrees, axis = vertical; or, axis = horizontal */

      g_return_val_if_fail (bbox_oriented.height == bbox_original.height, NULL);
      g_return_val_if_fail (bbox_oriented.width == bbox_original.width, NULL);

      gegl_rectangle_set (&bbox_destination, bbox_oriented.x, bbox_oriented.y, (guint) bbox_oriented.width, 1);

      bbox_source.x = bbox_original.x;
      bbox_source.y = bbox_original.y + bbox_original.height - 1;
      bbox_source.height = 1;
      bbox_source.width = bbox_original.width;

      if (orientation == PHOTOS_ORIENTATION_BOTTOM)
        {
          gint i;
          g_autofree guchar *buf = NULL;

          buf = g_malloc0_n (bbox_oriented.width, bpp);

          for (i = 0; i < bbox_original.height; i++)
            {
              gegl_buffer_get (buffer_original,
                               &bbox_source,
                               1.0,
                               format,
                               buf,
                               GEGL_AUTO_ROWSTRIDE,
                               GEGL_ABYSS_NONE);
              photos_gegl_buffer_apply_orientation_flip_in_place (buf, bpp, bbox_original.width);
              gegl_buffer_set (buffer_oriented, &bbox_destination, 0, format, buf, GEGL_AUTO_ROWSTRIDE);

              bbox_destination.y++;
              bbox_source.y--;
            }
        }
      else
        {
          gint i;

          for (i = 0; i < bbox_original.height; i++)
            {
              gegl_buffer_copy (buffer_original, &bbox_source, GEGL_ABYSS_NONE, buffer_oriented, &bbox_destination);
              bbox_destination.y++;
              bbox_source.y--;
            }
        }
    }
  else if (orientation == PHOTOS_ORIENTATION_LEFT || orientation == PHOTOS_ORIENTATION_LEFT_MIRROR)
    {
      GeglRectangle bbox_source;
      g_autofree guchar *buf = NULL;

      /* angle = -270 or 90 degrees counterclockwise */
      /* angle = -270 or 90 degrees counterclockwise, axis = horizontal */

      g_return_val_if_fail (bbox_oriented.height == bbox_original.width, NULL);
      g_return_val_if_fail (bbox_oriented.width == bbox_original.height, NULL);

      bbox_source.x = bbox_original.x + bbox_original.width - 1;
      bbox_source.y = bbox_original.y;
      bbox_source.height = bbox_original.height;
      bbox_source.width = 1;

      buf = g_malloc0_n (bbox_oriented.width, bpp);

      if (orientation == PHOTOS_ORIENTATION_LEFT)
        {
          GeglRectangle bbox_destination;
          gint i;

          gegl_rectangle_set (&bbox_destination, bbox_oriented.x, bbox_oriented.y, (guint) bbox_oriented.width, 1);

          for (i = 0; i < bbox_original.width; i++)
            {
              gegl_buffer_get (buffer_original,
                               &bbox_source,
                               1.0,
                               format,
                               buf,
                               GEGL_AUTO_ROWSTRIDE,
                               GEGL_ABYSS_NONE);
              gegl_buffer_set (buffer_oriented, &bbox_destination, 0, format, buf, GEGL_AUTO_ROWSTRIDE);
              bbox_destination.y++;
              bbox_source.x--;
            }
        }
      else
        {
          GeglRectangle bbox_destination;
          gint i;

          bbox_destination.x = bbox_oriented.x;
          bbox_destination.y = bbox_oriented.y + bbox_oriented.height - 1;
          bbox_destination.height = 1;
          bbox_destination.width = bbox_oriented.width;

          for (i = 0; i < bbox_original.width; i++)
            {
              gegl_buffer_get (buffer_original,
                               &bbox_source,
                               1.0,
                               format,
                               buf,
                               GEGL_AUTO_ROWSTRIDE,
                               GEGL_ABYSS_NONE);
              gegl_buffer_set (buffer_oriented, &bbox_destination, 0, format, buf, GEGL_AUTO_ROWSTRIDE);
              bbox_destination.y--;
              bbox_source.x--;
            }
        }
    }
  else if (orientation == PHOTOS_ORIENTATION_RIGHT || orientation == PHOTOS_ORIENTATION_RIGHT_MIRROR)
    {
      GeglRectangle bbox_destination;
      GeglRectangle bbox_source;
      g_autofree guchar *buf = NULL;

      /* angle = -90 or 270 degrees counterclockwise */
      /* angle = -90 or 270 degrees counterclockwise, axis = horizontal */

      g_return_val_if_fail (bbox_oriented.height == bbox_original.width, NULL);
      g_return_val_if_fail (bbox_oriented.width == bbox_original.height, NULL);

      gegl_rectangle_set (&bbox_destination, bbox_oriented.x, bbox_oriented.y, 1, (guint) bbox_oriented.height);

      bbox_source.x = bbox_original.x;
      bbox_source.y = bbox_original.y + bbox_original.height - 1;
      bbox_source.height = 1;
      bbox_source.width = bbox_original.width;

      buf = g_malloc0_n (bbox_oriented.height, bpp);

      if (orientation == PHOTOS_ORIENTATION_RIGHT)
        {
          gint i;

          for (i = 0; i < bbox_original.height; i++)
            {
              gegl_buffer_get (buffer_original,
                               &bbox_source,
                               1.0,
                               format,
                               buf,
                               GEGL_AUTO_ROWSTRIDE,
                               GEGL_ABYSS_NONE);
              gegl_buffer_set (buffer_oriented, &bbox_destination, 0, format, buf, GEGL_AUTO_ROWSTRIDE);
              bbox_destination.x++;
              bbox_source.y--;
            }
        }
      else
        {
          gint i;

          for (i = 0; i < bbox_original.height; i++)
            {
              gegl_buffer_get (buffer_original,
                               &bbox_source,
                               1.0,
                               format,
                               buf,
                               GEGL_AUTO_ROWSTRIDE,
                               GEGL_ABYSS_NONE);
              photos_gegl_buffer_apply_orientation_flip_in_place (buf, bpp, bbox_original.width);
              gegl_buffer_set (buffer_oriented, &bbox_destination, 0, format, buf, GEGL_AUTO_ROWSTRIDE);
              bbox_destination.x++;
              bbox_source.y--;
            }
        }
    }
  else if (orientation == PHOTOS_ORIENTATION_TOP_MIRROR)
    {
      GeglRectangle bbox_destination;
      GeglRectangle bbox_source;
      gint i;

      /* axis = vertical */

      g_return_val_if_fail (bbox_oriented.height == bbox_original.height, NULL);
      g_return_val_if_fail (bbox_oriented.width == bbox_original.width, NULL);

      bbox_destination.x = bbox_oriented.x + bbox_oriented.width - 1;
      bbox_destination.y = bbox_oriented.y;
      bbox_destination.height = bbox_oriented.height;
      bbox_destination.width = 1;

      gegl_rectangle_set (&bbox_source, bbox_original.x, bbox_original.y, 1, (guint) bbox_original.height);

      for (i = 0; i < bbox_original.width; i++)
        {
          gegl_buffer_copy (buffer_original, &bbox_source, GEGL_ABYSS_NONE, buffer_oriented, &bbox_destination);
          bbox_destination.x--;
          bbox_source.x++;
        }
    }
  else
    {
      g_return_val_if_reached (NULL);
    }

  ret_val = g_object_ref (buffer_oriented);

 out:
  return ret_val;
}


GeglBuffer *
photos_gegl_buffer_new_from_pixbuf (GdkPixbuf *pixbuf)
{
  const Babl *format;
  GeglBuffer *buffer = NULL;
  GeglRectangle bbox;
  gint height;
  gint stride;
  gint width;
  const guint8 *pixels;

  g_return_val_if_fail (GDK_IS_PIXBUF (pixbuf), NULL);

  height = gdk_pixbuf_get_height (pixbuf);
  width = gdk_pixbuf_get_width (pixbuf);
  gegl_rectangle_set (&bbox, 0, 0, (guint) width, (guint) height);

  if (gdk_pixbuf_get_has_alpha (pixbuf))
    format = babl_format ("R'G'B'A u8");
  else
    format = babl_format ("R'G'B' u8");

  buffer = gegl_buffer_new (&bbox, format);

  pixels = gdk_pixbuf_read_pixels (pixbuf);
  stride = gdk_pixbuf_get_rowstride (pixbuf);
  gegl_buffer_set (buffer, &bbox, 0, format, pixels, stride);

  return buffer;
}


static GeglBuffer *
photos_gegl_buffer_zoom (GeglBuffer *buffer, gdouble zoom, GCancellable *cancellable, GError **error)
{
  GeglBuffer *ret_val = NULL;
  GeglNode *buffer_sink;
  GeglNode *buffer_source;
  g_autoptr (GeglNode) graph = NULL;
  GeglNode *scale;

  graph = gegl_node_new ();
  buffer_source = gegl_node_new_child (graph, "operation", "gegl:buffer-source", "buffer", buffer, NULL);
  scale = gegl_node_new_child (graph, "operation", "gegl:scale-ratio", "x", zoom, "y", zoom, NULL);
  buffer_sink = gegl_node_new_child (graph, "operation", "gegl:buffer-sink", "buffer", &ret_val, NULL);
  gegl_node_link_many (buffer_source, scale, buffer_sink, NULL);
  gegl_node_process (buffer_sink);

  return ret_val;
}


static void
photos_gegl_buffer_zoom_in_thread_func (GTask *task,
                                        gpointer source_object,
                                        gpointer task_data,
                                        GCancellable *cancellable)
{
  GeglBuffer *buffer = GEGL_BUFFER (source_object);
  g_autoptr (GeglBuffer) result = NULL;
  const gchar *zoom_str = (const gchar *) task_data;
  gchar *endptr;
  gdouble zoom;

  zoom = g_ascii_strtod (zoom_str, &endptr);
  g_assert (*endptr == '\0');

  {
    g_autoptr (GError) error = NULL;

    result = photos_gegl_buffer_zoom (buffer, zoom, cancellable, &error);
    if (error != NULL)
      {
        g_task_return_error (task, g_steal_pointer (&error));
        goto out;
      }
  }

  g_task_return_pointer (task, g_object_ref (result), g_object_unref);

 out:
  return;
}


void
photos_gegl_buffer_zoom_async (GeglBuffer *buffer,
                               gdouble zoom,
                               GCancellable *cancellable,
                               GAsyncReadyCallback callback,
                               gpointer user_data)
{
  g_autoptr (GTask) task = NULL;
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
  return;
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


GdkPixbuf *
photos_gegl_create_pixbuf_from_node (GeglNode *node)
{
  GdkPixbuf *pixbuf = NULL;
  g_autoptr (GeglBuffer) buffer = NULL;

  buffer = photos_gegl_get_buffer_from_node (node, NULL);
  pixbuf = photos_gegl_pixbuf_new_from_buffer (buffer);

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
      g_type_ensure (PHOTOS_TYPE_OPERATION_SVG_MULTIPLY);

      g_once_init_leave (&once_init_value, 1);
    }
}


GeglBuffer *
photos_gegl_get_buffer_from_node (GeglNode *node, const Babl *format)
{
  GeglBuffer *buffer = NULL;
  g_autoptr (GeglNode) buffer_sink = NULL;
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

  return buffer;
}


void
photos_gegl_init (void)
{
  GeglConfig *config;
  gint threads;
  guint n_processors;

  gegl_init (NULL, NULL);

  n_processors = g_get_num_processors ();
  g_return_if_fail (n_processors > 0);

  /* The number of threads should match the number of physical CPU
   * cores, not the number of virtual hyper-threading cores. In the
   * absence of an API to get the number of physical CPU cores, we
   * assume that a number higher than one is indicative of
   * hyper-threading, and hence divide by two.
   */
  threads = (gint) (n_processors > 1 ? n_processors / 2 : n_processors);

  config = gegl_config ();
  g_object_set (config, "application-license", "GPL3", NULL);
  g_object_set (config, "threads", threads, NULL);
  g_object_set (config, "use-opencl", FALSE, NULL);
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


GdkPixbuf *
photos_gegl_pixbuf_new_from_buffer (GeglBuffer *buffer)
{
  const Babl *format_buffer;
  const Babl *format_pixbuf;
  g_autoptr (GBytes) bytes = NULL;
  GdkPixbuf *pixbuf = NULL;
  GeglRectangle bbox;
  gboolean has_alpha;
  gint stride;
  gpointer buf = NULL;
  gsize size;

  g_return_val_if_fail (GEGL_IS_BUFFER (buffer), NULL);

  bbox = *gegl_buffer_get_extent (buffer);
  format_buffer = gegl_buffer_get_format (buffer);
  has_alpha = babl_format_has_alpha (format_buffer);

  if (has_alpha)
    format_pixbuf = babl_format ("R'G'B'A u8");
  else
    format_pixbuf = babl_format ("R'G'B' u8");

  stride = gdk_pixbuf_calculate_rowstride (GDK_COLORSPACE_RGB, has_alpha, 8, bbox.width, bbox.height);
  if (stride == -1)
    goto out;

  buf = g_malloc0_n ((gsize) bbox.height, (gsize) stride);
  gegl_buffer_get (buffer, &bbox, 1.0, format_pixbuf, buf, stride, GEGL_ABYSS_NONE);

  size = (gsize) bbox.height * (gsize) stride;
  bytes = g_bytes_new_take (buf, size);
  pixbuf = gdk_pixbuf_new_from_bytes (bytes, GDK_COLORSPACE_RGB, has_alpha, 8, bbox.width, bbox.height, stride);

 out:
  return pixbuf;
}


static gboolean
photos_gegl_processor_process_idle (gpointer user_data)
{
  GTask *task = G_TASK (user_data);
  GeglProcessor *processor;
  gboolean more_work;
  gint64 end;
  gint64 start;
  gsize processing_time;

  processor = GEGL_PROCESSOR (g_task_get_source_object (task));
  processing_time = GPOINTER_TO_SIZE (g_task_get_task_data (task));

  if (g_task_return_error_if_cancelled (task))
    goto done;

  start = g_get_monotonic_time ();

  more_work = gegl_processor_work (processor, NULL);

  end = g_get_monotonic_time ();
  processing_time += (gsize) (end - start);
  g_task_set_task_data (task, GSIZE_TO_POINTER (processing_time), NULL);

  if (more_work)
    return G_SOURCE_CONTINUE;

  photos_debug (PHOTOS_DEBUG_GEGL, "GEGL: Processor: %" G_GSIZE_FORMAT, processing_time);

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
  g_autoptr (GTask) task = NULL;

  g_return_if_fail (GEGL_IS_PROCESSOR (processor));
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (processor, cancellable, callback, user_data);
  g_task_set_source_tag (task, photos_gegl_processor_process_async);
  g_task_set_task_data (task, GSIZE_TO_POINTER (0), NULL);

  g_idle_add_full (G_PRIORITY_DEFAULT_IDLE,
                   photos_gegl_processor_process_idle,
                   g_object_ref (task),
                   g_object_unref);
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


gboolean
photos_gegl_sanity_check (void)
{
  GeglConfig *config;
  gboolean ret_val = TRUE;
  gboolean use_opencl;
  gint threads;
  guint i;

  config = gegl_config ();
  g_object_get (config, "threads", &threads, "use-opencl", &use_opencl, NULL);
  photos_debug (PHOTOS_DEBUG_GEGL, "GEGL: Threads: %d", threads);
  photos_debug (PHOTOS_DEBUG_GEGL, "GEGL: Using OpenCL: %s", use_opencl ? "yes" : "no");

  for (i = 0; i < G_N_ELEMENTS (REQUIRED_GEGL_OPS); i++)
    {
      if (!gegl_has_operation (REQUIRED_GEGL_OPS[i]))
        {
          g_warning ("Unable to find GEGL operation %s: Check your GEGL install", REQUIRED_GEGL_OPS[i]);
          ret_val = FALSE;
          break;
        }
    }

  return ret_val;
}
