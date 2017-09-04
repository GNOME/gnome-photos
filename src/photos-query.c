/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2012 – 2017 Red Hat, Inc.
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

#include "photos-base-manager.h"
#include "photos-query.h"
#include "photos-utils.h"


const gchar *PHOTOS_QUERY_COLLECTIONS_IDENTIFIER = "photos:collection:";
const gchar *PHOTOS_QUERY_LOCAL_COLLECTIONS_IDENTIFIER = "photos:collection:local:";


PhotosQuery *
photos_query_new (PhotosSearchContextState *state, gchar *sparql)
{
  PhotosQuery *query;

  query = g_slice_new0 (PhotosQuery);

  if (state != NULL)
    {
      GObject *active_object;

      active_object = photos_base_manager_get_active_object (state->src_mngr);
      if (active_object != NULL)
        query->source = PHOTOS_SOURCE (g_object_ref (active_object));
    }

  query->sparql = sparql;

  return query;
}


void
photos_query_set_tag (PhotosQuery *query, const gchar *tag)
{
  photos_utils_set_string (&query->tag, tag);
}


void
photos_query_free (PhotosQuery *query)
{
  g_clear_object (&query->source);
  g_free (query->sparql);
  g_free (query->tag);
  g_slice_free (PhotosQuery, query);
}
