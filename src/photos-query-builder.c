/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2012 Red Hat, Inc.
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

#include <gio/gio.h>

#include "photos-query-builder.h"


static gchar *
photos_query_builder_convert_path_to_uri (const gchar *path)
{
  GFile *file;
  gchar *uri;

  if (path == NULL)
    return g_strdup ("");

  file = g_file_new_for_path (path);
  uri = g_file_get_uri (file);
  g_object_unref (file);

  return uri;
}


gchar *
photos_query_builder_filter_local (void)
{
  GFile *file;
  gchar *desktop_uri;
  gchar *documents_uri;
  gchar *download_uri;
  gchar *filter;
  const gchar *path;

  path = g_get_user_special_dir (G_USER_DIRECTORY_DESKTOP);
  desktop_uri = photos_query_builder_convert_path_to_uri (path);

  path = g_get_user_special_dir (G_USER_DIRECTORY_DOCUMENTS);
  documents_uri = photos_query_builder_convert_path_to_uri (path);

  path = g_get_user_special_dir (G_USER_DIRECTORY_DOWNLOAD);
  download_uri = photos_query_builder_convert_path_to_uri (path);

  filter = g_strdup_printf ("((fn:starts-with (nie:url (?urn), \"%s\"))"
                            " || (fn:starts-with (nie:url (?urn), \"%s\"))"
                            " || (fn:starts-with (nie:url (?urn), \"%s\")))",
                            desktop_uri,
                            documents_uri,
                            download_uri);
  g_free (desktop_uri);
  g_free (documents_uri);
  g_free (download_uri);

  return filter;
}
