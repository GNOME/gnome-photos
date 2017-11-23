/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2012 – 2017 Red Hat, Inc.
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

#ifndef PHOTOS_MAIN_WINDOW_H
#define PHOTOS_MAIN_WINDOW_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define PHOTOS_TYPE_MAIN_WINDOW (photos_main_window_get_type ())
G_DECLARE_FINAL_TYPE (PhotosMainWindow, photos_main_window, PHOTOS, MAIN_WINDOW, GtkApplicationWindow);

GtkWidget             *photos_main_window_new                    (GtkApplication *application);

void                   photos_main_window_show_about             (PhotosMainWindow *self);

G_END_DECLS

#endif /* PHOTOS_MAIN_WINDOW_H */
