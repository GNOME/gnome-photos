/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2018 Red Hat, Inc.
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

#ifndef PHOTOS_REMOVABLE_DEVICES_BUTTON_H
#define PHOTOS_REMOVABLE_DEVICES_BUTTON_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define PHOTOS_TYPE_REMOVABLE_DEVICES_BUTTON (photos_removable_devices_button_get_type ())
G_DECLARE_FINAL_TYPE (PhotosRemovableDevicesButton,
                      photos_removable_devices_button,
                      PHOTOS,
                      REMOVABLE_DEVICES_BUTTON,
                      GtkBin);

GtkWidget            *photos_removable_devices_button_new               (void);

G_END_DECLS

#endif /* PHOTOS_REMOVABLE_DEVICES_BUTTON_H */
