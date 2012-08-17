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

#ifndef PHOTOS_QUERY_BUILDER_H
#define PHOTOS_QUERY_BUILDER_H

#include <glib.h>

#include "photos-query.h"

G_BEGIN_DECLS

PhotosQuery  *photos_query_builder_count_query         (void);

PhotosQuery  *photos_query_builder_global_query        (void);

PhotosQuery  *photos_query_builder_single_query        (gint flags, const gchar *resource);

gchar        *photos_query_builder_filter_local        (void);

G_END_DECLS

#endif /* PHOTOS_QUERY_BUILDER */
