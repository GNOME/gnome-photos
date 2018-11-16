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


typedef struct _PhotosTestGeglFixture PhotosTestGeglFixture;

struct _PhotosTestGeglFixture
{
};


static void
photos_test_gegl_setup (PhotosTestGeglFixture *fixture, gconstpointer user_data)
{
}


static void
photos_test_gegl_teardown (PhotosTestGeglFixture *fixture, gconstpointer user_data)
{
}


static void
photos_test_gegl_buffer_apply_orientation_bottom_0 (PhotosTestGeglFixture *fixture, gconstpointer user_data)
{
}


gint
main (gint argc, gchar *argv[])
{
  gint exit_status;

  setlocale (LC_ALL, "");
  g_test_init (&argc, &argv, NULL);
  photos_debug_init ();
  photos_gegl_init ();
  photos_gegl_ensure_builtins ();

  g_test_add ("/gegl/buffer/apply_orientation/bottom-0",
              PhotosTestGeglFixture,
              NULL,
              photos_test_gegl_setup,
              photos_test_gegl_buffer_apply_orientation_bottom_0,
              photos_test_gegl_teardown);

  exit_status = g_test_run ();

  gegl_exit ();
  return exit_status;
}
