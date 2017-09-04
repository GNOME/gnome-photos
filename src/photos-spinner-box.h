/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2012 – 2017 Red Hat, Inc.
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

/* Based on code from:
 *   + Documents
 */

#ifndef PHOTOS_SPINNER_BOX_H
#define PHOTOS_SPINNER_BOX_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define PHOTOS_TYPE_SPINNER_BOX (photos_spinner_box_get_type ())
G_DECLARE_FINAL_TYPE (PhotosSpinnerBox, photos_spinner_box, PHOTOS, SPINNER_BOX, GtkRevealer);

GtkWidget             *photos_spinner_box_new                    (void);

void                   photos_spinner_box_start                  (PhotosSpinnerBox *self);

void                   photos_spinner_box_stop                   (PhotosSpinnerBox *self);

G_END_DECLS

#endif /* PHOTOS_SPINNER_BOX_H */
