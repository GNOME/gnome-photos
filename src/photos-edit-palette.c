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

#include "photos-edit-palette.h"
#include "photos-edit-palette-row.h"
#include "photos-item-manager.h"
#include "photos-search-context.h"
#include "photos-tool.h"
#include "photos-utils.h"


struct _PhotosEditPalette
{
  GtkListBox parent_instance;
  GList *tools;
  PhotosModeController *mode_cntrlr;
};

struct _PhotosEditPaletteClass
{
  GtkListBoxClass parent_class;

  /* signals */
  void        (*tool_changed)       (PhotosEditPalette *self, PhotosTool *tool);
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
photos_edit_palette_window_mode_changed (PhotosEditPalette *self, PhotosWindowMode mode, PhotosWindowMode old_mode)
{
  GtkListBoxRow *row;
  gint i;

  for (i = 0; (row = gtk_list_box_get_row_at_index (GTK_LIST_BOX (self), i)) != NULL; i++)
    photos_edit_palette_row_hide_details (PHOTOS_EDIT_PALETTE_ROW (row));

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
photos_edit_palette_update_header_func (GtkListBoxRow *row, GtkListBoxRow *before)
{
  GtkWidget *header;

  if (before == NULL)
    {
      gtk_list_box_row_set_header (row, NULL);
      return;
    }

  header = gtk_list_box_row_get_header (row);
  if (header == NULL)
    {
      header = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
      gtk_widget_show (header);
      gtk_list_box_row_set_header (row, header);
    }
}


static void
photos_edit_palette_dispose (GObject *object)
{
  PhotosEditPalette *self = PHOTOS_EDIT_PALETTE (object);

  g_list_free_full (self->tools, g_object_unref);
  self->tools = NULL;

  g_clear_object (&self->mode_cntrlr);

  G_OBJECT_CLASS (photos_edit_palette_parent_class)->dispose (object);
}


static void
photos_edit_palette_init (PhotosEditPalette *self)
{
  GApplication *app;
  GIOExtensionPoint *extension_point;
  GList *extensions;
  GList *l;
  GtkSizeGroup *size_group;
  PhotosSearchContextState *state;

  app = g_application_get_default ();
  state = photos_search_context_get_state (PHOTOS_SEARCH_CONTEXT (app));

  gtk_widget_set_vexpand (GTK_WIDGET (self), TRUE);
  gtk_list_box_set_selection_mode (GTK_LIST_BOX (self), GTK_SELECTION_NONE);
  gtk_list_box_set_header_func (GTK_LIST_BOX (self),
                                (GtkListBoxUpdateHeaderFunc) photos_edit_palette_update_header_func,
                                NULL,
                                NULL);

  extension_point = g_io_extension_point_lookup (PHOTOS_TOOL_EXTENSION_POINT_NAME);
  extensions = g_io_extension_point_get_extensions (extension_point);
  extensions = g_list_sort (extensions, photos_edit_palette_extensions_sort_func);

  size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

  for (l = extensions; l != NULL; l = l->next)
    {
      GIOExtension *extension = (GIOExtension *) l->data;
      GType type;
      GtkWidget *row;
      PhotosTool *tool;

      type = g_io_extension_get_type (extension);
      tool = PHOTOS_TOOL (g_object_new (type, NULL));
      self->tools = g_list_prepend (self->tools, g_object_ref (tool));

      row = photos_edit_palette_row_new (tool, size_group);
      gtk_container_add (GTK_CONTAINER (self), row);

      g_signal_connect_swapped (tool, "hide-requested", G_CALLBACK (photos_edit_palette_hide_requested), self);

      g_object_unref (tool);
    }

  self->mode_cntrlr = g_object_ref (state->mode_cntrlr);
  g_signal_connect_object (self->mode_cntrlr,
                           "window-mode-changed",
                           G_CALLBACK (photos_edit_palette_window_mode_changed),
                           self,
                           G_CONNECT_SWAPPED);

  gtk_widget_show_all (GTK_WIDGET (self));
  g_object_unref (size_group);
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
                                        G_STRUCT_OFFSET (PhotosEditPaletteClass, tool_changed),
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
