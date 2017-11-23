/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2012 – 2017 Red Hat, Inc.
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


#include "config.h"

#include <glib.h>

#include "photos-filterable.h"


G_DEFINE_INTERFACE (PhotosFilterable, photos_filterable, G_TYPE_OBJECT);


static gboolean
photos_filterable_default_get_builtin (PhotosFilterable *self)
{
  return FALSE;
}


static void
photos_filterable_default_init (PhotosFilterableInterface *iface)
{
  iface->get_builtin = photos_filterable_default_get_builtin;
}


gboolean
photos_filterable_get_builtin (PhotosFilterable *self)
{
  g_return_val_if_fail (PHOTOS_IS_FILTERABLE (self), FALSE);
  return PHOTOS_FILTERABLE_GET_IFACE (self)->get_builtin (self);
}


gchar *
photos_filterable_get_filter (PhotosFilterable *self)
{
  g_return_val_if_fail (PHOTOS_IS_FILTERABLE (self), NULL);
  return PHOTOS_FILTERABLE_GET_IFACE (self)->get_filter (self);
}


const gchar *
photos_filterable_get_id (PhotosFilterable *self)
{
  g_return_val_if_fail (PHOTOS_IS_FILTERABLE (self), NULL);
  return PHOTOS_FILTERABLE_GET_IFACE (self)->get_id (self);
}


gchar *
photos_filterable_get_where (PhotosFilterable *self)
{
  g_return_val_if_fail (PHOTOS_IS_FILTERABLE (self), NULL);
  return PHOTOS_FILTERABLE_GET_IFACE (self)->get_where (self);
}
