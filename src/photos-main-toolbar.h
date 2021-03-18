/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2012 – 2021 Red Hat, Inc.
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

#ifndef PHOTOS_MAIN_TOOLBAR_H
#define PHOTOS_MAIN_TOOLBAR_H

#include <gtk/gtk.h>

#include "photos-searchbar.h"

G_BEGIN_DECLS

#define PHOTOS_TYPE_MAIN_TOOLBAR (photos_main_toolbar_get_type ())
G_DECLARE_FINAL_TYPE (PhotosMainToolbar, photos_main_toolbar, PHOTOS, MAIN_TOOLBAR, GtkBox);

GtkWidget             *photos_main_toolbar_new                    (void);

PhotosSearchbar *      photos_main_toolbar_get_searchbar          (PhotosMainToolbar *self);

gboolean               photos_main_toolbar_handle_event           (PhotosMainToolbar *self, GdkEventKey *event);

gboolean               photos_main_toolbar_is_focus               (PhotosMainToolbar *self);

void                   photos_main_toolbar_reset_toolbar_mode     (PhotosMainToolbar *self);

void                   photos_main_toolbar_set_stack              (PhotosMainToolbar *self, GtkStack *stack);

G_END_DECLS

#endif /* PHOTOS_MAIN_TOOLBAR_H */
