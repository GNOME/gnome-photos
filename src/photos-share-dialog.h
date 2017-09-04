/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2016 Umang Jain
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

#ifndef PHOTOS_SHARE_DIALOG_H
#define PHOTOS_SHARE_DIALOG_H

#include <gtk/gtk.h>

#include "photos-base-item.h"
#include "photos-share-point.h"

G_BEGIN_DECLS

#define PHOTOS_TYPE_SHARE_DIALOG (photos_share_dialog_get_type ())
G_DECLARE_FINAL_TYPE (PhotosShareDialog, photos_share_dialog, PHOTOS, SHARE_DIALOG, GtkDialog);

GtkWidget          *photos_share_dialog_new                      (GtkWindow *parent, PhotosBaseItem *item);

PhotosSharePoint   *photos_share_dialog_get_selected_share_point (PhotosShareDialog *self);

G_END_DECLS

#endif /* PHOTOS_SHARE_DIALOG_H */
