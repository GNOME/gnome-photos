/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2015 Red Hat, Inc.
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

#ifndef PHOTOS_WIDGET_SHADER_H
#define PHOTOS_WIDGET_SHADER_H

#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define PHOTOS_TYPE_WIDGET_SHADER (photos_widget_shader_get_type ())

#define PHOTOS_WIDGET_SHADER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
   PHOTOS_TYPE_WIDGET_SHADER, PhotosWidgetShader))

#define PHOTOS_IS_WIDGET_SHADER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
   PHOTOS_TYPE_WIDGET_SHADER))

typedef struct _PhotosWidgetShader      PhotosWidgetShader;
typedef struct _PhotosWidgetShaderClass PhotosWidgetShaderClass;

GType                  photos_widget_shader_get_type               (void) G_GNUC_CONST;

PhotosWidgetShader    *photos_widget_shader_new                    (GtkWidget *widget);

void                   photos_widget_shader_set_active             (PhotosWidgetShader *self, gboolean active);

G_END_DECLS

#endif /* PHOTOS_WIDGET_SHADER_H */
