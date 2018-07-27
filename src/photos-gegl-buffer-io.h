/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 1999 The Free Software Foundation
 * Copyright © 2018 Red Hat, Inc.
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

/* Based on code from:
 *   + GdkPixbuf
 */

#ifndef PHOTOS_GEGL_BUFFER_IO_H
#define PHOTOS_GEGL_BUFFER_IO_H

#include <gegl.h>
#include <gio/gio.h>

G_BEGIN_DECLS

GeglBuffer    *photos_gegl_buffer_new_from_file                     (GFile *file,
                                                                     GCancellable *cancellable,
                                                                     GError **error);

void           photos_gegl_buffer_new_from_file_async               (GFile *file,
                                                                     gint priority,
                                                                     GCancellable *cancellable,
                                                                     GAsyncReadyCallback callback,
                                                                     gpointer user_data);

GeglBuffer    *photos_gegl_buffer_new_from_file_at_scale            (GFile *file,
                                                                     gint width,
                                                                     gint height,
                                                                     gboolean keep_aspect_ratio,
                                                                     GCancellable *cancellable,
                                                                     GError **error);

void           photos_gegl_buffer_new_from_file_at_scale_async      (GFile *file,
                                                                     gint width,
                                                                     gint height,
                                                                     gboolean keep_aspect_ratio,
                                                                     gint priority,
                                                                     GCancellable *cancellable,
                                                                     GAsyncReadyCallback callback,
                                                                     gpointer user_data);

GeglBuffer    *photos_gegl_buffer_new_from_file_finish              (GAsyncResult *res, GError **error);

GeglBuffer    *photos_gegl_buffer_new_from_stream                   (GInputStream *stream,
                                                                     GCancellable *cancellable,
                                                                     GError **error);

void           photos_gegl_buffer_new_from_stream_async             (GInputStream *stream,
                                                                     gint priority,
                                                                     GCancellable *cancellable,
                                                                     GAsyncReadyCallback callback,
                                                                     gpointer user_data);

GeglBuffer    *photos_gegl_buffer_new_from_stream_at_scale          (GInputStream *stream,
                                                                     gint width,
                                                                     gint height,
                                                                     gboolean keep_aspect_ratio,
                                                                     GCancellable *cancellable,
                                                                     GError **error);

void           photos_gegl_buffer_new_from_stream_at_scale_async    (GInputStream *stream,
                                                                     gint width,
                                                                     gint height,
                                                                     gboolean keep_aspect_ratio,
                                                                     gint priority,
                                                                     GCancellable *cancellable,
                                                                     GAsyncReadyCallback callback,
                                                                     gpointer user_data);

GeglBuffer    *photos_gegl_buffer_new_from_stream_finish            (GAsyncResult *res, GError **error);

G_END_DECLS

#endif /* PHOTOS_GEGL_BUFFER_IO_H */
