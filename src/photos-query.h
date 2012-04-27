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

#ifndef PHOTOS_QUERY_H
#define PHOTOS_QUERY_H

G_BEGIN_DECLS

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
  PHOTOS_QUERY_COLUMNS_RESOURCE_SHARED
} PhotosQueryColumns;

G_END_DECLS

#endif /* PHOTOS_QUERY_H */
