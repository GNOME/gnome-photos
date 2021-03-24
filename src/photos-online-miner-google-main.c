/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2021 Red Hat, Inc.
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

#include <glib.h>

#include "photos-debug.h"
#include "photos-online-miner-google.h"


gint
main (gint argc, gchar *argv[])
{
  g_autoptr (GApplication) app = NULL;
  gint exit_status = 0;

  photos_debug_init ();

  g_set_prgname (PACKAGE_TARNAME "-online-miner-google");

  app = photos_online_miner_google_new ();
  exit_status = g_application_run (app, argc, argv);

  return exit_status;
}
