/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2015 – 2018 Red Hat, Inc.
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

#include "photos-image-view-helper.h"


struct _PhotosImageViewHelper
{
  GObject parent_instance;
  gdouble zoom;
};

enum
{
  PROP_0,
  PROP_ZOOM
};


G_DEFINE_TYPE (PhotosImageViewHelper, photos_image_view_helper, G_TYPE_OBJECT);


static void
photos_image_view_helper_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
  PhotosImageViewHelper *self = PHOTOS_IMAGE_VIEW_HELPER (object);

  switch (prop_id)
    {
    case PROP_ZOOM:
      g_value_set_double (value, self->zoom);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
photos_image_view_helper_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
  PhotosImageViewHelper *self = PHOTOS_IMAGE_VIEW_HELPER (object);

  switch (prop_id)
    {
    case PROP_ZOOM:
      {
        gdouble zoom;

        zoom = g_value_get_double (value);
        photos_image_view_helper_set_zoom (self, zoom);
        break;
      }

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
photos_image_view_helper_init (PhotosImageViewHelper *self)
{
}


static void
photos_image_view_helper_class_init (PhotosImageViewHelperClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->get_property = photos_image_view_helper_get_property;
  object_class->set_property = photos_image_view_helper_set_property;

  g_object_class_install_property (object_class,
                                   PROP_ZOOM,
                                   g_param_spec_double ("zoom",
                                                        "Zoom",
                                                        "Zoom factor",
                                                        G_MINDOUBLE,
                                                        G_MAXDOUBLE,
                                                        1.0,
                                                        G_PARAM_EXPLICIT_NOTIFY | G_PARAM_READWRITE));
}


PhotosImageViewHelper *
photos_image_view_helper_new (void)
{
  return g_object_new (PHOTOS_TYPE_IMAGE_VIEW_HELPER, NULL);
}


gdouble
photos_image_view_helper_get_zoom (PhotosImageViewHelper *self)
{
  g_return_val_if_fail (PHOTOS_IS_IMAGE_VIEW_HELPER (self), 0.0);
  return self->zoom;
}


void
photos_image_view_helper_set_zoom (PhotosImageViewHelper *self, gdouble zoom)
{
  g_return_if_fail (PHOTOS_IS_IMAGE_VIEW_HELPER (self));
  g_return_if_fail (zoom > 0.0);

  if (G_APPROX_VALUE (self->zoom, zoom, PHOTOS_EPSILON))
    return;

  self->zoom = zoom;
  g_object_notify (G_OBJECT (self), "zoom");
}
