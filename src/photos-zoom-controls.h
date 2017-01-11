/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2017 Red Hat, Inc.
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

#ifndef PHOTOS_ZOOM_CONTROLS_H
#define PHOTOS_ZOOM_CONTROLS_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define PHOTOS_TYPE_ZOOM_CONTROLS (photos_zoom_controls_get_type ())
G_DECLARE_FINAL_TYPE (PhotosZoomControls, photos_zoom_controls, PHOTOS, ZOOM_CONTROLS, GtkBin);

GtkWidget       *photos_zoom_controls_new                (void);

gboolean         photos_zoom_controls_get_hover          (PhotosZoomControls *self);

G_END_DECLS

#endif /* PHOTOS_ZOOM_CONTROLS_H */
