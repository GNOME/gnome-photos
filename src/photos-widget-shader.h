/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2015 – 2017 Red Hat, Inc.
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

#ifndef PHOTOS_WIDGET_SHADER_H
#define PHOTOS_WIDGET_SHADER_H

#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define PHOTOS_TYPE_WIDGET_SHADER (photos_widget_shader_get_type ())
G_DECLARE_FINAL_TYPE (PhotosWidgetShader, photos_widget_shader, PHOTOS, WIDGET_SHADER, GInitiallyUnowned);

PhotosWidgetShader    *photos_widget_shader_new                    (GtkWidget *widget);

void                   photos_widget_shader_set_active             (PhotosWidgetShader *self, gboolean active);

G_END_DECLS

#endif /* PHOTOS_WIDGET_SHADER_H */
