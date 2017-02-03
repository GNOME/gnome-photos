/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2014 Pranav Kant
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

#ifndef PHOTOS_MEDIA_SERVER_ITEM_H
#define PHOTOS_MEDIA_SERVER_ITEM_H

#include "photos-base-item.h"

G_BEGIN_DECLS

#define PHOTOS_TYPE_MEDIA_SERVER_ITEM (photos_media_server_item_get_type ())
G_DECLARE_FINAL_TYPE (PhotosMediaServerItem, photos_media_server_item, PHOTOS, MEDIA_SERVER_ITEM, PhotosBaseItem);

G_END_DECLS

#endif /* PHOTOS_MEDIA_SERVER_ITEM_H */
