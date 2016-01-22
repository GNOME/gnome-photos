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

#include "photos-operation-add.h"


struct _PhotosOperationAdd
{
  GeglOperationPointComposer parent_instance;
  gboolean srgb;
  gfloat value;
};

struct _PhotosOperationAddClass
{
  GeglOperationPointComposerClass parent_class;
};

enum
{
  PROP_0,
  PROP_SRGB,
  PROP_VALUE
};


G_DEFINE_TYPE (PhotosOperationAdd, photos_operation_add, GEGL_TYPE_OPERATION_POINT_COMPOSER);


static void
photos_operation_add_prepare (GeglOperation *operation)
{
  PhotosOperationAdd *self = PHOTOS_OPERATION_ADD (operation);
  const Babl *format;

  if (self->srgb)
    format = babl_format ("R'aG'aB'aA float");
  else
    format = babl_format ("RaGaBaA float");

  gegl_operation_set_format (operation, "aux", format);
  gegl_operation_set_format (operation, "input", format);
  gegl_operation_set_format (operation, "output", format);
}


static gboolean
photos_operation_add_process (GeglOperation *operation,
                              void *in_buf,
                              void *aux_buf,
                              void *out_buf,
                              glong n_pixels,
                              const GeglRectangle *roi,
                              gint level)
{
  PhotosOperationAdd *self = PHOTOS_OPERATION_ADD (operation);
  gfloat *aux = aux_buf;
  gfloat *in = in_buf;
  gfloat *out = out_buf;
  glong i;

  if (aux == NULL)
    {
      for (i = 0; i < n_pixels; i++)
        {
          out[0] = self->value + in[0];
          out[1] = self->value + in[1];
          out[2] = self->value + in[2];
          out[3] = in[3];

          in += 4;
          out += 4;
        }
    }
  else
    {
      for (i = 0; i < n_pixels; i++)
        {
          out[0] = aux[0] + in[0];
          out[1] = aux[1] + in[1];
          out[2] = aux[2] + in[2];
          out[3] = aux[3] + in[3];

          aux += 4;
          in += 4;
          out += 4;
        }
    }

  return TRUE;
}


static void
photos_operation_add_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
  PhotosOperationAdd *self = PHOTOS_OPERATION_ADD (object);

  switch (prop_id)
    {
    case PROP_SRGB:
      g_value_set_boolean (value, self->srgb);
      break;

    case PROP_VALUE:
      g_value_set_double (value, (gdouble) self->value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
photos_operation_add_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
  PhotosOperationAdd *self = PHOTOS_OPERATION_ADD (object);

  switch (prop_id)
    {
    case PROP_SRGB:
      self->srgb = g_value_get_boolean (value);
      break;

    case PROP_VALUE:
      self->value = (gfloat) g_value_get_double (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
photos_operation_add_init (PhotosOperationAdd *self)
{
}


static void
photos_operation_add_class_init (PhotosOperationAddClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GeglOperationClass *operation_class = GEGL_OPERATION_CLASS (class);
  GeglOperationPointComposerClass *point_composer_class = GEGL_OPERATION_POINT_COMPOSER_CLASS (class);

  operation_class->opencl_support = FALSE;

  object_class->get_property = photos_operation_add_get_property;
  object_class->set_property = photos_operation_add_set_property;
  operation_class->prepare = photos_operation_add_prepare;
  point_composer_class->process = photos_operation_add_process;

  g_object_class_install_property (object_class,
                                   PROP_SRGB,
                                   g_param_spec_boolean ("srgb",
                                                         "sRGB",
                                                         "Use sRGB gamma instead of linear",
                                                         FALSE,
                                                         G_PARAM_CONSTRUCT | G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_VALUE,
                                   g_param_spec_double ("value",
                                                        "Value",
                                                        "Global value used if aux doesn't contain data",
                                                        -1.0,
                                                        1.0,
                                                        0.0,
                                                        G_PARAM_CONSTRUCT | G_PARAM_READWRITE));

  gegl_operation_class_set_keys (operation_class,
                                 "name", "photos:add",
                                 "title", "Add",
                                 "description", "Porter Duff operation add (d = cA + cB)",
                                 "categories", "compositors:porter-duff",
                                 NULL);
}
