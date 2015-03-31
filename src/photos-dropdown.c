/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2015 Alessandro Bono
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
#include <glib.h>

#include "photos-base-manager.h"
#include "photos-base-model.h"
#include "photos-dropdown.h"
#include "photos-search-context.h"


struct _PhotosDropdownPrivate
{
  GList *models;
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
photos_dropdown_add_manager (PhotosDropdown *self, PhotosBaseManager *mngr)
{
  PhotosDropdownPrivate *priv = self->priv;
  GMenu *menu;
  GtkWidget *popover;
  GtkWidget *w;
  PhotosBaseModel *model;

  model = photos_base_model_new (mngr);
  priv->models = g_list_prepend (priv->models, g_object_ref (model));

  /* HACK: see https://bugzilla.gnome.org/show_bug.cgi?id=733977 */
  popover = gtk_popover_new (NULL);
  menu = photos_base_model_get_model (model);
  gtk_popover_bind_model (GTK_POPOVER (popover), G_MENU_MODEL (menu), "app");
  w = g_object_ref (gtk_bin_get_child (GTK_BIN (popover)));
  gtk_container_remove (GTK_CONTAINER (popover), w);
  gtk_container_add (GTK_CONTAINER (self->priv->grid), w);
  g_object_unref (w);
  gtk_widget_set_valign (w, GTK_ALIGN_START);
  gtk_widget_set_vexpand (w, TRUE);
  gtk_widget_destroy (popover);

  g_object_unref (model);
}


static void
photos_dropdown_dispose (GObject *object)
{
  PhotosDropdown *self = PHOTOS_DROPDOWN (object);
  PhotosDropdownPrivate *priv = self->priv;

  g_clear_object (&priv->srch_mtch_mngr);
  g_clear_object (&priv->srch_typ_mngr);
  g_clear_object (&priv->src_mngr);

  g_list_free_full (priv->models, g_object_unref);
  priv->models = NULL;

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

  priv->grid = gtk_grid_new ();
  gtk_orientable_set_orientation (GTK_ORIENTABLE (priv->grid), GTK_ORIENTATION_HORIZONTAL);
  gtk_grid_set_row_homogeneous (GTK_GRID (priv->grid), TRUE);
  gtk_container_add (GTK_CONTAINER (self), priv->grid);

  photos_dropdown_add_manager (self, priv->src_mngr);
  photos_dropdown_add_manager (self, priv->srch_typ_mngr);
  photos_dropdown_add_manager (self, priv->srch_mtch_mngr);

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
  return g_object_new (PHOTOS_TYPE_DROPDOWN, "relative-to", relative_to, "position", GTK_POS_BOTTOM, NULL);
}
