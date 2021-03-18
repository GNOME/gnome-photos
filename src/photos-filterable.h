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

#ifndef PHOTOS_FILTERABLE_H
#define PHOTOS_FILTERABLE_H

#include <glib-object.h>

G_BEGIN_DECLS

#define PHOTOS_TYPE_FILTERABLE (photos_filterable_get_type ())
G_DECLARE_INTERFACE (PhotosFilterable, photos_filterable, PHOTOS, FILTERABLE, GObject);

struct _PhotosFilterableInterface
{
  GTypeInterface parent_iface;

  gboolean (*get_builtin) (PhotosFilterable *self);
  gchar *(*get_filter) (PhotosFilterable *self);
  const gchar *(*get_id) (PhotosFilterable *self);
  gboolean (*is_search_criterion) (PhotosFilterable *self);
};

gboolean            photos_filterable_get_builtin        (PhotosFilterable *self);

gchar              *photos_filterable_get_filter         (PhotosFilterable *self);

const gchar        *photos_filterable_get_id             (PhotosFilterable *self);

gboolean            photos_filterable_is_search_criterion  (PhotosFilterable *self);

G_END_DECLS

#endif /* PHOTOS_FILTERABLE_H */
