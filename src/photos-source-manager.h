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

#ifndef PHOTOS_SOURCE_MANAGER_H
#define PHOTOS_SOURCE_MANAGER_H

#include "photos-base-manager.h"

G_BEGIN_DECLS

#define PHOTOS_TYPE_SOURCE_MANAGER (photos_base_manager_get_type ())

#define PHOTOS_SOURCE_MANAGER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
   PHOTOS_TYPE_SOURCE_MANAGER, PhotosSourceManager))

#define PHOTOS_SOURCE_MANAGER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
   PHOTOS_TYPE_SOURCE_MANAGER, PhotosSourceManagerClass))

#define PHOTOS_IS_SOURCE_MANAGER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
   PHOTOS_TYPE_SOURCE_MANAGER))

#define PHOTOS_IS_SOURCE_MANAGER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
   PHOTOS_TYPE_SOURCE_MANAGER))

#define PHOTOS_SOURCE_MANAGER_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
   PHOTOS_TYPE_SOURCE_MANAGER, PhotosSourceManagerClass))

typedef struct _PhotosSourceManager        PhotosSourceManager;
typedef struct _PhotosSourceManagerClass   PhotosSourceManagerClass;
typedef struct _PhotosSourceManagerPrivate PhotosSourceManagerPrivate;

struct _PhotosSourceManager
{
  PhotosBaseManager parent_instance;
  PhotosSourceManagerPrivate *priv;
};

struct _PhotosSourceManagerClass
{
  PhotosBaseManagerClass parent_class;
};

GType                     photos_source_manager_get_type           (void) G_GNUC_CONST;

PhotosBaseManager        *photos_source_manager_new                (void);

G_END_DECLS

#endif /* PHOTOS_SOURCE_MANAGER_H */
