/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2015 – 2021 Red Hat, Inc.
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

#include "photos-edit-palette.h"
#include "photos-edit-palette-row.h"
#include "photos-tool.h"
#include "photos-utils.h"


struct _PhotosEditPalette
{
  GtkListBox parent_instance;
  GIOExtensionPoint *extension_point;
  GList *tools;
};

enum
{
  TOOL_CHANGED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };


G_DEFINE_TYPE (PhotosEditPalette, photos_edit_palette, GTK_TYPE_LIST_BOX);


static gint
photos_edit_palette_extensions_sort_func (gconstpointer a, gconstpointer b)
{
  GIOExtension *extension_a = (GIOExtension *) a;
  GIOExtension *extension_b = (GIOExtension *) b;
  gint priority_a;
  gint priority_b;

  priority_a = g_io_extension_get_priority (extension_a);
  priority_b = g_io_extension_get_priority (extension_b);
  return priority_a - priority_b;
}


static void
photos_edit_palette_hide_requested (PhotosEditPalette *self)
{
  g_signal_emit (self, signals[TOOL_CHANGED], 0, NULL);
}


static void
photos_edit_palette_row_activated (GtkListBox *box, GtkListBoxRow *row)
{
  PhotosEditPalette *self = PHOTOS_EDIT_PALETTE (box);
  GtkListBoxRow *other_row;
  PhotosTool *tool;
  gint i;

  photos_edit_palette_row_show_details (PHOTOS_EDIT_PALETTE_ROW (row));

  for (i = 0; (other_row = gtk_list_box_get_row_at_index (box, i)) != NULL; i++)
    {
      if (other_row == row)
        continue;

      photos_edit_palette_row_hide_details (PHOTOS_EDIT_PALETTE_ROW (other_row));
    }

  tool = photos_edit_palette_row_get_tool (PHOTOS_EDIT_PALETTE_ROW (row));
  g_signal_emit (self, signals[TOOL_CHANGED], 0, tool);
}


static void
photos_edit_palette_dispose (GObject *object)
{
  PhotosEditPalette *self = PHOTOS_EDIT_PALETTE (object);

  g_list_free_full (self->tools, g_object_unref);
  self->tools = NULL;

  G_OBJECT_CLASS (photos_edit_palette_parent_class)->dispose (object);
}


static void
photos_edit_palette_init (PhotosEditPalette *self)
{
  gtk_list_box_set_selection_mode (GTK_LIST_BOX (self), GTK_SELECTION_NONE);
  gtk_list_box_set_header_func (GTK_LIST_BOX (self), photos_utils_list_box_header_func, NULL, NULL);

  self->extension_point = g_io_extension_point_lookup (PHOTOS_TOOL_EXTENSION_POINT_NAME);

  gtk_widget_show_all (GTK_WIDGET (self));
}


static void
photos_edit_palette_class_init (PhotosEditPaletteClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkListBoxClass *list_box_class = GTK_LIST_BOX_CLASS (class);

  object_class->dispose = photos_edit_palette_dispose;
  list_box_class->row_activated = photos_edit_palette_row_activated;

  signals[TOOL_CHANGED] = g_signal_new ("tool-changed",
                                        G_TYPE_FROM_CLASS (class),
                                        G_SIGNAL_RUN_LAST,
                                        0,
                                        NULL, /* accumulator */
                                        NULL, /* accu_data */
                                        g_cclosure_marshal_VOID__OBJECT,
                                        G_TYPE_NONE,
                                        1,
                                        PHOTOS_TYPE_TOOL);
}


GtkWidget *
photos_edit_palette_new (void)
{
  return g_object_new (PHOTOS_TYPE_EDIT_PALETTE, NULL);
}


void
photos_edit_palette_hide_details (PhotosEditPalette *self)
{
  GtkListBoxRow *row;
  gint i;

  for (i = 0; (row = gtk_list_box_get_row_at_index (GTK_LIST_BOX (self), i)) != NULL; i++)
    photos_edit_palette_row_hide_details (PHOTOS_EDIT_PALETTE_ROW (row));

  g_signal_emit (self, signals[TOOL_CHANGED], 0, NULL);
}


void
photos_edit_palette_show (PhotosEditPalette *self)
{
  g_autoptr (GList) extensions = NULL;
  GList *l;
  g_autoptr (GtkSizeGroup) size_group = NULL;

  gtk_container_foreach (GTK_CONTAINER (self), (GtkCallback) gtk_widget_destroy, NULL);
  g_list_free_full (self->tools, g_object_unref);
  self->tools = NULL;

  extensions = g_io_extension_point_get_extensions (self->extension_point);
  extensions = g_list_copy (extensions);
  extensions = g_list_sort (extensions, photos_edit_palette_extensions_sort_func);

  size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

  for (l = extensions; l != NULL; l = l->next)
    {
      GIOExtension *extension = (GIOExtension *) l->data;
      GType type;
      GtkWidget *row;
      g_autoptr (PhotosTool) tool = NULL;

      type = g_io_extension_get_type (extension);
      tool = PHOTOS_TOOL (g_object_new (type, NULL));
      self->tools = g_list_prepend (self->tools, g_object_ref (tool));

      row = photos_edit_palette_row_new (tool, size_group);
      gtk_container_add (GTK_CONTAINER (self), row);
      photos_edit_palette_row_show (PHOTOS_EDIT_PALETTE_ROW (row));

      g_signal_connect_swapped (tool, "hide-requested", G_CALLBACK (photos_edit_palette_hide_requested), self);
    }

  gtk_widget_show_all (GTK_WIDGET (self));
}
