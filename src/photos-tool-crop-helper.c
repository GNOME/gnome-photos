/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2018 Red Hat, Inc.
 * Copyright © 2017 Umang Jain
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

#include "photos-tool-crop-helper.h"
#include "photos-utils.h"


struct _PhotosToolCropHelper
{
  GObject parent_instance;
  gdouble height;
  gdouble width;
  gdouble x;
  gdouble y;
};

enum
{
  PROP_0,
  PROP_HEIGHT,
  PROP_WIDTH,
  PROP_X,
  PROP_Y
};


G_DEFINE_TYPE (PhotosToolCropHelper, photos_tool_crop_helper, G_TYPE_OBJECT);


static void
photos_tool_crop_helper_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
  PhotosToolCropHelper *self = PHOTOS_TOOL_CROP_HELPER (object);

  switch (prop_id)
    {
    case PROP_HEIGHT:
      g_value_set_double (value, self->height);
      break;

    case PROP_WIDTH:
      g_value_set_double (value, self->width);
      break;

    case PROP_X:
      g_value_set_double (value, self->x);
      break;

    case PROP_Y:
      g_value_set_double (value, self->y);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
photos_tool_crop_helper_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
  PhotosToolCropHelper *self = PHOTOS_TOOL_CROP_HELPER (object);

  switch (prop_id)
    {
    case PROP_HEIGHT:
      {
        gdouble height;

        height = g_value_get_double (value);
        photos_tool_crop_helper_set_height (self, height);
        break;
      }

    case PROP_WIDTH:
      {
        gdouble width;

        width = g_value_get_double (value);
        photos_tool_crop_helper_set_width (self, width);
        break;
      }

    case PROP_X:
      {
        gdouble x;

        x = g_value_get_double (value);
        photos_tool_crop_helper_set_x (self, x);
        break;
      }

    case PROP_Y:
      {
        gdouble y;

        y = g_value_get_double (value);
        photos_tool_crop_helper_set_y (self, y);
        break;
      }

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
photos_tool_crop_helper_init (PhotosToolCropHelper *self)
{
}


static void
photos_tool_crop_helper_class_init (PhotosToolCropHelperClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->get_property = photos_tool_crop_helper_get_property;
  object_class->set_property = photos_tool_crop_helper_set_property;

  g_object_class_install_property (object_class,
                                   PROP_HEIGHT,
                                   g_param_spec_double ("height",
                                                        "Height",
                                                        "Height of the crop rectangle",
                                                        0.0,
                                                        G_MAXDOUBLE,
                                                        0.0,
                                                        G_PARAM_EXPLICIT_NOTIFY | G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_WIDTH,
                                   g_param_spec_double ("width",
                                                        "Width",
                                                        "Width of the crop rectangle",
                                                        0.0,
                                                        G_MAXDOUBLE,
                                                        0.0,
                                                        G_PARAM_EXPLICIT_NOTIFY | G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_X,
                                   g_param_spec_double ("x",
                                                        "X",
                                                        "X co-ordinate of top-left corner of the crop rectangle",
                                                        0.0,
                                                        G_MAXDOUBLE,
                                                        0.0,
                                                        G_PARAM_EXPLICIT_NOTIFY | G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_Y,
                                   g_param_spec_double ("y",
                                                        "Y",
                                                        "Y co-ordinate of top-left corner of the crop rectangle",
                                                        0.0,
                                                        G_MAXDOUBLE,
                                                        0.0,
                                                        G_PARAM_EXPLICIT_NOTIFY | G_PARAM_READWRITE));

}


PhotosToolCropHelper *
photos_tool_crop_helper_new (void)
{
  return g_object_new (PHOTOS_TYPE_TOOL_CROP_HELPER, NULL);
}


gdouble
photos_tool_crop_helper_get_height (PhotosToolCropHelper *self)
{
  g_return_val_if_fail (PHOTOS_IS_TOOL_CROP_HELPER (self), 0.0);
  return self->height;
}


gdouble
photos_tool_crop_helper_get_width (PhotosToolCropHelper *self)
{
  g_return_val_if_fail (PHOTOS_IS_TOOL_CROP_HELPER (self), 0.0);
  return self->width;
}


gdouble
photos_tool_crop_helper_get_x (PhotosToolCropHelper *self)
{
  g_return_val_if_fail (PHOTOS_IS_TOOL_CROP_HELPER (self), 0.0);
  return self->x;
}


gdouble
photos_tool_crop_helper_get_y (PhotosToolCropHelper *self)
{
  g_return_val_if_fail (PHOTOS_IS_TOOL_CROP_HELPER (self), 0.0);
  return self->y;
}


void
photos_tool_crop_helper_set_height (PhotosToolCropHelper *self, gdouble height)
{
  g_return_if_fail (PHOTOS_IS_TOOL_CROP_HELPER (self));
  g_return_if_fail (height >= 0.0);

  if (photos_utils_equal_double (self->height, height))
    goto out;

  self->height = height;
  g_object_notify (G_OBJECT (self), "height");

 out:
  return;
}


void
photos_tool_crop_helper_set_width (PhotosToolCropHelper *self, gdouble width)
{
  g_return_if_fail (PHOTOS_IS_TOOL_CROP_HELPER (self));
  g_return_if_fail (width >= 0.0);

  if (photos_utils_equal_double (self->width, width))
    goto out;

  self->width = width;
  g_object_notify (G_OBJECT (self), "width");

 out:
  return;
}


void
photos_tool_crop_helper_set_x (PhotosToolCropHelper *self, gdouble x)
{
  g_return_if_fail (PHOTOS_IS_TOOL_CROP_HELPER (self));
  g_return_if_fail (x >= 0.0);

  if (photos_utils_equal_double (self->x, x))
    goto out;

  self->x = x;
  g_object_notify (G_OBJECT (self), "x");

 out:
  return;
}


void
photos_tool_crop_helper_set_y (PhotosToolCropHelper *self, gdouble y)
{
  g_return_if_fail (PHOTOS_IS_TOOL_CROP_HELPER (self));
  g_return_if_fail (y >= 0.0);

  if (photos_utils_equal_double (self->y, y))
    goto out;

  self->y = y;
  g_object_notify (G_OBJECT (self), "y");

 out:
  return;
}
