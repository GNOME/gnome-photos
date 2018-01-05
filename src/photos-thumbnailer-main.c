/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2017 Red Hat, Inc.
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

#include <glib.h>

#include "egg-counter.h"
#include "photos-debug.h"
#include "photos-thumbnailer.h"


static void
photos_thumbnailer_main_counter_arena_foreach (EggCounter *counter, gpointer user_data)
{
  gint64 count;

  count = egg_counter_get (counter);
  photos_debug (PHOTOS_DEBUG_MEMORY, "%s.%s = %" G_GINT64_FORMAT, counter->category, counter->name, count);
}


gint
main (gint argc, gchar *argv[])
{
  EggCounterArena *counter_arena;
  GApplication *app;
  gint exit_status = 0;

  photos_debug_init ();

  g_set_prgname (PACKAGE_TARNAME "-thumbnailer");

  app = photos_thumbnailer_new ();
  exit_status = g_application_run (app, argc, argv);
  g_object_unref (app);

  counter_arena = egg_counter_arena_get_default ();
  egg_counter_arena_foreach (counter_arena, photos_thumbnailer_main_counter_arena_foreach, NULL);

  return exit_status;
}
