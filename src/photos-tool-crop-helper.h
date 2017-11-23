/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2018 Red Hat, Inc.
 * Copyright © 2017 Umang Jain
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

#ifndef PHOTOS_TOOL_CROP_HELPER_H
#define PHOTOS_TOOL_CROP_HELPER_H

#include <glib-object.h>

G_BEGIN_DECLS

#define PHOTOS_TYPE_TOOL_CROP_HELPER (photos_tool_crop_helper_get_type ())
G_DECLARE_FINAL_TYPE (PhotosToolCropHelper, photos_tool_crop_helper, PHOTOS, TOOL_CROP_HELPER, GObject);

PhotosToolCropHelper     *photos_tool_crop_helper_new                 (void);

gdouble                   photos_tool_crop_helper_get_height          (PhotosToolCropHelper *self);

gdouble                   photos_tool_crop_helper_get_width           (PhotosToolCropHelper *self);

gdouble                   photos_tool_crop_helper_get_x               (PhotosToolCropHelper *self);

gdouble                   photos_tool_crop_helper_get_y               (PhotosToolCropHelper *self);

void                      photos_tool_crop_helper_set_height          (PhotosToolCropHelper *self, gdouble height);

void                      photos_tool_crop_helper_set_width           (PhotosToolCropHelper *self, gdouble width);

void                      photos_tool_crop_helper_set_x               (PhotosToolCropHelper *self, gdouble x);

void                      photos_tool_crop_helper_set_y               (PhotosToolCropHelper *self, gdouble y);

G_END_DECLS

#endif /* PHOTOS_TOOL_CROP_HELPER_H */
