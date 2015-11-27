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

#ifndef PHOTOS_EDIT_PALETTE_ROW_H
#define PHOTOS_EDIT_PALETTE_ROW_H

#include <gtk/gtk.h>

#include "photos-tool.h"

G_BEGIN_DECLS

#define PHOTOS_TYPE_EDIT_PALETTE_ROW (photos_edit_palette_row_get_type ())

#define PHOTOS_EDIT_PALETTE_ROW(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
   PHOTOS_TYPE_EDIT_PALETTE_ROW, PhotosEditPaletteRow))

#define PHOTOS_IS_EDIT_PALETTE_ROW(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
   PHOTOS_TYPE_EDIT_PALETTE_ROW))

typedef struct _PhotosEditPaletteRow      PhotosEditPaletteRow;
typedef struct _PhotosEditPaletteRowClass PhotosEditPaletteRowClass;

GType                  photos_edit_palette_row_get_type               (void) G_GNUC_CONST;

GtkWidget             *photos_edit_palette_row_new                    (PhotosTool *tool, GtkSizeGroup *size_group);

PhotosTool            *photos_edit_palette_row_get_tool               (PhotosEditPaletteRow *self);

void                   photos_edit_palette_row_hide_details           (PhotosEditPaletteRow *self);

void                   photos_edit_palette_row_show_details           (PhotosEditPaletteRow *self);

G_END_DECLS

#endif /* PHOTOS_EDIT_PALETTE_ROW_H */
