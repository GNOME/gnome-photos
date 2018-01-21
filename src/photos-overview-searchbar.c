/*
 * Photos - access, organize and share your photos on GNOME
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
#include <libgd/gd.h>

#include "photos-base-item.h"
#include "photos-base-manager.h"
#include "photos-dropdown.h"
#include "photos-filterable.h"
#include "photos-overview-searchbar.h"
#include "photos-search-context.h"
#include "photos-search-controller.h"
#include "photos-search-type.h"


struct _PhotosOverviewSearchbar
{
  PhotosSearchbar parent_instance;
  GAction *select_all_action;
  GdTaggedEntry *search_entry;
  GdTaggedEntryTag *src_tag;
  GdTaggedEntryTag *srch_mtch_tag;
  GdTaggedEntryTag *srch_typ_tag;
  GtkWidget *dropdown_button;
  GtkWidget *search_container;
  PhotosBaseManager *src_mngr;
  PhotosBaseManager *srch_mtch_mngr;
  PhotosBaseManager *srch_typ_mngr;
  PhotosSearchController *srch_cntrlr;
};


G_DEFINE_TYPE (PhotosOverviewSearchbar, photos_overview_searchbar, PHOTOS_TYPE_SEARCHBAR);


static void
photos_overview_searchbar_active_changed (PhotosOverviewSearchbar *self,
                                          PhotosBaseManager *mngr,
                                          GdTaggedEntryTag *tag)
{
  GObject *object;
  const gchar *id;
  g_autofree gchar *name = NULL;

  object = photos_base_manager_get_active_object (mngr);
  id = photos_filterable_get_id (PHOTOS_FILTERABLE (object));
  g_object_get (object, "name", &name, NULL);

  if (g_strcmp0 (id, "all") == 0)
    gd_tagged_entry_remove_tag (self->search_entry, tag);
  else
    {
      gd_tagged_entry_tag_set_label (tag, name);
      gd_tagged_entry_add_tag (self->search_entry, tag);
    }

  gtk_entry_grab_focus_without_selecting (GTK_ENTRY (self->search_entry));
}


static void
photos_overview_searchbar_hide (PhotosSearchbar *searchbar)
{
  PhotosOverviewSearchbar *self = PHOTOS_OVERVIEW_SEARCHBAR (searchbar);

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->dropdown_button), FALSE);
  g_simple_action_set_enabled (G_SIMPLE_ACTION (self->select_all_action), TRUE);

  photos_base_manager_set_active_object_by_id (self->srch_typ_mngr, "all");
  photos_base_manager_set_active_object_by_id (self->srch_mtch_mngr, "all");
  photos_base_manager_set_active_object_by_id (self->src_mngr, "all");

  photos_search_controller_set_string (self->srch_cntrlr, "");

  PHOTOS_SEARCHBAR_CLASS (photos_overview_searchbar_parent_class)->hide (searchbar);
}


static void
photos_overview_searchbar_search_match_active_changed (PhotosOverviewSearchbar *self)
{
  photos_overview_searchbar_active_changed (self, self->srch_mtch_mngr, self->srch_mtch_tag);
}


static void
photos_overview_searchbar_search_string_changed (PhotosOverviewSearchbar *self, const gchar *str)
{
  gtk_entry_set_text (GTK_ENTRY (self->search_entry), str);
}


static void
photos_overview_searchbar_search_type_active_changed (PhotosOverviewSearchbar *self)
{
  photos_overview_searchbar_active_changed (self, self->srch_typ_mngr, self->srch_typ_tag);
}


static void
photos_overview_searchbar_show (PhotosSearchbar *searchbar)
{
  PhotosOverviewSearchbar *self = PHOTOS_OVERVIEW_SEARCHBAR (searchbar);

  g_simple_action_set_enabled (G_SIMPLE_ACTION (self->select_all_action), FALSE);

  PHOTOS_SEARCHBAR_CLASS (photos_overview_searchbar_parent_class)->show (searchbar);
}


static void
photos_overview_searchbar_source_active_changed (PhotosOverviewSearchbar *self)
{
  photos_overview_searchbar_active_changed (self, self->src_mngr, self->src_tag);
}


static void
photos_overview_searchbar_tag_button_clicked (PhotosOverviewSearchbar *self, GdTaggedEntryTag *tag)
{
  PhotosBaseManager *mngr = NULL;

  if (tag == self->src_tag)
    mngr = self->src_mngr;
  else if (tag == self->srch_mtch_tag)
    mngr = self->srch_mtch_mngr;
  else if (tag == self->srch_typ_tag)
    mngr = self->srch_typ_mngr;

  if (mngr != NULL)
    photos_base_manager_set_active_object_by_id (mngr, "all");
}


static void
photos_overview_searchbar_tag_clicked (PhotosOverviewSearchbar *self)
{
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->dropdown_button), TRUE);
}


static void
photos_overview_searchbar_create_search_widgets (PhotosSearchbar *searchbar)
{
  PhotosOverviewSearchbar *self = PHOTOS_OVERVIEW_SEARCHBAR (searchbar);
  GtkStyleContext *context;
  GtkWidget *dropdown;

  self->search_entry = gd_tagged_entry_new ();
  gtk_widget_set_size_request (GTK_WIDGET (self->search_entry), 500, -1);
  g_signal_connect_swapped (self->search_entry,
                            "tag-clicked",
                            G_CALLBACK (photos_overview_searchbar_tag_clicked),
                            self);
  g_signal_connect_swapped (self->search_entry,
                            "tag-button-clicked",
                            G_CALLBACK (photos_overview_searchbar_tag_button_clicked),
                            self);
  photos_searchbar_set_search_entry (PHOTOS_SEARCHBAR (self), GTK_WIDGET (self->search_entry));

  self->src_tag = gd_tagged_entry_tag_new (NULL);
  self->srch_mtch_tag = gd_tagged_entry_tag_new (NULL);
  self->srch_typ_tag = gd_tagged_entry_tag_new (NULL);

  g_signal_connect_object (self->srch_cntrlr,
                           "search-string-changed",
                           G_CALLBACK (photos_overview_searchbar_search_string_changed),
                           self,
                           G_CONNECT_SWAPPED);

  self->dropdown_button = gtk_menu_button_new ();
  dropdown = photos_dropdown_new ();
  gtk_menu_button_set_popover (GTK_MENU_BUTTON (self->dropdown_button), dropdown);

  self->search_container = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_set_halign (self->search_container, GTK_ALIGN_CENTER);
  context = gtk_widget_get_style_context (self->search_container);
  gtk_style_context_add_class (context, "linked");
  gtk_container_add (GTK_CONTAINER (self->search_container), GTK_WIDGET (self->search_entry));
  gtk_container_add (GTK_CONTAINER (self->search_container), self->dropdown_button);
  gtk_widget_show_all (self->search_container);
  photos_searchbar_set_search_container (PHOTOS_SEARCHBAR (self), self->search_container);
}


static void
photos_overview_searchbar_entry_changed (PhotosSearchbar *searchbar)
{
  PhotosOverviewSearchbar *self = PHOTOS_OVERVIEW_SEARCHBAR (searchbar);
  const gchar *current_text;

  current_text = gtk_entry_get_text (GTK_ENTRY (self->search_entry));

  g_signal_handlers_block_by_func (self->srch_cntrlr, photos_overview_searchbar_search_string_changed, self);
  photos_search_controller_set_string (self->srch_cntrlr, current_text);
  g_signal_handlers_unblock_by_func (self->srch_cntrlr, photos_overview_searchbar_search_string_changed, self);
}


static void
photos_overview_searchbar_constructed (GObject *object)
{
  PhotosOverviewSearchbar *self = PHOTOS_OVERVIEW_SEARCHBAR (object);

  G_OBJECT_CLASS (photos_overview_searchbar_parent_class)->constructed (object);

  photos_overview_searchbar_source_active_changed (self);
  photos_overview_searchbar_search_type_active_changed (self);
  photos_overview_searchbar_search_match_active_changed (self);
}


static void
photos_overview_searchbar_dispose (GObject *object)
{
  PhotosOverviewSearchbar *self = PHOTOS_OVERVIEW_SEARCHBAR (object);

  g_clear_object (&self->src_mngr);
  g_clear_object (&self->src_tag);
  g_clear_object (&self->srch_typ_tag);
  g_clear_object (&self->srch_mtch_mngr);
  g_clear_object (&self->srch_mtch_tag);
  g_clear_object (&self->srch_typ_mngr);
  g_clear_object (&self->srch_cntrlr);

  G_OBJECT_CLASS (photos_overview_searchbar_parent_class)->dispose (object);
}


static void
photos_overview_searchbar_init (PhotosOverviewSearchbar *self)
{
  GApplication *app;
  PhotosSearchContextState *state;

  app = g_application_get_default ();
  state = photos_search_context_get_state (PHOTOS_SEARCH_CONTEXT (app));

  self->select_all_action = g_action_map_lookup_action (G_ACTION_MAP (app), "select-all");

  self->src_mngr = g_object_ref (state->src_mngr);
  g_signal_connect_object (self->src_mngr,
                           "active-changed",
                           G_CALLBACK (photos_overview_searchbar_source_active_changed),
                           self,
                           G_CONNECT_SWAPPED);

  self->srch_mtch_mngr = g_object_ref (state->srch_mtch_mngr);
  g_signal_connect_object (self->srch_mtch_mngr,
                           "active-changed",
                           G_CALLBACK (photos_overview_searchbar_search_match_active_changed),
                           self,
                           G_CONNECT_SWAPPED);

  self->srch_typ_mngr = g_object_ref (state->srch_typ_mngr);
  g_signal_connect_object (self->srch_typ_mngr,
                           "active-changed",
                           G_CALLBACK (photos_overview_searchbar_search_type_active_changed),
                           self,
                           G_CONNECT_SWAPPED);

  self->srch_cntrlr = g_object_ref (state->srch_cntrlr);
}


static void
photos_overview_searchbar_class_init (PhotosOverviewSearchbarClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  PhotosSearchbarClass *searchbar_class = PHOTOS_SEARCHBAR_CLASS (class);

  object_class->constructed = photos_overview_searchbar_constructed;
  object_class->dispose = photos_overview_searchbar_dispose;
  searchbar_class->create_search_widgets = photos_overview_searchbar_create_search_widgets;
  searchbar_class->entry_changed = photos_overview_searchbar_entry_changed;
  searchbar_class->hide = photos_overview_searchbar_hide;
  searchbar_class->show = photos_overview_searchbar_show;
}


GtkWidget *
photos_overview_searchbar_new (void)
{
  return g_object_new (PHOTOS_TYPE_OVERVIEW_SEARCHBAR, NULL);
}
