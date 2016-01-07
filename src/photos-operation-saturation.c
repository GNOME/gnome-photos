/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2016 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */


#include "config.h"

#include <babl/babl.h>
#include <gegl.h>
#include <gegl-plugin.h>

#include "photos-operation-saturation.h"


struct _PhotosOperationSaturation
{
  GeglOperationPointFilter parent_instance;
  gfloat scale;
};

struct _PhotosOperationSaturationClass
{
  GeglOperationPointFilterClass parent_class;
};

enum
{
  PROP_0,
  PROP_SCALE
};


G_DEFINE_TYPE (PhotosOperationSaturation, photos_operation_saturation, GEGL_TYPE_OPERATION_POINT_FILTER);


static gboolean
photos_operation_saturation_process (GeglOperation *operation,
                                     void *in_buf,
                                     void *out_buf,
                                     glong n_pixels,
                                     const GeglRectangle *roi,
                                     gint level)
{
  PhotosOperationSaturation *self = PHOTOS_OPERATION_SATURATION (operation);
  gfloat *in = in_buf;
  gfloat *out = out_buf;
  glong i;

  for (i = 0; i < n_pixels; i++)
    {
      out[0] = in[0];
      out[1] = in[1] * self->scale;
      out[2] = in[2];

      in += 3;
      out += 3;
    }

  return TRUE;
}


static gboolean
photos_operation_saturation_process_alpha (GeglOperation *operation,
                                           void *in_buf,
                                           void *out_buf,
                                           glong n_pixels,
                                           const GeglRectangle *roi,
                                           gint level)
{
  PhotosOperationSaturation *self = PHOTOS_OPERATION_SATURATION (operation);
  gfloat *in = in_buf;
  gfloat *out = out_buf;
  glong i;

  for (i = 0; i < n_pixels; i++)
    {
      out[0] = in[0];
      out[1] = in[1] * self->scale;
      out[2] = in[2];
      out[3] = in[3];

      in += 4;
      out += 4;
    }

  return TRUE;
}


static void
photos_operation_saturation_prepare (GeglOperation *operation)
{
  GeglOperationPointFilterClass *point_filter_class = GEGL_OPERATION_POINT_FILTER_GET_CLASS (operation);
  const Babl *format;
  const Babl *input_format;

  input_format = gegl_operation_get_source_format (operation, "input");
  if (input_format == NULL)
    return;

  if (babl_format_has_alpha (input_format))
    {
      format = babl_format ("CIE LCH(ab) alpha float");
      point_filter_class->process = photos_operation_saturation_process_alpha;
    }
  else
    {
      format = babl_format ("CIE LCH(ab) float");
      point_filter_class->process = photos_operation_saturation_process;
    }

  gegl_operation_set_format (operation, "input", format);
  gegl_operation_set_format (operation, "output", format);
}


static void
photos_operation_saturation_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
  PhotosOperationSaturation *self = PHOTOS_OPERATION_SATURATION (object);

  switch (prop_id)
    {
    case PROP_SCALE:
      g_value_set_float (value, self->scale);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
photos_operation_saturation_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
  PhotosOperationSaturation *self = PHOTOS_OPERATION_SATURATION (object);

  switch (prop_id)
    {
    case PROP_SCALE:
      self->scale = g_value_get_float (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
photos_operation_saturation_init (PhotosOperationSaturation *self)
{
}


static void
photos_operation_saturation_class_init (PhotosOperationSaturationClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GeglOperationClass *operation_class = GEGL_OPERATION_CLASS (class);
  GeglOperationPointFilterClass *point_filter_class = GEGL_OPERATION_POINT_FILTER_CLASS (class);

  operation_class->opencl_support = FALSE;

  object_class->get_property = photos_operation_saturation_get_property;
  object_class->set_property = photos_operation_saturation_set_property;
  operation_class->prepare = photos_operation_saturation_prepare;
  point_filter_class->process = NULL; /* will be assigned in prepare */

  g_object_class_install_property (object_class,
                                   PROP_SCALE,
                                   g_param_spec_float ("scale",
                                                       "Scale",
                                                       "Strength of effect",
                                                       0.0,
                                                       2.0,
                                                       1.0,
                                                       G_PARAM_CONSTRUCT | G_PARAM_READWRITE));

  gegl_operation_class_set_keys (operation_class,
                                 "name", "photos:saturation",
                                 "title", "Saturation",
                                 "description", "Changes the saturation",
                                 "categories", "color",
                                 NULL);
}
