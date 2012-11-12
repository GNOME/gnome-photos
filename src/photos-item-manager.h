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

/* Based on code from:
 *   + Documents
 */

#ifndef PHOTOS_ITEM_MANAGER_H
#define PHOTOS_ITEM_MANAGER_H

#include <gtk/gtk.h>
#include <tracker-sparql.h>

#include "photos-base-item.h"
#include "photos-base-manager.h"

G_BEGIN_DECLS

#define PHOTOS_TYPE_ITEM_MANAGER (photos_item_manager_get_type ())

#define PHOTOS_ITEM_MANAGER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
   PHOTOS_TYPE_ITEM_MANAGER, PhotosItemManager))

#define PHOTOS_ITEM_MANAGER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
   PHOTOS_TYPE_ITEM_MANAGER, PhotosItemManagerClass))

#define PHOTOS_IS_ITEM_MANAGER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
   PHOTOS_TYPE_ITEM_MANAGER))

#define PHOTOS_IS_ITEM_MANAGER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
   PHOTOS_TYPE_ITEM_MANAGER))

#define PHOTOS_ITEM_MANAGER_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
   PHOTOS_TYPE_ITEM_MANAGER, PhotosItemManagerClass))

typedef struct _PhotosItemManager        PhotosItemManager;
typedef struct _PhotosItemManagerClass   PhotosItemManagerClass;
typedef struct _PhotosItemManagerPrivate PhotosItemManagerPrivate;

struct _PhotosItemManager
{
  PhotosBaseManager parent_instance;
  PhotosItemManagerPrivate *priv;
};

struct _PhotosItemManagerClass
{
  PhotosBaseManagerClass parent_class;
};

GType                     photos_item_manager_get_type           (void) G_GNUC_CONST;

PhotosBaseManager        *photos_item_manager_new                (void);

PhotosBaseItem           *photos_item_manager_add_item           (PhotosItemManager *self,
                                                                  TrackerSparqlCursor *cursor);

PhotosBaseItem           *photos_item_manager_create_item        (PhotosItemManager *self,
                                                                  TrackerSparqlCursor *cursor);

G_END_DECLS

#endif /* PHOTOS_ITEM_MANAGER_H */
