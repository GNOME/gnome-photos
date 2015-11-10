/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2015 Red Hat, Inc.
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

#include <gio/gio.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "photos-icons.h"
#include "photos-tool.h"
#include "photos-tool-colors.h"
#include "photos-utils.h"


struct _PhotosToolColors
{
  PhotosTool parent_instance;
  GAction *brightness_contrast;
  GtkWidget *box;
  GtkWidget *brightness_scale;
  GtkWidget *contrast_scale;
  guint brightness_contrast_value_changed_id;
};

struct _PhotosToolColorsClass
{
  PhotosToolClass parent_class;
};


G_DEFINE_TYPE_WITH_CODE (PhotosToolColors, photos_tool_colors, PHOTOS_TYPE_TOOL,
                         photos_utils_ensure_extension_points ();
                         g_io_extension_point_implement (PHOTOS_TOOL_EXTENSION_POINT_NAME,
                                                         g_define_type_id,
                                                         "colors",
                                                         200));


static const gdouble BRIGHTNESS_DEFAULT = 0.0;
static const gdouble BRIGHTNESS_MAXIMUM = 1.0;
static const gdouble BRIGHTNESS_MINIMUM = -1.0;
static const gdouble BRIGHTNESS_STEP = 0.1;
static const gdouble CONTRAST_DEFAULT = 1.0;
static const gdouble CONTRAST_MAXIMUM = 2.0;
static const gdouble CONTRAST_MINIMUM = 0.0;
static const gdouble CONTRAST_STEP = 0.1;


static gboolean
photos_tool_colors_brightness_contrast_value_changed_timeout (gpointer user_data)
{
  PhotosToolColors *self = PHOTOS_TOOL_COLORS (user_data);
  GVariantBuilder parameter;
  GVariantType *parameter_type;
  gdouble brightness;
  gdouble contrast;

  brightness = gtk_range_get_value (GTK_RANGE (self->brightness_scale));
  contrast = gtk_range_get_value (GTK_RANGE (self->contrast_scale));

  parameter_type = g_variant_type_new ("a{sd}");
  g_variant_builder_init (&parameter, parameter_type);
  g_variant_builder_add (&parameter, "{sd}", "brightness", brightness);
  g_variant_builder_add (&parameter, "{sd}", "contrast", contrast);
  g_action_activate (self->brightness_contrast, g_variant_builder_end (&parameter));

  self->brightness_contrast_value_changed_id = 0;
  return G_SOURCE_REMOVE;
}


static void
photos_tool_colors_brightness_contrast_value_changed (PhotosToolColors *self)
{
  if (self->brightness_contrast_value_changed_id != 0)
    g_source_remove (self->brightness_contrast_value_changed_id);

  self->brightness_contrast_value_changed_id
    = g_timeout_add (150,
                     photos_tool_colors_brightness_contrast_value_changed_timeout,
                     self);
}


static void
photos_tool_colors_activate (PhotosTool *tool, PhotosBaseItem *item, GeglGtkView *view)
{
  PhotosToolColors *self = PHOTOS_TOOL_COLORS (tool);
  gdouble brightness;
  gdouble contrast;

  if (!photos_base_item_operation_get (item,
                                       "gegl:brightness-contrast",
                                       "brightness", &brightness,
                                       "contrast", &contrast,
                                       NULL))
    {
      brightness = BRIGHTNESS_DEFAULT;
      contrast = CONTRAST_DEFAULT;
    }

  brightness = CLAMP (brightness, BRIGHTNESS_MINIMUM, BRIGHTNESS_MAXIMUM);
  contrast = CLAMP (contrast, CONTRAST_MINIMUM, CONTRAST_MAXIMUM);

  g_signal_handlers_block_by_func (self->brightness_scale,
                                   photos_tool_colors_brightness_contrast_value_changed,
                                   self);
  g_signal_handlers_block_by_func (self->contrast_scale,
                                   photos_tool_colors_brightness_contrast_value_changed,
                                   self);
  gtk_range_set_value (GTK_RANGE (self->brightness_scale), brightness);
  gtk_range_set_value (GTK_RANGE (self->contrast_scale), contrast);
  g_signal_handlers_unblock_by_func (self->brightness_scale,
                                     photos_tool_colors_brightness_contrast_value_changed,
                                     self);
  g_signal_handlers_unblock_by_func (self->contrast_scale,
                                     photos_tool_colors_brightness_contrast_value_changed,
                                     self);
}


static GtkWidget *
photos_tool_colors_get_widget (PhotosTool *tool)
{
  PhotosToolColors *self = PHOTOS_TOOL_COLORS (tool);
  return self->box;
}


static void
photos_tool_colors_dispose (GObject *object)
{
  PhotosToolColors *self = PHOTOS_TOOL_COLORS (object);

  g_clear_object (&self->box);

  G_OBJECT_CLASS (photos_tool_colors_parent_class)->dispose (object);
}


static void
photos_tool_colors_finalize (GObject *object)
{
  PhotosToolColors *self = PHOTOS_TOOL_COLORS (object);

  if (self->brightness_contrast_value_changed_id != 0)
    g_source_remove (self->brightness_contrast_value_changed_id);

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
  self->brightness_contrast = g_action_map_lookup_action (G_ACTION_MAP (app), "brightness-contrast-current");

  /* We really need a GtkBox here. A GtkGrid won't work because it
   * doesn't expand the children to fill the full width of the
   * palette.
   */
  self->box = g_object_ref_sink (gtk_box_new (GTK_ORIENTATION_VERTICAL, 12));

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 3);
  gtk_container_add (GTK_CONTAINER (self->box), box);

  label = gtk_label_new (_("Brightness"));
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  context = gtk_widget_get_style_context (label);
  gtk_style_context_add_class (context, "dim-label");
  gtk_container_add (GTK_CONTAINER (box), label);

  self->brightness_scale = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL,
                                                     BRIGHTNESS_MINIMUM,
                                                     BRIGHTNESS_MAXIMUM,
                                                     BRIGHTNESS_STEP);
  gtk_scale_add_mark (GTK_SCALE (self->brightness_scale), BRIGHTNESS_DEFAULT, GTK_POS_BOTTOM, NULL);
  gtk_scale_set_draw_value (GTK_SCALE (self->brightness_scale), FALSE);
  gtk_container_add (GTK_CONTAINER (box), self->brightness_scale);
  g_signal_connect_swapped (self->brightness_scale,
                            "value-changed",
                            G_CALLBACK (photos_tool_colors_brightness_contrast_value_changed),
                            self);

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 3);
  gtk_container_add (GTK_CONTAINER (self->box), box);

  label = gtk_label_new (_("Contrast"));
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  context = gtk_widget_get_style_context (label);
  gtk_style_context_add_class (context, "dim-label");
  gtk_container_add (GTK_CONTAINER (box), label);

  self->contrast_scale = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL,
                                                   CONTRAST_MINIMUM,
                                                   CONTRAST_MAXIMUM,
                                                   CONTRAST_STEP);
  gtk_scale_add_mark (GTK_SCALE (self->contrast_scale), CONTRAST_DEFAULT, GTK_POS_BOTTOM, NULL);
  gtk_scale_set_draw_value (GTK_SCALE (self->contrast_scale), FALSE);
  gtk_container_add (GTK_CONTAINER (box), self->contrast_scale);
  g_signal_connect_swapped (self->contrast_scale,
                            "value-changed",
                            G_CALLBACK (photos_tool_colors_brightness_contrast_value_changed),
                            self);
}


static void
photos_tool_colors_class_init (PhotosToolColorsClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  PhotosToolClass *tool_class = PHOTOS_TOOL_CLASS (class);

  tool_class->icon_name = PHOTOS_ICON_IMAGE_ADJUST_COLOR_SYMBOLIC;
  tool_class->name = _("Colors");

  object_class->dispose = photos_tool_colors_dispose;
  object_class->finalize = photos_tool_colors_finalize;
  tool_class->activate = photos_tool_colors_activate;
  tool_class->get_widget = photos_tool_colors_get_widget;
}
