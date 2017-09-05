/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2013 – 2017 Red Hat, Inc.
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

#include <glib.h>
#include <tracker-sparql.h>

#include "photos-search-controller.h"


struct _PhotosSearchController
{
  GObject parent_instance;
  gchar *str;
};

enum
{
  SEARCH_STRING_CHANGED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };


G_DEFINE_TYPE (PhotosSearchController, photos_search_controller, G_TYPE_OBJECT);


static void
photos_search_controller_finalize (GObject *object)
{
  PhotosSearchController *self = PHOTOS_SEARCH_CONTROLLER (object);

  g_free (self->str);

  G_OBJECT_CLASS (photos_search_controller_parent_class)->finalize (object);
}


static void
photos_search_controller_init (PhotosSearchController *self)
{
  self->str = g_strdup ("");
}


static void
photos_search_controller_class_init (PhotosSearchControllerClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->finalize = photos_search_controller_finalize;

  signals[SEARCH_STRING_CHANGED] = g_signal_new ("search-string-changed",
                                                  G_TYPE_FROM_CLASS (class),
                                                  G_SIGNAL_RUN_LAST,
                                                  0,
                                                  NULL, /*accumulator */
                                                  NULL, /*accu_data */
                                                  g_cclosure_marshal_VOID__STRING,
                                                  G_TYPE_NONE,
                                                  1,
                                                  G_TYPE_STRING);
}


PhotosSearchController *
photos_search_controller_new (void)
{
  return g_object_new (PHOTOS_TYPE_SEARCH_CONTROLLER, NULL);
}


const gchar *
photos_search_controller_get_string (PhotosSearchController *self)
{
  return self->str;
}


gchar **
photos_search_controller_get_terms (PhotosSearchController *self)
{
  gchar *escaped_str;
  gchar *str;
  gchar **terms;

  escaped_str = tracker_sparql_escape_string (self->str);
  str = g_utf8_casefold (escaped_str, -1);
  /* TODO: find out what str.replace(/ + /g, ' ') does */
  terms = g_strsplit (str, " ", -1);
  g_free (str);
  g_free (escaped_str);
  return terms;
}


void
photos_search_controller_set_string (PhotosSearchController *self, const gchar *str)
{
  if (g_strcmp0 (self->str, str) == 0)
    return;

  g_free (self->str);
  self->str = g_strdup (str);
  g_signal_emit (self, signals[SEARCH_STRING_CHANGED], 0, self->str);
}
