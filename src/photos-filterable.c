/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2012 – 2016 Red Hat, Inc.
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

#include <glib.h>

#include "photos-filterable.h"


G_DEFINE_INTERFACE (PhotosFilterable, photos_filterable, G_TYPE_INVALID);


static void
photos_filterable_default_init (PhotosFilterableInterface *iface)
{
}


gchar *
photos_filterable_get_filter (PhotosFilterable *iface)
{
  g_return_val_if_fail (PHOTOS_IS_FILTERABLE (iface), NULL);
  return PHOTOS_FILTERABLE_GET_INTERFACE (iface)->get_filter (iface);
}


const gchar *
photos_filterable_get_id (PhotosFilterable *self)
{
  g_return_val_if_fail (PHOTOS_IS_FILTERABLE (self), NULL);
  return PHOTOS_FILTERABLE_GET_INTERFACE (self)->get_id (self);
}


gchar *
photos_filterable_get_where (PhotosFilterable *iface)
{
  g_return_val_if_fail (PHOTOS_IS_FILTERABLE (iface), NULL);
  return PHOTOS_FILTERABLE_GET_INTERFACE (iface)->get_where (iface);
}
