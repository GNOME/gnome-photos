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

#include <math.h>

#include <babl/babl.h>
#include <gegl.h>
#include <gegl-plugin.h>

#include "photos-operation-radial-gradient.h"


typedef void (*PhotosOperationProcessFunc) (GeglOperation *, void *, glong, const GeglRectangle *, gint);

struct _PhotosOperationRadialGradient
{
  GeglOperationPointRender parent_instance;
  GeglColor *color_end;
  GeglColor *color_start;
  PhotosOperationProcessFunc process;
  gboolean ignore_abyss;
  gfloat r0;
  gfloat r1;
  gfloat x;
  gfloat y;
};

struct _PhotosOperationRadialGradientClass
{
  GeglOperationPointRenderClass parent_class;
};

enum
{
  PROP_0,
  PROP_COLOR_END,
  PROP_COLOR_START,
  PROP_IGNORE_ABYSS,
  PROP_R0,
  PROP_R1,
  PROP_X,
  PROP_Y
};


G_DEFINE_TYPE (PhotosOperationRadialGradient, photos_operation_radial_gradient, GEGL_TYPE_OPERATION_POINT_RENDER);


static gfloat
photos_operation_radial_gradient_calculate_distance (gfloat x1, gfloat y1, gfloat x2, gfloat y2)
{
  const gfloat dx = x1 - x2;
  const gfloat dx2 = dx * dx;
  const gfloat dy = y1 - y2;
  const gfloat dy2 = dy * dy;
  gfloat distance;

  distance = sqrtf (dx2 + dy2);
  return distance;
}


static void
photos_operation_radial_gradient_process_zero (GeglOperation *operation,
                                               void *out_buf,
                                               glong n_pixels,
                                               const GeglRectangle *roi,
                                               gint level)
{
  const Babl *format;
  GeglColor *transparent;
  gfloat pixel_transparent[4];
  gint bpp;

  format = babl_format ("R'G'B'A float");
  bpp = babl_format_get_bytes_per_pixel (format);

  transparent = gegl_color_new ("#00000000");
  gegl_color_get_pixel (transparent, format, pixel_transparent);
  gegl_memset_pattern (out_buf, pixel_transparent, bpp, n_pixels);

  g_object_unref (transparent);
}


static void
photos_operation_radial_gradient_process_with_abyss (GeglOperation *operation,
                                                     void *out_buf,
                                                     glong n_pixels,
                                                     const GeglRectangle *roi,
                                                     gint level)
{
  PhotosOperationRadialGradient *self = PHOTOS_OPERATION_RADIAL_GRADIENT (operation);
  const Babl *format;
  const gfloat rdiff = self->r1 - self->r0;
  gfloat pixel_end[4];
  gfloat pixel_start[4];
  gfloat *out = out_buf;
  const gint x1 = roi->x + roi->width;
  const gint y1 = roi->y + roi->height;
  gint x;
  gint y;

  format = babl_format ("R'G'B'A float");
  gegl_color_get_pixel (self->color_end, format, pixel_end);
  gegl_color_get_pixel (self->color_start, format, pixel_start);

  for (y = roi->y; y < y1; y++)
    {
      for (x = roi->x; x < x1; x++, out += 4)
        {
          gfloat distance;
          gfloat v;
          gint c;

          distance = photos_operation_radial_gradient_calculate_distance (x, y, self->x, self->y);
          if (distance < self->r0 + GEGL_FLOAT_EPSILON)
            v = 0.0f;
          else if (distance > self->r1 - GEGL_FLOAT_EPSILON)
            v = 1.0f;
          else
            v = (distance - self->r0) / rdiff;

          for (c = 0; c < 4; c++)
            out[c] = pixel_start[c] * (1.0f - v) + pixel_end[c] * v;
        }
    }
}


static void
photos_operation_radial_gradient_process_without_abyss (GeglOperation *operation,
                                                        void *out_buf,
                                                        glong n_pixels,
                                                        const GeglRectangle *roi,
                                                        gint level)
{
  PhotosOperationRadialGradient *self = PHOTOS_OPERATION_RADIAL_GRADIENT (operation);
  const Babl *format;
  GeglColor *transparent;
  const gfloat rdiff = self->r1 - self->r0;
  gfloat pixel_end[4];
  gfloat pixel_start[4];
  gfloat pixel_transparent[4];
  gfloat *out = out_buf;
  const gint x1 = roi->x + roi->width;
  const gint y1 = roi->y + roi->height;
  gint bpp;
  gint x;
  gint y;

  format = babl_format ("R'G'B'A float");
  bpp = babl_format_get_bytes_per_pixel (format);

  transparent = gegl_color_new ("#00000000");
  gegl_color_get_pixel (transparent, format, pixel_transparent);
  gegl_memset_pattern (out_buf, pixel_transparent, bpp, n_pixels);

  gegl_color_get_pixel (self->color_end, format, pixel_end);
  gegl_color_get_pixel (self->color_start, format, pixel_start);

  for (y = roi->y; y < y1; y++)
    {
      for (x = roi->x; x < x1; x++, out += 4)
        {
          gfloat distance;
          gfloat v;
          gint c;

          distance = photos_operation_radial_gradient_calculate_distance (x, y, self->x, self->y);
          if (distance < self->r0 - GEGL_FLOAT_EPSILON)
            continue;
          else if (distance > self->r1 + GEGL_FLOAT_EPSILON)
            continue;

          if (distance < self->r0 + GEGL_FLOAT_EPSILON)
            v = 0.0f;
          else if (distance > self->r1 - GEGL_FLOAT_EPSILON)
            v = 1.0f;
          else
            v = (distance - self->r0) / rdiff;

          for (c = 0; c < 4; c++)
            out[c] = pixel_start[c] * (1.0f - v) + pixel_end[c] * v;
        }
    }

  g_object_unref (transparent);
}


static GeglRectangle
photos_operation_radial_gradient_get_bounding_box (GeglOperation *operation)
{
  GeglRectangle bbox;

  bbox = gegl_rectangle_infinite_plane ();
  return bbox;
}


static void
photos_operation_radial_gradient_prepare (GeglOperation *operation)
{
  PhotosOperationRadialGradient *self = PHOTOS_OPERATION_RADIAL_GRADIENT (operation);
  const Babl* format;
  const gfloat rdiff = self->r1 - self->r0;

  if (GEGL_FLOAT_IS_ZERO (rdiff))
    self->process = photos_operation_radial_gradient_process_zero;
  else if (self->ignore_abyss)
    self->process = photos_operation_radial_gradient_process_without_abyss;
  else
    self->process = photos_operation_radial_gradient_process_with_abyss;

  format = babl_format ("R'G'B'A float");
  gegl_operation_set_format (operation, "output", format);
}


static gboolean
photos_operation_radial_gradient_process (GeglOperation *operation,
                                          void *out_buf,
                                          glong n_pixels,
                                          const GeglRectangle *roi,
                                          gint level)
{
  PhotosOperationRadialGradient *self = PHOTOS_OPERATION_RADIAL_GRADIENT (operation);

  self->process (operation, out_buf, n_pixels, roi, level);
  return TRUE;
}


static void
photos_operation_radial_gradient_dispose (GObject *object)
{
  PhotosOperationRadialGradient *self = PHOTOS_OPERATION_RADIAL_GRADIENT (object);

  g_clear_object (&self->color_end);
  g_clear_object (&self->color_start);

  G_OBJECT_CLASS (photos_operation_radial_gradient_parent_class)->dispose (object);
}


static void
photos_operation_radial_gradient_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
  PhotosOperationRadialGradient *self = PHOTOS_OPERATION_RADIAL_GRADIENT (object);

  switch (prop_id)
    {
    case PROP_COLOR_END:
      g_value_set_object (value, self->color_end);
      break;

    case PROP_COLOR_START:
      g_value_set_object (value, self->color_start);
      break;

    case PROP_IGNORE_ABYSS:
      g_value_set_boolean (value, self->ignore_abyss);
      break;

    case PROP_R0:
      g_value_set_double (value, (gdouble) self->r0);
      break;

    case PROP_R1:
      g_value_set_double (value, (gdouble) self->r1);
      break;

    case PROP_X:
      g_value_set_double (value, (gdouble) self->x);
      break;

    case PROP_Y:
      g_value_set_double (value, (gdouble) self->y);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
photos_operation_radial_gradient_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
  PhotosOperationRadialGradient *self = PHOTOS_OPERATION_RADIAL_GRADIENT (object);

  switch (prop_id)
    {
    case PROP_COLOR_END:
      g_clear_object (&self->color_end);
      self->color_end = GEGL_COLOR (g_value_dup_object (value));
      break;

    case PROP_COLOR_START:
      g_clear_object (&self->color_start);
      self->color_start = GEGL_COLOR (g_value_dup_object (value));
      break;

    case PROP_IGNORE_ABYSS:
      self->ignore_abyss = g_value_get_boolean (value);
      break;

    case PROP_R0:
      self->r0 = (gfloat) g_value_get_double (value);
      break;

    case PROP_R1:
      self->r1 = (gfloat) g_value_get_double (value);
      break;

    case PROP_X:
      self->x = (gfloat) g_value_get_double (value);
      break;

    case PROP_Y:
      self->y = (gfloat) g_value_get_double (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
photos_operation_radial_gradient_init (PhotosOperationRadialGradient *self)
{
}


static void
photos_operation_radial_gradient_class_init (PhotosOperationRadialGradientClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GeglOperationClass *operation_class = GEGL_OPERATION_CLASS (class);
  GeglOperationPointRenderClass *point_render_class = GEGL_OPERATION_POINT_RENDER_CLASS (class);

  operation_class->no_cache = TRUE;
  operation_class->opencl_support = FALSE;

  object_class->dispose = photos_operation_radial_gradient_dispose;
  object_class->get_property = photos_operation_radial_gradient_get_property;
  object_class->set_property = photos_operation_radial_gradient_set_property;
  operation_class->get_bounding_box = photos_operation_radial_gradient_get_bounding_box;
  operation_class->prepare = photos_operation_radial_gradient_prepare;
  point_render_class->process = photos_operation_radial_gradient_process;

  g_object_class_install_property (object_class,
                                   PROP_COLOR_END,
                                   gegl_param_spec_color_from_string ("color-end",
                                                                      "Outer Color",
                                                                      "The color at the edge of the end circle",
                                                                      "black",
                                                                      G_PARAM_CONSTRUCT | G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_COLOR_START,
                                   gegl_param_spec_color_from_string ("color-start",
                                                                      "Inner Color",
                                                                      "The color at the edge of the start circle",
                                                                      "white",
                                                                      G_PARAM_CONSTRUCT | G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_IGNORE_ABYSS,
                                   g_param_spec_boolean ("ignore-abyss",
                                                         "Ignore Abyss",
                                                         "Don't draw beyond the edges of the circles",
                                                         FALSE,
                                                         G_PARAM_CONSTRUCT | G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_R0,
                                   g_param_spec_double ("r0",
                                                        "Inner Radius",
                                                        "The radius of the start circle",
                                                        0.0,
                                                        G_MAXDOUBLE,
                                                        0.0,
                                                        G_PARAM_CONSTRUCT | G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_R1,
                                   g_param_spec_double ("r1",
                                                        "Outer Radius",
                                                        "The radius of the end circle",
                                                        0.0,
                                                        G_MAXDOUBLE,
                                                        25.0,
                                                        G_PARAM_CONSTRUCT | G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_X,
                                   g_param_spec_double ("x",
                                                        "X",
                                                        "The x-coordinate of the circles' center",
                                                        0.0,
                                                        G_MAXDOUBLE,
                                                        25.0,
                                                        G_PARAM_CONSTRUCT | G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_Y,
                                   g_param_spec_double ("y",
                                                        "Y",
                                                        "The y-coordinate of the circles' center",
                                                        0.0,
                                                        G_MAXDOUBLE,
                                                        25.0,
                                                        G_PARAM_CONSTRUCT | G_PARAM_READWRITE));

  gegl_operation_class_set_keys (operation_class,
                                 "name", "photos:radial-gradient",
                                 "title", "Radial Gradient",
                                 "description", "Radial gradient renderer",
                                 "categories", "render:gradient",
                                 NULL);
}
