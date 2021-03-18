/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2015 – 2021 Red Hat, Inc.
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

#include <math.h>

#include <gio/gio.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "photos-tool.h"
#include "photos-tool-colors.h"
#include "photos-utils.h"


struct _PhotosToolColors
{
  PhotosTool parent_instance;
  GAction *blacks_exposure;
  GAction *contrast;
  GAction *saturation;
  GAction *shadows_highlights;
  GtkWidget *blacks_scale;
  GtkWidget *contrast_scale;
  GtkWidget *exposure_scale;
  GtkWidget *grid;
  GtkWidget *highlights_scale;
  GtkWidget *saturation_scale;
  GtkWidget *shadows_scale;
  guint blacks_exposure_value_changed_id;
  guint contrast_value_changed_id;
  guint saturation_value_changed_id;
  guint shadows_highlights_value_changed_id;
};


G_DEFINE_TYPE_WITH_CODE (PhotosToolColors, photos_tool_colors, PHOTOS_TYPE_TOOL,
                         photos_utils_ensure_extension_points ();
                         g_io_extension_point_implement (PHOTOS_TOOL_EXTENSION_POINT_NAME,
                                                         g_define_type_id,
                                                         "colors",
                                                         200));


static const gdouble BLACKS_DEFAULT = 0.0;
static const gdouble BLACKS_MAXIMUM = 0.1;
static const gdouble BLACKS_MINIMUM = -0.1;
static const gdouble BLACKS_STEP = 0.01;
static const gdouble CONTRAST_DEFAULT = 0.0;
static const gdouble CONTRAST_MAXIMUM = 1.0;
static const gdouble CONTRAST_MINIMUM = -1.0;
static const gdouble CONTRAST_STEP = 0.05;
static const gdouble EXPOSURE_DEFAULT = 0.0;
static const gdouble EXPOSURE_MAXIMUM = 3.0;
static const gdouble EXPOSURE_MINIMUM = -3.0;
static const gdouble EXPOSURE_STEP = 0.3;
static const gdouble HIGHLIGHTS_DEFAULT = -1.0;
static const gdouble HIGHLIGHTS_MAXIMUM = -1.0;
static const gdouble HIGHLIGHTS_MINIMUM = -16.0;
static const gdouble HIGHLIGHTS_STEP = 0.5;
static const gdouble SATURATION_DEFAULT = 1.0;
static const gdouble SATURATION_MAXIMUM = 2.0;
static const gdouble SATURATION_MINIMUM = 0.0;
static const gdouble SATURATION_STEP = 0.1;
static const gdouble SHADOWS_DEFAULT = 1.0;
static const gdouble SHADOWS_MAXIMUM = 16.0;
static const gdouble SHADOWS_MINIMUM = 1.0;
static const gdouble SHADOWS_STEP = 0.5;


static gboolean
photos_tool_colors_blacks_exposure_value_changed_timeout (gpointer user_data)
{
  PhotosToolColors *self = PHOTOS_TOOL_COLORS (user_data);
  GVariantBuilder parameter;
  gdouble blacks;
  gdouble exposure;

  blacks = gtk_range_get_value (GTK_RANGE (self->blacks_scale));
  exposure = gtk_range_get_value (GTK_RANGE (self->exposure_scale));

  g_variant_builder_init (&parameter, G_VARIANT_TYPE ("a{sd}"));
  g_variant_builder_add (&parameter, "{sd}", "blacks", blacks);
  g_variant_builder_add (&parameter, "{sd}", "exposure", exposure);
  g_action_activate (self->blacks_exposure, g_variant_builder_end (&parameter));

  self->blacks_exposure_value_changed_id = 0;
  return G_SOURCE_REMOVE;
}


static gboolean
photos_tool_colors_contrast_value_changed_timeout (gpointer user_data)
{
  PhotosToolColors *self = PHOTOS_TOOL_COLORS (user_data);
  GVariant *parameter;
  gdouble contrast;
  gdouble contrast_real;

  contrast = gtk_range_get_value (GTK_RANGE (self->contrast_scale));
  contrast_real = pow (2.0, contrast);
  parameter = g_variant_new_double (contrast_real);
  g_action_activate (self->contrast, parameter);

  self->contrast_value_changed_id = 0;
  return G_SOURCE_REMOVE;
}


static gboolean
photos_tool_colors_saturation_value_changed_timeout (gpointer user_data)
{
  PhotosToolColors *self = PHOTOS_TOOL_COLORS (user_data);
  GVariant *parameter;
  gdouble value;

  value = gtk_range_get_value (GTK_RANGE (self->saturation_scale));
  parameter = g_variant_new_double (value);
  g_action_activate (self->saturation, parameter);

  self->saturation_value_changed_id = 0;
  return G_SOURCE_REMOVE;
}


static gboolean
photos_tool_colors_shadows_highlights_value_changed_timeout (gpointer user_data)
{
  PhotosToolColors *self = PHOTOS_TOOL_COLORS (user_data);
  GVariantBuilder parameter;
  GVariantType *parameter_type;
  gdouble shadows;
  gdouble shadows_real;
  gdouble highlights;
  gdouble highlights_real;

  highlights = gtk_range_get_value (GTK_RANGE (self->highlights_scale));
  shadows = gtk_range_get_value (GTK_RANGE (self->shadows_scale));

  highlights_real = -25.0 * log2 (-highlights);
  shadows_real = 25.0 * log2 (shadows);
  parameter_type = g_variant_type_new ("a{sd}");
  g_variant_builder_init (&parameter, parameter_type);
  g_variant_builder_add (&parameter, "{sd}", "highlights", highlights_real);
  g_variant_builder_add (&parameter, "{sd}", "shadows", shadows_real);
  g_action_activate (self->shadows_highlights, g_variant_builder_end (&parameter));

  g_variant_type_free (parameter_type);
  self->shadows_highlights_value_changed_id = 0;
  return G_SOURCE_REMOVE;
}


static void
photos_tool_colors_blacks_exposure_value_changed (PhotosToolColors *self)
{
  if (self->blacks_exposure_value_changed_id != 0)
    g_source_remove (self->blacks_exposure_value_changed_id);

  self->blacks_exposure_value_changed_id = g_timeout_add (150,
                                                          photos_tool_colors_blacks_exposure_value_changed_timeout,
                                                          self);
}


static void
photos_tool_colors_contrast_value_changed (PhotosToolColors *self)
{
  if (self->contrast_value_changed_id != 0)
    g_source_remove (self->contrast_value_changed_id);

  self->contrast_value_changed_id = g_timeout_add (150, photos_tool_colors_contrast_value_changed_timeout, self);
}


static void
photos_tool_colors_saturation_value_changed (PhotosToolColors *self)
{
  if (self->saturation_value_changed_id != 0)
    g_source_remove (self->saturation_value_changed_id);

  self->saturation_value_changed_id = g_timeout_add (150,
                                                     photos_tool_colors_saturation_value_changed_timeout,
                                                     self);
}


static void
photos_tool_colors_shadows_highlights_value_changed (PhotosToolColors *self)
{
  if (self->shadows_highlights_value_changed_id != 0)
    g_source_remove (self->shadows_highlights_value_changed_id);

  self->shadows_highlights_value_changed_id
    = g_timeout_add (150,
                     photos_tool_colors_shadows_highlights_value_changed_timeout,
                     self);
}


static void
photos_tool_colors_activate (PhotosTool *tool, PhotosBaseItem *item, PhotosImageView *view)
{
  PhotosToolColors *self = PHOTOS_TOOL_COLORS (tool);
  gdouble blacks;
  gdouble contrast;
  gdouble contrast_real;
  gdouble exposure;
  gdouble highlights;
  gdouble highlights_real;
  gdouble saturation;
  gdouble shadows;
  gdouble shadows_real;

  if (photos_base_item_operation_get (item,
                                      "gegl:brightness-contrast",
                                      "contrast", &contrast_real,
                                      NULL))
    {
      contrast = log2 (contrast_real);
    }
  else
    {
      contrast = CONTRAST_DEFAULT;
    }

  if (!photos_base_item_operation_get (item, "gegl:exposure", "black-level", &blacks, "exposure", &exposure, NULL))
    {
      blacks = BLACKS_DEFAULT;
      exposure = EXPOSURE_DEFAULT;
    }

  if (!photos_base_item_operation_get (item, "photos:saturation", "scale", &saturation, NULL))
    saturation = SATURATION_DEFAULT;

  if (photos_base_item_operation_get (item,
                                      "gegl:shadows-highlights",
                                      "highlights", &highlights_real,
                                      "shadows", &shadows_real,
                                      NULL))
    {
      highlights = -1.0 * pow (2.0, highlights_real / -25.0);
      shadows = pow (2.0, shadows_real / 25.0);
    }
  else
    {
      shadows = SHADOWS_DEFAULT;
      highlights = HIGHLIGHTS_DEFAULT;
    }

  blacks = CLAMP (blacks, BLACKS_MINIMUM, BLACKS_MAXIMUM);
  contrast = CLAMP (contrast, CONTRAST_MINIMUM, CONTRAST_MAXIMUM);
  exposure = CLAMP (exposure, EXPOSURE_MINIMUM, EXPOSURE_MAXIMUM);
  highlights = CLAMP (highlights, HIGHLIGHTS_MINIMUM, HIGHLIGHTS_MAXIMUM);
  saturation = CLAMP (saturation, SATURATION_MINIMUM, SATURATION_MAXIMUM);
  shadows = CLAMP (shadows, SHADOWS_MINIMUM, SHADOWS_MAXIMUM);

  g_signal_handlers_block_by_func (self->blacks_scale, photos_tool_colors_blacks_exposure_value_changed, self);
  g_signal_handlers_block_by_func (self->contrast_scale,
                                   photos_tool_colors_contrast_value_changed,
                                   self);
  g_signal_handlers_block_by_func (self->exposure_scale, photos_tool_colors_blacks_exposure_value_changed, self);
  g_signal_handlers_block_by_func (self->highlights_scale, photos_tool_colors_shadows_highlights_value_changed, self);
  g_signal_handlers_block_by_func (self->saturation_scale, photos_tool_colors_saturation_value_changed, self);
  g_signal_handlers_block_by_func (self->shadows_scale, photos_tool_colors_shadows_highlights_value_changed, self);
  gtk_range_set_value (GTK_RANGE (self->blacks_scale), blacks);
  gtk_range_set_value (GTK_RANGE (self->contrast_scale), contrast);
  gtk_range_set_value (GTK_RANGE (self->exposure_scale), exposure);
  gtk_range_set_value (GTK_RANGE (self->highlights_scale), highlights);
  gtk_range_set_value (GTK_RANGE (self->saturation_scale), saturation);
  gtk_range_set_value (GTK_RANGE (self->shadows_scale), shadows);
  g_signal_handlers_unblock_by_func (self->blacks_scale, photos_tool_colors_blacks_exposure_value_changed, self);
  g_signal_handlers_unblock_by_func (self->contrast_scale,
                                     photos_tool_colors_contrast_value_changed,
                                     self);
  g_signal_handlers_unblock_by_func (self->exposure_scale, photos_tool_colors_blacks_exposure_value_changed, self);
  g_signal_handlers_unblock_by_func (self->highlights_scale, photos_tool_colors_shadows_highlights_value_changed, self);
  g_signal_handlers_unblock_by_func (self->saturation_scale, photos_tool_colors_saturation_value_changed, self);
  g_signal_handlers_unblock_by_func (self->shadows_scale, photos_tool_colors_shadows_highlights_value_changed, self);

  g_signal_emit_by_name (self, "activated");
}


static GtkWidget *
photos_tool_colors_get_widget (PhotosTool *tool)
{
  PhotosToolColors *self = PHOTOS_TOOL_COLORS (tool);
  return self->grid;
}


static void
photos_tool_colors_dispose (GObject *object)
{
  PhotosToolColors *self = PHOTOS_TOOL_COLORS (object);

  g_clear_object (&self->grid);

  G_OBJECT_CLASS (photos_tool_colors_parent_class)->dispose (object);
}


static void
photos_tool_colors_finalize (GObject *object)
{
  PhotosToolColors *self = PHOTOS_TOOL_COLORS (object);

  if (self->blacks_exposure_value_changed_id != 0)
    g_source_remove (self->blacks_exposure_value_changed_id);

  if (self->contrast_value_changed_id != 0)
    g_source_remove (self->contrast_value_changed_id);

  if (self->saturation_value_changed_id != 0)
    g_source_remove (self->saturation_value_changed_id);

  if (self->shadows_highlights_value_changed_id != 0)
    g_source_remove (self->shadows_highlights_value_changed_id);

  G_OBJECT_CLASS (photos_tool_colors_parent_class)->dispose (object);
}


static void
photos_tool_colors_init (PhotosToolColors *self)
{
  GApplication *app;
  GtkStyleContext *context;
  GtkWidget *box;
  GtkWidget *label;

  app = g_application_get_default ();
  self->blacks_exposure = g_action_map_lookup_action (G_ACTION_MAP (app), "blacks-exposure-current");
  self->contrast = g_action_map_lookup_action (G_ACTION_MAP (app), "contrast-current");
  self->saturation = g_action_map_lookup_action (G_ACTION_MAP (app), "saturation-current");
  self->shadows_highlights = g_action_map_lookup_action (G_ACTION_MAP (app), "shadows-highlights-current");

  self->grid = g_object_ref_sink (gtk_grid_new ());
  gtk_orientable_set_orientation (GTK_ORIENTABLE (self->grid), GTK_ORIENTATION_VERTICAL);
  gtk_grid_set_row_spacing (GTK_GRID (self->grid), 12);

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 3);
  gtk_container_add (GTK_CONTAINER (self->grid), box);

  label = gtk_label_new (_("Exposure"));
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  context = gtk_widget_get_style_context (label);
  gtk_style_context_add_class (context, "dim-label");
  gtk_container_add (GTK_CONTAINER (box), label);

  self->exposure_scale = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL,
                                                   EXPOSURE_MINIMUM,
                                                   EXPOSURE_MAXIMUM,
                                                   EXPOSURE_STEP);
  gtk_widget_set_hexpand (self->exposure_scale, TRUE);
  gtk_scale_add_mark (GTK_SCALE (self->exposure_scale), EXPOSURE_DEFAULT, GTK_POS_BOTTOM, NULL);
  gtk_scale_set_draw_value (GTK_SCALE (self->exposure_scale), FALSE);
  gtk_container_add (GTK_CONTAINER (box), self->exposure_scale);
  g_signal_connect_swapped (self->exposure_scale,
                            "value-changed",
                            G_CALLBACK (photos_tool_colors_blacks_exposure_value_changed),
                            self);
  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 3);
  gtk_container_add (GTK_CONTAINER (self->grid), box);

  label = gtk_label_new (_("Contrast"));
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  context = gtk_widget_get_style_context (label);
  gtk_style_context_add_class (context, "dim-label");
  gtk_container_add (GTK_CONTAINER (box), label);

  self->contrast_scale = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL,
                                                   CONTRAST_MINIMUM,
                                                   CONTRAST_MAXIMUM,
                                                   CONTRAST_STEP);
  gtk_widget_set_hexpand (self->contrast_scale, TRUE);
  gtk_scale_add_mark (GTK_SCALE (self->contrast_scale), CONTRAST_DEFAULT, GTK_POS_BOTTOM, NULL);
  gtk_scale_set_draw_value (GTK_SCALE (self->contrast_scale), FALSE);
  gtk_container_add (GTK_CONTAINER (box), self->contrast_scale);
  g_signal_connect_swapped (self->contrast_scale,
                            "value-changed",
                            G_CALLBACK (photos_tool_colors_contrast_value_changed),
                            self);
  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 3);
  gtk_container_add (GTK_CONTAINER (self->grid), box);

  label = gtk_label_new (_("Blacks"));
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  context = gtk_widget_get_style_context (label);
  gtk_style_context_add_class (context, "dim-label");
  gtk_container_add (GTK_CONTAINER (box), label);

  self->blacks_scale = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL,
                                                 BLACKS_MINIMUM,
                                                 BLACKS_MAXIMUM,
                                                 BLACKS_STEP);
  gtk_widget_set_hexpand (self->blacks_scale, TRUE);
  gtk_scale_add_mark (GTK_SCALE (self->blacks_scale), CONTRAST_DEFAULT, GTK_POS_BOTTOM, NULL);
  gtk_scale_set_draw_value (GTK_SCALE (self->blacks_scale), FALSE);
  gtk_container_add (GTK_CONTAINER (box), self->blacks_scale);
  g_signal_connect_swapped (self->blacks_scale,
                            "value-changed",
                            G_CALLBACK (photos_tool_colors_blacks_exposure_value_changed),
                            self);
  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 3);
  gtk_container_add (GTK_CONTAINER (self->grid), box);

  label = gtk_label_new (_("Saturation"));
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  context = gtk_widget_get_style_context (label);
  gtk_style_context_add_class (context, "dim-label");
  gtk_container_add (GTK_CONTAINER (box), label);

  self->saturation_scale = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL,
                                                     SATURATION_MINIMUM,
                                                     SATURATION_MAXIMUM,
                                                     SATURATION_STEP);
  gtk_widget_set_hexpand (self->saturation_scale, TRUE);
  gtk_scale_add_mark (GTK_SCALE (self->saturation_scale), SATURATION_DEFAULT, GTK_POS_BOTTOM, NULL);
  gtk_scale_set_draw_value (GTK_SCALE (self->saturation_scale), FALSE);
  gtk_container_add (GTK_CONTAINER (box), self->saturation_scale);
  g_signal_connect_swapped (self->saturation_scale,
                            "value-changed",
                            G_CALLBACK (photos_tool_colors_saturation_value_changed),
                            self);

  label = gtk_label_new (_("Shadows"));
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  context = gtk_widget_get_style_context (label);
  gtk_style_context_add_class (context, "dim-label");
  gtk_container_add (GTK_CONTAINER (box), label);

  self->shadows_scale = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL,
                                                  SHADOWS_MINIMUM,
                                                  SHADOWS_MAXIMUM,
                                                  SHADOWS_STEP);
  gtk_widget_set_hexpand (self->shadows_scale, TRUE);
  gtk_scale_add_mark (GTK_SCALE (self->shadows_scale), SHADOWS_DEFAULT, GTK_POS_BOTTOM, NULL);
  gtk_scale_set_draw_value (GTK_SCALE (self->shadows_scale), FALSE);
  gtk_container_add (GTK_CONTAINER (box), self->shadows_scale);
  g_signal_connect_swapped (self->shadows_scale,
                            "value-changed",
                            G_CALLBACK (photos_tool_colors_shadows_highlights_value_changed),
                            self);

  label = gtk_label_new (_("Highlights"));
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  context = gtk_widget_get_style_context (label);
  gtk_style_context_add_class (context, "dim-label");
  gtk_container_add (GTK_CONTAINER (box), label);

  self->highlights_scale = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL,
                                                     HIGHLIGHTS_MINIMUM,
                                                     HIGHLIGHTS_MAXIMUM,
                                                     HIGHLIGHTS_STEP);
  gtk_widget_set_hexpand (self->highlights_scale, TRUE);
  gtk_scale_add_mark (GTK_SCALE (self->highlights_scale), SHADOWS_DEFAULT, GTK_POS_BOTTOM, NULL);
  gtk_scale_set_draw_value (GTK_SCALE (self->highlights_scale), FALSE);
  gtk_container_add (GTK_CONTAINER (box), self->highlights_scale);
  g_signal_connect_swapped (self->highlights_scale,
                            "value-changed",
                            G_CALLBACK (photos_tool_colors_shadows_highlights_value_changed),
                            self);

}


static void
photos_tool_colors_class_init (PhotosToolColorsClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  PhotosToolClass *tool_class = PHOTOS_TOOL_CLASS (class);

  tool_class->icon_name = "image-adjust-color-symbolic";
  tool_class->name = _("Colors");

  object_class->dispose = photos_tool_colors_dispose;
  object_class->finalize = photos_tool_colors_finalize;
  tool_class->activate = photos_tool_colors_activate;
  tool_class->get_widget = photos_tool_colors_get_widget;
}
