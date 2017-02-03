/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2016 – 2017 Red Hat, Inc.
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

#ifndef PHOTOS_SHARE_POINT_MANAGER_H
#define PHOTOS_SHARE_POINT_MANAGER_H

#include "photos-base-item.h"
#include "photos-base-manager.h"

G_BEGIN_DECLS

#define PHOTOS_TYPE_SHARE_POINT_MANAGER (photos_share_point_manager_get_type ())

#define PHOTOS_SHARE_POINT_MANAGER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
   PHOTOS_TYPE_SHARE_POINT_MANAGER, PhotosSharePointManager))

#define PHOTOS_IS_SHARE_POINT_MANAGER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
   PHOTOS_TYPE_SHARE_POINT_MANAGER))

typedef struct _PhotosSharePointManager      PhotosSharePointManager;
typedef struct _PhotosSharePointManagerClass PhotosSharePointManagerClass;

GType                     photos_share_point_manager_get_type           (void) G_GNUC_CONST;

PhotosBaseManager        *photos_share_point_manager_dup_singleton      (void);

gboolean                  photos_share_point_manager_can_share          (PhotosSharePointManager *self,
                                                                         PhotosBaseItem *item);

GList                    *photos_share_point_manager_get_for_item       (PhotosSharePointManager *self,
                                                                         PhotosBaseItem *item);

G_END_DECLS

#endif /* PHOTOS_SHARE_POINT_MANAGER_H */
