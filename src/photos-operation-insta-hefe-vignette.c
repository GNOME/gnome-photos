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
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gegl.h>
#include <gegl-plugin.h>

#include "photos-operation-insta-hefe-vignette.h"


struct _PhotosOperationInstaHefeVignette
{
  GeglOperationPointRender parent_instance;
  gdouble height;
  gdouble height_ratio;
  gdouble width;
  gdouble width_ratio;
  gdouble x;
  gdouble y;
};

struct _PhotosOperationInstaHefeVignetteClass
{
  GeglOperationPointRenderClass parent_class;
};

enum
{
  PROP_0,
  PROP_HEIGHT,
  PROP_WIDTH,
  PROP_X,
  PROP_Y
};


G_DEFINE_TYPE (PhotosOperationInstaHefeVignette,
               photos_operation_insta_hefe_vignette,
               GEGL_TYPE_OPERATION_POINT_RENDER);


static GdkPixbuf *vignette;
static guchar *vignette_pixels;
static gint vignette_channels;
static gint vignette_height;
static gint vignette_rowstride;
static gint vignette_width;


static void
photos_operation_insta_hefe_vignette_get_rgb (PhotosOperationInstaHefeVignette *self,
                                              gint x,
                                              gint y,
                                              guint8 *out_r,
                                              guint8 *out_g,
                                              guint8 *out_b)
{
  gint pixbuf_x;
  gint pixbuf_y;
  guchar *pixel;

  pixbuf_x = (gint) (((gdouble) x - self->x) * self->width_ratio + 0.5);
  pixbuf_y = (gint) (((gdouble) y - self->y) * self->height_ratio + 0.5);
  pixel = vignette_pixels + (vignette_rowstride * pixbuf_y) + vignette_channels * pixbuf_x;

  /* We don't want optional out parameters because they will
   * introduce if branches in this hot path.
  */
  *out_r = (guint8) pixel[0];
  *out_g = (guint8) pixel[1];
  *out_b = (guint8) pixel[2];
}


static GeglRectangle
photos_operation_insta_hefe_vignette_get_bounding_box (GeglOperation *operation)
{
  PhotosOperationInstaHefeVignette *self = PHOTOS_OPERATION_INSTA_HEFE_VIGNETTE (operation);
  GeglRectangle bbox;

  gegl_rectangle_set (&bbox, (gint) self->x, (gint) self->y, (gint) self->width, (gint) self->height);
  return bbox;
}


static void
photos_operation_insta_hefe_vignette_prepare (GeglOperation *operation)
{
  const Babl* format;

  format = babl_format ("R'G'B'A u8");
  gegl_operation_set_format (operation, "output", format);
}


static gboolean
photos_operation_insta_hefe_vignette_process (GeglOperation *operation,
                                              void *out_buf,
                                              glong n_pixels,
                                              const GeglRectangle *roi,
                                              gint level)
{
  PhotosOperationInstaHefeVignette *self = PHOTOS_OPERATION_INSTA_HEFE_VIGNETTE (operation);
  const gint x1 = roi->x + roi->width;
  const gint y1 = roi->y + roi->height;
  gint x;
  gint y;
  guint8 *out = out_buf;

  for (y = roi->y; y < y1; y++)
    {
      for (x = roi->x; x < x1; x++)
        {
          photos_operation_insta_hefe_vignette_get_rgb (self, x, y, &out[0], &out[1], &out[2]);
          out[3] = 255;
          out += 4;
        }
    }

  return TRUE;
}


static void
photos_operation_insta_hefe_vignette_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
  PhotosOperationInstaHefeVignette *self = PHOTOS_OPERATION_INSTA_HEFE_VIGNETTE (object);

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
photos_operation_insta_hefe_vignette_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
  PhotosOperationInstaHefeVignette *self = PHOTOS_OPERATION_INSTA_HEFE_VIGNETTE (object);

  switch (prop_id)
    {
    case PROP_HEIGHT:
      self->height = g_value_get_double (value);
      self->height_ratio = vignette_height / self->height;
      break;

    case PROP_WIDTH:
      self->width = g_value_get_double (value);
      self->width_ratio = vignette_width / self->width;
      break;

    case PROP_X:
      self->x = g_value_get_double (value);
      break;

    case PROP_Y:
      self->y = g_value_get_double (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
photos_operation_insta_hefe_vignette_init (PhotosOperationInstaHefeVignette *self)
{
  if (vignette == NULL)
    {
      GError *error;

      error = NULL;
      vignette = gdk_pixbuf_new_from_resource ("/org/gnome/Photos/vignette.png", &error);
      g_assert_no_error (error);

      vignette_channels = gdk_pixbuf_get_n_channels (vignette);
      g_assert_cmpint (vignette_channels, ==, 3);

      vignette_pixels = gdk_pixbuf_get_pixels (vignette);
      vignette_rowstride = gdk_pixbuf_get_rowstride (vignette);
      vignette_height = gdk_pixbuf_get_height (vignette);
      vignette_width = gdk_pixbuf_get_width (vignette);
    }
}


static void
photos_operation_insta_hefe_vignette_class_init (PhotosOperationInstaHefeVignetteClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GeglOperationClass *operation_class = GEGL_OPERATION_CLASS (class);
  GeglOperationPointRenderClass *point_render_class = GEGL_OPERATION_POINT_RENDER_CLASS (class);

  operation_class->opencl_support = FALSE;

  object_class->get_property = photos_operation_insta_hefe_vignette_get_property;
  object_class->set_property = photos_operation_insta_hefe_vignette_set_property;
  operation_class->get_bounding_box = photos_operation_insta_hefe_vignette_get_bounding_box;
  operation_class->prepare = photos_operation_insta_hefe_vignette_prepare;
  point_render_class->process = photos_operation_insta_hefe_vignette_process;

  g_object_class_install_property (object_class,
                                   PROP_HEIGHT,
                                   g_param_spec_double ("height",
                                                        "Height",
                                                        "Vertical extent",
                                                        0.0,
                                                        G_MAXDOUBLE,
                                                        10.0,
                                                        G_PARAM_CONSTRUCT | G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_WIDTH,
                                   g_param_spec_double ("width",
                                                        "Width",
                                                        "Horizontal extent",
                                                        0.0,
                                                        G_MAXDOUBLE,
                                                        10.0,
                                                        G_PARAM_CONSTRUCT | G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_X,
                                   g_param_spec_double ("x",
                                                        "X",
                                                        "Horizontal position",
                                                        0.0,
                                                        G_MAXDOUBLE,
                                                        0.0,
                                                        G_PARAM_CONSTRUCT | G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_Y,
                                   g_param_spec_double ("y",
                                                        "Y",
                                                        "Vertical position",
                                                        0.0,
                                                        G_MAXDOUBLE,
                                                        0.0,
                                                        G_PARAM_CONSTRUCT | G_PARAM_READWRITE));

  gegl_operation_class_set_keys (operation_class,
                                 "name", "photos:insta-hefe-vignette",
                                 "title", "Insta Hefe Vignette",
                                 "description", "Apply the Hefe vignette to an image",
                                 "categories", "hidden",
                                 NULL);
}
