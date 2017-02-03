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

#ifndef PHOTOS_EXPORT_DIALOG_H
#define PHOTOS_EXPORT_DIALOG_H

#include <gtk/gtk.h>

#include "photos-base-item.h"

G_BEGIN_DECLS

#define PHOTOS_TYPE_EXPORT_DIALOG (photos_export_dialog_get_type ())
G_DECLARE_FINAL_TYPE (PhotosExportDialog, photos_export_dialog, PHOTOS, EXPORT_DIALOG, GtkDialog);

GtkWidget          *photos_export_dialog_new                (GtkWindow *parent, PhotosBaseItem *item);

const gchar        *photos_export_dialog_get_dir_name       (PhotosExportDialog *self);

gdouble             photos_export_dialog_get_zoom           (PhotosExportDialog *self);

G_END_DECLS

#endif /* PHOTOS_EXPORT_DIALOG_H */
