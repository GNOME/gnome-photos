/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2015 – 2017 Red Hat, Inc.
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

#ifndef PHOTOS_TOOL_FILTER_BUTTON_H
#define PHOTOS_TOOL_FILTER_BUTTON_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define PHOTOS_TYPE_TOOL_FILTER_BUTTON (photos_tool_filter_button_get_type ())

#define PHOTOS_TOOL_FILTER_BUTTON(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
   PHOTOS_TYPE_TOOL_FILTER_BUTTON, PhotosToolFilterButton))

#define PHOTOS_IS_TOOL_FILTER_BUTTON(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
   PHOTOS_TYPE_TOOL_FILTER_BUTTON))

typedef struct _PhotosToolFilterButton      PhotosToolFilterButton;
typedef struct _PhotosToolFilterButtonClass PhotosToolFilterButtonClass;

GType                  photos_tool_filter_button_get_type               (void) G_GNUC_CONST;

GtkWidget             *photos_tool_filter_button_new                    (GtkWidget *group_member,
                                                                         const gchar *label);

GtkWidget             *photos_tool_filter_button_get_group              (PhotosToolFilterButton *self);

void                   photos_tool_filter_button_set_active             (PhotosToolFilterButton *self,
                                                                         gboolean is_active);

void                   photos_tool_filter_button_set_image              (PhotosToolFilterButton *self,
                                                                         GtkWidget *image);

G_END_DECLS

#endif /* PHOTOS_TOOL_FILTER_BUTTON_H */
