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

#ifndef PHOTOS_ITEM_MODEL_H
#define PHOTOS_ITEM_MODEL_H

#include <gtk/gtk.h>

#include "photos-base-item.h"

G_BEGIN_DECLS

#define PHOTOS_TYPE_ITEM_MODEL (photos_view_embed_get_type ())

#define PHOTOS_ITEM_MODEL(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
   PHOTOS_TYPE_ITEM_MODEL, PhotosItemModel))

#define PHOTOS_ITEM_MODEL_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
   PHOTOS_TYPE_ITEM_MODEL, PhotosItemModelClass))

#define PHOTOS_IS_ITEM_MODEL(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
   PHOTOS_TYPE_ITEM_MODEL))

#define PHOTOS_IS_ITEM_MODEL_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
   PHOTOS_TYPE_ITEM_MODEL))

#define PHOTOS_ITEM_MODEL_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
   PHOTOS_TYPE_ITEM_MODEL, PhotosItemModelClass))

typedef enum
{
  PHOTOS_ITEM_MODEL_URN,
  PHOTOS_ITEM_MODEL_URI,
  PHOTOS_ITEM_MODEL_NAME,
  PHOTOS_ITEM_MODEL_AUTHOR,
  PHOTOS_ITEM_MODEL_ICON,
  PHOTOS_ITEM_MODEL_MTIME,
  PHOTOS_ITEM_MODEL_SELECTED
} PhotosItemModelColumns;

typedef struct _PhotosItemModel        PhotosItemModel;
typedef struct _PhotosItemModelClass   PhotosItemModelClass;
typedef struct _PhotosItemModelPrivate PhotosItemModelPrivate;

struct _PhotosItemModel
{
  GtkListStore parent_instance;
  PhotosItemModelPrivate *priv;
};

struct _PhotosItemModelClass
{
  GtkListStoreClass parent_class;
};

GType             photos_item_model_get_type               (void) G_GNUC_CONST;

GtkListStore     *photos_item_model_new                    (void);

void              photos_item_model_item_added             (PhotosItemModel *self, PhotosBaseItem *item);

void              photos_item_model_item_removed           (PhotosItemModel *self, PhotosBaseItem *item);

G_END_DECLS

#endif /* PHOTOS_ITEM_MODEL_H */
