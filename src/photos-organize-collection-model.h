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

#ifndef PHOTOS_ORGANIZE_COLLECTION_MODEL_H
#define PHOTOS_ORGANIZE_COLLECTION_MODEL_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define PHOTOS_TYPE_ORGANIZE_COLLECTION_MODEL (photos_organize_collection_model_get_type ())
G_DECLARE_FINAL_TYPE (PhotosOrganizeCollectionModel,
                      photos_organize_collection_model,
                      PHOTOS,
                      ORGANIZE_COLLECTION_MODEL,
                      GtkListStore);

#define PHOTOS_COLLECTION_PLACEHOLDER_ID "collection-placeholder"

typedef enum
{
  PHOTOS_ORGANIZE_MODEL_ID,
  PHOTOS_ORGANIZE_MODEL_NAME,
  PHOTOS_ORGANIZE_MODEL_STATE
} PhotosOrganizeModelColumns;

GtkListStore     *photos_organize_collection_model_new                    (void);

GtkTreePath      *photos_organize_collection_model_add_placeholder        (PhotosOrganizeCollectionModel *self);

GtkTreePath      *photos_organize_collection_model_get_placeholder        (PhotosOrganizeCollectionModel *self,
                                                                           gboolean                       forget);

void              photos_organize_collection_model_refresh_collection_state (PhotosOrganizeCollectionModel *self);

void              photos_organize_collection_model_remove_placeholder     (PhotosOrganizeCollectionModel *self);

G_END_DECLS

#endif /* PHOTOS_ORGANIZE_COLLECTION_MODEL_H */
