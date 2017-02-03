/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2015 – 2017 Red Hat, Inc.
 * Copyright © 2015 Umang Jain
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

#ifndef PHOTOS_EDIT_PALETTE_ROW_H
#define PHOTOS_EDIT_PALETTE_ROW_H

#include <gtk/gtk.h>

#include "photos-tool.h"

G_BEGIN_DECLS

#define PHOTOS_TYPE_EDIT_PALETTE_ROW (photos_edit_palette_row_get_type ())
G_DECLARE_FINAL_TYPE (PhotosEditPaletteRow, photos_edit_palette_row, PHOTOS, EDIT_PALETTE_ROW, GtkListBoxRow);

GtkWidget             *photos_edit_palette_row_new                    (PhotosTool *tool, GtkSizeGroup *size_group);

PhotosTool            *photos_edit_palette_row_get_tool               (PhotosEditPaletteRow *self);

void                   photos_edit_palette_row_hide_details           (PhotosEditPaletteRow *self);

void                   photos_edit_palette_row_show_details           (PhotosEditPaletteRow *self);

void                   photos_edit_palette_row_show                   (PhotosEditPaletteRow *self);

G_END_DECLS

#endif /* PHOTOS_EDIT_PALETTE_ROW_H */
