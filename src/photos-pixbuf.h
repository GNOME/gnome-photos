/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2017 Red Hat, Inc.
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

#ifndef PHOTOS_PIXBUF_H
#define PHOTOS_PIXBUF_H

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gio/gio.h>

G_BEGIN_DECLS

void             photos_pixbuf_new_from_file_at_size_async     (const gchar *filename,
                                                                gint width,
                                                                gint height,
                                                                GCancellable *cancellable,
                                                                GAsyncReadyCallback callback,
                                                                gpointer user_data);

GdkPixbuf       *photos_pixbuf_new_from_file_at_size_finish    (GAsyncResult *res, GError **error);

G_END_DECLS

#endif /* PHOTOS_PIXBUF_H */
