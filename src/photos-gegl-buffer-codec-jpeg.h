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

/* Based on code from:
 *   + GdkPixbuf
 */

#ifndef PHOTOS_GEGL_BUFFER_CODEC_JPEG_H
#define PHOTOS_GEGL_BUFFER_CODEC_JPEG_H

#include "photos-gegl-buffer-codec.h"

G_BEGIN_DECLS

#define PHOTOS_TYPE_GEGL_BUFFER_CODEC_JPEG (photos_gegl_buffer_codec_jpeg_get_type ())
G_DECLARE_FINAL_TYPE (PhotosGeglBufferCodecJpeg,
                      photos_gegl_buffer_codec_jpeg,
                      PHOTOS,
                      GEGL_BUFFER_CODEC_JPEG,
                      PhotosGeglBufferCodec);

G_END_DECLS

#endif /* PHOTOS_GEGL_BUFFER_CODEC_JPEG_H */
