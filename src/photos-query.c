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

#include "photos-query.h"
#include "photos-source-manager.h"


PhotosQuery *
photos_query_new (gchar *sparql)
{
  PhotosBaseManager *src_mngr;
  PhotosQuery *query;

  query = g_slice_new0 (PhotosQuery);

  src_mngr = photos_source_manager_new ();
  query->source = PHOTOS_SOURCE (photos_base_manager_get_active_object (src_mngr));
  g_object_unref (src_mngr);

  query->sparql = sparql;

  return query;
}


void
photos_query_free (PhotosQuery *query)
{
  g_object_unref (query->source);
  g_free (query->sparql);
  g_slice_free (PhotosQuery, query);
}
