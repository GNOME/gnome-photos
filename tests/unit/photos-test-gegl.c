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
  const Babl *format;
  GAsyncResult *res;
  GFile *destination;
  GFile *source;
  GMainContext *context;
  GMainLoop *loop;
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
photos_test_gegl_setup (PhotosTestGeglFixture *fixture, gdouble height, gdouble width)
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
                              "height", height,
                              "width", width,
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

  fixture->format = gegl_buffer_get_format (fixture->buffer);
  g_assert_true (fixture->format == format);

  g_object_unref (checkerboard_color1);
  g_object_unref (checkerboard_color2);
  g_object_unref (path_fill);
  g_object_unref (path_stroke);
  g_object_unref (path_d);
}


static void
photos_test_gegl_setup_even_dimensions (PhotosTestGeglFixture *fixture, gconstpointer user_data)
{
  g_autofree gchar *checksum = NULL;

  photos_test_gegl_setup (fixture, 200.0, 200.0);

  checksum = photos_gegl_compute_checksum_for_buffer (G_CHECKSUM_SHA256, fixture->buffer);
  g_assert_cmpstr (checksum, ==, "4eaa696bb0758b890cb4764a96eb6f88f7c7a0f7ca8042f381381fc9ffdac19b");
}


static void
photos_test_gegl_setup_odd_dimensions (PhotosTestGeglFixture *fixture, gconstpointer user_data)
{
  g_autofree gchar *checksum = NULL;

  photos_test_gegl_setup (fixture, 195.0, 195.0);

  checksum = photos_gegl_compute_checksum_for_buffer (G_CHECKSUM_SHA256, fixture->buffer);
  g_assert_cmpstr (checksum, ==, "fb42e2fcd0959a73e7bce22f3a549c03406598a745e7f5bb7feaa38e836fd7a1");
}


static void
photos_test_gegl_teardown (PhotosTestGeglFixture *fixture, gconstpointer user_data)
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
photos_test_gegl_async (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosTestGeglFixture *fixture = (PhotosTestGeglFixture *) user_data;

  g_assert_null (fixture->res);
  fixture->res = g_object_ref (res);
  g_main_loop_quit (fixture->loop);
}


static void
photos_test_gegl_buffer_check_orientation (PhotosTestGeglFixture *fixture,
                                           GQuark orientation,
                                           const gchar *checksum)
{
  const Babl *format_oriented;
  g_autoptr (GeglBuffer) buffer_oriented = NULL;
  g_autofree gchar *checksum_oriented = NULL;

  buffer_oriented = photos_gegl_buffer_apply_orientation (fixture->buffer, orientation);
  g_assert_true (GEGL_IS_BUFFER (buffer_oriented));

  photos_test_gegl_buffer_save_to_file (buffer_oriented, fixture->destination);

  format_oriented = gegl_buffer_get_format (buffer_oriented);
  g_assert_true (format_oriented == fixture->format);

  checksum_oriented = photos_gegl_compute_checksum_for_buffer (G_CHECKSUM_SHA256, buffer_oriented);
  g_assert_cmpstr (checksum_oriented, ==, checksum);
}


static void
photos_test_gegl_buffer_check_zoom (PhotosTestGeglFixture *fixture, double zoom, const gchar *checksum)
{
  g_autoptr (GeglBuffer) buffer_zoomed = NULL;
  g_autofree gchar *checksum_zoomed = NULL;

  photos_gegl_buffer_zoom_async (fixture->buffer, zoom, NULL, photos_test_gegl_async, fixture);
  g_main_loop_run (fixture->loop);

  {
    g_autoptr (GError) error = NULL;

    buffer_zoomed = photos_gegl_buffer_zoom_finish (fixture->buffer, fixture->res, &error);
    g_assert_no_error (error);
  }

  g_assert_true (GEGL_IS_BUFFER (buffer_zoomed));
  photos_test_gegl_buffer_save_to_file (buffer_zoomed, fixture->destination);

  checksum_zoomed = photos_gegl_compute_checksum_for_buffer (G_CHECKSUM_SHA256, buffer_zoomed);
  g_assert_cmpstr (checksum_zoomed, ==, checksum);
}


static void
photos_test_gegl_buffer_apply_orientation_bottom_0 (PhotosTestGeglFixture *fixture, gconstpointer user_data)
{
  photos_test_gegl_buffer_check_orientation (fixture,
                                             PHOTOS_ORIENTATION_BOTTOM,
                                             "0497b2a1ff9aa9f6e7aa526b55157133d01331a0869602a4d4cc1f7a7bef420e");
}


static void
photos_test_gegl_buffer_apply_orientation_bottom_1 (PhotosTestGeglFixture *fixture, gconstpointer user_data)
{
  photos_test_gegl_buffer_check_orientation (fixture,
                                             PHOTOS_ORIENTATION_BOTTOM,
                                             "3d48c4f33015c4d0a0cbb9d360d3d2e9b76469ea103fc848fe87e2c924e9fac6");
}


static void
photos_test_gegl_buffer_apply_orientation_bottom_mirror_0 (PhotosTestGeglFixture *fixture, gconstpointer user_data)
{
  photos_test_gegl_buffer_check_orientation (fixture,
                                             PHOTOS_ORIENTATION_BOTTOM_MIRROR,
                                             "13fda7c43a734c14a0bde04bf7f02439ffeea3ae96df83257c61d26a162413dc");
}


static void
photos_test_gegl_buffer_apply_orientation_bottom_mirror_1 (PhotosTestGeglFixture *fixture, gconstpointer user_data)
{
  photos_test_gegl_buffer_check_orientation (fixture,
                                             PHOTOS_ORIENTATION_BOTTOM_MIRROR,
                                             "4f0baf15c92979db451114f0a8ed7cd5f420bd5cd78fc608454f05d6d18fef22");
}


static void
photos_test_gegl_buffer_apply_orientation_left_0 (PhotosTestGeglFixture *fixture, gconstpointer user_data)
{
  photos_test_gegl_buffer_check_orientation (fixture,
                                             PHOTOS_ORIENTATION_LEFT,
                                             "45a8e2c222c6f74b9d784b7bb521f871f8badb6ecd5e185505a8ed701505af11");
}


static void
photos_test_gegl_buffer_apply_orientation_left_1 (PhotosTestGeglFixture *fixture, gconstpointer user_data)
{
  photos_test_gegl_buffer_check_orientation (fixture,
                                             PHOTOS_ORIENTATION_LEFT,
                                             "d01f6c2acef43a97892aca978e20ba2bc51eff270b5eb8afed7c7bb9688ac0f0");
}


static void
photos_test_gegl_buffer_apply_orientation_left_mirror_0 (PhotosTestGeglFixture *fixture, gconstpointer user_data)
{
  photos_test_gegl_buffer_check_orientation (fixture,
                                             PHOTOS_ORIENTATION_LEFT_MIRROR,
                                             "2459ca11c8d9e851ce161f1d0d61d9cbe7279d54585b42b71949eb3031faab20");
}


static void
photos_test_gegl_buffer_apply_orientation_left_mirror_1 (PhotosTestGeglFixture *fixture, gconstpointer user_data)
{
  photos_test_gegl_buffer_check_orientation (fixture,
                                             PHOTOS_ORIENTATION_LEFT_MIRROR,
                                             "1e6697c518615474b68f16620ccab1c0cbd3c719e38d18ddbd34034734913963");
}


static void
photos_test_gegl_buffer_apply_orientation_right_0 (PhotosTestGeglFixture *fixture, gconstpointer user_data)
{
  photos_test_gegl_buffer_check_orientation (fixture,
                                             PHOTOS_ORIENTATION_RIGHT,
                                             "20b01f6e841a06ec8445c6446db4abe5ad65a3497044c5f5fcf3bcac1e4cc80a");
}


static void
photos_test_gegl_buffer_apply_orientation_right_1 (PhotosTestGeglFixture *fixture, gconstpointer user_data)
{
  photos_test_gegl_buffer_check_orientation (fixture,
                                             PHOTOS_ORIENTATION_RIGHT,
                                             "d52e677a3239e361c3811fdc6ccc709d0dea58378afea9c9ccebe087e7d6f7f9");
}


static void
photos_test_gegl_buffer_apply_orientation_right_mirror_0 (PhotosTestGeglFixture *fixture, gconstpointer user_data)
{
  photos_test_gegl_buffer_check_orientation (fixture,
                                             PHOTOS_ORIENTATION_RIGHT_MIRROR,
                                             "9f2e6fe50c65cdcd4a1268dfba9f3102206549f6cd6c8a37d74d374ba8451445");
}


static void
photos_test_gegl_buffer_apply_orientation_right_mirror_1 (PhotosTestGeglFixture *fixture, gconstpointer user_data)
{
  photos_test_gegl_buffer_check_orientation (fixture,
                                             PHOTOS_ORIENTATION_RIGHT_MIRROR,
                                             "ba36edfcc9931a643b79ab1b4b5559e7c53c3de996c4c4c8dd92b17b92bc2d7c");
}


static void
photos_test_gegl_buffer_apply_orientation_top_0 (PhotosTestGeglFixture *fixture, gconstpointer user_data)
{
  photos_test_gegl_buffer_check_orientation (fixture,
                                             PHOTOS_ORIENTATION_TOP,
                                             "4eaa696bb0758b890cb4764a96eb6f88f7c7a0f7ca8042f381381fc9ffdac19b");
}


static void
photos_test_gegl_buffer_apply_orientation_top_1 (PhotosTestGeglFixture *fixture, gconstpointer user_data)
{
  photos_test_gegl_buffer_check_orientation (fixture,
                                             PHOTOS_ORIENTATION_TOP,
                                             "fb42e2fcd0959a73e7bce22f3a549c03406598a745e7f5bb7feaa38e836fd7a1");
}


static void
photos_test_gegl_buffer_apply_orientation_top_mirror_0 (PhotosTestGeglFixture *fixture, gconstpointer user_data)
{
  photos_test_gegl_buffer_check_orientation (fixture,
                                             PHOTOS_ORIENTATION_TOP_MIRROR,
                                             "c3bdf8f600308a9dc0f9d4ba040df3978bdd1ec95350edc18059ac130ad4c8e9");
}


static void
photos_test_gegl_buffer_apply_orientation_top_mirror_1 (PhotosTestGeglFixture *fixture, gconstpointer user_data)
{
  photos_test_gegl_buffer_check_orientation (fixture,
                                             PHOTOS_ORIENTATION_TOP_MIRROR,
                                             "381f2e004c5e5d0a2b22e014cda830cbb6007118dd5aaea2e812705d086f89d0");
}


static void
photos_test_gegl_buffer_zoom_in_0 (PhotosTestGeglFixture *fixture, gconstpointer user_data)
{
  photos_test_gegl_buffer_check_zoom (fixture,
                                      1.4,
                                      "870ea2a5096a642484c13c7a280137daf091f0b4a72bd6648956a2fb28f2962d");
}


static void
photos_test_gegl_buffer_zoom_in_1 (PhotosTestGeglFixture *fixture, gconstpointer user_data)
{
  photos_test_gegl_buffer_check_zoom (fixture,
                                      4.0,
                                      "a4b3fd6dc09bd802a39f7bc4ff4fc796ca2f7a3a109c0cdbff67088d0326f69a");
}


static void
photos_test_gegl_buffer_zoom_nop (PhotosTestGeglFixture *fixture, gconstpointer user_data)
{
  photos_test_gegl_buffer_check_zoom (fixture,
                                      1.0,
                                      "4eaa696bb0758b890cb4764a96eb6f88f7c7a0f7ca8042f381381fc9ffdac19b");
}


static void
photos_test_gegl_buffer_zoom_out_0 (PhotosTestGeglFixture *fixture, gconstpointer user_data)
{
  photos_test_gegl_buffer_check_zoom (fixture,
                                      0.6,
                                      "2ef3362a9d874742fef8c042eb406a648148ef66bd0b222167c1d6caafee9469");
}


static void
photos_test_gegl_buffer_zoom_out_1 (PhotosTestGeglFixture *fixture, gconstpointer user_data)
{
  photos_test_gegl_buffer_check_zoom (fixture,
                                      0.25,
                                      "09f1aff1b8d8936255df661095694bdf871264d992ccdea5b5193f24a0af5ea2");
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
              NULL,
              photos_test_gegl_setup_even_dimensions,
              photos_test_gegl_buffer_apply_orientation_bottom_0,
              photos_test_gegl_teardown);

  g_test_add ("/gegl/buffer/apply_orientation/bottom-1",
              PhotosTestGeglFixture,
              NULL,
              photos_test_gegl_setup_odd_dimensions,
              photos_test_gegl_buffer_apply_orientation_bottom_1,
              photos_test_gegl_teardown);

  g_test_add ("/gegl/buffer/apply_orientation/bottom-mirror-0",
              PhotosTestGeglFixture,
              NULL,
              photos_test_gegl_setup_even_dimensions,
              photos_test_gegl_buffer_apply_orientation_bottom_mirror_0,
              photos_test_gegl_teardown);

  g_test_add ("/gegl/buffer/apply_orientation/bottom-mirror-1",
              PhotosTestGeglFixture,
              NULL,
              photos_test_gegl_setup_odd_dimensions,
              photos_test_gegl_buffer_apply_orientation_bottom_mirror_1,
              photos_test_gegl_teardown);

  g_test_add ("/gegl/buffer/apply_orientation/left-0",
              PhotosTestGeglFixture,
              NULL,
              photos_test_gegl_setup_even_dimensions,
              photos_test_gegl_buffer_apply_orientation_left_0,
              photos_test_gegl_teardown);

  g_test_add ("/gegl/buffer/apply_orientation/left-1",
              PhotosTestGeglFixture,
              NULL,
              photos_test_gegl_setup_odd_dimensions,
              photos_test_gegl_buffer_apply_orientation_left_1,
              photos_test_gegl_teardown);

  g_test_add ("/gegl/buffer/apply_orientation/left-mirror-0",
              PhotosTestGeglFixture,
              NULL,
              photos_test_gegl_setup_even_dimensions,
              photos_test_gegl_buffer_apply_orientation_left_mirror_0,
              photos_test_gegl_teardown);

  g_test_add ("/gegl/buffer/apply_orientation/left-mirror-1",
              PhotosTestGeglFixture,
              NULL,
              photos_test_gegl_setup_odd_dimensions,
              photos_test_gegl_buffer_apply_orientation_left_mirror_1,
              photos_test_gegl_teardown);

  g_test_add ("/gegl/buffer/apply_orientation/right-0",
              PhotosTestGeglFixture,
              NULL,
              photos_test_gegl_setup_even_dimensions,
              photos_test_gegl_buffer_apply_orientation_right_0,
              photos_test_gegl_teardown);

  g_test_add ("/gegl/buffer/apply_orientation/right-1",
              PhotosTestGeglFixture,
              NULL,
              photos_test_gegl_setup_odd_dimensions,
              photos_test_gegl_buffer_apply_orientation_right_1,
              photos_test_gegl_teardown);

  g_test_add ("/gegl/buffer/apply_orientation/right-mirror-0",
              PhotosTestGeglFixture,
              NULL,
              photos_test_gegl_setup_even_dimensions,
              photos_test_gegl_buffer_apply_orientation_right_mirror_0,
              photos_test_gegl_teardown);

  g_test_add ("/gegl/buffer/apply_orientation/right-mirror-1",
              PhotosTestGeglFixture,
              NULL,
              photos_test_gegl_setup_odd_dimensions,
              photos_test_gegl_buffer_apply_orientation_right_mirror_1,
              photos_test_gegl_teardown);

  g_test_add ("/gegl/buffer/apply_orientation/top-0",
              PhotosTestGeglFixture,
              NULL,
              photos_test_gegl_setup_even_dimensions,
              photos_test_gegl_buffer_apply_orientation_top_0,
              photos_test_gegl_teardown);

  g_test_add ("/gegl/buffer/apply_orientation/top-1",
              PhotosTestGeglFixture,
              NULL,
              photos_test_gegl_setup_odd_dimensions,
              photos_test_gegl_buffer_apply_orientation_top_1,
              photos_test_gegl_teardown);

  g_test_add ("/gegl/buffer/apply_orientation/top-mirror-0",
              PhotosTestGeglFixture,
              NULL,
              photos_test_gegl_setup_even_dimensions,
              photos_test_gegl_buffer_apply_orientation_top_mirror_0,
              photos_test_gegl_teardown);

  g_test_add ("/gegl/buffer/apply_orientation/top-mirror-1",
              PhotosTestGeglFixture,
              NULL,
              photos_test_gegl_setup_odd_dimensions,
              photos_test_gegl_buffer_apply_orientation_top_mirror_1,
              photos_test_gegl_teardown);

  g_test_add ("/gegl/buffer/zoom/in-0",
              PhotosTestGeglFixture,
              NULL,
              photos_test_gegl_setup_even_dimensions,
              photos_test_gegl_buffer_zoom_in_0,
              photos_test_gegl_teardown);

  g_test_add ("/gegl/buffer/zoom/in-1",
              PhotosTestGeglFixture,
              NULL,
              photos_test_gegl_setup_even_dimensions,
              photos_test_gegl_buffer_zoom_in_1,
              photos_test_gegl_teardown);

  g_test_add ("/gegl/buffer/zoom/nop",
              PhotosTestGeglFixture,
              NULL,
              photos_test_gegl_setup_even_dimensions,
              photos_test_gegl_buffer_zoom_nop,
              photos_test_gegl_teardown);

  g_test_add ("/gegl/buffer/zoom/out-0",
              PhotosTestGeglFixture,
              NULL,
              photos_test_gegl_setup_even_dimensions,
              photos_test_gegl_buffer_zoom_out_0,
              photos_test_gegl_teardown);

  g_test_add ("/gegl/buffer/zoom/out-1",
              PhotosTestGeglFixture,
              NULL,
              photos_test_gegl_setup_even_dimensions,
              photos_test_gegl_buffer_zoom_out_1,
              photos_test_gegl_teardown);

  exit_status = g_test_run ();

  gegl_exit ();
  return exit_status;
}
