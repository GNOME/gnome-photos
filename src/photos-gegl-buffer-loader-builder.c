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


#include "config.h"

#include "photos-gegl-buffer-loader-builder.h"


struct _PhotosGeglBufferLoaderBuilder
{
  GObject parent_instance;
  GFile *file;
  gboolean keep_aspect_ratio;
  gboolean sealed;
  gint height;
  gint width;
};


G_DEFINE_TYPE (PhotosGeglBufferLoaderBuilder, photos_gegl_buffer_loader_builder, G_TYPE_OBJECT);


static void
photos_gegl_buffer_loader_builder_dispose (GObject *object)
{
  PhotosGeglBufferLoaderBuilder *self = PHOTOS_GEGL_BUFFER_LOADER_BUILDER (object);

  g_clear_object (&self->file);

  G_OBJECT_CLASS (photos_gegl_buffer_loader_builder_parent_class)->dispose (object);
}


static void
photos_gegl_buffer_loader_builder_init (PhotosGeglBufferLoaderBuilder *self)
{
  self->keep_aspect_ratio = TRUE;
  self->height = -1;
  self->width = -1;
}


static void
photos_gegl_buffer_loader_builder_class_init (PhotosGeglBufferLoaderBuilderClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->dispose = photos_gegl_buffer_loader_builder_dispose;
}


PhotosGeglBufferLoaderBuilder *
photos_gegl_buffer_loader_builder_new (void)
{
  return g_object_new (PHOTOS_TYPE_GEGL_BUFFER_LOADER_BUILDER, NULL);
}


PhotosGeglBufferLoaderBuilder *
photos_gegl_buffer_loader_builder_set_file (PhotosGeglBufferLoaderBuilder *self, GFile *file)
{
  g_return_val_if_fail (PHOTOS_IS_GEGL_BUFFER_LOADER_BUILDER (self), NULL);
  g_return_val_if_fail (!self->sealed, NULL);

  g_return_val_if_fail (G_IS_FILE (file), NULL);

  g_set_object (&self->file, file);
  return self;
}


PhotosGeglBufferLoaderBuilder *
photos_gegl_buffer_loader_builder_set_height (PhotosGeglBufferLoaderBuilder *self, gint height)
{
  g_return_val_if_fail (PHOTOS_IS_GEGL_BUFFER_LOADER_BUILDER (self), NULL);
  g_return_val_if_fail (height >= -1, NULL);
  g_return_val_if_fail (height != 0, NULL);
  g_return_val_if_fail (!self->sealed, NULL);

  self->height = height;
  return self;
}


PhotosGeglBufferLoaderBuilder *
photos_gegl_buffer_loader_builder_set_keep_aspect_ratio (PhotosGeglBufferLoaderBuilder *self,
                                                         gboolean keep_aspect_ratio)
{
  g_return_val_if_fail (PHOTOS_IS_GEGL_BUFFER_LOADER_BUILDER (self), NULL);
  g_return_val_if_fail (!self->sealed, NULL);

  self->keep_aspect_ratio = keep_aspect_ratio;
  return self;
}


PhotosGeglBufferLoaderBuilder *
photos_gegl_buffer_loader_builder_set_width (PhotosGeglBufferLoaderBuilder *self, gint width)
{
  g_return_val_if_fail (PHOTOS_IS_GEGL_BUFFER_LOADER_BUILDER (self), NULL);
  g_return_val_if_fail (width >= -1, NULL);
  g_return_val_if_fail (width != 0, NULL);
  g_return_val_if_fail (!self->sealed, NULL);

  self->width = width;
  return self;
}


PhotosGeglBufferLoader *
photos_gegl_buffer_loader_builder_to_loader (PhotosGeglBufferLoaderBuilder *self)
{
  PhotosGeglBufferLoader *loader;

  g_return_val_if_fail (PHOTOS_IS_GEGL_BUFFER_LOADER_BUILDER (self), NULL);
  g_return_val_if_fail (!self->sealed, NULL);

  self->sealed = TRUE;
  loader = g_object_new (PHOTOS_TYPE_GEGL_BUFFER_LOADER,
                         "file", self->file,
                         "height", self->height,
                         "keep-aspect-ratio", self->keep_aspect_ratio,
                         "width", self->width,
                         NULL);

  g_return_val_if_fail (self->sealed, NULL);
  return loader;
}
