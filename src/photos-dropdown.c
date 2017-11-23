/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2015 – 2017 Alessandro Bono
 * Copyright © 2014 – 2017 Red Hat, Inc.
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


struct _PhotosDropdown
{
  GtkPopover parent_instance;
  GList *models;
  GtkWidget *grid;
  GtkWidget *match_view;
  GtkWidget *source_view;
  GtkWidget *type_view;
  PhotosBaseManager *srch_mtch_mngr;
  PhotosBaseManager *srch_typ_mngr;
  PhotosBaseManager *src_mngr;
};


G_DEFINE_TYPE (PhotosDropdown, photos_dropdown, GTK_TYPE_POPOVER);


static void
photos_dropdown_add_manager (PhotosDropdown *self, PhotosBaseManager *mngr)
{
  GMenu *menu;
  GtkWidget *popover;
  GtkWidget *w;
  PhotosBaseModel *model;

  model = photos_base_model_new (mngr);
  self->models = g_list_prepend (self->models, g_object_ref (model));

  /* HACK: see https://bugzilla.gnome.org/show_bug.cgi?id=733977 */
  popover = gtk_popover_new (NULL);
  menu = photos_base_model_get_model (model);
  gtk_popover_bind_model (GTK_POPOVER (popover), G_MENU_MODEL (menu), "app");
  w = g_object_ref (gtk_bin_get_child (GTK_BIN (popover)));
  gtk_container_remove (GTK_CONTAINER (popover), w);
  gtk_container_add (GTK_CONTAINER (self->grid), w);
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

  g_clear_object (&self->srch_mtch_mngr);
  g_clear_object (&self->srch_typ_mngr);
  g_clear_object (&self->src_mngr);

  g_list_free_full (self->models, g_object_unref);
  self->models = NULL;

  G_OBJECT_CLASS (photos_dropdown_parent_class)->dispose (object);
}


static void
photos_dropdown_init (PhotosDropdown *self)
{
  GApplication *app;
  PhotosSearchContextState *state;

  gtk_widget_init_template (GTK_WIDGET (self));

  app = g_application_get_default ();
  state = photos_search_context_get_state (PHOTOS_SEARCH_CONTEXT (app));

  self->srch_mtch_mngr = g_object_ref (state->srch_mtch_mngr);
  self->srch_typ_mngr = g_object_ref (state->srch_typ_mngr);
  self->src_mngr = g_object_ref (state->src_mngr);

  photos_dropdown_add_manager (self, self->src_mngr);
  photos_dropdown_add_manager (self, self->srch_typ_mngr);
  photos_dropdown_add_manager (self, self->srch_mtch_mngr);
}


static void
photos_dropdown_class_init (PhotosDropdownClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->dispose = photos_dropdown_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Photos/dropdown.ui");
  gtk_widget_class_bind_template_child (widget_class, PhotosDropdown, grid);
}


GtkWidget *
photos_dropdown_new (void)
{
  return g_object_new (PHOTOS_TYPE_DROPDOWN, NULL);
}
