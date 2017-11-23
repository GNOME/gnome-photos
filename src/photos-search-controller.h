/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2013 – 2017 Red Hat, Inc.
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

#ifndef PHOTOS_SEARCH_CONTROLLER_H
#define PHOTOS_SEARCH_CONTROLLER_H

#include <glib-object.h>

G_BEGIN_DECLS

#define PHOTOS_TYPE_SEARCH_CONTROLLER (photos_search_controller_get_type ())
G_DECLARE_FINAL_TYPE (PhotosSearchController, photos_search_controller, PHOTOS, SEARCH_CONTROLLER, GObject)

PhotosSearchController    *photos_search_controller_new            (void);

const gchar               *photos_search_controller_get_string     (PhotosSearchController *self);

gchar                    **photos_search_controller_get_terms      (PhotosSearchController *self);

void                       photos_search_controller_set_string     (PhotosSearchController *self, const gchar *str);

G_END_DECLS

#endif /* PHOTOS_SEARCH_CONTROLLER_H */
