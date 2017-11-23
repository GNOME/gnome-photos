/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2016 – 2017 Red Hat, Inc.
 * Copyright © 2014 Saurav Agarwalla
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

#ifndef PHOTOS_GOOGLE_ITEM_H
#define PHOTOS_GOOGLE_ITEM_H

#include "photos-base-item.h"

G_BEGIN_DECLS

#define PHOTOS_TYPE_GOOGLE_ITEM (photos_google_item_get_type ())
G_DECLARE_FINAL_TYPE (PhotosGoogleItem, photos_google_item, PHOTOS, GOOGLE_ITEM, PhotosBaseItem);

G_END_DECLS

#endif /* PHOTOS_GOOGLE_ITEM_H */
