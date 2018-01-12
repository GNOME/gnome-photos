/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2018 Red Hat, Inc.
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

#ifndef PHOTOS_REMOVABLE_DEVICE_WIDGET_H
#define PHOTOS_REMOVABLE_DEVICE_WIDGET_H

#include <gtk/gtk.h>

#include "photos-source.h"

G_BEGIN_DECLS

#define PHOTOS_TYPE_REMOVABLE_DEVICE_WIDGET (photos_removable_device_widget_get_type ())
G_DECLARE_FINAL_TYPE (PhotosRemovableDeviceWidget,
                      photos_removable_device_widget,
                      PHOTOS,
                      REMOVABLE_DEVICE_WIDGET,
                      GtkBin);

GtkWidget            *photos_removable_device_widget_new               (PhotosSource *source);

void                  photos_removable_device_widget_set_source        (PhotosRemovableDeviceWidget *self,
                                                                        PhotosSource *source);

G_END_DECLS

#endif /* PHOTOS_REMOVABLE_DEVICE_WIDGET_H */
