/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2017 Red Hat, Inc.
 * Copyright © 2017 Thomas Manni
 * Copyright © 2012 – 2015 Ulrich Pegelow
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
 *   + Darktable
 *   + GEGL
 */


#include "config.h"

#include <math.h>

#include <babl/babl.h>
#include <gegl.h>

#include "photos-operation-shadows-highlights-correction.h"


struct _PhotosOperationShadowsHighlightsCorrection
{
  GeglOperationPointComposer parent_instance;
  gdouble compress;
  gdouble highlights;
  gdouble highlights_color_correct;
  gdouble shadows;
  gdouble shadows_color_correct;
  gdouble whitepoint;
};

enum
{
  PROP_0,
  PROP_COMPRESS,
  PROP_HIGHLIGHTS,
  PROP_HIGHLIGHTS_COLOR_CORRECT,
  PROP_SHADOWS,
  PROP_SHADOWS_COLOR_CORRECT,
  PROP_WHITEPOINT
};


G_DEFINE_TYPE (PhotosOperationShadowsHighlightsCorrection,
               photos_operation_shadows_highlights_correction,
               GEGL_TYPE_OPERATION_POINT_COMPOSER);


static GeglRectangle
photos_operation_shadows_highlights_correction_get_bounding_box (GeglOperation *operation)
{
  GeglRectangle *in_bbox;
  GeglRectangle bbox;

  gegl_rectangle_set (&bbox, 0, 0, 0, 0);

  in_bbox = gegl_operation_source_get_bounding_box (operation, "input");
  if (in_bbox == NULL)
    goto out;

  bbox = *in_bbox;

 out:
  return bbox;
}


static void
photos_operation_shadows_highlights_correction_prepare (GeglOperation *operation)
{
  const Babl *format_cie_l;
  const Babl *format_cie_laba;

  format_cie_l = babl_format ("CIE L float");
  format_cie_laba = babl_format ("CIE Lab alpha float");

  gegl_operation_set_format (operation, "aux", format_cie_l);
  gegl_operation_set_format (operation, "input", format_cie_laba);
  gegl_operation_set_format (operation, "output", format_cie_laba);
}


static gboolean
photos_operation_shadows_highlights_correction_process (GeglOperation *operation,
                                                        void *in_buf,
                                                        void *aux_buf,
                                                        void *out_buf,
                                                        glong n_pixels,
                                                        const GeglRectangle *roi,
                                                        gint level)
{
  PhotosOperationShadowsHighlightsCorrection *self = PHOTOS_OPERATION_SHADOWS_HIGHLIGHTS_CORRECTION (operation);
  gfloat *aux = aux_buf;
  gfloat *in = in_buf;
  gfloat *out = out_buf;
  gfloat compress;
  gfloat compress_100 = (gfloat) self->compress / 100.0f;
  gfloat compress_inverted;
  gfloat highlights;
  gfloat highlights_100 = (gfloat) self->highlights / 100.0f;
  gfloat highlights_color_correct;
  gfloat highlights_color_correct_100 = (gfloat) self->highlights_color_correct / 100.0f;
  gfloat highlights_color_correct_inverted;
  gfloat highlights_sign_negated;
  gfloat low_approximation = 0.01f;
  gfloat shadows;
  gfloat shadows_100 = (gfloat) self->shadows / 100.0f;
  gfloat shadows_color_correct;
  gfloat shadows_color_correct_100 = (gfloat) self->shadows_color_correct / 100.0f;
  gfloat shadows_color_correct_inverted;
  gfloat shadows_sign;
  gfloat whitepoint = 1.0f - (gfloat) self->whitepoint / 100.0f;
  glong i;

  compress = fminf (compress_100, 0.99f);
  g_return_val_if_fail (compress >= 0.0f, FALSE);

  compress_inverted = 1.0f - compress;

  g_return_val_if_fail (-1.0f <= highlights_100 && highlights_100 <= 1.0f, FALSE);
  highlights = 2.0f * highlights_100;

  g_return_val_if_fail (0.0f <= highlights_color_correct_100 && highlights_color_correct_100 <= 1.0f, FALSE);
  highlights_sign_negated = -highlights < 0.0f ? -1.0f : 1.0f;
  highlights_color_correct = (highlights_color_correct_100 - 0.5f) * highlights_sign_negated + 0.5f;
  highlights_color_correct_inverted = 1.0f - highlights_color_correct;

  g_return_val_if_fail (-1.0f <= shadows_100 && shadows_100 <= 1.0f, FALSE);
  shadows = 2.0f * shadows_100;

  g_return_val_if_fail (0.0f <= shadows_color_correct_100 && shadows_color_correct_100 <= 1.0f, FALSE);
  shadows_sign = shadows < 0.0f ? -1.0f : 1.0f;
  shadows_color_correct = (shadows_color_correct_100 - 0.5f) * shadows_sign + 0.5f;
  shadows_color_correct_inverted = 1.0f - shadows_color_correct;

  g_return_val_if_fail (whitepoint >= 0.01f, FALSE);

  for (i = 0; i < n_pixels; i++)
    {
      gfloat ta[3];
      gfloat tb0;
      gfloat highlights_xform;
      gfloat highlights2 = highlights * highlights;
      gfloat shadows_xform;
      gfloat shadows2 = shadows * shadows;

      ta[0] = in[0] / 100.0f;
      ta[1] = in[1] / 128.0f;
      ta[2] = in[2] / 128.0f;

      tb0 = (100.0f - aux[0]) / 100.0f;

      ta[0] = ta[0] > 0.0f ? ta[0] / whitepoint : ta[0];
      tb0 = tb0 > 0.0f ? tb0 / whitepoint : tb0;

      highlights_xform = CLAMP (1.0f - tb0 / compress_inverted, 0.0f, 1.0f);

      while (highlights2 > 0.0f)
        {
          gfloat chunk;
          gfloat href;
          gfloat la = ta[0];
          gfloat la_abs;
          gfloat la_inverted = 1.0f - la;
          gfloat la_inverted_abs;
          gfloat la_inverted_sign;
          gfloat lb;
          gfloat lref;
          gfloat optrans;
          gfloat optrans_inverted;

          la_inverted_sign = la_inverted < 0.0f ? -1.0f : 1.0f;
          lb = (tb0 - 0.5f) * highlights_sign_negated * la_inverted_sign + 0.5f;

          la_abs = fabsf (la);
          lref = copysignf (la_abs > low_approximation ? 1.0f / la_abs : 1.0f / low_approximation, la);

          la_inverted_abs = fabsf (la_inverted);
          href = copysignf (la_inverted_abs > low_approximation ? 1.0f / la_inverted_abs : 1.0f / low_approximation,
                            la_inverted);

          chunk = fminf (highlights2, 1.0f);
          optrans = chunk * highlights_xform;
          optrans_inverted = 1.0f - optrans;

          highlights2 -= 1.0f;

          ta[0] = la * optrans_inverted
            + (la > 0.5f ? 1.0f - (1.0f - 2.0f * (la - 0.5f)) * (1.0f - lb) : 2.0f * la * lb) * optrans;

          ta[1] = ta[1] * optrans_inverted
            + ta[1] * (ta[0] * lref * highlights_color_correct_inverted
                       + (1.0f - ta[0]) * href * highlights_color_correct) * optrans;

          ta[2] = ta[2] * optrans_inverted
            + ta[2] * (ta[0] * lref * highlights_color_correct_inverted
                       + (1.0f - ta[0]) * href * highlights_color_correct) * optrans;
        }

      shadows_xform = CLAMP (tb0 / compress_inverted - compress / compress_inverted, 0.0f, 1.0f);

      while (shadows2 > 0.0f)
        {
          gfloat chunk;
          gfloat href;
          gfloat la = ta[0];
          gfloat la_abs;
          gfloat la_inverted = 1.0f - la;
          gfloat la_inverted_abs;
          gfloat la_inverted_sign;
          gfloat lb;
          gfloat lref;
          gfloat optrans;
          gfloat optrans_inverted;

          la_inverted_sign = la_inverted < 0.0f ? -1.0f : 1.0f;
          lb = (tb0 - 0.5f) * shadows_sign * la_inverted_sign + 0.5f;

          la_abs = fabsf (la);
          lref = copysignf (la_abs > low_approximation ? 1.0f / la_abs : 1.0f / low_approximation, la);

          la_inverted_abs = fabsf (la_inverted);
          href = copysignf (la_inverted_abs > low_approximation ? 1.0f / la_inverted_abs : 1.0f / low_approximation,
                            la_inverted);

          chunk = fminf (shadows2, 1.0f);
          optrans = chunk * shadows_xform;
          optrans_inverted = 1.0f - optrans;

          shadows2 -= 1.0f;

          ta[0] = la * optrans_inverted
            + (la > 0.5f ? 1.0f - (1.0f - 2.0f * (la - 0.5f)) * (1.0f - lb) : 2.0f * la * lb) * optrans;

          ta[1] = ta[1] * optrans_inverted
            + ta[1] * (ta[0] * lref * shadows_color_correct
                       + (1.0f - ta[0]) * href * shadows_color_correct_inverted) * optrans;

          ta[2] = ta[2] * optrans_inverted
            + ta[2] * (ta[0] * lref * shadows_color_correct
                       + (1.0f - ta[0]) * href * shadows_color_correct_inverted) * optrans;
        }

      out[0] = ta[0] * 100.0f;
      out[1] = ta[1] * 128.0f;
      out[2] = ta[2] * 128.0f;
      out[3] = in[3];

      aux++;
      in += 4;
      out += 4;
    }

  return TRUE;
}


static void
photos_operation_shadows_highlights_correction_get_property (GObject *object,
                                                             guint prop_id,
                                                             GValue *value,
                                                             GParamSpec *pspec)
{
  PhotosOperationShadowsHighlightsCorrection *self = PHOTOS_OPERATION_SHADOWS_HIGHLIGHTS_CORRECTION (object);

  switch (prop_id)
    {
    case PROP_COMPRESS:
      g_value_set_double (value, self->compress);
      break;

    case PROP_HIGHLIGHTS:
      g_value_set_double (value, self->highlights);
      break;

    case PROP_HIGHLIGHTS_COLOR_CORRECT:
      g_value_set_double (value, self->highlights_color_correct);
      break;

    case PROP_SHADOWS:
      g_value_set_double (value, self->shadows);
      break;

    case PROP_SHADOWS_COLOR_CORRECT:
      g_value_set_double (value, self->shadows_color_correct);
      break;

    case PROP_WHITEPOINT:
      g_value_set_double (value, self->whitepoint);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
photos_operation_shadows_highlights_correction_set_property (GObject *object,
                                                             guint prop_id,
                                                             const GValue *value,
                                                             GParamSpec *pspec)
{
  PhotosOperationShadowsHighlightsCorrection *self = PHOTOS_OPERATION_SHADOWS_HIGHLIGHTS_CORRECTION (object);

  switch (prop_id)
    {
    case PROP_COMPRESS:
      self->compress = g_value_get_double (value);
      break;

    case PROP_HIGHLIGHTS:
      self->highlights = g_value_get_double (value);
      break;

    case PROP_HIGHLIGHTS_COLOR_CORRECT:
      self->highlights_color_correct = g_value_get_double (value);
      break;

    case PROP_SHADOWS:
      self->shadows = g_value_get_double (value);
      break;

    case PROP_SHADOWS_COLOR_CORRECT:
      self->shadows_color_correct = g_value_get_double (value);
      break;

    case PROP_WHITEPOINT:
      self->whitepoint = g_value_get_double (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
photos_operation_shadows_highlights_correction_init (PhotosOperationShadowsHighlightsCorrection *self)
{
}


static void
photos_operation_shadows_highlights_correction_class_init (PhotosOperationShadowsHighlightsCorrectionClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GeglOperationClass *operation_class = GEGL_OPERATION_CLASS (class);
  GeglOperationPointComposerClass *point_composer_class = GEGL_OPERATION_POINT_COMPOSER_CLASS (class);

  operation_class->opencl_support = FALSE;

  object_class->get_property = photos_operation_shadows_highlights_correction_get_property;
  object_class->set_property = photos_operation_shadows_highlights_correction_set_property;
  operation_class->get_bounding_box = photos_operation_shadows_highlights_correction_get_bounding_box;
  operation_class->prepare = photos_operation_shadows_highlights_correction_prepare;
  point_composer_class->process = photos_operation_shadows_highlights_correction_process;

  g_object_class_install_property (object_class,
                                   PROP_COMPRESS,
                                   g_param_spec_double ("compress",
                                                        "Compress",
                                                        "Compress the effect on shadows/highlights and preserve "
                                                        "midtones",
                                                        0.0,
                                                        100.0,
                                                        50.0,
                                                        G_PARAM_CONSTRUCT | G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_HIGHLIGHTS,
                                   g_param_spec_double ("highlights",
                                                        "Highlights",
                                                        "Adjust exposure of highlights",
                                                        -100.0,
                                                        100.0,
                                                        -50.0,
                                                        G_PARAM_CONSTRUCT | G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_HIGHLIGHTS_COLOR_CORRECT,
                                   g_param_spec_double ("highlights-color-correct",
                                                        "Highlights Color Correct",
                                                        "Adjust saturation of highlights",
                                                        0.0,
                                                        100.0,
                                                        50.0,
                                                        G_PARAM_CONSTRUCT | G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_SHADOWS,
                                   g_param_spec_double ("shadows",
                                                        "Shadows",
                                                        "Adjust exposure of shadows",
                                                        -100.0,
                                                        100.0,
                                                        50.0,
                                                        G_PARAM_CONSTRUCT | G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_SHADOWS_COLOR_CORRECT,
                                   g_param_spec_double ("shadows-color-correct",
                                                        "Shadows Color Correct",
                                                        "Adjust saturation of shadows",
                                                        0.0,
                                                        100.0,
                                                        100.0,
                                                        G_PARAM_CONSTRUCT | G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_WHITEPOINT,
                                   g_param_spec_double ("whitepoint",
                                                        "Whitepoint",
                                                        "Shift white point",
                                                        -10.0,
                                                        10.0,
                                                        0.0,
                                                        G_PARAM_CONSTRUCT | G_PARAM_READWRITE));

  gegl_operation_class_set_keys (operation_class,
                                 "name", "photos:shadows-highlights-correction",
                                 "title", "Shadows Highlights Correction",
                                 "description", "Adjust shadows and highlights using a blurred auxiliary",
                                 "categories", "hidden",
                                 "license", "GPL3+",
                                 NULL);
}
