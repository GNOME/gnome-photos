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

#ifndef PHOTOS_VIEW_MODEL_H
#define PHOTOS_VIEW_MODEL_H

#include <gtk/gtk.h>

#include "photos-base-item.h"
#include "photos-item-manager.h"

G_BEGIN_DECLS

#define PHOTOS_TYPE_VIEW_MODEL (photos_view_model_get_type ())

#define PHOTOS_VIEW_MODEL(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
   PHOTOS_TYPE_VIEW_MODEL, PhotosViewModel))

#define PHOTOS_IS_VIEW_MODEL(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
   PHOTOS_TYPE_VIEW_MODEL))

typedef enum
{
  PHOTOS_VIEW_MODEL_URN,
  PHOTOS_VIEW_MODEL_URI,
  PHOTOS_VIEW_MODEL_NAME,
  PHOTOS_VIEW_MODEL_AUTHOR,
  PHOTOS_VIEW_MODEL_ICON,
  PHOTOS_VIEW_MODEL_MTIME,
  PHOTOS_VIEW_MODEL_SELECTED,
  PHOTOS_VIEW_MODEL_PULSE /* unused */
} PhotosViewModelColumns;

typedef struct _PhotosViewModel      PhotosViewModel;
typedef struct _PhotosViewModelClass PhotosViewModelClass;

GType             photos_view_model_get_type               (void) G_GNUC_CONST;

GtkListStore     *photos_view_model_new                    (PhotosWindowMode mode);

void              photos_view_model_item_added             (PhotosViewModel *self, PhotosBaseItem *item);

void              photos_view_model_item_removed           (PhotosViewModel *self, PhotosBaseItem *item);

G_END_DECLS

#endif /* PHOTOS_VIEW_MODEL_H */
