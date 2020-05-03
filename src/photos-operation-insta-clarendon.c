/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2020 Samuel Zachara.
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

#include <babl/babl.h>
#include <gegl.h>

#include <math.h>

#include "photos-operation-insta-clarendon.h"

typedef void (*PhotosOperationProcessFunc) (GeglOperation *, void *, void *, glong, const GeglRectangle *, gint);

struct _PhotosOperationInstaClarendon
{
  GeglOperationPointFilter parent_instance;
};


G_DEFINE_TYPE (PhotosOperationInstaClarendon, photos_operation_insta_clarendon, GEGL_TYPE_OPERATION_POINT_FILTER);


static void
photos_operation_insta_clarendon_prepare (GeglOperation *operation)
{
  const Babl *format;

  format = babl_format ("R'G'B' u8");
  gegl_operation_set_format (operation, "input", format);
  gegl_operation_set_format (operation, "output", format);
}


static gboolean
photos_operation_insta_clarendon_process (GeglOperation *operation,
                                     void *in_buf,
                                     void *out_buf,
                                     glong n_pixels,
                                     const GeglRectangle *roi,
                                     gint level)
{
  guint8 *in = in_buf;
  guint8 *out = out_buf;
  glong i;

  for (i = 0; i < n_pixels; i++)
    {

      const uint b = in[2];
      const uint b2 = b * b;
      const uint b3 = b2 * b;
      const ulong b4 = b3 * b;

      const uint g = in[1];
      const uint g2 = g * g;
      const uint g3 = g2 * g;
      const ulong g4 = g3 * g;

      const uint r = in[0];
      const uint r2 = r * r;
      const uint r3 = r2 * r;
      const ulong r4 = r3 * r;

      //option with degrees 4, 1
      int r_out = 18.37f - 1.05f * r - 0.0276f * g + 0.03275f * r2 - 0.001056f * r * g - 0.000152f * r3 + 2.006e-6f *r2 * g + 2.091e-7f * r4 + 9.682e-9f * r3 * g;
      int g_out = 6.87f - 0.1453 * g + 0.02435 * g2 - 0.0001355 * g3 + 2.267e-7 * g4;
      int b_out = 13.3f + 0.4149f * b - 0.08369f * g + 0.01699f * b2 - 0.001413f * b * g - 9.235e-5f * b3 + 1.239e-5f *b2 * g + 1.334e-7f * b4 - 2.221e-8f * b3 * g;

      if (r_out > 255)
        {
          r_out = 255;
        }

      if (g_out > 255)
        {
          g_out = 255;
        }

      if (b_out > 255)
        {
          b_out = 255;
        }

      if (r_out < 0)
        {
          r_out = 0;
        }

      if (g_out < 0)
        {
          g_out = 0;
        }

      if (b_out < 0)
        {
          b_out = 0;
        }

      out[0] = r_out;
      out[1] = g_out;
      out[2] = b_out;

      in += 3;
      out += 3;
    }
  return TRUE;
}


static void
photos_operation_insta_clarendon_init (PhotosOperationInstaClarendon *self)
{
}


static void
photos_operation_insta_clarendon_class_init (PhotosOperationInstaClarendonClass *class)
{
  GeglOperationClass *operation_class = GEGL_OPERATION_CLASS (class);
  GeglOperationPointFilterClass *point_filter_class = GEGL_OPERATION_POINT_FILTER_CLASS (class);

  operation_class->opencl_support = FALSE;

  operation_class->prepare = photos_operation_insta_clarendon_prepare;
  point_filter_class->process = photos_operation_insta_clarendon_process;

  gegl_operation_class_set_keys (operation_class,
                                 "name", "photos:insta-clarendon",
                                 "title", "Clarendon",
                                 "description", "Apply the Clarendon filter to an image",
                                 "categories", "hidden",
                                 NULL);
}
