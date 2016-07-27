/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2016 Red Hat, Inc.
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

#ifndef PHOTOS_SHARE_POINT_ONLINE_H
#define PHOTOS_SHARE_POINT_ONLINE_H

#include "photos-share-point.h"
#include "photos-source.h"

G_BEGIN_DECLS

#define PHOTOS_TYPE_SHARE_POINT_ONLINE (photos_share_point_online_get_type ())
G_DECLARE_DERIVABLE_TYPE (PhotosSharePointOnline,
                          photos_share_point_online,
                          PHOTOS, SHARE_POINT_ONLINE,
                          PhotosSharePoint)

typedef struct _PhotosSharePointOnlinePrivate PhotosSharePointOnlinePrivate;

struct _PhotosSharePointOnlineClass
{
  PhotosSharePointClass parent_class;
};

PhotosSource           *photos_share_point_online_get_source             (PhotosSharePointOnline *self);

G_END_DECLS

#endif /* PHOTOS_SHARE_POINT_ONLINE_H */
