/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2012 – 2017 Red Hat, Inc.
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
 *   + Documents
 */


#include "config.h"

#include <glib.h>

#include "egg-counter.h"
#include "photos-application.h"
#include "photos-debug.h"
#include "photos-remote-display-manager.h"


static void
photos_main_counter_arena_foreach (EggCounter *counter, gpointer user_data)
{
  gint64 count;

  count = egg_counter_get (counter);
  photos_debug (PHOTOS_DEBUG_MEMORY, "%s.%s = %" G_GINT64_FORMAT, counter->category, counter->name, count);
}


gint
main (gint argc, gchar *argv[])
{
  EggCounterArena *counter_arena;
  gint exit_status;

  {
    g_autoptr (GApplication) app = NULL;
    g_autoptr (PhotosRemoteDisplayManager) remote_display_mngr = NULL;

    photos_debug_init ();

    app = photos_application_new ();
    if (g_getenv ("GNOME_PHOTOS_PERSIST") != NULL)
      g_application_hold (app);

    remote_display_mngr = photos_remote_display_manager_dup_singleton ();
    exit_status = g_application_run (app, argc, argv);
  }

  counter_arena = egg_counter_arena_get_default ();
  egg_counter_arena_foreach (counter_arena, photos_main_counter_arena_foreach, NULL);

  return exit_status;
}
