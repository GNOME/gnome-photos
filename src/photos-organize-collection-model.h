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

#ifndef PHOTOS_ORGANIZE_COLLECTION_MODEL_H
#define PHOTOS_ORGANIZE_COLLECTION_MODEL_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define PHOTOS_TYPE_ORGANIZE_COLLECTION_MODEL (photos_organize_collection_model_get_type ())

#define PHOTOS_ORGANIZE_COLLECTION_MODEL(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
   PHOTOS_TYPE_ORGANIZE_COLLECTION_MODEL, PhotosOrganizeCollectionModel))

#define PHOTOS_ORGANIZE_COLLECTION_MODEL_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
   PHOTOS_TYPE_ORGANIZE_COLLECTION_MODEL, PhotosOrganizeCollectionModelClass))

#define PHOTOS_IS_ORGANIZE_COLLECTION_MODEL(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
   PHOTOS_TYPE_ORGANIZE_COLLECTION_MODEL))

#define PHOTOS_IS_ORGANIZE_COLLECTION_MODEL_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
   PHOTOS_TYPE_ORGANIZE_COLLECTION_MODEL))

#define PHOTOS_ORGANIZE_COLLECTION_MODEL_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
   PHOTOS_TYPE_ORGANIZE_COLLECTION_MODEL, PhotosOrganizeCollectionModelClass))

#define PHOTOS_COLLECTION_PLACEHOLDER_ID "collection-placeholder"

typedef enum
{
  PHOTOS_ORGANIZE_MODEL_ID,
  PHOTOS_ORGANIZE_MODEL_NAME,
  PHOTOS_ORGANIZE_MODEL_STATE
} PhotosOrganizeModelColumns;

typedef struct _PhotosOrganizeCollectionModel        PhotosOrganizeCollectionModel;
typedef struct _PhotosOrganizeCollectionModelClass   PhotosOrganizeCollectionModelClass;
typedef struct _PhotosOrganizeCollectionModelPrivate PhotosOrganizeCollectionModelPrivate;

struct _PhotosOrganizeCollectionModel
{
  GtkListStore parent_instance;
  PhotosOrganizeCollectionModelPrivate *priv;
};

struct _PhotosOrganizeCollectionModelClass
{
  GtkListStoreClass parent_class;
};

GType             photos_organize_collection_model_get_type               (void) G_GNUC_CONST;

GtkListStore     *photos_organize_collection_model_new                    (void);

GtkTreePath      *photos_organize_collection_model_add_placeholder        (PhotosOrganizeCollectionModel *self);

GtkTreePath      *photos_organize_collection_model_get_placeholder        (PhotosOrganizeCollectionModel *self,
                                                                           gboolean                       forget);

void              photos_organize_collection_model_refresh_collection_state (PhotosOrganizeCollectionModel *self);

void              photos_organize_collection_model_remove_placeholder     (PhotosOrganizeCollectionModel *self);

G_END_DECLS

#endif /* PHOTOS_ORGANIZE_COLLECTION_MODEL_H */
