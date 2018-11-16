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
  GFile *destination_0;
  GFile *destination_1;
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
photos_test_gegl_pixbuf_save_to_file (GdkPixbuf *pixbuf, GFile *file)
{
  g_autoptr (GFileOutputStream) stream = NULL;

  g_assert_true (GDK_IS_PIXBUF (pixbuf));
  g_assert_true (G_IS_FILE (file));

  {
    g_autoptr (GError) error = NULL;

    stream = g_file_replace (file, NULL, FALSE, G_FILE_CREATE_REPLACE_DESTINATION, NULL, &error);
    g_assert_no_error (error);
  }

  {
    g_autoptr (GError) error = NULL;

    gdk_pixbuf_save_to_stream (pixbuf, G_OUTPUT_STREAM (stream), "png", NULL, &error, NULL);
    g_assert_no_error (error);
  }
}


static void
photos_test_gegl_setup (PhotosTestGeglFixture *fixture, const Babl *format, gdouble height, gdouble width)
{
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

    fixture->destination_0 = g_file_new_tmp (PACKAGE_TARNAME "-destination0-XXXXXX.png", &iostream, &error);
    g_assert_no_error (error);
  }

  {
    g_autoptr (GError) error = NULL;
    g_autoptr (GFileIOStream) iostream = NULL;

    fixture->destination_1 = g_file_new_tmp (PACKAGE_TARNAME "-destination1-XXXXXX.png", &iostream, &error);
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
photos_test_gegl_setup_no_alpha_even_dimensions (PhotosTestGeglFixture *fixture, gconstpointer user_data)
{
  const Babl *format;
  g_autofree gchar *checksum = NULL;

  format = babl_format ("R'G'B' u8");
  photos_test_gegl_setup (fixture, format, 200.0, 200.0);

  checksum = photos_gegl_compute_checksum_for_buffer (G_CHECKSUM_SHA256, fixture->buffer);
  g_assert_cmpstr (checksum, ==, "7d29086e1be9919e93cc13539cbe270e82432333b953c5f444114746df7358cb");
}


static void
photos_test_gegl_setup_no_alpha_odd_dimensions (PhotosTestGeglFixture *fixture, gconstpointer user_data)
{
  const Babl *format;
  g_autofree gchar *checksum = NULL;

  format = babl_format ("R'G'B' u8");
  photos_test_gegl_setup (fixture, format, 195.0, 195.0);

  checksum = photos_gegl_compute_checksum_for_buffer (G_CHECKSUM_SHA256, fixture->buffer);
  g_assert_cmpstr (checksum, ==, "fd03ef9089a1e485f7674765a2ce73c0ade54f3c0071dec9884c6b50f7891539");
}


static void
photos_test_gegl_setup_with_alpha_even_dimensions (PhotosTestGeglFixture *fixture, gconstpointer user_data)
{
  const Babl *format;
  g_autofree gchar *checksum = NULL;

  format = babl_format ("R'G'B'A u8");
  photos_test_gegl_setup (fixture, format, 200.0, 200.0);

  checksum = photos_gegl_compute_checksum_for_buffer (G_CHECKSUM_SHA256, fixture->buffer);
  g_assert_cmpstr (checksum, ==, "4eaa696bb0758b890cb4764a96eb6f88f7c7a0f7ca8042f381381fc9ffdac19b");
}


static void
photos_test_gegl_setup_with_alpha_odd_dimensions (PhotosTestGeglFixture *fixture, gconstpointer user_data)
{
  const Babl *format;
  g_autofree gchar *checksum = NULL;

  format = babl_format ("R'G'B'A u8");
  photos_test_gegl_setup (fixture, format, 195.0, 195.0);

  checksum = photos_gegl_compute_checksum_for_buffer (G_CHECKSUM_SHA256, fixture->buffer);
  g_assert_cmpstr (checksum, ==, "fb42e2fcd0959a73e7bce22f3a549c03406598a745e7f5bb7feaa38e836fd7a1");
}


static void
photos_test_gegl_teardown (PhotosTestGeglFixture *fixture, gconstpointer user_data)
{
  g_clear_object (&fixture->res);

  g_file_delete (fixture->destination_0, NULL, NULL);
  g_object_unref (fixture->destination_0);

  g_file_delete (fixture->destination_1, NULL, NULL);
  g_object_unref (fixture->destination_1);

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
photos_test_gegl_buffer_check_conversion (PhotosTestGeglFixture *fixture,
                                          gboolean has_alpha,
                                          const gchar *checksum_pixbuf,
                                          const gchar *checksum_buffer)
{
  const Babl *format;
  g_autoptr (GBytes) bytes = NULL;
  g_autoptr (GdkPixbuf) pixbuf = NULL;
  g_autoptr (GeglBuffer) buffer = NULL;
  GeglRectangle bbox;
  GeglRectangle bbox_buffer;
  gboolean has_alpha_buffer;
  gboolean has_alpha_pixbuf;
  g_autofree gchar *checksum_buffer_converted = NULL;
  g_autofree gchar *checksum_pixbuf_converted = NULL;
  gint height_pixbuf;
  gint width_pixbuf;

  bbox = *gegl_buffer_get_extent (fixture->buffer);

  pixbuf = photos_gegl_pixbuf_new_from_buffer (fixture->buffer);
  g_assert_true (GDK_IS_PIXBUF (pixbuf));

  photos_test_gegl_pixbuf_save_to_file (pixbuf, fixture->destination_0);

  has_alpha_pixbuf = gdk_pixbuf_get_has_alpha (pixbuf);
  g_assert_true (has_alpha_pixbuf == has_alpha);

  height_pixbuf = gdk_pixbuf_get_height (pixbuf);
  g_assert_cmpint (height_pixbuf, ==, bbox.height);

  width_pixbuf = gdk_pixbuf_get_width (pixbuf);
  g_assert_cmpint (width_pixbuf, ==, bbox.width);

  bytes = gdk_pixbuf_read_pixel_bytes (pixbuf);
  checksum_pixbuf_converted = g_compute_checksum_for_bytes (G_CHECKSUM_SHA256, bytes);
  g_assert_cmpstr (checksum_pixbuf_converted, ==, checksum_pixbuf);

  buffer = photos_gegl_buffer_new_from_pixbuf (pixbuf);
  g_assert_true (GEGL_IS_BUFFER (buffer));

  photos_test_gegl_buffer_save_to_file (buffer, fixture->destination_1);

  format = gegl_buffer_get_format (buffer);
  has_alpha_buffer = (gboolean) babl_format_has_alpha (format);
  g_assert_true (has_alpha_buffer == has_alpha);
  g_assert_true (has_alpha_buffer == has_alpha_pixbuf);

  bbox_buffer = *gegl_buffer_get_extent (buffer);
  g_assert_cmpint (bbox_buffer.height, ==, bbox.height);
  g_assert_cmpint (bbox_buffer.width, ==, bbox.width);

  checksum_buffer_converted = photos_gegl_compute_checksum_for_buffer (G_CHECKSUM_SHA256, buffer);
  g_assert_cmpstr (checksum_buffer_converted, ==, checksum_buffer);
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

  photos_test_gegl_buffer_save_to_file (buffer_oriented, fixture->destination_0);

  format_oriented = gegl_buffer_get_format (buffer_oriented);
  g_assert_true (format_oriented == fixture->format);

  checksum_oriented = photos_gegl_compute_checksum_for_buffer (G_CHECKSUM_SHA256, buffer_oriented);
  g_assert_cmpstr (checksum_oriented, ==, checksum);
}


static void
photos_test_gegl_buffer_check_pixbuf (PhotosTestGeglFixture *fixture, gboolean has_alpha, const gchar *checksum)
{
  g_autoptr (GBytes) bytes = NULL;
  g_autoptr (GdkPixbuf) pixbuf = NULL;
  GeglNode *buffer_source;
  g_autoptr (GeglNode) graph = NULL;
  gboolean has_alpha_pixbuf;
  g_autofree gchar *checksum_pixbuf = NULL;

  graph = gegl_node_new ();
  buffer_source = gegl_node_new_child (graph, "operation", "gegl:buffer-source", "buffer", fixture->buffer, NULL);
  gegl_node_process (buffer_source);

  pixbuf = photos_gegl_create_pixbuf_from_node (buffer_source);
  g_assert_true (GDK_IS_PIXBUF (pixbuf));

  photos_test_gegl_pixbuf_save_to_file (pixbuf, fixture->destination_0);

  has_alpha_pixbuf = gdk_pixbuf_get_has_alpha (pixbuf);
  g_assert_true (has_alpha_pixbuf == has_alpha);

  bytes = gdk_pixbuf_read_pixel_bytes (pixbuf);
  checksum_pixbuf = g_compute_checksum_for_bytes (G_CHECKSUM_SHA256, bytes);
  g_assert_cmpstr (checksum_pixbuf, ==, checksum);
}


static void
photos_test_gegl_buffer_check_zoom (PhotosTestGeglFixture *fixture, double zoom, const gchar *checksum)
{
  const Babl *format_zoomed_converted;
  g_autoptr (GeglBuffer) buffer_zoomed = NULL;
  g_autoptr (GeglBuffer) buffer_zoomed_converted = NULL;
  GeglRectangle bbox;
  g_autofree gchar *checksum_zoomed_converted = NULL;

  photos_gegl_buffer_zoom_async (fixture->buffer, zoom, NULL, photos_test_gegl_async, fixture);
  g_main_loop_run (fixture->loop);

  {
    g_autoptr (GError) error = NULL;

    buffer_zoomed = photos_gegl_buffer_zoom_finish (fixture->buffer, fixture->res, &error);
    g_assert_no_error (error);
  }

  g_assert_true (GEGL_IS_BUFFER (buffer_zoomed));

  bbox = *gegl_buffer_get_extent (buffer_zoomed);
  format_zoomed_converted = babl_format ("R'G'B'A u8");
  buffer_zoomed_converted = gegl_buffer_new (&bbox, format_zoomed_converted);
  gegl_buffer_copy (buffer_zoomed, &bbox, GEGL_ABYSS_NONE, buffer_zoomed_converted, &bbox);

  photos_test_gegl_buffer_save_to_file (buffer_zoomed_converted, fixture->destination_0);

  checksum_zoomed_converted = photos_gegl_compute_checksum_for_buffer (G_CHECKSUM_SHA256, buffer_zoomed_converted);
  g_assert_cmpstr (checksum_zoomed_converted, ==, checksum);
}


static void
photos_test_gegl_buffer_apply_orientation_bottom_0 (PhotosTestGeglFixture *fixture, gconstpointer user_data)
{
  photos_test_gegl_buffer_check_orientation (fixture,
                                             PHOTOS_ORIENTATION_BOTTOM,
                                             "ae8c52d296cec87c2f396c83c46a473d3e46d116705883122c02c6bd4d56946c");
}


static void
photos_test_gegl_buffer_apply_orientation_bottom_1 (PhotosTestGeglFixture *fixture, gconstpointer user_data)
{
  photos_test_gegl_buffer_check_orientation (fixture,
                                             PHOTOS_ORIENTATION_BOTTOM,
                                             "f4372b5399d4e8c56a56c17be1c6685fe968ad596b87d2426f86801cabd2035e");
}


static void
photos_test_gegl_buffer_apply_orientation_bottom_2 (PhotosTestGeglFixture *fixture, gconstpointer user_data)
{
  photos_test_gegl_buffer_check_orientation (fixture,
                                             PHOTOS_ORIENTATION_BOTTOM,
                                             "0497b2a1ff9aa9f6e7aa526b55157133d01331a0869602a4d4cc1f7a7bef420e");
}


static void
photos_test_gegl_buffer_apply_orientation_bottom_3 (PhotosTestGeglFixture *fixture, gconstpointer user_data)
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
                                             "6a212bc4309c26059d13b8faba6c0b545986b30f6d87fca459822aa8f3fcf5bf");
}


static void
photos_test_gegl_buffer_apply_orientation_bottom_mirror_1 (PhotosTestGeglFixture *fixture, gconstpointer user_data)
{
  photos_test_gegl_buffer_check_orientation (fixture,
                                             PHOTOS_ORIENTATION_BOTTOM_MIRROR,
                                             "558189b007b961672db1cc68c3dd6a0f0eebc8a10dac64faf20052c955dd7452");
}


static void
photos_test_gegl_buffer_apply_orientation_bottom_mirror_2 (PhotosTestGeglFixture *fixture, gconstpointer user_data)
{
  photos_test_gegl_buffer_check_orientation (fixture,
                                             PHOTOS_ORIENTATION_BOTTOM_MIRROR,
                                             "13fda7c43a734c14a0bde04bf7f02439ffeea3ae96df83257c61d26a162413dc");
}


static void
photos_test_gegl_buffer_apply_orientation_bottom_mirror_3 (PhotosTestGeglFixture *fixture, gconstpointer user_data)
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
                                             "b68f750a272f91e2ef3a0e154008f4f4b4f631436ac69fc8dc8bdcd4975d9f6d");
}


static void
photos_test_gegl_buffer_apply_orientation_left_1 (PhotosTestGeglFixture *fixture, gconstpointer user_data)
{
  photos_test_gegl_buffer_check_orientation (fixture,
                                             PHOTOS_ORIENTATION_LEFT,
                                             "693626691a06a16114a0d54d2ddfe297a935e7a3dc40e5a577926243c0751c95");
}


static void
photos_test_gegl_buffer_apply_orientation_left_2 (PhotosTestGeglFixture *fixture, gconstpointer user_data)
{
  photos_test_gegl_buffer_check_orientation (fixture,
                                             PHOTOS_ORIENTATION_LEFT,
                                             "45a8e2c222c6f74b9d784b7bb521f871f8badb6ecd5e185505a8ed701505af11");
}


static void
photos_test_gegl_buffer_apply_orientation_left_3 (PhotosTestGeglFixture *fixture, gconstpointer user_data)
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
                                             "3d1de5d101c44a3c3da37fcd257551c74217e5a5db2b98b5bbb041f5a0f645c7");
}


static void
photos_test_gegl_buffer_apply_orientation_left_mirror_1 (PhotosTestGeglFixture *fixture, gconstpointer user_data)
{
  photos_test_gegl_buffer_check_orientation (fixture,
                                             PHOTOS_ORIENTATION_LEFT_MIRROR,
                                             "546c4c885f7d0f03472dcee2a535af153cc560c7c9055aedcba0331a489cfea3");
}


static void
photos_test_gegl_buffer_apply_orientation_left_mirror_2 (PhotosTestGeglFixture *fixture, gconstpointer user_data)
{
  photos_test_gegl_buffer_check_orientation (fixture,
                                             PHOTOS_ORIENTATION_LEFT_MIRROR,
                                             "2459ca11c8d9e851ce161f1d0d61d9cbe7279d54585b42b71949eb3031faab20");
}


static void
photos_test_gegl_buffer_apply_orientation_left_mirror_3 (PhotosTestGeglFixture *fixture, gconstpointer user_data)
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
                                             "1b76f6c70f7119730f201bc6b103be09a389ee86443d2eabf84f9627c6640278");
}


static void
photos_test_gegl_buffer_apply_orientation_right_1 (PhotosTestGeglFixture *fixture, gconstpointer user_data)
{
  photos_test_gegl_buffer_check_orientation (fixture,
                                             PHOTOS_ORIENTATION_RIGHT,
                                             "8e42df2e236d2cea30bd65a39e32a24dbd8379312245aac76773d45f775e515e");
}


static void
photos_test_gegl_buffer_apply_orientation_right_2 (PhotosTestGeglFixture *fixture, gconstpointer user_data)
{
  photos_test_gegl_buffer_check_orientation (fixture,
                                             PHOTOS_ORIENTATION_RIGHT,
                                             "20b01f6e841a06ec8445c6446db4abe5ad65a3497044c5f5fcf3bcac1e4cc80a");
}


static void
photos_test_gegl_buffer_apply_orientation_right_3 (PhotosTestGeglFixture *fixture, gconstpointer user_data)
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
                                             "6ecbe1cf2b42ddee48ca1aaeef363f7df045fa32f528ccc89221f240e0f5d236");
}


static void
photos_test_gegl_buffer_apply_orientation_right_mirror_1 (PhotosTestGeglFixture *fixture, gconstpointer user_data)
{
  photos_test_gegl_buffer_check_orientation (fixture,
                                             PHOTOS_ORIENTATION_RIGHT_MIRROR,
                                             "5334cda92e746043e970ea7bb207254544cbe447daf585f990fe7cd13c15f624");
}


static void
photos_test_gegl_buffer_apply_orientation_right_mirror_2 (PhotosTestGeglFixture *fixture, gconstpointer user_data)
{
  photos_test_gegl_buffer_check_orientation (fixture,
                                             PHOTOS_ORIENTATION_RIGHT_MIRROR,
                                             "9f2e6fe50c65cdcd4a1268dfba9f3102206549f6cd6c8a37d74d374ba8451445");
}


static void
photos_test_gegl_buffer_apply_orientation_right_mirror_3 (PhotosTestGeglFixture *fixture, gconstpointer user_data)
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
                                             "7d29086e1be9919e93cc13539cbe270e82432333b953c5f444114746df7358cb");
}


static void
photos_test_gegl_buffer_apply_orientation_top_1 (PhotosTestGeglFixture *fixture, gconstpointer user_data)
{
  photos_test_gegl_buffer_check_orientation (fixture,
                                             PHOTOS_ORIENTATION_TOP,
                                             "fd03ef9089a1e485f7674765a2ce73c0ade54f3c0071dec9884c6b50f7891539");
}


static void
photos_test_gegl_buffer_apply_orientation_top_2 (PhotosTestGeglFixture *fixture, gconstpointer user_data)
{
  photos_test_gegl_buffer_check_orientation (fixture,
                                             PHOTOS_ORIENTATION_TOP,
                                             "4eaa696bb0758b890cb4764a96eb6f88f7c7a0f7ca8042f381381fc9ffdac19b");
}


static void
photos_test_gegl_buffer_apply_orientation_top_3 (PhotosTestGeglFixture *fixture, gconstpointer user_data)
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
                                             "2d96c1e1249adfad6c9becc857d5cdd2a2358b31d191428610c764d8427c868f");
}


static void
photos_test_gegl_buffer_apply_orientation_top_mirror_1 (PhotosTestGeglFixture *fixture, gconstpointer user_data)
{
  photos_test_gegl_buffer_check_orientation (fixture,
                                             PHOTOS_ORIENTATION_TOP_MIRROR,
                                             "955a53bbf71c1282639784103cc4b290005c79571740b44b31c26d5b94f3c0d7");
}


static void
photos_test_gegl_buffer_apply_orientation_top_mirror_2 (PhotosTestGeglFixture *fixture, gconstpointer user_data)
{
  photos_test_gegl_buffer_check_orientation (fixture,
                                             PHOTOS_ORIENTATION_TOP_MIRROR,
                                             "c3bdf8f600308a9dc0f9d4ba040df3978bdd1ec95350edc18059ac130ad4c8e9");
}


static void
photos_test_gegl_buffer_apply_orientation_top_mirror_3 (PhotosTestGeglFixture *fixture, gconstpointer user_data)
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
                                      "104c92c9461528a7391abe6312523a86a3eb2b7be2036261af8888ace6d7ae4c");
}


static void
photos_test_gegl_buffer_zoom_in_1 (PhotosTestGeglFixture *fixture, gconstpointer user_data)
{
  photos_test_gegl_buffer_check_zoom (fixture,
                                      4.0,
                                      "824f440437cec0375f16cfded616c93c25a804b4e7b8ce113d8e88176e58e752");
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
                                      "290d10b98a54c3dcee88f08dfd9f5bd485c1d371434250b57b6a042a84250cb0");
}


static void
photos_test_gegl_buffer_zoom_out_1 (PhotosTestGeglFixture *fixture, gconstpointer user_data)
{
  photos_test_gegl_buffer_check_zoom (fixture,
                                      0.25,
                                      "d4b5e8504e9e8ae74815dfa52e5086c6e60c36993bfba59bded2c86740f4b6c6");
}


static void
photos_test_gegl_legacy_convert_between_buffer_pixbuf_0 (PhotosTestGeglFixture *fixture, gconstpointer user_data)
{
  photos_test_gegl_buffer_check_conversion (fixture,
                                            FALSE,
                                            "7d29086e1be9919e93cc13539cbe270e82432333b953c5f444114746df7358cb",
                                            "7d29086e1be9919e93cc13539cbe270e82432333b953c5f444114746df7358cb");
}


static void
photos_test_gegl_legacy_convert_between_buffer_pixbuf_1 (PhotosTestGeglFixture *fixture, gconstpointer user_data)
{
  photos_test_gegl_buffer_check_conversion (fixture,
                                            FALSE,
                                            "932a094d0b987c06dbb496d86d52dc743122685507ed57e3c25e17a2e5952d93",
                                            "fd03ef9089a1e485f7674765a2ce73c0ade54f3c0071dec9884c6b50f7891539");
}


static void
photos_test_gegl_legacy_convert_between_buffer_pixbuf_2 (PhotosTestGeglFixture *fixture, gconstpointer user_data)
{
  photos_test_gegl_buffer_check_conversion (fixture,
                                            TRUE,
                                            "4eaa696bb0758b890cb4764a96eb6f88f7c7a0f7ca8042f381381fc9ffdac19b",
                                            "4eaa696bb0758b890cb4764a96eb6f88f7c7a0f7ca8042f381381fc9ffdac19b");
}


static void
photos_test_gegl_legacy_convert_between_buffer_pixbuf_3 (PhotosTestGeglFixture *fixture, gconstpointer user_data)
{
  photos_test_gegl_buffer_check_conversion (fixture,
                                            TRUE,
                                            "fb42e2fcd0959a73e7bce22f3a549c03406598a745e7f5bb7feaa38e836fd7a1",
                                            "fb42e2fcd0959a73e7bce22f3a549c03406598a745e7f5bb7feaa38e836fd7a1");
}


static void
photos_test_gegl_legacy_create_pixbuf_from_node_0 (PhotosTestGeglFixture *fixture, gconstpointer user_data)
{
  photos_test_gegl_buffer_check_pixbuf (fixture,
                                        FALSE,
                                        "7d29086e1be9919e93cc13539cbe270e82432333b953c5f444114746df7358cb");
}


static void
photos_test_gegl_legacy_create_pixbuf_from_node_1 (PhotosTestGeglFixture *fixture, gconstpointer user_data)
{
  photos_test_gegl_buffer_check_pixbuf (fixture,
                                        TRUE,
                                        "4eaa696bb0758b890cb4764a96eb6f88f7c7a0f7ca8042f381381fc9ffdac19b");
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

  g_test_add ("/gegl/buffer/apply-orientation/bottom-0",
              PhotosTestGeglFixture,
              NULL,
              photos_test_gegl_setup_no_alpha_even_dimensions,
              photos_test_gegl_buffer_apply_orientation_bottom_0,
              photos_test_gegl_teardown);

  g_test_add ("/gegl/buffer/apply-orientation/bottom-1",
              PhotosTestGeglFixture,
              NULL,
              photos_test_gegl_setup_no_alpha_odd_dimensions,
              photos_test_gegl_buffer_apply_orientation_bottom_1,
              photos_test_gegl_teardown);

  g_test_add ("/gegl/buffer/apply-orientation/bottom-2",
              PhotosTestGeglFixture,
              NULL,
              photos_test_gegl_setup_with_alpha_even_dimensions,
              photos_test_gegl_buffer_apply_orientation_bottom_2,
              photos_test_gegl_teardown);

  g_test_add ("/gegl/buffer/apply-orientation/bottom-3",
              PhotosTestGeglFixture,
              NULL,
              photos_test_gegl_setup_with_alpha_odd_dimensions,
              photos_test_gegl_buffer_apply_orientation_bottom_3,
              photos_test_gegl_teardown);

  g_test_add ("/gegl/buffer/apply-orientation/bottom-mirror-0",
              PhotosTestGeglFixture,
              NULL,
              photos_test_gegl_setup_no_alpha_even_dimensions,
              photos_test_gegl_buffer_apply_orientation_bottom_mirror_0,
              photos_test_gegl_teardown);

  g_test_add ("/gegl/buffer/apply-orientation/bottom-mirror-1",
              PhotosTestGeglFixture,
              NULL,
              photos_test_gegl_setup_no_alpha_odd_dimensions,
              photos_test_gegl_buffer_apply_orientation_bottom_mirror_1,
              photos_test_gegl_teardown);

  g_test_add ("/gegl/buffer/apply-orientation/bottom-mirror-2",
              PhotosTestGeglFixture,
              NULL,
              photos_test_gegl_setup_with_alpha_even_dimensions,
              photos_test_gegl_buffer_apply_orientation_bottom_mirror_2,
              photos_test_gegl_teardown);

  g_test_add ("/gegl/buffer/apply-orientation/bottom-mirror-3",
              PhotosTestGeglFixture,
              NULL,
              photos_test_gegl_setup_with_alpha_odd_dimensions,
              photos_test_gegl_buffer_apply_orientation_bottom_mirror_3,
              photos_test_gegl_teardown);

  g_test_add ("/gegl/buffer/apply-orientation/left-0",
              PhotosTestGeglFixture,
              NULL,
              photos_test_gegl_setup_no_alpha_even_dimensions,
              photos_test_gegl_buffer_apply_orientation_left_0,
              photos_test_gegl_teardown);

  g_test_add ("/gegl/buffer/apply-orientation/left-1",
              PhotosTestGeglFixture,
              NULL,
              photos_test_gegl_setup_no_alpha_odd_dimensions,
              photos_test_gegl_buffer_apply_orientation_left_1,
              photos_test_gegl_teardown);

  g_test_add ("/gegl/buffer/apply-orientation/left-2",
              PhotosTestGeglFixture,
              NULL,
              photos_test_gegl_setup_with_alpha_even_dimensions,
              photos_test_gegl_buffer_apply_orientation_left_2,
              photos_test_gegl_teardown);

  g_test_add ("/gegl/buffer/apply-orientation/left-3",
              PhotosTestGeglFixture,
              NULL,
              photos_test_gegl_setup_with_alpha_odd_dimensions,
              photos_test_gegl_buffer_apply_orientation_left_3,
              photos_test_gegl_teardown);

  g_test_add ("/gegl/buffer/apply-orientation/left-mirror-0",
              PhotosTestGeglFixture,
              NULL,
              photos_test_gegl_setup_no_alpha_even_dimensions,
              photos_test_gegl_buffer_apply_orientation_left_mirror_0,
              photos_test_gegl_teardown);

  g_test_add ("/gegl/buffer/apply-orientation/left-mirror-1",
              PhotosTestGeglFixture,
              NULL,
              photos_test_gegl_setup_no_alpha_odd_dimensions,
              photos_test_gegl_buffer_apply_orientation_left_mirror_1,
              photos_test_gegl_teardown);

  g_test_add ("/gegl/buffer/apply-orientation/left-mirror-2",
              PhotosTestGeglFixture,
              NULL,
              photos_test_gegl_setup_with_alpha_even_dimensions,
              photos_test_gegl_buffer_apply_orientation_left_mirror_2,
              photos_test_gegl_teardown);

  g_test_add ("/gegl/buffer/apply-orientation/left-mirror-3",
              PhotosTestGeglFixture,
              NULL,
              photos_test_gegl_setup_with_alpha_odd_dimensions,
              photos_test_gegl_buffer_apply_orientation_left_mirror_3,
              photos_test_gegl_teardown);

  g_test_add ("/gegl/buffer/apply-orientation/right-0",
              PhotosTestGeglFixture,
              NULL,
              photos_test_gegl_setup_no_alpha_even_dimensions,
              photos_test_gegl_buffer_apply_orientation_right_0,
              photos_test_gegl_teardown);

  g_test_add ("/gegl/buffer/apply-orientation/right-1",
              PhotosTestGeglFixture,
              NULL,
              photos_test_gegl_setup_no_alpha_odd_dimensions,
              photos_test_gegl_buffer_apply_orientation_right_1,
              photos_test_gegl_teardown);

  g_test_add ("/gegl/buffer/apply-orientation/right-2",
              PhotosTestGeglFixture,
              NULL,
              photos_test_gegl_setup_with_alpha_even_dimensions,
              photos_test_gegl_buffer_apply_orientation_right_2,
              photos_test_gegl_teardown);

  g_test_add ("/gegl/buffer/apply-orientation/right-3",
              PhotosTestGeglFixture,
              NULL,
              photos_test_gegl_setup_with_alpha_odd_dimensions,
              photos_test_gegl_buffer_apply_orientation_right_3,
              photos_test_gegl_teardown);

  g_test_add ("/gegl/buffer/apply-orientation/right-mirror-0",
              PhotosTestGeglFixture,
              NULL,
              photos_test_gegl_setup_no_alpha_even_dimensions,
              photos_test_gegl_buffer_apply_orientation_right_mirror_0,
              photos_test_gegl_teardown);

  g_test_add ("/gegl/buffer/apply-orientation/right-mirror-1",
              PhotosTestGeglFixture,
              NULL,
              photos_test_gegl_setup_no_alpha_odd_dimensions,
              photos_test_gegl_buffer_apply_orientation_right_mirror_1,
              photos_test_gegl_teardown);

  g_test_add ("/gegl/buffer/apply-orientation/right-mirror-2",
              PhotosTestGeglFixture,
              NULL,
              photos_test_gegl_setup_with_alpha_even_dimensions,
              photos_test_gegl_buffer_apply_orientation_right_mirror_2,
              photos_test_gegl_teardown);

  g_test_add ("/gegl/buffer/apply-orientation/right-mirror-3",
              PhotosTestGeglFixture,
              NULL,
              photos_test_gegl_setup_with_alpha_odd_dimensions,
              photos_test_gegl_buffer_apply_orientation_right_mirror_3,
              photos_test_gegl_teardown);

  g_test_add ("/gegl/buffer/apply-orientation/top-0",
              PhotosTestGeglFixture,
              NULL,
              photos_test_gegl_setup_no_alpha_even_dimensions,
              photos_test_gegl_buffer_apply_orientation_top_0,
              photos_test_gegl_teardown);

  g_test_add ("/gegl/buffer/apply-orientation/top-1",
              PhotosTestGeglFixture,
              NULL,
              photos_test_gegl_setup_no_alpha_odd_dimensions,
              photos_test_gegl_buffer_apply_orientation_top_1,
              photos_test_gegl_teardown);

  g_test_add ("/gegl/buffer/apply-orientation/top-2",
              PhotosTestGeglFixture,
              NULL,
              photos_test_gegl_setup_with_alpha_even_dimensions,
              photos_test_gegl_buffer_apply_orientation_top_2,
              photos_test_gegl_teardown);

  g_test_add ("/gegl/buffer/apply-orientation/top-3",
              PhotosTestGeglFixture,
              NULL,
              photos_test_gegl_setup_with_alpha_odd_dimensions,
              photos_test_gegl_buffer_apply_orientation_top_3,
              photos_test_gegl_teardown);

  g_test_add ("/gegl/buffer/apply-orientation/top-mirror-0",
              PhotosTestGeglFixture,
              NULL,
              photos_test_gegl_setup_no_alpha_even_dimensions,
              photos_test_gegl_buffer_apply_orientation_top_mirror_0,
              photos_test_gegl_teardown);

  g_test_add ("/gegl/buffer/apply-orientation/top-mirror-1",
              PhotosTestGeglFixture,
              NULL,
              photos_test_gegl_setup_no_alpha_odd_dimensions,
              photos_test_gegl_buffer_apply_orientation_top_mirror_1,
              photos_test_gegl_teardown);

  g_test_add ("/gegl/buffer/apply-orientation/top-mirror-2",
              PhotosTestGeglFixture,
              NULL,
              photos_test_gegl_setup_with_alpha_even_dimensions,
              photos_test_gegl_buffer_apply_orientation_top_mirror_2,
              photos_test_gegl_teardown);

  g_test_add ("/gegl/buffer/apply-orientation/top-mirror-3",
              PhotosTestGeglFixture,
              NULL,
              photos_test_gegl_setup_with_alpha_odd_dimensions,
              photos_test_gegl_buffer_apply_orientation_top_mirror_3,
              photos_test_gegl_teardown);

  g_test_add ("/gegl/buffer/zoom/in-0",
              PhotosTestGeglFixture,
              NULL,
              photos_test_gegl_setup_with_alpha_even_dimensions,
              photos_test_gegl_buffer_zoom_in_0,
              photos_test_gegl_teardown);

  g_test_add ("/gegl/buffer/zoom/in-1",
              PhotosTestGeglFixture,
              NULL,
              photos_test_gegl_setup_with_alpha_even_dimensions,
              photos_test_gegl_buffer_zoom_in_1,
              photos_test_gegl_teardown);

  g_test_add ("/gegl/buffer/zoom/nop",
              PhotosTestGeglFixture,
              NULL,
              photos_test_gegl_setup_with_alpha_even_dimensions,
              photos_test_gegl_buffer_zoom_nop,
              photos_test_gegl_teardown);

  g_test_add ("/gegl/buffer/zoom/out-0",
              PhotosTestGeglFixture,
              NULL,
              photos_test_gegl_setup_with_alpha_even_dimensions,
              photos_test_gegl_buffer_zoom_out_0,
              photos_test_gegl_teardown);

  g_test_add ("/gegl/buffer/zoom/out-1",
              PhotosTestGeglFixture,
              NULL,
              photos_test_gegl_setup_with_alpha_even_dimensions,
              photos_test_gegl_buffer_zoom_out_1,
              photos_test_gegl_teardown);

  g_test_add ("/gegl/legacy/convert-between-buffer-pixbuf-0",
              PhotosTestGeglFixture,
              NULL,
              photos_test_gegl_setup_no_alpha_even_dimensions,
              photos_test_gegl_legacy_convert_between_buffer_pixbuf_0,
              photos_test_gegl_teardown);

  g_test_add ("/gegl/legacy/convert-between-buffer-pixbuf-1",
              PhotosTestGeglFixture,
              NULL,
              photos_test_gegl_setup_no_alpha_odd_dimensions,
              photos_test_gegl_legacy_convert_between_buffer_pixbuf_1,
              photos_test_gegl_teardown);

  g_test_add ("/gegl/legacy/convert-between-buffer-pixbuf-2",
              PhotosTestGeglFixture,
              NULL,
              photos_test_gegl_setup_with_alpha_even_dimensions,
              photos_test_gegl_legacy_convert_between_buffer_pixbuf_2,
              photos_test_gegl_teardown);

  g_test_add ("/gegl/legacy/convert-between-buffer-pixbuf-3",
              PhotosTestGeglFixture,
              NULL,
              photos_test_gegl_setup_with_alpha_odd_dimensions,
              photos_test_gegl_legacy_convert_between_buffer_pixbuf_3,
              photos_test_gegl_teardown);

  g_test_add ("/gegl/legacy/create_pixbuf_from_node-0",
              PhotosTestGeglFixture,
              NULL,
              photos_test_gegl_setup_no_alpha_even_dimensions,
              photos_test_gegl_legacy_create_pixbuf_from_node_0,
              photos_test_gegl_teardown);

  g_test_add ("/gegl/legacy/create_pixbuf_from_node-1",
              PhotosTestGeglFixture,
              NULL,
              photos_test_gegl_setup_with_alpha_even_dimensions,
              photos_test_gegl_legacy_create_pixbuf_from_node_1,
              photos_test_gegl_teardown);

  exit_status = g_test_run ();

  gegl_exit ();
  return exit_status;
}
