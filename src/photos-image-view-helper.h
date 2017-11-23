/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2015 – 2017 Red Hat, Inc.
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

#ifndef PHOTOS_IMAGE_VIEW_HELPER_H
#define PHOTOS_IMAGE_VIEW_HELPER_H

#include <glib-object.h>

G_BEGIN_DECLS

#define PHOTOS_TYPE_IMAGE_VIEW_HELPER (photos_image_view_helper_get_type ())
G_DECLARE_FINAL_TYPE (PhotosImageViewHelper, photos_image_view_helper, PHOTOS, IMAGE_VIEW_HELPER, GObject);

PhotosImageViewHelper    *photos_image_view_helper_new         (void);

gdouble                   photos_image_view_helper_get_zoom    (PhotosImageViewHelper *self);

void                      photos_image_view_helper_set_zoom    (PhotosImageViewHelper *self, gdouble zoom);

G_END_DECLS

#endif /* PHOTOS_IMAGE_VIEW_HELPER_H */
