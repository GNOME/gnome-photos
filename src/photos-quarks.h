/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2013 – 2017 Red Hat, Inc.
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

#ifndef PHOTOS_QUARKS_H
#define PHOTOS_QUARKS_H

#include <glib.h>

G_BEGIN_DECLS

#define PHOTOS_FLASH_OFF (photos_quarks_flash_off_quark ())
#define PHOTOS_FLASH_ON (photos_quarks_flash_on_quark ())
#define PHOTOS_ORIENTATION_BOTTOM (photos_quarks_orientation_bottom_quark ())
#define PHOTOS_ORIENTATION_BOTTOM_MIRROR (photos_quarks_orientation_bottom_mirror_quark ())
#define PHOTOS_ORIENTATION_LEFT (photos_quarks_orientation_left_quark ())
#define PHOTOS_ORIENTATION_LEFT_MIRROR (photos_quarks_orientation_left_mirror_quark ())
#define PHOTOS_ORIENTATION_RIGHT (photos_quarks_orientation_right_quark ())
#define PHOTOS_ORIENTATION_RIGHT_MIRROR (photos_quarks_orientation_right_mirror_quark ())
#define PHOTOS_ORIENTATION_TOP (photos_quarks_orientation_top_quark ())
#define PHOTOS_ORIENTATION_TOP_MIRROR (photos_quarks_orientation_top_mirror_quark ())

GQuark           photos_quarks_flash_off_quark             (void);

GQuark           photos_quarks_flash_on_quark              (void);

GQuark           photos_quarks_orientation_bottom_quark    (void);

GQuark           photos_quarks_orientation_bottom_mirror_quark (void);

GQuark           photos_quarks_orientation_left_quark      (void);

GQuark           photos_quarks_orientation_left_mirror_quark (void);

GQuark           photos_quarks_orientation_right_quark     (void);

GQuark           photos_quarks_orientation_right_mirror_quark (void);

GQuark           photos_quarks_orientation_top_quark       (void);

GQuark           photos_quarks_orientation_top_mirror_quark (void);

G_END_DECLS

#endif /* PHOTOS_QUARKS_H */
