/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2018 – 2019 Red Hat, Inc.
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

  exit_status = g_test_run ();

  gegl_exit ();
  return exit_status;
}
