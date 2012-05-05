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

#ifndef PHOTOS_LOCAL_ITEM_H
#define PHOTOS_LOCAL_ITEM_H

#include <tracker-sparql.h>

#include "photos-base-item.h"

G_BEGIN_DECLS

#define PHOTOS_TYPE_LOCAL_ITEM (photos_local_item_get_type ())
#define PHOTOS_LOCAL_ITEM(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
   PHOTOS_TYPE_LOCAL_ITEM, PhotosLocalItem))

#define PHOTOS_LOCAL_ITEM_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
   PHOTOS_TYPE_LOCAL_ITEM, PhotosLocalItemClass))

#define PHOTOS_IS_LOCAL_ITEM(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
   PHOTOS_TYPE_LOCAL_ITEM))

#define PHOTOS_IS_LOCAL_ITEM_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
   PHOTOS_TYPE_LOCAL_ITEM))

#define PHOTOS_LOCAL_ITEM_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
   PHOTOS_TYPE_LOCAL_ITEM, PhotosLocalItemClass))

typedef struct _PhotosLocalItem        PhotosLocalItem;
typedef struct _PhotosLocalItemClass   PhotosLocalItemClass;
typedef struct _PhotosLocalItemPrivate PhotosLocalItemPrivate;

struct _PhotosLocalItem
{
  PhotosBaseItem parent_instance;
  PhotosLocalItemPrivate *priv;
};

struct _PhotosLocalItemClass
{
  PhotosBaseItemClass parent_class;
};

GType               photos_local_item_get_type           (void) G_GNUC_CONST;

PhotosBaseItem     *photos_local_item_new                (TrackerSparqlCursor *cursor);

G_END_DECLS

#endif /* PHOTOS_LOCAL_ITEM_H */
