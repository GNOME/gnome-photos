/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2012 – 2017 Red Hat, Inc.
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

#ifndef PHOTOS_ORGANIZE_COLLECTION_VIEW_H
#define PHOTOS_ORGANIZE_COLLECTION_VIEW_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define PHOTOS_TYPE_ORGANIZE_COLLECTION_VIEW (photos_organize_collection_view_get_type ())
G_DECLARE_FINAL_TYPE (PhotosOrganizeCollectionView,
                      photos_organize_collection_view,
                      PHOTOS,
                      ORGANIZE_COLLECTION_VIEW,
                      GtkTreeView);

GtkWidget        *photos_organize_collection_view_new                    (void);

void              photos_organize_collection_view_add_collection         (PhotosOrganizeCollectionView *self);

void              photos_organize_collection_view_confirmed_choice       (PhotosOrganizeCollectionView *self);

G_END_DECLS

#endif /* PHOTOS_ORGANIZE_COLLECTION_VIEW_H */
