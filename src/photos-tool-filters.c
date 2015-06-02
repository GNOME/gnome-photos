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

#include "photos-icons.h"
#include "photos-operation-insta-common.h"
#include "photos-tool.h"
#include "photos-tool-filters.h"
#include "photos-utils.h"


struct _PhotosToolFilters
{
  PhotosTool parent_instance;
  GtkWidget *grid;
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


static void
photos_tool_filters_activate (PhotosTool *tool, PhotosBaseItem *item, GeglGtkView *view)
{
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

  g_clear_object (&self->grid);

  G_OBJECT_CLASS (photos_tool_filters_parent_class)->dispose (object);
}


static void
photos_tool_filters_init (PhotosToolFilters *self)
{
  GtkWidget *button;
  guint row = 0;

  self->grid = g_object_ref_sink (gtk_grid_new ());

  button = gtk_button_new_with_label (_("None"));
  gtk_actionable_set_action_name (GTK_ACTIONABLE (button), "app.insta-current");
  gtk_actionable_set_action_target (GTK_ACTIONABLE (button), "n", (gint16) PHOTOS_OPERATION_INSTA_PRESET_NONE);
  gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
  gtk_grid_attach (GTK_GRID (self->grid), button, 0, row, 1, 1);

  button = gtk_button_new_with_label (_("1977"));
  gtk_actionable_set_action_name (GTK_ACTIONABLE (button), "app.insta-current");
  gtk_actionable_set_action_target (GTK_ACTIONABLE (button), "n", (gint16) PHOTOS_OPERATION_INSTA_PRESET_1977);
  gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
  gtk_grid_attach (GTK_GRID (self->grid), button, 1, row, 1, 1);
  row++;

  button = gtk_button_new_with_label (_("Brannan"));
  gtk_actionable_set_action_name (GTK_ACTIONABLE (button), "app.insta-current");
  gtk_actionable_set_action_target (GTK_ACTIONABLE (button), "n", (gint16) PHOTOS_OPERATION_INSTA_PRESET_BRANNAN);
  gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
  gtk_grid_attach (GTK_GRID (self->grid), button, 0, row, 1, 1);

  button = gtk_button_new_with_label (_("Gotham"));
  gtk_actionable_set_action_name (GTK_ACTIONABLE (button), "app.insta-current");
  gtk_actionable_set_action_target (GTK_ACTIONABLE (button), "n", (gint16) PHOTOS_OPERATION_INSTA_PRESET_GOTHAM);
  gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
  gtk_grid_attach (GTK_GRID (self->grid), button, 1, row, 1, 1);
  row++;

  button = gtk_button_new_with_label (_("Gray"));
  gtk_actionable_set_action_name (GTK_ACTIONABLE (button), "app.insta-current");
  gtk_actionable_set_action_target (GTK_ACTIONABLE (button), "n", (gint16) PHOTOS_OPERATION_INSTA_PRESET_GRAY);
  gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
  gtk_grid_attach (GTK_GRID (self->grid), button, 0, row, 1, 1);

  button = gtk_button_new_with_label (_("Nashville"));
  gtk_actionable_set_action_name (GTK_ACTIONABLE (button), "app.insta-current");
  gtk_actionable_set_action_target (GTK_ACTIONABLE (button), "n", (gint16) PHOTOS_OPERATION_INSTA_PRESET_NASHVILLE);
  gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
  gtk_grid_attach (GTK_GRID (self->grid), button, 1, row, 1, 1);
  row++;
}


static void
photos_tool_filters_class_init (PhotosToolFiltersClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  PhotosToolClass *tool_class = PHOTOS_TOOL_CLASS (class);

  tool_class->icon_name = PHOTOS_ICON_IMAGE_FILTER_SYMBOLIC;
  tool_class->name = _("Filters");

  object_class->dispose = photos_tool_filters_dispose;
  tool_class->activate = photos_tool_filters_activate;
  tool_class->get_widget = photos_tool_filters_get_widget;
}
