/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2012 Red Hat, Inc.
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

/* Based on code by the Documents team.
 */

#ifndef PHOTOS_UTILS_H
#define PHOTOS_UTILS_H

#include <gio/gio.h>
#include <glib.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

void             photos_utils_alpha_gtk_widget            (GtkWidget *widget);

gchar           *photos_utils_filename_strip_extension    (const gchar *filename_with_extension);

GList           *photos_utils_get_urns_from_paths         (GList *paths, GtkTreeModel *model);

void             photos_utils_queue_thumbnail_job_for_file_async (GFile *file,
                                                                  GAsyncReadyCallback callback,
                                                                  gpointer user_data);

gboolean         photos_utils_queue_thumbnail_job_for_file_finish (GAsyncResult *res);


G_END_DECLS

#endif /* PHOTOS_UTILS_H */
