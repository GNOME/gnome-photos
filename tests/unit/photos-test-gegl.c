/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2018 – 2021 Red Hat, Inc.
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
#include "photos-glib.h"
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
  GeglNode *crop;
  GeglNode *path;
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
                              "transform", "translate (40.0, 40.0)",
                              NULL);

  buffer_sink = gegl_node_new_child (graph,
                                     "operation", "gegl:buffer-sink",
                                     "buffer", &buffer,
                                     "format", format,
                                     NULL);

  gegl_node_link_many (checkerboard, crop, path, buffer_sink, NULL);
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
  g_assert_cmpstr (checksum, ==, "f3f8ea1b6680da7dbf08ecf5ba76da6f4ece29b48bcdeaab2be808f8a09c874f");
}


static void
photos_test_gegl_setup_no_alpha_odd_dimensions (PhotosTestGeglFixture *fixture, gconstpointer user_data)
{
  const Babl *format;
  g_autofree gchar *checksum = NULL;

  format = babl_format ("R'G'B' u8");
  photos_test_gegl_setup (fixture, format, 195.0, 195.0);

  checksum = photos_gegl_compute_checksum_for_buffer (G_CHECKSUM_SHA256, fixture->buffer);
  g_assert_cmpstr (checksum, ==, "e031a5cab0e7a3794e34a91859618c93c8fdd1babd1fedf39f10f9585b0b5fa8");
}


static void
photos_test_gegl_setup_with_alpha_even_dimensions (PhotosTestGeglFixture *fixture, gconstpointer user_data)
{
  const Babl *format;
  g_autofree gchar *checksum = NULL;

  format = babl_format ("R'G'B'A u8");
  photos_test_gegl_setup (fixture, format, 200.0, 200.0);

  checksum = photos_gegl_compute_checksum_for_buffer (G_CHECKSUM_SHA256, fixture->buffer);
  g_assert_cmpstr (checksum, ==, "2b759cc636f78ff70ef197b9b9495214f9e8de6b3743175f25186c24d9caed5f");
}


static void
photos_test_gegl_setup_with_alpha_odd_dimensions (PhotosTestGeglFixture *fixture, gconstpointer user_data)
{
  const Babl *format;
  g_autofree gchar *checksum = NULL;

  format = babl_format ("R'G'B'A u8");
  photos_test_gegl_setup (fixture, format, 195.0, 195.0);

  checksum = photos_gegl_compute_checksum_for_buffer (G_CHECKSUM_SHA256, fixture->buffer);
  g_assert_cmpstr (checksum, ==, "a9e40c84633473fb2168bcb7424bf1d294d886d61c42139a9d787510c4be31ea");
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
photos_test_gegl_buffer_check_zoom (PhotosTestGeglFixture *fixture,
                                    double zoom,
                                    const gchar *const *checksums,
                                    gint height,
                                    gint width,
                                    gint x,
                                    gint y)
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
  g_assert_cmpint (bbox.height, ==, height);
  g_assert_cmpint (bbox.width, ==, width);
  g_assert_cmpint (bbox.x, ==, x);
  g_assert_cmpint (bbox.y, ==, y);

  format_zoomed_converted = babl_format ("R'G'B'A u8");
  buffer_zoomed_converted = photos_gegl_buffer_convert (buffer_zoomed, format_zoomed_converted);
  photos_test_gegl_buffer_save_to_file (buffer_zoomed_converted, fixture->destination_0);

  checksum_zoomed_converted = photos_gegl_compute_checksum_for_buffer (G_CHECKSUM_SHA256, buffer_zoomed_converted);
  photos_glib_assert_strv_contains (checksums, checksum_zoomed_converted);
}


static void
photos_test_gegl_buffer_apply_orientation_bottom_0 (PhotosTestGeglFixture *fixture, gconstpointer user_data)
{
  photos_test_gegl_buffer_check_orientation (fixture,
                                             PHOTOS_ORIENTATION_BOTTOM,
                                             "cef0b22992ab6a348c0872e273623efaf47eeea74a8e76693a0edff6fc580693");
}


static void
photos_test_gegl_buffer_apply_orientation_bottom_1 (PhotosTestGeglFixture *fixture, gconstpointer user_data)
{
  photos_test_gegl_buffer_check_orientation (fixture,
                                             PHOTOS_ORIENTATION_BOTTOM,
                                             "acdc7ea47099b330f9989ec59509ead990009697ce166ed11ad7dfdc95b2b5b6");
}


static void
photos_test_gegl_buffer_apply_orientation_bottom_2 (PhotosTestGeglFixture *fixture, gconstpointer user_data)
{
  photos_test_gegl_buffer_check_orientation (fixture,
                                             PHOTOS_ORIENTATION_BOTTOM,
                                             "90688f78ab46bf06e5152614d2c7fa70bb3ad25a9acd668cc6050e0e30874d4e");
}


static void
photos_test_gegl_buffer_apply_orientation_bottom_3 (PhotosTestGeglFixture *fixture, gconstpointer user_data)
{
  photos_test_gegl_buffer_check_orientation (fixture,
                                             PHOTOS_ORIENTATION_BOTTOM,
                                             "449e604a78535ccf73ed461d95fd4060a528d2f5ed391fce0ced4acd25d2ec0d");
}


static void
photos_test_gegl_buffer_apply_orientation_bottom_mirror_0 (PhotosTestGeglFixture *fixture, gconstpointer user_data)
{
  photos_test_gegl_buffer_check_orientation (fixture,
                                             PHOTOS_ORIENTATION_BOTTOM_MIRROR,
                                             "a438c34fecd20aa65f82515deda6b0c31d2d38f867bb901414898d41d427dba7");
}


static void
photos_test_gegl_buffer_apply_orientation_bottom_mirror_1 (PhotosTestGeglFixture *fixture, gconstpointer user_data)
{
  photos_test_gegl_buffer_check_orientation (fixture,
                                             PHOTOS_ORIENTATION_BOTTOM_MIRROR,
                                             "b775b9b00c61a84f93a36058658f4d4eefb89399ab1863f2c47f75d1887a78d4");
}


static void
photos_test_gegl_buffer_apply_orientation_bottom_mirror_2 (PhotosTestGeglFixture *fixture, gconstpointer user_data)
{
  photos_test_gegl_buffer_check_orientation (fixture,
                                             PHOTOS_ORIENTATION_BOTTOM_MIRROR,
                                             "5e81b8d2b1b1cc52cb641953fa3397b5ba69b6b87421fe1d53cb3550af9b88de");
}


static void
photos_test_gegl_buffer_apply_orientation_bottom_mirror_3 (PhotosTestGeglFixture *fixture, gconstpointer user_data)
{
  photos_test_gegl_buffer_check_orientation (fixture,
                                             PHOTOS_ORIENTATION_BOTTOM_MIRROR,
                                             "f4b0abfd0db8ec93d9134f9aca49a6de8f1deda9bd5fca9313bca2f817d44261");
}


static void
photos_test_gegl_buffer_apply_orientation_left_0 (PhotosTestGeglFixture *fixture, gconstpointer user_data)
{
  photos_test_gegl_buffer_check_orientation (fixture,
                                             PHOTOS_ORIENTATION_LEFT,
                                             "59f410566e750fc926f5c2a43154f5ddd2de78df3079cff4bacfa5fb53865876");
}


static void
photos_test_gegl_buffer_apply_orientation_left_1 (PhotosTestGeglFixture *fixture, gconstpointer user_data)
{
  photos_test_gegl_buffer_check_orientation (fixture,
                                             PHOTOS_ORIENTATION_LEFT,
                                             "2c103e3d285c9f774350d4a01917b9b6bf50f9caa9ef6e2294cfc9abb817be10");
}


static void
photos_test_gegl_buffer_apply_orientation_left_2 (PhotosTestGeglFixture *fixture, gconstpointer user_data)
{
  photos_test_gegl_buffer_check_orientation (fixture,
                                             PHOTOS_ORIENTATION_LEFT,
                                             "ca4d2a966ee770b4086cd1bee0d18d11775ee842b1a7d4df4e7c86bece46ac4d");
}


static void
photos_test_gegl_buffer_apply_orientation_left_3 (PhotosTestGeglFixture *fixture, gconstpointer user_data)
{
  photos_test_gegl_buffer_check_orientation (fixture,
                                             PHOTOS_ORIENTATION_LEFT,
                                             "d987a0a1387b7e656c060aa8c881e10cc0af18f8ec6628fc123c9d3f2b21bf4a");
}


static void
photos_test_gegl_buffer_apply_orientation_left_mirror_0 (PhotosTestGeglFixture *fixture, gconstpointer user_data)
{
  photos_test_gegl_buffer_check_orientation (fixture,
                                             PHOTOS_ORIENTATION_LEFT_MIRROR,
                                             "7b0c871b8da3ff7bc6667681b81fd3cccaebd1ea1c1bf6f2ac3169bc8d27824a");
}


static void
photos_test_gegl_buffer_apply_orientation_left_mirror_1 (PhotosTestGeglFixture *fixture, gconstpointer user_data)
{
  photos_test_gegl_buffer_check_orientation (fixture,
                                             PHOTOS_ORIENTATION_LEFT_MIRROR,
                                             "f06a564ce9ec17ca6be2f12f3405a0f315e74d0e8710d2b695d49e31af1acaec");
}


static void
photos_test_gegl_buffer_apply_orientation_left_mirror_2 (PhotosTestGeglFixture *fixture, gconstpointer user_data)
{
  photos_test_gegl_buffer_check_orientation (fixture,
                                             PHOTOS_ORIENTATION_LEFT_MIRROR,
                                             "b59c54a7ef2ff72a21777e8e17e4cb00a4a9b75f3246fe448d0e198486b595da");
}


static void
photos_test_gegl_buffer_apply_orientation_left_mirror_3 (PhotosTestGeglFixture *fixture, gconstpointer user_data)
{
  photos_test_gegl_buffer_check_orientation (fixture,
                                             PHOTOS_ORIENTATION_LEFT_MIRROR,
                                             "861c54e46b0269c5647b39143f2c1a0cce5602a206cbffd7c40afd1047e82900");
}


static void
photos_test_gegl_buffer_apply_orientation_right_0 (PhotosTestGeglFixture *fixture, gconstpointer user_data)
{
  photos_test_gegl_buffer_check_orientation (fixture,
                                             PHOTOS_ORIENTATION_RIGHT,
                                             "9a951b3dec1094f95bc2decb30ff10adbcef428fa4e558ff79486423cc134d94");
}


static void
photos_test_gegl_buffer_apply_orientation_right_1 (PhotosTestGeglFixture *fixture, gconstpointer user_data)
{
  photos_test_gegl_buffer_check_orientation (fixture,
                                             PHOTOS_ORIENTATION_RIGHT,
                                             "32c126ebd027e06d7c457c2908b5494c1ec86959d86ff74c2905701b40db3202");
}


static void
photos_test_gegl_buffer_apply_orientation_right_2 (PhotosTestGeglFixture *fixture, gconstpointer user_data)
{
  photos_test_gegl_buffer_check_orientation (fixture,
                                             PHOTOS_ORIENTATION_RIGHT,
                                             "116187d941c7456952f96907c6b11cbf7a52dbcd8dfa084838deb66e45a6abf2");
}


static void
photos_test_gegl_buffer_apply_orientation_right_3 (PhotosTestGeglFixture *fixture, gconstpointer user_data)
{
  photos_test_gegl_buffer_check_orientation (fixture,
                                             PHOTOS_ORIENTATION_RIGHT,
                                             "fb7d1b4e4617dfae6a01d5f008c3459bdfad3d1f8eaa9969dc8e66025af0d91e");
}


static void
photos_test_gegl_buffer_apply_orientation_right_mirror_0 (PhotosTestGeglFixture *fixture, gconstpointer user_data)
{
  photos_test_gegl_buffer_check_orientation (fixture,
                                             PHOTOS_ORIENTATION_RIGHT_MIRROR,
                                             "44d05e57860c7778c61b8781b42a6ed4b1da260d7026a2123563706cdf61702a");
}


static void
photos_test_gegl_buffer_apply_orientation_right_mirror_1 (PhotosTestGeglFixture *fixture, gconstpointer user_data)
{
  photos_test_gegl_buffer_check_orientation (fixture,
                                             PHOTOS_ORIENTATION_RIGHT_MIRROR,
                                             "70dc3694c576e9fc4c63a33cc1a51afe59a52fbe0bd36b6671e909d7b5ddf124");
}


static void
photos_test_gegl_buffer_apply_orientation_right_mirror_2 (PhotosTestGeglFixture *fixture, gconstpointer user_data)
{
  photos_test_gegl_buffer_check_orientation (fixture,
                                             PHOTOS_ORIENTATION_RIGHT_MIRROR,
                                             "6e7130188741822803c6c3c36ad36221c16033b94ec2b4540830ce1dc7f8372b");
}


static void
photos_test_gegl_buffer_apply_orientation_right_mirror_3 (PhotosTestGeglFixture *fixture, gconstpointer user_data)
{
  photos_test_gegl_buffer_check_orientation (fixture,
                                             PHOTOS_ORIENTATION_RIGHT_MIRROR,
                                             "adbf01e7f0290cf20cd07b70197b379ad52aff4141f881ceb686aa3c0a3162f1");
}


static void
photos_test_gegl_buffer_apply_orientation_top_0 (PhotosTestGeglFixture *fixture, gconstpointer user_data)
{
  photos_test_gegl_buffer_check_orientation (fixture,
                                             PHOTOS_ORIENTATION_TOP,
                                             "f3f8ea1b6680da7dbf08ecf5ba76da6f4ece29b48bcdeaab2be808f8a09c874f");
}


static void
photos_test_gegl_buffer_apply_orientation_top_1 (PhotosTestGeglFixture *fixture, gconstpointer user_data)
{
  photos_test_gegl_buffer_check_orientation (fixture,
                                             PHOTOS_ORIENTATION_TOP,
                                             "e031a5cab0e7a3794e34a91859618c93c8fdd1babd1fedf39f10f9585b0b5fa8");
}


static void
photos_test_gegl_buffer_apply_orientation_top_2 (PhotosTestGeglFixture *fixture, gconstpointer user_data)
{
  photos_test_gegl_buffer_check_orientation (fixture,
                                             PHOTOS_ORIENTATION_TOP,
                                             "2b759cc636f78ff70ef197b9b9495214f9e8de6b3743175f25186c24d9caed5f");
}


static void
photos_test_gegl_buffer_apply_orientation_top_3 (PhotosTestGeglFixture *fixture, gconstpointer user_data)
{
  photos_test_gegl_buffer_check_orientation (fixture,
                                             PHOTOS_ORIENTATION_TOP,
                                             "a9e40c84633473fb2168bcb7424bf1d294d886d61c42139a9d787510c4be31ea");
}


static void
photos_test_gegl_buffer_apply_orientation_top_mirror_0 (PhotosTestGeglFixture *fixture, gconstpointer user_data)
{
  photos_test_gegl_buffer_check_orientation (fixture,
                                             PHOTOS_ORIENTATION_TOP_MIRROR,
                                             "2a518907c86e4edc21e8dc3f5248d6766ca84761ea4024031264d8d0bd60ba65");
}


static void
photos_test_gegl_buffer_apply_orientation_top_mirror_1 (PhotosTestGeglFixture *fixture, gconstpointer user_data)
{
  photos_test_gegl_buffer_check_orientation (fixture,
                                             PHOTOS_ORIENTATION_TOP_MIRROR,
                                             "6477df1bb3c47ad79faa2e7a8398d462eee3c09141593d06bcff35c53beba702");
}


static void
photos_test_gegl_buffer_apply_orientation_top_mirror_2 (PhotosTestGeglFixture *fixture, gconstpointer user_data)
{
  photos_test_gegl_buffer_check_orientation (fixture,
                                             PHOTOS_ORIENTATION_TOP_MIRROR,
                                             "88e5059c2d65f75a9b21e52cd35906865c84c0645321a59b7c86406e51a72d82");
}


static void
photos_test_gegl_buffer_apply_orientation_top_mirror_3 (PhotosTestGeglFixture *fixture, gconstpointer user_data)
{
  photos_test_gegl_buffer_check_orientation (fixture,
                                             PHOTOS_ORIENTATION_TOP_MIRROR,
                                             "9d5e5a3efe72f3560abe93e2ab14633548c6500274566d37e3a2745b7b9e2b02");
}


static void
photos_test_gegl_buffer_zoom_in_0 (PhotosTestGeglFixture *fixture, gconstpointer user_data)
{
  const gchar *const checksums[] =
    {
      "12d60499ebbf9533040792debe28c8bcdebb5ac6b26e2864b26347f42fade116",
#if PHOTOS_GEGL_BABL_CHECK_VERSION(0, 1, 67)
      "caaa64e34505b94bd64fee635376b76439566cc1fd8f73e39bd3ca710af540e3",
#endif
      NULL
    };

  photos_test_gegl_buffer_check_zoom (fixture, 1.4, checksums, 280, 280, 0, 0);
}


static void
photos_test_gegl_buffer_zoom_in_1 (PhotosTestGeglFixture *fixture, gconstpointer user_data)
{
  const gchar *const checksums[] =
    {
      "f8a0d6eb8c2fdc3f5592f39beaea9477aaf33760e5985a092c99bbb02e735c21",
#if PHOTOS_GEGL_BABL_CHECK_VERSION(0, 1, 67)
      "1e0f18651c3e1c5578c317e4070c024d2e70679b79de6a3696f204f47d157dd2",
#endif
      NULL
    };

  photos_test_gegl_buffer_check_zoom (fixture, 4.0, checksums, 800, 800, 0, 0);
}


static void
photos_test_gegl_buffer_zoom_nop (PhotosTestGeglFixture *fixture, gconstpointer user_data)
{
  const gchar *const checksums[] =
    {
      "2b759cc636f78ff70ef197b9b9495214f9e8de6b3743175f25186c24d9caed5f",
      NULL
    };

  photos_test_gegl_buffer_check_zoom (fixture, 1.0, checksums, 200, 200, 0, 0);
}


static void
photos_test_gegl_buffer_zoom_out_0 (PhotosTestGeglFixture *fixture, gconstpointer user_data)
{
  const gchar *const checksums[] =
    {
      "d8c9c2c09079e0064f0633a2e05ed3ddce4f00cda3991b87d8cd79cccf319f6d",
      NULL
    };

  photos_test_gegl_buffer_check_zoom (fixture, 0.6, checksums, 120, 120, 0, 0);
}


static void
photos_test_gegl_buffer_zoom_out_1 (PhotosTestGeglFixture *fixture, gconstpointer user_data)
{
  const gchar *const checksums[] =
    {
      "82cfa8a533f8800bd213e47fd52593a8f3b78de2bcd4d9b28084cb7825e50e23",
      NULL
    };

  photos_test_gegl_buffer_check_zoom (fixture, 0.25, checksums, 50, 50, 0, 0);
}


static void
photos_test_gegl_legacy_convert_between_buffer_pixbuf_0 (PhotosTestGeglFixture *fixture, gconstpointer user_data)
{
  photos_test_gegl_buffer_check_conversion (fixture,
                                            FALSE,
                                            "f3f8ea1b6680da7dbf08ecf5ba76da6f4ece29b48bcdeaab2be808f8a09c874f",
                                            "f3f8ea1b6680da7dbf08ecf5ba76da6f4ece29b48bcdeaab2be808f8a09c874f");
}


static void
photos_test_gegl_legacy_convert_between_buffer_pixbuf_1 (PhotosTestGeglFixture *fixture, gconstpointer user_data)
{
  photos_test_gegl_buffer_check_conversion (fixture,
                                            FALSE,
                                            "bac358627643173f32bf2a7c2740fcaa8ade0fc0800622e06655b7ca78e11bdd",
                                            "e031a5cab0e7a3794e34a91859618c93c8fdd1babd1fedf39f10f9585b0b5fa8");
}


static void
photos_test_gegl_legacy_convert_between_buffer_pixbuf_2 (PhotosTestGeglFixture *fixture, gconstpointer user_data)
{
  photos_test_gegl_buffer_check_conversion (fixture,
                                            TRUE,
                                            "2b759cc636f78ff70ef197b9b9495214f9e8de6b3743175f25186c24d9caed5f",
                                            "2b759cc636f78ff70ef197b9b9495214f9e8de6b3743175f25186c24d9caed5f");
}


static void
photos_test_gegl_legacy_convert_between_buffer_pixbuf_3 (PhotosTestGeglFixture *fixture, gconstpointer user_data)
{
  photos_test_gegl_buffer_check_conversion (fixture,
                                            TRUE,
                                            "a9e40c84633473fb2168bcb7424bf1d294d886d61c42139a9d787510c4be31ea",
                                            "a9e40c84633473fb2168bcb7424bf1d294d886d61c42139a9d787510c4be31ea");
}


static void
photos_test_gegl_legacy_create_pixbuf_from_node_0 (PhotosTestGeglFixture *fixture, gconstpointer user_data)
{
  photos_test_gegl_buffer_check_pixbuf (fixture,
                                        FALSE,
                                        "f3f8ea1b6680da7dbf08ecf5ba76da6f4ece29b48bcdeaab2be808f8a09c874f");
}


static void
photos_test_gegl_legacy_create_pixbuf_from_node_1 (PhotosTestGeglFixture *fixture, gconstpointer user_data)
{
  photos_test_gegl_buffer_check_pixbuf (fixture,
                                        TRUE,
                                        "2b759cc636f78ff70ef197b9b9495214f9e8de6b3743175f25186c24d9caed5f");
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

  g_test_add ("/gegl/buffer/zoom/nop",
              PhotosTestGeglFixture,
              NULL,
              photos_test_gegl_setup_with_alpha_even_dimensions,
              photos_test_gegl_buffer_zoom_nop,
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
