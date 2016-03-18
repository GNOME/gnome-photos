/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2015 – 2016 Red Hat, Inc.
 * Copyright © 2015 Umang Jain
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

#include "photos-edit-palette-row.h"


struct _PhotosEditPaletteRow
{
  GtkListBoxRow parent_instance;
  GtkSizeGroup *size_group;
  GtkWidget *details_revealer;
  GtkWidget *row_revealer;
  PhotosTool *tool;
};

struct _PhotosEditPaletteRowClass
{
  GtkListBoxRowClass parent_class;
};

enum
{
  PROP_0,
  PROP_SIZE_GROUP,
  PROP_TOOL
};


G_DEFINE_TYPE (PhotosEditPaletteRow, photos_edit_palette_row, GTK_TYPE_LIST_BOX_ROW);


static void
photos_edit_palette_row_constructed (GObject *object)
{
  PhotosEditPaletteRow *self = PHOTOS_EDIT_PALETTE_ROW (object);
  GtkWidget *grid0;
  GtkWidget *grid1;
  GtkWidget *image;
  GtkWidget *label;
  GtkWidget *tool_widget;
  const gchar *icon_name;
  const gchar *name;
  gchar *name_markup;

  G_OBJECT_CLASS (photos_edit_palette_row_parent_class)->constructed (object);

  self->row_revealer = gtk_revealer_new();
  gtk_revealer_set_transition_type (GTK_REVEALER (self->row_revealer), GTK_REVEALER_TRANSITION_TYPE_SLIDE_DOWN);
  gtk_container_add (GTK_CONTAINER (self), self->row_revealer);

  grid0 = gtk_grid_new ();
  gtk_widget_set_margin_bottom (grid0, 6);
  gtk_widget_set_margin_start (grid0, 18);
  gtk_widget_set_margin_end (grid0, 18);
  gtk_widget_set_margin_top (grid0, 6);
  gtk_orientable_set_orientation (GTK_ORIENTABLE (grid0), GTK_ORIENTATION_VERTICAL);
  gtk_container_add (GTK_CONTAINER (self->row_revealer), grid0);

  grid1 = gtk_grid_new ();
  gtk_orientable_set_orientation (GTK_ORIENTABLE (grid1), GTK_ORIENTATION_HORIZONTAL);
  gtk_grid_set_column_spacing (GTK_GRID (grid1), 12);
  gtk_container_add (GTK_CONTAINER (grid0), grid1);

  icon_name = photos_tool_get_icon_name (self->tool);
  image = gtk_image_new_from_icon_name (icon_name, GTK_ICON_SIZE_BUTTON);
  gtk_container_add (GTK_CONTAINER (grid1), image);

  name = photos_tool_get_name (self->tool);
  label = gtk_label_new (NULL);
  name_markup = g_strdup_printf ("<b>%s</b>", name);
  gtk_label_set_markup (GTK_LABEL (label), name_markup);
  gtk_container_add (GTK_CONTAINER (grid1), label);

  self->details_revealer = gtk_revealer_new ();
  gtk_revealer_set_transition_type (GTK_REVEALER (self->details_revealer),
                                    GTK_REVEALER_TRANSITION_TYPE_SLIDE_DOWN);
  gtk_container_add (GTK_CONTAINER (grid0), self->details_revealer);

  tool_widget = photos_tool_get_widget (self->tool);
  gtk_widget_set_margin_bottom (tool_widget, 12);
  gtk_widget_set_margin_top (tool_widget, 12);
  gtk_container_add (GTK_CONTAINER (self->details_revealer), tool_widget);
  gtk_size_group_add_widget (self->size_group, tool_widget);

  g_signal_connect_swapped (self->tool, "hide-requested", G_CALLBACK (photos_edit_palette_row_hide_details), self);

  gtk_widget_show_all (GTK_WIDGET (self));

  g_free (name_markup);
  g_clear_object (&self->size_group); /* We will not need it any more */
}


static void
photos_edit_palette_row_dispose (GObject *object)
{
  PhotosEditPaletteRow *self = PHOTOS_EDIT_PALETTE_ROW (object);

  g_clear_object (&self->size_group);
  g_clear_object (&self->tool);

  G_OBJECT_CLASS (photos_edit_palette_row_parent_class)->dispose (object);
}


static void
photos_edit_palette_row_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
  PhotosEditPaletteRow *self = PHOTOS_EDIT_PALETTE_ROW (object);

  switch (prop_id)
    {
    case PROP_TOOL:
      g_value_set_object (value, self->tool);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
photos_edit_palette_row_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
  PhotosEditPaletteRow *self = PHOTOS_EDIT_PALETTE_ROW (object);

  switch (prop_id)
    {
    case PROP_SIZE_GROUP:
      self->size_group = GTK_SIZE_GROUP (g_value_dup_object (value));
      break;

    case PROP_TOOL:
      self->tool = PHOTOS_TOOL (g_value_dup_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
photos_edit_palette_row_init (PhotosEditPaletteRow *self)
{
}


static void
photos_edit_palette_row_class_init (PhotosEditPaletteRowClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->constructed = photos_edit_palette_row_constructed;
  object_class->dispose = photos_edit_palette_row_dispose;
  object_class->get_property = photos_edit_palette_row_get_property;
  object_class->set_property = photos_edit_palette_row_set_property;

  g_object_class_install_property (object_class,
                                   PROP_SIZE_GROUP,
                                   g_param_spec_object ("size-group",
                                                        "GtkSizeGroup object",
                                                        "Ensures the same width for all tool widgets",
                                                        GTK_TYPE_SIZE_GROUP,
                                                        G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE));

  g_object_class_install_property (object_class,
                                   PROP_TOOL,
                                   g_param_spec_object ("tool",
                                                        "PhotosTool object",
                                                        "The tool associated with this row",
                                                        PHOTOS_TYPE_TOOL,
                                                        G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE));
}


GtkWidget *
photos_edit_palette_row_new (PhotosTool *tool, GtkSizeGroup *size_group)
{
  return g_object_new (PHOTOS_TYPE_EDIT_PALETTE_ROW, "size-group", size_group, "tool", tool, NULL);
}


PhotosTool *
photos_edit_palette_row_get_tool (PhotosEditPaletteRow *self)
{
  return self->tool;
}


void
photos_edit_palette_row_hide_details (PhotosEditPaletteRow *self)
{
  gtk_list_box_row_set_activatable (GTK_LIST_BOX_ROW (self), TRUE);
  gtk_revealer_set_reveal_child (GTK_REVEALER (self->details_revealer), FALSE);
}


void
photos_edit_palette_row_show_details (PhotosEditPaletteRow *self)
{
  gtk_list_box_row_set_activatable (GTK_LIST_BOX_ROW (self), FALSE);
  gtk_revealer_set_reveal_child (GTK_REVEALER (self->details_revealer), TRUE);
}

void
photos_edit_palette_row_show (PhotosEditPaletteRow *self)
{
  gtk_revealer_set_reveal_child (GTK_REVEALER (self->row_revealer), TRUE);
}
