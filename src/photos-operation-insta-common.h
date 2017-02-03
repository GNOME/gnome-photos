/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2015 – 2017 Red Hat, Inc.
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

#ifndef PHOTOS_OPERATION_INSTA_COMMON_H
#define PHOTOS_OPERATION_INSTA_COMMON_H

G_BEGIN_DECLS

typedef enum
{
  PHOTOS_OPERATION_INSTA_PRESET_NONE,
  PHOTOS_OPERATION_INSTA_PRESET_1947,
  PHOTOS_OPERATION_INSTA_PRESET_CALISTOGA,
  PHOTOS_OPERATION_INSTA_PRESET_CAAP,
  PHOTOS_OPERATION_INSTA_PRESET_MOGADISHU,
  PHOTOS_OPERATION_INSTA_PRESET_HOMETOWN,

  PHOTOS_OPERATION_INSTA_PRESET_1977 = PHOTOS_OPERATION_INSTA_PRESET_1947,
  PHOTOS_OPERATION_INSTA_PRESET_BRANNAN = PHOTOS_OPERATION_INSTA_PRESET_CALISTOGA,
  PHOTOS_OPERATION_INSTA_PRESET_HEFE = PHOTOS_OPERATION_INSTA_PRESET_CAAP,
  PHOTOS_OPERATION_INSTA_PRESET_GOTHAM = PHOTOS_OPERATION_INSTA_PRESET_MOGADISHU,
  PHOTOS_OPERATION_INSTA_PRESET_NASHVILLE = PHOTOS_OPERATION_INSTA_PRESET_HOMETOWN,
} PhotosOperationInstaPreset;

G_END_DECLS

#endif /* PHOTOS_OPERATION_INSTA_COMMON_H */
