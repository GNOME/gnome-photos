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

#ifndef PHOTOS_GEGL_BUFFER_CODEC_H
#define PHOTOS_GEGL_BUFFER_CODEC_H

#include <gegl.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define PHOTOS_GEGL_BUFFER_CODEC_EXTENSION_POINT_NAME "photos-gegl-buffer-codec"

#define PHOTOS_TYPE_GEGL_BUFFER_CODEC (photos_gegl_buffer_codec_get_type ())
G_DECLARE_DERIVABLE_TYPE (PhotosGeglBufferCodec, photos_gegl_buffer_codec, PHOTOS, GEGL_BUFFER_CODEC, GObject);

typedef struct _PhotosGeglBufferCodecPrivate PhotosGeglBufferCodecPrivate;

struct _PhotosGeglBufferCodecClass
{
  GObjectClass parent_class;

  GStrv mime_types;

  /* virtual methods */
  GeglBuffer  *(*get_buffer)                (PhotosGeglBufferCodec *self);
  gboolean     (*load_begin)                (PhotosGeglBufferCodec *self, GError **error);
  gboolean     (*load_increment)            (PhotosGeglBufferCodec *self,
                                             const guchar *buf,
                                             gsize count,
                                             GError **error);
  gboolean     (*load_stop)                 (PhotosGeglBufferCodec *self, GError **error);

  /* signals */
  void         (*size_prepared)             (PhotosGeglBufferCodec *self, gint width, gint height);
};

GeglBuffer         *photos_gegl_buffer_codec_get_buffer                (PhotosGeglBufferCodec *self);

gboolean            photos_gegl_buffer_codec_get_can_set_size          (PhotosGeglBufferCodec *self);

gdouble             photos_gegl_buffer_codec_get_height                (PhotosGeglBufferCodec *self);

gdouble             photos_gegl_buffer_codec_get_width                 (PhotosGeglBufferCodec *self);

gboolean            photos_gegl_buffer_codec_load_begin                (PhotosGeglBufferCodec *self,
                                                                        GError **error);

gboolean            photos_gegl_buffer_codec_load_increment            (PhotosGeglBufferCodec *self,
                                                                        const guchar *buf,
                                                                        gsize count,
                                                                        GError **error);

gboolean            photos_gegl_buffer_codec_load_stop                 (PhotosGeglBufferCodec *self,
                                                                        GError **error);

void                photos_gegl_buffer_codec_set_height                (PhotosGeglBufferCodec *self,
                                                                        gdouble height);

void                photos_gegl_buffer_codec_set_width                 (PhotosGeglBufferCodec *self,
                                                                        gdouble width);

G_END_DECLS

#endif /* PHOTOS_GEGL_BUFFER_CODEC_H */
