/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2012, 2013 Red Hat, Inc.
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

#ifndef PHOTOS_COLLECTION_MANAGER_H
#define PHOTOS_COLLECTION_MANAGER_H

#include "photos-base-manager.h"

G_BEGIN_DECLS

#define PHOTOS_TYPE_COLLECTION_MANAGER (photos_collection_manager_get_type ())

#define PHOTOS_COLLECTION_MANAGER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
   PHOTOS_TYPE_COLLECTION_MANAGER, PhotosCollectionManager))

#define PHOTOS_COLLECTION_MANAGER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
   PHOTOS_TYPE_COLLECTION_MANAGER, PhotosCollectionManagerClass))

#define PHOTOS_IS_COLLECTION_MANAGER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
   PHOTOS_TYPE_COLLECTION_MANAGER))

#define PHOTOS_IS_COLLECTION_MANAGER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
   PHOTOS_TYPE_COLLECTION_MANAGER))

#define PHOTOS_COLLECTION_MANAGER_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
   PHOTOS_TYPE_COLLECTION_MANAGER, PhotosCollectionManagerClass))

typedef struct _PhotosCollectionManager        PhotosCollectionManager;
typedef struct _PhotosCollectionManagerClass   PhotosCollectionManagerClass;
typedef struct _PhotosCollectionManagerPrivate PhotosCollectionManagerPrivate;

struct _PhotosCollectionManager
{
  PhotosBaseManager parent_instance;
  PhotosCollectionManagerPrivate *priv;
};

struct _PhotosCollectionManagerClass
{
  PhotosBaseManagerClass parent_class;
};

GType                     photos_collection_manager_get_type           (void) G_GNUC_CONST;

PhotosBaseManager        *photos_collection_manager_new                (void);

gchar                    *photos_collection_manager_get_where          (PhotosCollectionManager *self);

G_END_DECLS

#endif /* PHOTOS_COLLECTION_MANAGER_H */
