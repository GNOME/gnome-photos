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

#ifndef PHOTOS_EMPTY_RESULTS_BOX_H
#define PHOTOS_EMPTY_RESULTS_BOX_H

#include <gtk/gtk.h>

#include "photos-item-manager.h"

G_BEGIN_DECLS

#define PHOTOS_TYPE_EMPTY_RESULTS_BOX (photos_empty_results_box_get_type ())
G_DECLARE_FINAL_TYPE (PhotosEmptyResultsBox, photos_empty_results_box, PHOTOS, EMPTY_RESULTS_BOX, GtkGrid);

GtkWidget          *photos_empty_results_box_new                (PhotosWindowMode mode);

G_END_DECLS

#endif /* PHOTOS_EMPTY_RESULTS_BOX_H */
