/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2016 – 2017 Red Hat, Inc.
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

#ifndef PHOTOS_SHARE_POINT_EMAIL_H
#define PHOTOS_SHARE_POINT_EMAIL_H

#include "photos-share-point.h"

G_BEGIN_DECLS

#define PHOTOS_TYPE_SHARE_POINT_EMAIL (photos_share_point_email_get_type ())
G_DECLARE_FINAL_TYPE (PhotosSharePointEmail, photos_share_point_email, PHOTOS, SHARE_POINT_EMAIL, PhotosSharePoint);

G_END_DECLS

#endif /* PHOTOS_SHARE_POINT_EMAIL_H */
