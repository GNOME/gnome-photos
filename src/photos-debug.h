/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2015 Pranav Kant
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

#ifndef PHOTOS_DEBUG_H
#define PHOTOS_DEBUG_H

#include <glib.h>

G_BEGIN_DECLS

typedef enum
{
  PHOTOS_DEBUG_APPLICATION = 1 << 0,
  PHOTOS_DEBUG_DLNA       = 1 << 1,
  PHOTOS_DEBUG_GEGL       = 1 << 2,
  PHOTOS_DEBUG_MEMORY     = 1 << 3,
  PHOTOS_DEBUG_NETWORK    = 1 << 4,
  PHOTOS_DEBUG_THUMBNAILER = 1 << 5,
  PHOTOS_DEBUG_TRACKER    = 1 << 6
} PhotosDebugFlags;

void        photos_debug_init          (void);

void        photos_debug               (guint flags, const char *fmt, ...) G_GNUC_PRINTF (2, 3);

G_END_DECLS

#endif /* PHOTOS_DEBUG_H */
