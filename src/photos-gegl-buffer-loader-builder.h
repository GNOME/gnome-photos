/*
 * Photos - access, organize and share your photos on GNOME
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

#ifndef PHOTOS_GEGL_BUFFER_LOADER_BUILDER_H
#define PHOTOS_GEGL_BUFFER_LOADER_BUILDER_H

#include <gio/gio.h>

#include "photos-gegl-buffer-loader.h"

G_BEGIN_DECLS

#define PHOTOS_TYPE_GEGL_BUFFER_LOADER_BUILDER (photos_gegl_buffer_loader_builder_get_type ())
G_DECLARE_FINAL_TYPE (PhotosGeglBufferLoaderBuilder,
                      photos_gegl_buffer_loader_builder,
                      PHOTOS,
                      GEGL_BUFFER_LOADER_BUILDER,
                      GObject);

PhotosGeglBufferLoaderBuilder *photos_gegl_buffer_loader_builder_new         (void);

PhotosGeglBufferLoaderBuilder *photos_gegl_buffer_loader_builder_set_file    (PhotosGeglBufferLoaderBuilder *self,
                                                                              GFile *file);

PhotosGeglBufferLoaderBuilder *photos_gegl_buffer_loader_builder_set_height  (PhotosGeglBufferLoaderBuilder *self,
                                                                              gint height);

PhotosGeglBufferLoaderBuilder *photos_gegl_buffer_loader_builder_set_keep_aspect_ratio (PhotosGeglBufferLoaderBuilder *self,
                                                                                        gboolean keep_aspect_ratio);

PhotosGeglBufferLoaderBuilder *photos_gegl_buffer_loader_builder_set_width   (PhotosGeglBufferLoaderBuilder *self,
                                                                              gint width);

PhotosGeglBufferLoader        *photos_gegl_buffer_loader_builder_to_loader   (PhotosGeglBufferLoaderBuilder *self);

G_END_DECLS

#endif /* PHOTOS_GEGL_BUFFER_LOADER_BUILDER_H */
