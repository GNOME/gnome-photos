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

#ifndef PHOTOS_SOURCE_H
#define PHOTOS_SOURCE_H

#include <gio/gio.h>
#include <goa/goa.h>

G_BEGIN_DECLS

#define PHOTOS_TYPE_SOURCE (photos_source_get_type ())
G_DECLARE_FINAL_TYPE (PhotosSource, photos_source, PHOTOS, SOURCE, GObject);

#define PHOTOS_SOURCE_STOCK_ALL   "all"
#define PHOTOS_SOURCE_STOCK_LOCAL "local"

PhotosSource       *photos_source_new                    (const gchar *id, const gchar *name, gboolean builtin);

PhotosSource       *photos_source_new_from_goa_object    (GoaObject *object);

PhotosSource       *photos_source_new_from_mount         (GMount *mount);

const gchar        *photos_source_get_name               (PhotosSource *self);

GoaObject          *photos_source_get_goa_object         (PhotosSource *self);

GIcon              *photos_source_get_icon               (PhotosSource *self);

GMount             *photos_source_get_mount              (PhotosSource *self);

GIcon              *photos_source_get_symbolic_icon      (PhotosSource *self);

G_END_DECLS

#endif /* PHOTOS_SOURCE_H */
