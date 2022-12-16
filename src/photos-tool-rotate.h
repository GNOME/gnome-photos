/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright (c) 2022 Cedric Bellegarde <cedric.bellegarde@adishatz.org>
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

#ifndef PHOTOS_TOOL_ROTATE_H
#define PHOTOS_TOOL_ROTATE_H

#include <glib-object.h>

G_BEGIN_DECLS

#define PHOTOS_TYPE_TOOL_ROTATE (photos_tool_rotate_get_type ())
G_DECLARE_FINAL_TYPE (PhotosToolRotate, photos_tool_rotate, PHOTOS, TOOL_ROTATE, PhotosTool);

G_END_DECLS

#endif /* PHOTOS_TOOL_ROTATE_H */

