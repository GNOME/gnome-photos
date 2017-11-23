/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2014 – 2017 Red Hat, Inc.
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

#ifndef PHOTOS_SEARCH_MATCH_H
#define PHOTOS_SEARCH_MATCH_H

#include <glib-object.h>

G_BEGIN_DECLS

#define PHOTOS_TYPE_SEARCH_MATCH (photos_search_match_get_type ())
G_DECLARE_FINAL_TYPE (PhotosSearchMatch, photos_search_match, PHOTOS, SEARCH_MATCH, GObject);

#define PHOTOS_SEARCH_MATCH_STOCK_ALL "all"
#define PHOTOS_SEARCH_MATCH_STOCK_AUTHOR "author"
#define PHOTOS_SEARCH_MATCH_STOCK_TITLE "title"

PhotosSearchMatch    *photos_search_match_new                (const gchar *id,
                                                              const gchar *name,
                                                              const gchar *filter);

void                  photos_search_match_set_filter_term    (PhotosSearchMatch *self, const gchar *term);


G_END_DECLS

#endif /* PHOTOS_SEARCH_MATCH_H */
