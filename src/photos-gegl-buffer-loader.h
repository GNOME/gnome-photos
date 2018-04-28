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

#ifndef PHOTOS_GEGL_BUFFER_LOADER_H
#define PHOTOS_GEGL_BUFFER_LOADER_H

#include <gegl.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define PHOTOS_TYPE_GEGL_BUFFER_LOADER (photos_gegl_buffer_loader_get_type ())
G_DECLARE_FINAL_TYPE (PhotosGeglBufferLoader,
                      photos_gegl_buffer_loader,
                      PHOTOS,
                      GEGL_BUFFER_LOADER,
                      GObject);

GeglBuffer              *photos_gegl_buffer_loader_get_buffer                 (PhotosGeglBufferLoader *self);

GFile                   *photos_gegl_buffer_loader_get_file                   (PhotosGeglBufferLoader *self);

gint                     photos_gegl_buffer_loader_get_height                 (PhotosGeglBufferLoader *self);

gboolean                 photos_gegl_buffer_loader_get_keep_aspect_ratio      (PhotosGeglBufferLoader *self);

gint                     photos_gegl_buffer_loader_get_width                  (PhotosGeglBufferLoader *self);

gboolean                 photos_gegl_buffer_loader_close                      (PhotosGeglBufferLoader *self,
                                                                               GError **error);

gboolean                 photos_gegl_buffer_loader_write_bytes                (PhotosGeglBufferLoader *self,
                                                                               GBytes *bytes,
                                                                               GCancellable *cancellable,
                                                                               GError **error);

G_END_DECLS

#endif /* PHOTOS_GEGL_BUFFER_LOADER_H */
