/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2018 Red Hat, Inc.
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

#include <locale.h>

#include <gegl.h>
#include <gio/gio.h>
#include <glib.h>

#include "photos-debug.h"
#include "photos-gegl.h"
#include "photos-gegl-buffer-loader.h"
#include "photos-gegl-buffer-loader-builder.h"


typedef struct _PhotosTestGeglBufferFixture PhotosTestGeglBufferFixture;

struct _PhotosTestGeglBufferFixture
{
  const Babl *format;
  GAsyncResult *res;
  GFile *destination;
  GFile *source;
  GMainContext *context;
  GMainLoop *loop;
  GeglBuffer *buffer;
};


static gchar *
photos_test_gegl_buffer_filename_to_uri (const gchar *filename)
{
  g_autoptr (GFile) file = NULL;
  g_autofree gchar *path_relative = NULL;
  gchar *uri = NULL;

  path_relative = g_test_build_filename (G_TEST_DIST, filename, NULL);
  file = g_file_new_for_path (path_relative);
  uri = g_file_get_uri (file);
  return uri;
}


static void
photos_test_gegl_buffer_save_to_file (GeglBuffer *buffer, GFile *file)
{
  GeglNode *buffer_source;
  GeglNode *png_save;
  g_autoptr (GeglNode) graph = NULL;
  g_autofree gchar *path = NULL;

  g_assert_true (GEGL_IS_BUFFER (buffer));
  g_assert_true (G_IS_FILE (file));

  graph = gegl_node_new ();
  buffer_source = gegl_node_new_child (graph, "operation", "gegl:buffer-source", "buffer", buffer, NULL);

  path = g_file_get_path (file);
  png_save = gegl_node_new_child (graph, "operation", "gegl:png-save", "bitdepth", 8, "path", path, NULL);

  gegl_node_link (buffer_source, png_save);
  gegl_node_process (png_save);
}


static void
photos_test_gegl_buffer_setup (PhotosTestGeglBufferFixture *fixture, gconstpointer user_data)
{
  const Babl *format;
  g_autoptr (GeglBuffer) buffer = NULL;
  GeglColor *checkerboard_color1 = NULL; /* TODO: use g_autoptr */
  GeglColor *checkerboard_color2 = NULL; /* TODO: use g_autoptr */
  GeglColor *path_fill = NULL; /* TODO: use g_autoptr */
  GeglColor *path_stroke = NULL; /* TODO: use g_autoptr */
  GeglNode *buffer_sink;
  GeglNode *checkerboard;
  GeglNode *convert_format;
  GeglNode *crop;
  GeglNode *over;
  GeglNode *path;
  GeglNode *translate;
  g_autoptr (GeglNode) graph = NULL;
  GeglPath *path_d = NULL; /* TODO: use g_autoptr */

  {
    g_autoptr (GError) error = NULL;
    g_autoptr (GFileIOStream) iostream = NULL;

    fixture->destination = g_file_new_tmp (PACKAGE_TARNAME "-destination-XXXXXX.png", &iostream, &error);
    g_assert_no_error (error);
  }

  {
    g_autoptr (GError) error = NULL;
    g_autoptr (GFileIOStream) iostream = NULL;

    fixture->source = g_file_new_tmp (PACKAGE_TARNAME "-source-XXXXXX.png", &iostream, &error);
    g_assert_no_error (error);
  }

  fixture->context = g_main_context_new ();
  g_main_context_push_thread_default (fixture->context);
  fixture->loop = g_main_loop_new (fixture->context, FALSE);

  graph = gegl_node_new ();

  checkerboard_color1 = gegl_color_new ("rgb(0.25, 0.25, 0.25)");
  checkerboard_color2 = gegl_color_new ("rgb(0.75, 0.75, 0.75)");
  checkerboard = gegl_node_new_child (graph,
                                      "operation", "gegl:checkerboard",
                                      "color1", checkerboard_color1,
                                      "color2", checkerboard_color2,
                                      "x", 5,
                                      "y", 5,
                                      NULL);

  crop = gegl_node_new_child (graph,
                              "operation", "gegl:crop",
                              "height", 2000.0,
                              "width", 640.0,
                              NULL);

  path_d = gegl_path_new_from_string ("M708,374.36218 "
                                      "C0,46.94421 -38.0558,85 -85,85 "
                                      "C-46.9442,0 -85,-38.05579 -85,-85 "
                                      "C0,-46.9442 38.0558,-85 85,-85 "
                                      "C46.9442,0 85,38.0558 85,85 "
                                      "z");
  path_fill = gegl_color_new ("#d3d7cf");
  //path_stroke = gegl_color_new ("rgb(0.0, 0.6, 1.0)");
  path = gegl_node_new_child (graph,
                              "operation", "gegl:path",
                              "d", path_d,
                              "fill", path_fill,
                              //"stroke", path_stroke,
                              //"stroke-hardness", 1.0,
                              //"stroke-width", 2.0,
                              "transform", "translate(817,157)",
                              NULL);

  translate = gegl_node_new_child (graph, "operation", "gegl:nop", NULL);

  over = gegl_node_new_child (graph, "operation", "svg:src-over", NULL);

  format = babl_format ("R'G'B'A u8");
  convert_format = gegl_node_new_child (graph, "operation", "gegl:convert-format", "format", format, NULL);

  buffer_sink = gegl_node_new_child (graph, "operation", "gegl:buffer-sink", "buffer", &buffer, NULL);

  gegl_node_link (path, translate);
  gegl_node_connect_to (translate, "output", over, "aux");
  gegl_node_link_many (checkerboard, crop, over, convert_format, buffer_sink, NULL);
  gegl_node_process (buffer_sink);

  fixture->buffer = g_object_ref (buffer);
  photos_test_gegl_buffer_save_to_file (fixture->buffer, fixture->source);

  fixture->format = gegl_buffer_get_format (fixture->buffer);
  g_assert_true (fixture->format == format);

  g_object_unref (checkerboard_color1);
  g_object_unref (checkerboard_color2);
  g_object_unref (path_fill);
  g_object_unref (path_stroke);
  g_object_unref (path_d);
}


static void
photos_test_gegl_buffer_teardown (PhotosTestGeglBufferFixture *fixture, gconstpointer user_data)
{
  g_clear_object (&fixture->res);

  g_file_delete (fixture->destination, NULL, NULL);
  g_object_unref (fixture->destination);

  g_file_delete (fixture->source, NULL, NULL);
  g_object_unref (fixture->source);

  g_main_context_pop_thread_default (fixture->context);
  g_main_context_unref (fixture->context);
  g_main_loop_unref (fixture->loop);

  g_object_unref (fixture->buffer);
}


static void
photos_test_gegl_buffer_async (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosTestGeglBufferFixture *fixture = (PhotosTestGeglBufferFixture *) user_data;

  g_assert_null (fixture->res);
  fixture->res = g_object_ref (res);
  g_main_loop_quit (fixture->loop);
}


static void
photos_test_gegl_buffer_loader_builder (void)
{
  GFile *file_loader;
  g_autoptr (GFile) file_builder = NULL;
  GeglBuffer *buffer;
  g_autoptr (PhotosGeglBufferLoader) loader = NULL;
  g_autoptr (PhotosGeglBufferLoaderBuilder) builder = NULL;
  gint height;
  gint width;

  builder = photos_gegl_buffer_loader_builder_new ();

  file_builder = g_file_new_for_uri ("resource:///org/gnome/Photos/gegl/vignette.png");
  photos_gegl_buffer_loader_builder_set_file (builder, file_builder);

  photos_gegl_buffer_loader_builder_set_height (builder, 200);
  photos_gegl_buffer_loader_builder_set_keep_aspect_ratio (builder, FALSE);
  photos_gegl_buffer_loader_builder_set_width (builder, 300);

  loader = photos_gegl_buffer_loader_builder_to_loader (builder);
  g_assert_true (PHOTOS_IS_GEGL_BUFFER_LOADER (loader));

  buffer = photos_gegl_buffer_loader_get_buffer (loader);
  g_assert_null (buffer);

  file_loader = photos_gegl_buffer_loader_get_file (loader);
  g_assert_true (file_loader == file_builder);

  height = photos_gegl_buffer_loader_get_height (loader);
  g_assert_cmpint (height, ==, 200);

  g_assert_false (photos_gegl_buffer_loader_get_keep_aspect_ratio (loader));

  width = photos_gegl_buffer_loader_get_width (loader);
  g_assert_cmpint (width, ==, 300);
}


static void
photos_test_gegl_buffer_loader_builder_defaults (void)
{
  GFile *file = NULL;
  GeglBuffer *buffer;
  g_autoptr (PhotosGeglBufferLoader) loader = NULL;
  g_autoptr (PhotosGeglBufferLoaderBuilder) builder = NULL;
  gint height;
  gint width;

  builder = photos_gegl_buffer_loader_builder_new ();
  loader = photos_gegl_buffer_loader_builder_to_loader (builder);
  g_assert_true (PHOTOS_IS_GEGL_BUFFER_LOADER (loader));

  buffer = photos_gegl_buffer_loader_get_buffer (loader);
  g_assert_null (buffer);

  file = photos_gegl_buffer_loader_get_file (loader);
  g_assert_null (file);

  height = photos_gegl_buffer_loader_get_height (loader);
  g_assert_cmpint (height, ==, -1);

  g_assert_true (photos_gegl_buffer_loader_get_keep_aspect_ratio (loader));

  width = photos_gegl_buffer_loader_get_width (loader);
  g_assert_cmpint (width, ==, -1);
}


static void
photos_test_gegl_buffer_new_from_file_0 (PhotosTestGeglBufferFixture *fixture, gconstpointer user_data)
{
  g_assert_not_reached ();
}


gint
main (gint argc, gchar *argv[])
{
  gint exit_status;

  setlocale (LC_ALL, "");
  g_setenv ("GEGL_THREADS", "1", FALSE);
  g_test_init (&argc, &argv, NULL);
  photos_debug_init ();
  photos_gegl_init ();
  photos_gegl_ensure_builtins ();

  g_test_add_func ("/gegl/buffer/loader-builder", photos_test_gegl_buffer_loader_builder);
  g_test_add_func ("/gegl/buffer/loader-builder-defaults", photos_test_gegl_buffer_loader_builder_defaults);

  g_test_add ("/gegl/buffer/new/from-file-0",
              PhotosTestGeglBufferFixture,
              NULL,
              photos_test_gegl_buffer_setup,
              photos_test_gegl_buffer_new_from_file_0,
              photos_test_gegl_buffer_teardown);

  exit_status = g_test_run ();

  gegl_exit ();
  return exit_status;
}
