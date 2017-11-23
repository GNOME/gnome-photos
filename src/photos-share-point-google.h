/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2016 Umang Jain
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

#ifndef PHOTOS_SHARE_POINT_GOOGLE_H
#define PHOTOS_SHARE_POINT_GOOGLE_H

#include "photos-share-point-online.h"

G_BEGIN_DECLS

#define PHOTOS_TYPE_SHARE_POINT_GOOGLE (photos_share_point_google_get_type ())
G_DECLARE_FINAL_TYPE (PhotosSharePointGoogle,
                      photos_share_point_google,
                      PHOTOS,
                      SHARE_POINT_GOOGLE,
                      PhotosSharePointOnline);

G_END_DECLS

#endif /* PHOTOS_SHARE_POINT_GOOGLE_H */
