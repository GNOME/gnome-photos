/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2012 – 2021 Red Hat, Inc.
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

#ifndef PHOTOS_QUERY_H
#define PHOTOS_QUERY_H

#include <glib-object.h>

#include "photos-search-context.h"
#include "photos-source.h"

G_BEGIN_DECLS

#define PHOTOS_QUERY_COLLECTIONS_IDENTIFIER "photos:collection:"
#define PHOTOS_QUERY_LOCAL_COLLECTIONS_IDENTIFIER "photos:collection:local:"

#define PHOTOS_TYPE_QUERY (photos_query_get_type ())
G_DECLARE_FINAL_TYPE (PhotosQuery, photos_query, PHOTOS, QUERY, GObject);

typedef enum
{
  PHOTOS_QUERY_COLUMNS_URN,
  PHOTOS_QUERY_COLUMNS_URI,
  PHOTOS_QUERY_COLUMNS_FILENAME,
  PHOTOS_QUERY_COLUMNS_MIME_TYPE,
  PHOTOS_QUERY_COLUMNS_TITLE,
  PHOTOS_QUERY_COLUMNS_AUTHOR,
  PHOTOS_QUERY_COLUMNS_MTIME,
  PHOTOS_QUERY_COLUMNS_IDENTIFIER,
  PHOTOS_QUERY_COLUMNS_RDF_TYPE,
  PHOTOS_QUERY_COLUMNS_RESOURCE_URN,
  PHOTOS_QUERY_COLUMNS_RESOURCE_FAVORITE,
  PHOTOS_QUERY_COLUMNS_RESOURCE_SHARED,
  PHOTOS_QUERY_COLUMNS_DATE_CREATED,
  PHOTOS_QUERY_COLUMNS_WIDTH,
  PHOTOS_QUERY_COLUMNS_HEIGHT,
  PHOTOS_QUERY_COLUMNS_EQUIPMENT,
  PHOTOS_QUERY_COLUMNS_ORIENTATION,
  PHOTOS_QUERY_COLUMNS_EXPOSURE_TIME,
  PHOTOS_QUERY_COLUMNS_FNUMBER,
  PHOTOS_QUERY_COLUMNS_FOCAL_LENGTH,
  PHOTOS_QUERY_COLUMNS_ISO_SPEED,
  PHOTOS_QUERY_COLUMNS_FLASH,
  PHOTOS_QUERY_COLUMNS_LOCATION
} PhotosQueryColumns;

typedef enum
{
  PHOTOS_QUERY_FLAGS_NONE           = 0,
  PHOTOS_QUERY_FLAGS_UNFILTERED     = 1 << 0,
  PHOTOS_QUERY_FLAGS_COLLECTIONS    = 1 << 1,
  PHOTOS_QUERY_FLAGS_FAVORITES      = 1 << 2,
  PHOTOS_QUERY_FLAGS_IMPORT         = 1 << 3,
  PHOTOS_QUERY_FLAGS_LOCAL          = 1 << 4,
  PHOTOS_QUERY_FLAGS_OVERVIEW       = 1 << 5,
  PHOTOS_QUERY_FLAGS_SEARCH         = 1 << 6,
  PHOTOS_QUERY_FLAGS_UNLIMITED      = 1 << 7
} PhotosQueryFlags;

PhotosQuery     *photos_query_new           (PhotosSearchContextState *state, const gchar *sparql);

const gchar     *photos_query_get_sparql    (PhotosQuery *self);

PhotosSource    *photos_query_get_source    (PhotosQuery *self);

const gchar     *photos_query_get_tag       (PhotosQuery *self);

void             photos_query_set_tag       (PhotosQuery *self, const gchar *tag);

G_END_DECLS

#endif /* PHOTOS_QUERY_H */
