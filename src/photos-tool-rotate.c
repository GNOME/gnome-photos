/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright (c) 2022 Cedric Bellegarde <cedric.bellegarde@adishatz.org>
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

#include <gio/gio.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "photos-tool.h"
#include "photos-tool-rotate.h"
#include "photos-utils.h"


struct _PhotosToolRotate
{
  PhotosTool parent_instance;
  GAction *rotate;
  GtkWidget *grid;
  gint current;
};


G_DEFINE_TYPE_WITH_CODE (PhotosToolRotate, photos_tool_rotate, PHOTOS_TYPE_TOOL,
                         photos_utils_ensure_extension_points ();
                         g_io_extension_point_implement (PHOTOS_TOOL_EXTENSION_POINT_NAME,
                                                         g_define_type_id,
                                                         "rotate",
                                                         300));

static void
photos_tool_rotate_activate (PhotosTool *tool, PhotosBaseItem *item, PhotosImageView *view)
{
}

static void
photos_tool_rotate_left_clicked (GtkButton *button, gpointer user_data)
{
  PhotosToolRotate *self = PHOTOS_TOOL_ROTATE (user_data);
  GVariant *parameter;

  self->current += 90;
  parameter = g_variant_new_int16 (self->current);
  g_action_activate (self->rotate, parameter);
}

static void
photos_tool_rotate_right_clicked (GtkButton *button, gpointer user_data)
{
  PhotosToolRotate *self = PHOTOS_TOOL_ROTATE (user_data);
  GVariant *parameter;

  self->current -= 90;
  parameter = g_variant_new_int16 (self->current);
  g_action_activate (self->rotate, parameter);
}


static GtkWidget *
photos_tool_rotate_get_widget (PhotosTool *tool)
{
  PhotosToolRotate *self = PHOTOS_TOOL_ROTATE (tool);
  return self->grid;
}


static void
photos_tool_rotate_dispose (GObject *object)
{
  PhotosToolRotate *self = PHOTOS_TOOL_ROTATE (object);

  g_clear_object (&self->grid);

  G_OBJECT_CLASS (photos_tool_rotate_parent_class)->dispose (object);
}


static void
photos_tool_rotate_finalize (GObject *object)
{
  G_OBJECT_CLASS (photos_tool_rotate_parent_class)->dispose (object);
}


static void
photos_tool_rotate_init (PhotosToolRotate *self)
{
  GApplication *app;
  GtkWidget *box;
  GtkWidget *button;
  GtkStyleContext *context;

  self->current = 0;

  app = g_application_get_default ();
  self->rotate = g_action_map_lookup_action (G_ACTION_MAP (app), "rotate-current");

  self->grid = g_object_ref_sink (gtk_grid_new ());
  gtk_orientable_set_orientation (GTK_ORIENTABLE (self->grid), GTK_ORIENTATION_VERTICAL);
  gtk_grid_set_row_spacing (GTK_GRID (self->grid), 12);

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  context = gtk_widget_get_style_context (box);
  gtk_style_context_add_class (context, "linked");
  gtk_container_add (GTK_CONTAINER (self->grid), box);

  button = gtk_button_new_with_label (_("Rotate left"));
  g_signal_connect (G_OBJECT (button), "clicked", G_CALLBACK (photos_tool_rotate_left_clicked), self);
  gtk_container_add (GTK_CONTAINER (box), button);

  button = gtk_button_new_with_label (_("Rotate right"));
  g_signal_connect (G_OBJECT (button), "clicked", G_CALLBACK (photos_tool_rotate_right_clicked), self);
  gtk_container_add (GTK_CONTAINER (box), button);
}


static void
photos_tool_rotate_class_init (PhotosToolRotateClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  PhotosToolClass *tool_class = PHOTOS_TOOL_CLASS (class);

  tool_class->icon_name = "object-rotate-right-symbolic";
  tool_class->name = _("Rotate");

  object_class->dispose = photos_tool_rotate_dispose;
  object_class->finalize = photos_tool_rotate_finalize;
  tool_class->activate = photos_tool_rotate_activate;
  tool_class->get_widget = photos_tool_rotate_get_widget;
}

