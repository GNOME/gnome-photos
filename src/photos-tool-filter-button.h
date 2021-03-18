/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2015 – 2021 Red Hat, Inc.
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

#ifndef PHOTOS_TOOL_FILTER_BUTTON_H
#define PHOTOS_TOOL_FILTER_BUTTON_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define PHOTOS_TYPE_TOOL_FILTER_BUTTON (photos_tool_filter_button_get_type ())
G_DECLARE_FINAL_TYPE (PhotosToolFilterButton, photos_tool_filter_button, PHOTOS, TOOL_FILTER_BUTTON, GtkBin);

GtkWidget             *photos_tool_filter_button_new                    (GtkWidget *group_member,
                                                                         const gchar *label);

GtkWidget             *photos_tool_filter_button_get_group              (PhotosToolFilterButton *self);

void                   photos_tool_filter_button_set_active             (PhotosToolFilterButton *self,
                                                                         gboolean is_active);

void                   photos_tool_filter_button_set_image              (PhotosToolFilterButton *self,
                                                                         GtkWidget *image);

G_END_DECLS

#endif /* PHOTOS_TOOL_FILTER_BUTTON_H */
