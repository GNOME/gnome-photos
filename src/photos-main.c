/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2012, 2014 Red Hat, Inc.
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

/* Based on code from:
 *   + Documents
 */


#include "config.h"

#include <locale.h>

#include <glib.h>
#include <glib/gi18n.h>

#include "photos-application.h"
#include "photos-debug.h"
#include "photos-remote-display-manager.h"


gint
main (gint argc, gchar *argv[])
{
  GtkApplication *app;
  PhotosRemoteDisplayManager *remote_display_mngr;
  gint exit_status;

  setlocale (LC_ALL, "");

  photos_debug_init ();

  bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);

  g_set_prgname (PACKAGE_TARNAME);

  app = photos_application_new ();
  if (g_getenv ("GNOME_PHOTOS_PERSIST") != NULL)
    g_application_hold (G_APPLICATION (app));

  remote_display_mngr = photos_remote_display_manager_dup_singleton ();
  exit_status = g_application_run (G_APPLICATION (app), argc, argv);
  g_object_unref (remote_display_mngr);
  g_object_unref (app);

  return exit_status;
}
