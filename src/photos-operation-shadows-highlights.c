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

#include "photos-operation-shadows-highlights.h"


struct _PhotosOperationShadowsHighlights
{
  GeglOperationMeta parent_instance;
  const Babl *format_blur;
  GeglNode *convert_blur;
  GeglNode *input;
  GeglNode *output;
  gfloat compress;
  gfloat highlights;
  gfloat highlights_color_correct;
  gfloat radius;
  gfloat shadows;
  gfloat shadows_color_correct;
  gfloat whitepoint;
};

enum
{
  PROP_0,
  PROP_COMPRESS,
  PROP_HIGHLIGHTS,
  PROP_HIGHLIGHTS_COLOR_CORRECT,
  PROP_RADIUS,
  PROP_SHADOWS,
  PROP_SHADOWS_COLOR_CORRECT,
  PROP_WHITEPOINT
};


G_DEFINE_TYPE (PhotosOperationShadowsHighlights, photos_operation_shadows_highlights, GEGL_TYPE_OPERATION_META);


static gboolean
photos_operation_shadows_highlights_is_nop (PhotosOperationShadowsHighlights *self)
{
  return GEGL_FLOAT_EQUAL (self->shadows, 0.0f)
    && GEGL_FLOAT_EQUAL (self->highlights, 0.0f)
    && GEGL_FLOAT_EQUAL (self->whitepoint, 0.0f);
}


static void
photos_operation_shadows_highlights_setup (PhotosOperationShadowsHighlights *self)
{
  g_autoptr (GSList) children = NULL;
  GSList *l;
  GeglOperation *operation = GEGL_OPERATION (self);

  g_return_if_fail (GEGL_IS_NODE (operation->node));
  g_return_if_fail (GEGL_IS_NODE (self->input));
  g_return_if_fail (GEGL_IS_NODE (self->output));

  self->convert_blur = NULL;

  children = gegl_node_get_children (operation->node);
  for (l = children; l != NULL; l = l->next)
    {
      GeglNode *child = GEGL_NODE (l->data);

      if (child == self->input || child == self->output)
        continue;

      gegl_node_remove_child (operation->node, child);
    }

  if (photos_operation_shadows_highlights_is_nop (self))
    {
      gegl_node_link (self->input, self->output);
    }
  else
    {
      GeglNode *blur;
      GeglNode *shadows_highlights;

      blur = gegl_node_new_child (operation->node,
                                  "operation", "gegl:gaussian-blur",
                                  "abyss-policy", 1,
                                  NULL);

      if (self->format_blur == NULL)
        self->format_blur = babl_format ("YaA float");

      self->convert_blur = gegl_node_new_child (operation->node,
                                                "operation", "gegl:convert-format",
                                                "format", self->format_blur,
                                                NULL);

      shadows_highlights = gegl_node_new_child (operation->node,
                                                "operation", "photos:shadows-highlights-correction",
                                                NULL);

      gegl_node_link_many (self->input, self->convert_blur, blur, NULL);
      gegl_node_connect_to (blur, "output", shadows_highlights, "aux");

      gegl_node_link_many (self->input, shadows_highlights, self->output, NULL);

      gegl_operation_meta_redirect (operation, "radius", blur, "std-dev-x");
      gegl_operation_meta_redirect (operation, "radius", blur, "std-dev-y");

      gegl_operation_meta_redirect (operation, "compress", shadows_highlights, "compress");
      gegl_operation_meta_redirect (operation, "highlights", shadows_highlights, "highlights");
      gegl_operation_meta_redirect (operation, "highlights-color-correct",
                                    shadows_highlights, "highlights-color-correct");
      gegl_operation_meta_redirect (operation, "shadows", shadows_highlights, "shadows");
      gegl_operation_meta_redirect (operation, "shadows-color-correct", shadows_highlights, "shadows-color-correct");
      gegl_operation_meta_redirect (operation, "whitepoint", shadows_highlights, "whitepoint");

      gegl_operation_meta_watch_nodes (operation, blur, self->convert_blur, shadows_highlights, NULL);
    }
}


static void
photos_operation_shadows_highlights_attach (GeglOperation *operation)
{
  PhotosOperationShadowsHighlights *self = PHOTOS_OPERATION_SHADOWS_HIGHLIGHTS (operation);

  self->input = gegl_node_get_output_proxy (operation->node, "input");
  self->output = gegl_node_get_output_proxy (operation->node, "output");
  photos_operation_shadows_highlights_setup (self);
}


static void
photos_operation_shadows_highlights_prepare (GeglOperation *operation)
{
  PhotosOperationShadowsHighlights *self = PHOTOS_OPERATION_SHADOWS_HIGHLIGHTS (operation);
  const Babl *format_blur = NULL;
  const Babl *format_input;

  format_input = gegl_operation_get_source_format (operation, "input");
  if (format_input == NULL)
    {
      format_blur = babl_format ("YaA float");
      goto out;
    }

  if (babl_format_has_alpha (format_input))
    format_blur = babl_format ("YaA float");
  else
    format_blur = babl_format ("Y float");

 out:
  g_return_if_fail (format_blur != NULL);

  if (self->format_blur != format_blur)
    {
      self->format_blur = format_blur;
      if (self->convert_blur != NULL)
        gegl_node_set (self->convert_blur, "format", self->format_blur, NULL);
    }
}


static void
photos_operation_shadows_highlights_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
  PhotosOperationShadowsHighlights *self = PHOTOS_OPERATION_SHADOWS_HIGHLIGHTS (object);

  switch (prop_id)
    {
    case PROP_COMPRESS:
      g_value_set_double (value, (gdouble) self->compress);
      break;

    case PROP_HIGHLIGHTS:
      g_value_set_double (value, (gdouble) self->highlights);
      break;

    case PROP_HIGHLIGHTS_COLOR_CORRECT:
      g_value_set_double (value, (gdouble) self->highlights_color_correct);
      break;

    case PROP_RADIUS:
      g_value_set_double (value, (gdouble) self->radius);
      break;

    case PROP_SHADOWS:
      g_value_set_double (value, (gdouble) self->shadows);
      break;

    case PROP_SHADOWS_COLOR_CORRECT:
      g_value_set_double (value, (gdouble) self->shadows_color_correct);
      break;

    case PROP_WHITEPOINT:
      g_value_set_double (value, (gdouble) self->whitepoint);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
photos_operation_shadows_highlights_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
  PhotosOperationShadowsHighlights *self = PHOTOS_OPERATION_SHADOWS_HIGHLIGHTS (object);
  gboolean is_nop;
  gboolean was_nop;

  was_nop = photos_operation_shadows_highlights_is_nop (self);

  switch (prop_id)
    {
    case PROP_COMPRESS:
      self->compress = (gfloat) g_value_get_double (value);
      break;

    case PROP_HIGHLIGHTS:
      self->highlights = (gfloat) g_value_get_double (value);
      break;

    case PROP_HIGHLIGHTS_COLOR_CORRECT:
      self->highlights_color_correct = (gfloat) g_value_get_double (value);
      break;

    case PROP_RADIUS:
      self->radius = (gfloat) g_value_get_double (value);
      break;

    case PROP_SHADOWS:
      self->shadows = (gfloat) g_value_get_double (value);
      break;

    case PROP_SHADOWS_COLOR_CORRECT:
      self->shadows_color_correct = (gfloat) g_value_get_double (value);
      break;

    case PROP_WHITEPOINT:
      self->whitepoint = (gfloat) g_value_get_double (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }

  is_nop = photos_operation_shadows_highlights_is_nop (self);
  if (GEGL_OPERATION (self)->node != NULL && is_nop != was_nop)
    photos_operation_shadows_highlights_setup (self);
}


static void
photos_operation_shadows_highlights_init (PhotosOperationShadowsHighlights *self)
{
}


static void
photos_operation_shadows_highlights_class_init (PhotosOperationShadowsHighlightsClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GeglOperationClass *operation_class = GEGL_OPERATION_CLASS (class);

  operation_class->opencl_support = FALSE;

  object_class->get_property = photos_operation_shadows_highlights_get_property;
  object_class->set_property = photos_operation_shadows_highlights_set_property;
  operation_class->attach = photos_operation_shadows_highlights_attach;
  operation_class->prepare = photos_operation_shadows_highlights_prepare;

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
                                   PROP_RADIUS,
                                   g_param_spec_double ("radius",
                                                        "Radius",
                                                        "Spatial extent",
                                                        0.1,
                                                        1500.0,
                                                        100.0,
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
                                 "name", "photos:shadows-highlights",
                                 "title", "Shadows Highlights",
                                 "description", "Adjust shadows and highlights",
                                 "categories", "light",
                                 "license", "GPL3+",
                                 NULL);
}
