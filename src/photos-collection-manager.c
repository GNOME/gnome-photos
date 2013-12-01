/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2012, 2013, 2014 Red Hat, Inc.
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

#include "photos-base-item.h"
#include "photos-collection-manager.h"


G_DEFINE_TYPE (PhotosCollectionManager, photos_collection_manager, PHOTOS_TYPE_BASE_MANAGER);


static gchar *
photos_collection_manager_get_where (PhotosBaseManager *mngr, gint flags)
{
  GObject *collection;

  collection = photos_base_manager_get_active_object (mngr);
  if (collection == NULL)
    return g_strdup ("");

  return photos_base_item_get_where (PHOTOS_BASE_ITEM (collection));
}


static void
photos_collection_manager_init (PhotosCollectionManager *self)
{
}


static void
photos_collection_manager_class_init (PhotosCollectionManagerClass *class)
{
  PhotosBaseManagerClass *base_manager_class = PHOTOS_BASE_MANAGER_CLASS (class);

  base_manager_class->get_where = photos_collection_manager_get_where;
}


PhotosBaseManager *
photos_collection_manager_new (void)
{
  return g_object_new (PHOTOS_TYPE_COLLECTION_MANAGER, NULL);
}
