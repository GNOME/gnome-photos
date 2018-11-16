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
#include <glib.h>

#include "photos-debug.h"
#include "photos-gegl.h"
#include "photos-quarks.h"


typedef struct _PhotosTestGeglFixture PhotosTestGeglFixture;

struct _PhotosTestGeglFixture
{
  GFile *destination;
  GFile *source;
  GeglBuffer *buffer;
};


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
photos_test_gegl_setup (PhotosTestGeglFixture *fixture, gconstpointer user_data)
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
  gboolean even_dimensions = GPOINTER_TO_INT (user_data);
  gdouble crop_dimension;

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

  crop_dimension = even_dimensions ? 200.0 : 195;
  crop = gegl_node_new_child (graph,
                              "operation", "gegl:crop",
                              "height", crop_dimension,
                              "width", crop_dimension,
                              NULL);

  path_d = gegl_path_new_from_string ("M0,50 "
                                      "C0,78 24,100 50,100 "
                                      "C77,100 100,78 100,50 "
                                      "C100,45 99,40 98,35 "
                                      "C82,35 66,35 50,35 "
                                      "C42,35 35,42 35,50 "
                                      "C35,58 42,65 50,65 "
                                      "C56,65 61,61 64,56 "
                                      "C67,51 75,55 73,60 "
                                      "C69,69 60,75 50,75 "
                                      "C36,75 25,64 25,50 "
                                      "C25,36 36,25 50,25 "
                                      "L93,25 "
                                      "C83,9 67,0 49,0 "
                                      "C25,0 0,20 0,50 "
                                      "z");
  path_fill = gegl_color_new ("rgb(0.0, 0.6, 1.0)");
  path_stroke = gegl_color_new ("rgb(0.0, 0.6, 1.0)");
  path = gegl_node_new_child (graph,
                              "operation", "gegl:path",
                              "d", path_d,
                              "fill", path_fill,
                              "stroke", path_stroke,
                              "stroke-hardness", 1.0,
                              "stroke-width", 2.0,
                              NULL);

  translate = gegl_node_new_child (graph, "operation", "gegl:translate", "x", 40.0, "y", 40.0, NULL);

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

  g_object_unref (checkerboard_color1);
  g_object_unref (checkerboard_color2);
  g_object_unref (path_fill);
  g_object_unref (path_stroke);
  g_object_unref (path_d);
}


static void
photos_test_gegl_teardown (PhotosTestGeglFixture *fixture, gconstpointer user_data)
{
  g_object_unref (fixture->buffer);

  g_file_delete (fixture->destination, NULL, NULL);
  g_object_unref (fixture->destination);

  g_file_delete (fixture->source, NULL, NULL);
  g_object_unref (fixture->source);
}


static void
photos_test_gegl_buffer_apply_orientation_bottom_0 (PhotosTestGeglFixture *fixture, gconstpointer user_data)
{
  g_autoptr (GeglBuffer) buffer_oriented = NULL;
  g_autofree gchar *checksum = NULL;

  buffer_oriented = photos_gegl_buffer_apply_orientation (fixture->buffer, PHOTOS_ORIENTATION_BOTTOM);
  photos_test_gegl_buffer_save_to_file (buffer_oriented, fixture->destination);
  checksum = photos_gegl_compute_checksum_for_buffer (G_CHECKSUM_SHA256, buffer_oriented);
  g_assert_cmpstr (checksum, ==, "0497b2a1ff9aa9f6e7aa526b55157133d01331a0869602a4d4cc1f7a7bef420e");
}


static void
photos_test_gegl_buffer_apply_orientation_bottom_1 (PhotosTestGeglFixture *fixture, gconstpointer user_data)
{
  g_autoptr (GeglBuffer) buffer_oriented = NULL;
  g_autofree gchar *checksum = NULL;

  buffer_oriented = photos_gegl_buffer_apply_orientation (fixture->buffer, PHOTOS_ORIENTATION_BOTTOM);
  photos_test_gegl_buffer_save_to_file (buffer_oriented, fixture->destination);
  checksum = photos_gegl_compute_checksum_for_buffer (G_CHECKSUM_SHA256, buffer_oriented);
  g_assert_cmpstr (checksum, ==, "3d48c4f33015c4d0a0cbb9d360d3d2e9b76469ea103fc848fe87e2c924e9fac6");
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

  g_test_add ("/gegl/buffer/apply_orientation/bottom-0",
              PhotosTestGeglFixture,
              GINT_TO_POINTER (TRUE),
              photos_test_gegl_setup,
              photos_test_gegl_buffer_apply_orientation_bottom_0,
              photos_test_gegl_teardown);

  g_test_add ("/gegl/buffer/apply_orientation/bottom-1",
              PhotosTestGeglFixture,
              GINT_TO_POINTER (FALSE),
              photos_test_gegl_setup,
              photos_test_gegl_buffer_apply_orientation_bottom_1,
              photos_test_gegl_teardown);

  exit_status = g_test_run ();

  gegl_exit ();
  return exit_status;
}
