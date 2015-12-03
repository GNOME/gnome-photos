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
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "photos-application.h"
#include "photos-icons.h"
#include "photos-operation-insta-common.h"
#include "photos-tool.h"
#include "photos-tool-filter-button.h"
#include "photos-tool-filters.h"
#include "photos-utils.h"


struct _PhotosToolFilters
{
  PhotosTool parent_instance;
  GList *buttons;
  GtkWidget *grid;
  PhotosBaseItem *item;
  guint create_preview_id;
};

struct _PhotosToolFiltersClass
{
  PhotosToolClass parent_class;
};


G_DEFINE_TYPE_WITH_CODE (PhotosToolFilters, photos_tool_filters, PHOTOS_TYPE_TOOL,
                         photos_utils_ensure_extension_points ();
                         g_io_extension_point_implement (PHOTOS_TOOL_EXTENSION_POINT_NAME,
                                                         g_define_type_id,
                                                         "filters",
                                                         500));


static gboolean
photos_tool_filters_create_preview_idle (gpointer user_data)
{
  PhotosToolFilters *self = PHOTOS_TOOL_FILTERS (user_data);
  GApplication *app;
  GtkWidget *button;
  GtkWidget *image;
  GVariant *target_value;
  PhotosOperationInstaPreset preset;
  cairo_surface_t *surface;
  gboolean ret_val = G_SOURCE_CONTINUE;
  gint scale;

  g_return_val_if_fail (self->buttons != NULL, G_SOURCE_REMOVE);

  app = g_application_get_default ();
  scale = photos_application_get_scale_factor (PHOTOS_APPLICATION (app));

  button = GTK_WIDGET (self->buttons->data);
  target_value = gtk_actionable_get_action_target_value (GTK_ACTIONABLE (button));
  preset = (PhotosOperationInstaPreset) g_variant_get_int16 (target_value);

  surface = photos_base_item_create_preview (self->item,
                                             96,
                                             scale,
                                             "photos:insta-filter",
                                             "preset", preset,
                                             NULL);
  image = gtk_image_new_from_surface (surface);
  photos_tool_filter_button_set_image (PHOTOS_TOOL_FILTER_BUTTON (button), image);

  gtk_widget_show (image);

  self->buttons = g_list_remove_link (self->buttons, self->buttons);
  if (self->buttons == NULL)
    {
      self->create_preview_id = 0;
      ret_val = G_SOURCE_REMOVE;
    }

  cairo_surface_destroy (surface);
  return ret_val;
}


static void
photos_tool_filters_activate (PhotosTool *tool, PhotosBaseItem *item, GeglGtkView *view)
{
  PhotosToolFilters *self = PHOTOS_TOOL_FILTERS (tool);

  if (self->buttons == NULL || self->create_preview_id != 0)
    return;

  g_clear_object (&self->item);
  self->item = g_object_ref (item);

  self->create_preview_id = g_idle_add_full (G_PRIORITY_LOW, photos_tool_filters_create_preview_idle, self, NULL);
}


static GtkWidget *
photos_tool_filters_get_widget (PhotosTool *tool)
{
  PhotosToolFilters *self = PHOTOS_TOOL_FILTERS (tool);
  return self->grid;
}


static void
photos_tool_filters_dispose (GObject *object)
{
  PhotosToolFilters *self = PHOTOS_TOOL_FILTERS (object);

  if (self->create_preview_id != 0)
    {
      g_source_remove (self->create_preview_id);
      self->create_preview_id = 0;
    }

  g_clear_object (&self->grid);
  g_clear_object (&self->item);

  G_OBJECT_CLASS (photos_tool_filters_parent_class)->dispose (object);
}


static void
photos_tool_filters_finalize (GObject *object)
{
  PhotosToolFilters *self = PHOTOS_TOOL_FILTERS (object);

  g_list_free (self->buttons);

  G_OBJECT_CLASS (photos_tool_filters_parent_class)->finalize (object);
}


static void
photos_tool_filters_init (PhotosToolFilters *self)
{
  GtkWidget *button;
  GtkWidget *group = NULL;
  guint row = 0;

  self->grid = g_object_ref_sink (gtk_grid_new ());

  button = photos_tool_filter_button_new (group, _("None"));
  gtk_actionable_set_action_name (GTK_ACTIONABLE (button), "app.insta-current");
  gtk_actionable_set_action_target (GTK_ACTIONABLE (button), "n", (gint16) PHOTOS_OPERATION_INSTA_PRESET_NONE);
  gtk_grid_attach (GTK_GRID (self->grid), button, 0, row, 1, 1);
  group = photos_tool_filter_button_get_group (PHOTOS_TOOL_FILTER_BUTTON (button));
  self->buttons = g_list_prepend (self->buttons, button);

  button = photos_tool_filter_button_new (group, _("1977"));
  gtk_actionable_set_action_name (GTK_ACTIONABLE (button), "app.insta-current");
  gtk_actionable_set_action_target (GTK_ACTIONABLE (button), "n", (gint16) PHOTOS_OPERATION_INSTA_PRESET_1977);
  gtk_grid_attach (GTK_GRID (self->grid), button, 1, row, 1, 1);
  self->buttons = g_list_prepend (self->buttons, button);
  row++;

  button = photos_tool_filter_button_new (group, _("Brannan"));
  gtk_actionable_set_action_name (GTK_ACTIONABLE (button), "app.insta-current");
  gtk_actionable_set_action_target (GTK_ACTIONABLE (button), "n", (gint16) PHOTOS_OPERATION_INSTA_PRESET_BRANNAN);
  gtk_grid_attach (GTK_GRID (self->grid), button, 0, row, 1, 1);
  self->buttons = g_list_prepend (self->buttons, button);

  button = photos_tool_filter_button_new (group, _("Gotham"));
  gtk_actionable_set_action_name (GTK_ACTIONABLE (button), "app.insta-current");
  gtk_actionable_set_action_target (GTK_ACTIONABLE (button), "n", (gint16) PHOTOS_OPERATION_INSTA_PRESET_GOTHAM);
  gtk_grid_attach (GTK_GRID (self->grid), button, 1, row, 1, 1);
  self->buttons = g_list_prepend (self->buttons, button);
  row++;

  button = photos_tool_filter_button_new (group, _("Gray"));
  gtk_actionable_set_action_name (GTK_ACTIONABLE (button), "app.insta-current");
  gtk_actionable_set_action_target (GTK_ACTIONABLE (button), "n", (gint16) PHOTOS_OPERATION_INSTA_PRESET_GRAY);
  gtk_grid_attach (GTK_GRID (self->grid), button, 0, row, 1, 1);
  self->buttons = g_list_prepend (self->buttons, button);

  button = photos_tool_filter_button_new (group, _("Nashville"));
  gtk_actionable_set_action_name (GTK_ACTIONABLE (button), "app.insta-current");
  gtk_actionable_set_action_target (GTK_ACTIONABLE (button), "n", (gint16) PHOTOS_OPERATION_INSTA_PRESET_NASHVILLE);
  gtk_grid_attach (GTK_GRID (self->grid), button, 1, row, 1, 1);
  self->buttons = g_list_prepend (self->buttons, button);
  row++;

  self->buttons = g_list_reverse (self->buttons);
}


static void
photos_tool_filters_class_init (PhotosToolFiltersClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  PhotosToolClass *tool_class = PHOTOS_TOOL_CLASS (class);

  tool_class->icon_name = PHOTOS_ICON_IMAGE_FILTER_SYMBOLIC;
  tool_class->name = _("Filters");

  object_class->dispose = photos_tool_filters_dispose;
  object_class->finalize = photos_tool_filters_finalize;
  tool_class->activate = photos_tool_filters_activate;
  tool_class->get_widget = photos_tool_filters_get_widget;
}
