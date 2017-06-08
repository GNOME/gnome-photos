/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2015 Pranav Kant
 * Copyright © 2016 – 2017 Red Hat, Inc.
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

#include <stdarg.h>

#include <glib.h>

#include "photos-debug.h"


static PhotosDebugFlags debug_flags;


void
photos_debug_init (void)
{
  const GDebugKey keys[] =
    {
      { "application", PHOTOS_DEBUG_APPLICATION },
      { "dlna", PHOTOS_DEBUG_DLNA },
      { "gegl", PHOTOS_DEBUG_GEGL },
      { "memory", PHOTOS_DEBUG_MEMORY },
      { "network", PHOTOS_DEBUG_NETWORK },
      { "thumbnailer", PHOTOS_DEBUG_THUMBNAILER },
      { "tracker", PHOTOS_DEBUG_TRACKER }
    };
  const gchar *debug_string;

  debug_string = g_getenv ("GNOME_PHOTOS_DEBUG");
  debug_flags = g_parse_debug_string (debug_string, keys, G_N_ELEMENTS (keys));
}


void photos_debug (guint flags, const char *fmt, ...)
{
  if ((debug_flags & flags) != 0)
    {
      gchar *message;
      va_list ap;

      va_start (ap, fmt);
      message = g_strdup_vprintf (fmt, ap);
      va_end (ap);

      g_debug ("%s", message);

      g_free (message);
    }
}
