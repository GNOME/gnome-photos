/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2016 – 2017 Red Hat, Inc.
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

#include "photos-operation-insta-hefe-curve.h"


struct _PhotosOperationInstaHefeCurve
{
  GeglOperationPointFilter parent_instance;
};

struct _PhotosOperationInstaHefeCurveClass
{
  GeglOperationPointFilterClass parent_class;
};


G_DEFINE_TYPE (PhotosOperationInstaHefeCurve, photos_operation_insta_hefe_curve, GEGL_TYPE_OPERATION_POINT_FILTER);


static void
photos_operation_insta_hefe_curve_prepare (GeglOperation *operation)
{
  const Babl *format;

  format = babl_format ("R'G'B'A float");
  gegl_operation_set_format (operation, "input", format);
  gegl_operation_set_format (operation, "output", format);
}


static gboolean
photos_operation_insta_hefe_curve_process (GeglOperation *operation,
                                           void *in_buf,
                                           void *out_buf,
                                           glong n_pixels,
                                           const GeglRectangle *roi,
                                           gint level)
{
  gfloat *in = in_buf;
  gfloat *out = out_buf;
  glong i;

  for (i = 0; i < n_pixels; i++)
    {
      const float b = in[2];
      const float b2 = b * b;
      const float b3 = b2 * b;
      const float g = in[1];
      const float g2 = g * g;
      const float g3 = g2 * g;
      const float r = in[0];
      const float r2 = r * r;
      const float r3 = r2 * r;

      out[0] = -13.47f * r3 * r3 + 41.23f * r3 * r2 - 45.04f * r2 * r2 + 19.17f * r3 - 1.492f * r2 + 0.5954f * r;
      out[1] = -12.28f * g3 * g3 + 41.09f * g3 * g2 - 50.52f * g2 * g2 + 26.03f * g3 - 3.916f * g2 + 0.58f * g;
      out[2] = -1.066f * b3 * b3 + 9.679f * b3 * b2 - 19.09f * b2 * b2 + 12.92f * b3 - 1.835f * b2 + 0.3487f * b;
      out[3] = in[3];

      in += 4;
      out += 4;
    }

  return TRUE;
}


static void
photos_operation_insta_hefe_curve_init (PhotosOperationInstaHefeCurve *self)
{
}


static void
photos_operation_insta_hefe_curve_class_init (PhotosOperationInstaHefeCurveClass *class)
{
  GeglOperationClass *operation_class = GEGL_OPERATION_CLASS (class);
  GeglOperationPointFilterClass *point_filter_class = GEGL_OPERATION_POINT_FILTER_CLASS (class);

  operation_class->opencl_support = FALSE;

  operation_class->prepare = photos_operation_insta_hefe_curve_prepare;
  point_filter_class->process = photos_operation_insta_hefe_curve_process;

  gegl_operation_class_set_keys (operation_class,
                                 "name", "photos:insta-hefe-curve",
                                 "title", "Insta Hefe Curve",
                                 "description", "Apply the Hefe curve to an image",
                                 "categories", "hidden",
                                 NULL);
}
