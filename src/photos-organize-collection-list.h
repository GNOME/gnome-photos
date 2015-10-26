/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2015 Alessandro Bono
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

#ifndef PHOTOS_ORGANIZE_COLLECTION_LIST_H
#define PHOTOS_ORGANIZE_COLLECTION_LIST_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define PHOTOS_TYPE_ORGANIZE_COLLECTION_LIST (photos_organize_collection_list_get_type ())

#define PHOTOS_ORGANIZE_COLLECTION_LIST(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
   PHOTOS_TYPE_ORGANIZE_COLLECTION_LIST, PhotosOrganizeCollectionList))

#define PHOTOS_IS_ORGANIZE_COLLECTION_LIST(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
   PHOTOS_TYPE_ORGANIZE_COLLECTION_LIST))

typedef struct _PhotosOrganizeCollectionList      PhotosOrganizeCollectionList;
typedef struct _PhotosOrganizeCollectionListClass PhotosOrganizeCollectionListClass;

GType             photos_organize_collection_list_get_type             (void) G_GNUC_CONST;

GtkWidget        *photos_organize_collection_list_new                  (void);

gboolean          photos_organize_collection_list_is_empty             (PhotosOrganizeCollectionList *self);

gboolean          photos_organize_collection_list_is_valid_name        (PhotosOrganizeCollectionList *self,
                                                                        const gchar *name);

G_END_DECLS

#endif /* PHOTOS_ORGANIZE_COLLECTION_LIST_H */
