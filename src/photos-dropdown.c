/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2014 Red Hat, Inc.
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

/* Based on code from:
 *   + Documents
 */


#include "config.h"

#include <gio/gio.h>

#include "photos-base-view.h"
#include "photos-dropdown.h"
#include "photos-search-context.h"


struct _PhotosDropdownPrivate
{
  GtkWidget *grid;
  GtkWidget *match_view;
  GtkWidget *source_view;
  GtkWidget *type_view;
  PhotosBaseManager *srch_mtch_mngr;
  PhotosBaseManager *srch_typ_mngr;
  PhotosBaseManager *src_mngr;
};


G_DEFINE_TYPE_WITH_PRIVATE (PhotosDropdown, photos_dropdown, GTK_TYPE_POPOVER);


static void
photos_dropdown_dispose (GObject *object)
{
  PhotosDropdown *self = PHOTOS_DROPDOWN (object);
  PhotosDropdownPrivate *priv = self->priv;

  g_clear_object (&priv->srch_mtch_mngr);
  g_clear_object (&priv->srch_typ_mngr);
  g_clear_object (&priv->src_mngr);

  G_OBJECT_CLASS (photos_dropdown_parent_class)->dispose (object);
}


static void
photos_dropdown_init (PhotosDropdown *self)
{
  PhotosDropdownPrivate *priv;
  GApplication *app;
  GtkStyleContext *context;
  PhotosSearchContextState *state;

  self->priv = photos_dropdown_get_instance_private (self);
  priv = self->priv;

  app = g_application_get_default ();
  state = photos_search_context_get_state (PHOTOS_SEARCH_CONTEXT (app));

  priv->srch_mtch_mngr = g_object_ref (state->srch_mtch_mngr);
  priv->srch_typ_mngr = g_object_ref (state->srch_typ_mngr);
  priv->src_mngr = g_object_ref (state->src_mngr);

  priv->match_view = photos_base_view_new (priv->srch_mtch_mngr);
  priv->source_view = photos_base_view_new (priv->src_mngr);
  priv->type_view = photos_base_view_new (priv->srch_typ_mngr);

  priv->grid = gtk_grid_new ();
  gtk_widget_set_margin_start (priv->grid, 10);
  gtk_widget_set_margin_end (priv->grid, 10);
  gtk_widget_set_margin_bottom (priv->grid, 10);
  gtk_widget_set_margin_top (priv->grid, 10);
  gtk_orientable_set_orientation (GTK_ORIENTABLE (priv->grid), GTK_ORIENTATION_HORIZONTAL);
  gtk_grid_set_row_homogeneous (GTK_GRID (priv->grid), TRUE);
  gtk_container_add (GTK_CONTAINER (self), priv->grid);

  gtk_container_add (GTK_CONTAINER (priv->grid), priv->source_view);
  gtk_container_add (GTK_CONTAINER (priv->grid), priv->type_view);
  gtk_container_add (GTK_CONTAINER (priv->grid), priv->match_view);

  context = gtk_widget_get_style_context (GTK_WIDGET (self));
  gtk_style_context_add_class (context, "photos-dropdown");
  gtk_widget_hide (GTK_WIDGET(self));
  gtk_widget_show_all (GTK_WIDGET (priv->grid));
}


static void
photos_dropdown_class_init (PhotosDropdownClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->dispose = photos_dropdown_dispose;
}


GtkWidget *
photos_dropdown_new (GtkWidget *relative_to)
{
  return g_object_new (PHOTOS_TYPE_DROPDOWN, "relative-to", relative_to, "height_request", 240, NULL);
}
