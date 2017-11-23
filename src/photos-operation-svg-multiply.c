/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2017 Red Hat, Inc.
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
 *   + GEGL
 */


#include "config.h"

#include <babl/babl.h>
#include <gegl.h>

#include "photos-operation-svg-multiply.h"


struct _PhotosOperationSvgMultiply
{
  GeglOperationPointComposer parent_instance;
  gboolean srgb;
};

enum
{
  PROP_0,
  PROP_SRGB
};


G_DEFINE_TYPE (PhotosOperationSvgMultiply, photos_operation_svg_multiply, GEGL_TYPE_OPERATION_POINT_COMPOSER);


static void
photos_operation_svg_multiply_prepare (GeglOperation *operation)
{
  PhotosOperationSvgMultiply *self = PHOTOS_OPERATION_SVG_MULTIPLY (operation);
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
photos_operation_svg_multiply_operation_process (GeglOperation *operation,
                                                 GeglOperationContext *context,
                                                 const gchar *output_pad,
                                                 const GeglRectangle *roi,
                                                 gint level)
{
  GObject *aux;
  GObject *input;
  const GeglRectangle *aux_bbox = NULL;
  const GeglRectangle *in_bbox = NULL;
  gboolean ret_val = TRUE;

  aux = gegl_operation_context_get_object (context, "aux");
  if (aux != NULL)
    aux_bbox = gegl_buffer_get_abyss (GEGL_BUFFER (aux));

  input = gegl_operation_context_get_object (context, "input");
  if (input != NULL)
    in_bbox = gegl_buffer_get_abyss (GEGL_BUFFER (input));

  if (aux == NULL || (input != NULL && !gegl_rectangle_intersect (NULL, aux_bbox, roi)))
    {
      gegl_operation_context_set_object (context, "output", input);
      goto out;
    }

  if (input == NULL || (aux != NULL && !gegl_rectangle_intersect (NULL, in_bbox, roi)))
    {
      gegl_operation_context_set_object (context, "output", aux);
      goto out;
    }

  ret_val = GEGL_OPERATION_CLASS (photos_operation_svg_multiply_parent_class)->process (operation,
                                                                                        context,
                                                                                        output_pad,
                                                                                        roi,
                                                                                        level);

 out:
  return ret_val;
}


static gboolean
photos_operation_svg_multiply_point_composer_process (GeglOperation *operation,
                                                      void *in_buf,
                                                      void *aux_buf,
                                                      void *out_buf,
                                                      glong n_pixels,
                                                      const GeglRectangle *roi,
                                                      gint level)
{
  gfloat *aux = aux_buf;
  gfloat *in = in_buf;
  gfloat *out = out_buf;
  glong i;

  g_return_val_if_fail (aux != NULL, FALSE);
  g_return_val_if_fail (in != NULL, FALSE);

  for (i = 0; i < n_pixels; i++)
    {
      const gfloat aA = aux[3];
      const gfloat aB = in[3];
      const gfloat aR = aA + aB * (1 - aA);
      gint j;

      out[3] = aR;

      for (j = 0; j < 3; j++)
        {
          const gfloat xA = aux[j];
          const gfloat xB = in[j];
          gfloat xR;

          xR = (1 - aB) * xA + (1 - aA) * xB + xA * xB;
          out[j] = CLAMP (xR, 0.0f, aR);
        }

      aux += 4;
      in += 4;
      out += 4;
    }

  return TRUE;
}


static void
photos_operation_svg_multiply_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
  PhotosOperationSvgMultiply *self = PHOTOS_OPERATION_SVG_MULTIPLY (object);

  switch (prop_id)
    {
    case PROP_SRGB:
      g_value_set_boolean (value, self->srgb);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
photos_operation_svg_multiply_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
  PhotosOperationSvgMultiply *self = PHOTOS_OPERATION_SVG_MULTIPLY (object);

  switch (prop_id)
    {
    case PROP_SRGB:
      self->srgb = g_value_get_boolean (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
photos_operation_svg_multiply_init (PhotosOperationSvgMultiply *self)
{
}


static void
photos_operation_svg_multiply_class_init (PhotosOperationSvgMultiplyClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GeglOperationClass *operation_class = GEGL_OPERATION_CLASS (class);
  GeglOperationPointComposerClass *point_composer_class = GEGL_OPERATION_POINT_COMPOSER_CLASS (class);

  operation_class->opencl_support = FALSE;

  object_class->get_property = photos_operation_svg_multiply_get_property;
  object_class->set_property = photos_operation_svg_multiply_set_property;
  operation_class->prepare = photos_operation_svg_multiply_prepare;
  operation_class->process = photos_operation_svg_multiply_operation_process;
  point_composer_class->process = photos_operation_svg_multiply_point_composer_process;

  g_object_class_install_property (object_class,
                                   PROP_SRGB,
                                   g_param_spec_boolean ("srgb",
                                                         "sRGB",
                                                         "Use sRGB gamma instead of linear",
                                                         FALSE,
                                                         G_PARAM_CONSTRUCT | G_PARAM_READWRITE));

  gegl_operation_class_set_keys (operation_class,
                                 "name", "photos:svg-multiply",
                                 "title", "SVG Multiply",
                                 "description", "SVG blend operation multiply",
                                 "categories", "compositors:svgfilter",
                                 NULL);
}
