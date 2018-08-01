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

enum
{
  PROP_0,
  PROP_FILE,
  PROP_HEIGHT,
  PROP_KEEP_ASPECT_RATIO,
  PROP_WIDTH
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
photos_gegl_buffer_loader_builder_set_property (GObject *object,
                                                guint prop_id,
                                                const GValue *value,
                                                GParamSpec *pspec)
{
  PhotosGeglBufferLoaderBuilder *self = PHOTOS_GEGL_BUFFER_LOADER_BUILDER (object);

  switch (prop_id)
    {
    case PROP_FILE:
      {
        GFile *file;

        file = G_FILE (g_value_get_object (value));
        photos_gegl_buffer_loader_builder_set_file (self, file);
        break;
      }

    case PROP_HEIGHT:
      {
        gint height;

        height = g_value_get_int (value);
        photos_gegl_buffer_loader_builder_set_height (self, height);
        break;
      }

    case PROP_KEEP_ASPECT_RATIO:
      {
        gboolean keep_aspect_ratio;

        keep_aspect_ratio = g_value_get_boolean (value);
        photos_gegl_buffer_loader_builder_set_keep_aspect_ratio (self, keep_aspect_ratio);
        break;
      }

    case PROP_WIDTH:
      {
        gint width;

        width = g_value_get_int (value);
        photos_gegl_buffer_loader_builder_set_width (self, width);
        break;
      }

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
photos_gegl_buffer_loader_builder_init (PhotosGeglBufferLoaderBuilder *self)
{
}


static void
photos_gegl_buffer_loader_builder_class_init (PhotosGeglBufferLoaderBuilderClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->dispose = photos_gegl_buffer_loader_builder_dispose;
  object_class->set_property = photos_gegl_buffer_loader_builder_set_property;

  g_object_class_install_property (object_class,
                                   PROP_FILE,
                                   g_param_spec_object ("file",
                                                        "File",
                                                        "The file from which to load a GeglBuffer",
                                                        G_TYPE_FILE,
                                                        G_PARAM_EXPLICIT_NOTIFY
                                                        | G_PARAM_STATIC_STRINGS
                                                        | G_PARAM_WRITABLE));

  g_object_class_install_property (object_class,
                                   PROP_HEIGHT,
                                   g_param_spec_int ("height",
                                                     "Height",
                                                     "The desired height of the GeglBuffer being loaded",
                                                     -1,
                                                     G_MAXINT,
                                                     -1,
                                                     G_PARAM_EXPLICIT_NOTIFY
                                                     | G_PARAM_STATIC_STRINGS
                                                     | G_PARAM_WRITABLE));

  g_object_class_install_property (object_class,
                                   PROP_KEEP_ASPECT_RATIO,
                                   g_param_spec_boolean ("keep-aspect-ratio",
                                                         "Keep aspect ratio",
                                                         "Whether to keep the aspect ratio of the GeglBuffer when "
                                                         "scaling, or not",
                                                         TRUE,
                                                         G_PARAM_EXPLICIT_NOTIFY
                                                         | G_PARAM_STATIC_STRINGS
                                                         | G_PARAM_WRITABLE));

  g_object_class_install_property (object_class,
                                   PROP_WIDTH,
                                   g_param_spec_int ("width",
                                                     "Width",
                                                     "The desired width of the GeglBuffer being loaded",
                                                     -1,
                                                     G_MAXINT,
                                                     -1,
                                                     G_PARAM_EXPLICIT_NOTIFY
                                                     | G_PARAM_STATIC_STRINGS
                                                     | G_PARAM_WRITABLE));
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

  if (g_set_object (&self->file, file))
    g_object_notify (G_OBJECT (self), "file");

  return self;
}


PhotosGeglBufferLoaderBuilder *
photos_gegl_buffer_loader_builder_set_height (PhotosGeglBufferLoaderBuilder *self, gint height)
{
  g_return_val_if_fail (PHOTOS_IS_GEGL_BUFFER_LOADER_BUILDER (self), NULL);
  g_return_val_if_fail (height >= -1, NULL);
  g_return_val_if_fail (height != 0, NULL);
  g_return_val_if_fail (!self->sealed, NULL);

  if (self->height != height)
    {
      self->height = height;
      g_object_notify (G_OBJECT (self), "height");
    }

  return self;
}


PhotosGeglBufferLoaderBuilder *
photos_gegl_buffer_loader_builder_set_keep_aspect_ratio (PhotosGeglBufferLoaderBuilder *self,
                                                         gboolean keep_aspect_ratio)
{
  g_return_val_if_fail (PHOTOS_IS_GEGL_BUFFER_LOADER_BUILDER (self), NULL);
  g_return_val_if_fail (!self->sealed, NULL);

  if (self->keep_aspect_ratio != keep_aspect_ratio)
    {
      self->keep_aspect_ratio = keep_aspect_ratio;
      g_object_notify (G_OBJECT (self), "keep-aspect-ratio");
    }

  return self;
}


PhotosGeglBufferLoaderBuilder *
photos_gegl_buffer_loader_builder_set_width (PhotosGeglBufferLoaderBuilder *self, gint width)
{
  g_return_val_if_fail (PHOTOS_IS_GEGL_BUFFER_LOADER_BUILDER (self), NULL);
  g_return_val_if_fail (width >= -1, NULL);
  g_return_val_if_fail (width != 0, NULL);
  g_return_val_if_fail (!self->sealed, NULL);

  if (self->width != width)
    {
      self->width = width;
      g_object_notify (G_OBJECT (self), "width");
    }

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
