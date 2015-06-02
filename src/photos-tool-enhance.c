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
#include "photos-tool-enhance.h"
#include "photos-utils.h"


struct _PhotosToolEnhance
{
  PhotosTool parent_instance;
  GAction *denoise;
  GAction *sharpen;
  GtkWidget *box;
  GtkWidget *denoise_scale;
  GtkWidget *sharpen_scale;
  guint denoise_value_changed_id;
  guint sharpen_value_changed_id;
};

struct _PhotosToolEnhanceClass
{
  PhotosToolClass parent_class;
};


G_DEFINE_TYPE_WITH_CODE (PhotosToolEnhance, photos_tool_enhance, PHOTOS_TYPE_TOOL,
                         photos_utils_ensure_extension_points ();
                         g_io_extension_point_implement (PHOTOS_TOOL_EXTENSION_POINT_NAME,
                                                         g_define_type_id,
                                                         "enhance",
                                                         300));


static const gdouble DENOISE_ITERATIONS_DEFAULT = 0.0;
static const gdouble DENOISE_ITERATIONS_MAXIMUM = 8.0;
static const gdouble DENOISE_ITERATIONS_MINIMUM = 0.0;
static const gdouble DENOISE_ITERATIONS_STEP = 0.5;
static const gdouble SHARPEN_SCALE_DEFAULT = 0.0;
static const gdouble SHARPEN_SCALE_MAXIMUM = 10.0;
static const gdouble SHARPEN_SCALE_MINIMUM = 0.0;
static const gdouble SHARPEN_SCALE_STEP = 0.5;


static gboolean
photos_tool_enhance_denoise_value_changed_timeout (gpointer user_data)
{
  PhotosToolEnhance *self = PHOTOS_TOOL_ENHANCE (user_data);
  GVariant *parameter;
  guint16 value;

  value = (guint16) gtk_range_get_value (GTK_RANGE (self->denoise_scale));
  parameter = g_variant_new_uint16 (value);
  g_action_activate (self->denoise, parameter);

  self->denoise_value_changed_id = 0;
  return G_SOURCE_REMOVE;
}


static void
photos_tool_enhance_denoise_value_changed (PhotosToolEnhance *self)
{
  if (self->denoise_value_changed_id != 0)
    g_source_remove (self->denoise_value_changed_id);

  self->denoise_value_changed_id = g_timeout_add (150, photos_tool_enhance_denoise_value_changed_timeout, self);
}


static gboolean
photos_tool_enhance_sharpen_value_changed_timeout (gpointer user_data)
{
  PhotosToolEnhance *self = PHOTOS_TOOL_ENHANCE (user_data);
  GVariant *parameter;
  gdouble value;

  value = gtk_range_get_value (GTK_RANGE (self->sharpen_scale));
  parameter = g_variant_new_double (value);
  g_action_activate (self->sharpen, parameter);

  self->sharpen_value_changed_id = 0;
  return G_SOURCE_REMOVE;
}


static void
photos_tool_enhance_sharpen_value_changed (PhotosToolEnhance *self)
{
  if (self->sharpen_value_changed_id != 0)
    g_source_remove (self->sharpen_value_changed_id);

  self->sharpen_value_changed_id = g_timeout_add (150, photos_tool_enhance_sharpen_value_changed_timeout, self);
}


static void
photos_tool_enhance_activate (PhotosTool *tool, PhotosBaseItem *item, GeglGtkView *view)
{
  PhotosToolEnhance *self = PHOTOS_TOOL_ENHANCE (tool);
  gdouble sharpen_scale;
  gint denoise_iterations;

  if (!photos_base_item_operation_get (item, "gegl:noise-reduction", "iterations", &denoise_iterations, NULL))
    denoise_iterations = DENOISE_ITERATIONS_DEFAULT;

  denoise_iterations = CLAMP (denoise_iterations, DENOISE_ITERATIONS_MINIMUM, DENOISE_ITERATIONS_MAXIMUM);

  g_signal_handlers_block_by_func (self->denoise_scale, photos_tool_enhance_denoise_value_changed, self);
  gtk_range_set_value (GTK_RANGE (self->denoise_scale), (gdouble) denoise_iterations);
  g_signal_handlers_unblock_by_func (self->denoise_scale, photos_tool_enhance_denoise_value_changed, self);

  if (!photos_base_item_operation_get (item, "gegl:unsharp-mask", "scale", &sharpen_scale, NULL))
    sharpen_scale = SHARPEN_SCALE_DEFAULT;

  sharpen_scale = CLAMP (sharpen_scale, SHARPEN_SCALE_MINIMUM, SHARPEN_SCALE_MAXIMUM);

  g_signal_handlers_block_by_func (self->sharpen_scale, photos_tool_enhance_sharpen_value_changed, self);
  gtk_range_set_value (GTK_RANGE (self->sharpen_scale), sharpen_scale);
  g_signal_handlers_unblock_by_func (self->sharpen_scale, photos_tool_enhance_sharpen_value_changed, self);
}


static GtkWidget *
photos_tool_enhance_get_widget (PhotosTool *tool)
{
  PhotosToolEnhance *self = PHOTOS_TOOL_ENHANCE (tool);
  return self->box;
}


static void
photos_tool_enhance_dispose (GObject *object)
{
  PhotosToolEnhance *self = PHOTOS_TOOL_ENHANCE (object);

  g_clear_object (&self->box);

  G_OBJECT_CLASS (photos_tool_enhance_parent_class)->dispose (object);
}


static void
photos_tool_enhance_finalize (GObject *object)
{
  PhotosToolEnhance *self = PHOTOS_TOOL_ENHANCE (object);

  if (self->denoise_value_changed_id != 0)
    g_source_remove (self->denoise_value_changed_id);

  if (self->sharpen_value_changed_id != 0)
    g_source_remove (self->sharpen_value_changed_id);

  G_OBJECT_CLASS (photos_tool_enhance_parent_class)->dispose (object);
}


static void
photos_tool_enhance_init (PhotosToolEnhance *self)
{
  GApplication *app;
  GtkStyleContext *context;
  GtkWidget *box;
  GtkWidget *label;

  app = g_application_get_default ();
  self->denoise = g_action_map_lookup_action (G_ACTION_MAP (app), "denoise-current");
  self->sharpen = g_action_map_lookup_action (G_ACTION_MAP (app), "sharpen-current");

  /* We really need a GtkBox here. A GtkGrid won't work because it
   * doesn't expand the children to fill the full width of the
   * palette.
   */
  self->box = g_object_ref_sink (gtk_box_new (GTK_ORIENTATION_VERTICAL, 12));

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 3);
  gtk_container_add (GTK_CONTAINER (self->box), box);

  label = gtk_label_new (_("Sharpen"));
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  context = gtk_widget_get_style_context (label);
  gtk_style_context_add_class (context, "dim-label");
  gtk_container_add (GTK_CONTAINER (box), label);

  self->sharpen_scale = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL,
                                                  SHARPEN_SCALE_MINIMUM,
                                                  SHARPEN_SCALE_MAXIMUM,
                                                  SHARPEN_SCALE_STEP);
  gtk_scale_set_draw_value (GTK_SCALE (self->sharpen_scale), FALSE);
  gtk_container_add (GTK_CONTAINER (box), self->sharpen_scale);
  g_signal_connect_swapped (self->sharpen_scale,
                            "value-changed",
                            G_CALLBACK (photos_tool_enhance_sharpen_value_changed),
                            self);

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 3);
  gtk_container_add (GTK_CONTAINER (self->box), box);

  label = gtk_label_new (_("Denoise"));
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  context = gtk_widget_get_style_context (label);
  gtk_style_context_add_class (context, "dim-label");
  gtk_container_add (GTK_CONTAINER (box), label);

  self->denoise_scale = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL,
                                                  DENOISE_ITERATIONS_MINIMUM,
                                                  DENOISE_ITERATIONS_MAXIMUM,
                                                  DENOISE_ITERATIONS_STEP);
  gtk_scale_set_draw_value (GTK_SCALE (self->denoise_scale), FALSE);
  gtk_container_add (GTK_CONTAINER (box), self->denoise_scale);
  g_signal_connect_swapped (self->denoise_scale,
                            "value-changed",
                            G_CALLBACK (photos_tool_enhance_denoise_value_changed),
                            self);
}


static void
photos_tool_enhance_class_init (PhotosToolEnhanceClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  PhotosToolClass *tool_class = PHOTOS_TOOL_CLASS (class);

  tool_class->icon_name = PHOTOS_ICON_IMAGE_AUTO_ADJUST_SYMBOLIC;
  tool_class->name = _("Enhance");

  object_class->dispose = photos_tool_enhance_dispose;
  object_class->finalize = photos_tool_enhance_finalize;
  tool_class->activate = photos_tool_enhance_activate;
  tool_class->get_widget = photos_tool_enhance_get_widget;
}
