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

#ifndef PHOTOS_EXPORT_DIALOG_H
#define PHOTOS_EXPORT_DIALOG_H

#include <gtk/gtk.h>

#include "photos-base-item.h"

struct PhotosExportDialogData
{
  GFile *directory;
  GList *items;
  gdouble zoom;
};

G_BEGIN_DECLS

#define PHOTOS_TYPE_EXPORT_DIALOG (photos_export_dialog_get_type ())
G_DECLARE_FINAL_TYPE (PhotosExportDialog, photos_export_dialog, PHOTOS, EXPORT_DIALOG, GtkDialog);

GtkWidget                      *photos_export_dialog_new                (GtkWindow *parent, GList *items);

struct PhotosExportDialogData  *photos_export_dialog_get_export_data    (PhotosExportDialog *self);

void                            photos_export_dialog_free_export_data   (struct PhotosExportDialogData *export_data);

G_END_DECLS

#endif /* PHOTOS_EXPORT_DIALOG_H */

