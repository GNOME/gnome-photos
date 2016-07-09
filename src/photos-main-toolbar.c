/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2012 – 2016 Red Hat, Inc.
 * Copyright © 2014 Saurav Agarwalla
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

#include <glib.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "photos-dlna-renderers-manager.h"
#include "photos-dropdown.h"
#include "photos-header-bar.h"
#include "photos-icons.h"
#include "photos-item-manager.h"
#include "photos-main-toolbar.h"
#include "photos-overview-searchbar.h"
#include "photos-remote-display-manager.h"
#include "photos-search-context.h"
#include "photos-selection-controller.h"


struct _PhotosMainToolbar
{
  GtkBox parent_instance;
  GAction *search;
  GSimpleAction *gear_menu;
  GtkWidget *coll_back_button;
  GtkWidget *favorite_button;
  GtkWidget *overlay;
  GtkWidget *remote_display_button;
  GtkWidget *searchbar;
  GtkWidget *selection_button;
  GtkWidget *selection_menu;
  GtkWidget *toolbar;
  PhotosBaseManager *item_mngr;
  PhotosModeController *mode_cntrlr;
  PhotosRemoteDisplayManager *remote_mngr;
  PhotosSelectionController *sel_cntrlr;
};

struct _PhotosMainToolbarClass
{
  GtkBoxClass parent_class;
};

enum
{
  PROP_0,
  PROP_OVERLAY
};


G_DEFINE_TYPE (PhotosMainToolbar, photos_main_toolbar, GTK_TYPE_BOX);


static void photos_main_toolbar_favorite_button_update (PhotosMainToolbar *self, gboolean favorite);


static void
photos_main_toolbar_set_toolbar_title (PhotosMainToolbar *self)
{
  PhotosBaseItem *active_collection;
  PhotosWindowMode window_mode;
  gboolean selection_mode;
  gchar *primary = NULL;

  active_collection = photos_item_manager_get_active_collection (PHOTOS_ITEM_MANAGER (self->item_mngr));
  selection_mode = photos_selection_controller_get_selection_mode (self->sel_cntrlr);
  window_mode = photos_mode_controller_get_window_mode (self->mode_cntrlr);

  if (window_mode == PHOTOS_WINDOW_MODE_OVERVIEW
      || window_mode == PHOTOS_WINDOW_MODE_COLLECTIONS
      || window_mode == PHOTOS_WINDOW_MODE_FAVORITES
      || window_mode == PHOTOS_WINDOW_MODE_SEARCH)
    {
      if (!selection_mode)
        {
          if (active_collection != NULL)
            primary = g_strdup (photos_base_item_get_name (active_collection));
        }
      else
        {
          GList *selection;
          gchar *label;
          guint length;

          selection = photos_selection_controller_get_selection (self->sel_cntrlr);
          length = g_list_length (selection);
          if (length == 0)
            label = g_strdup(_("Click on items to select them"));
          else
            label = g_strdup_printf (ngettext ("%d selected", "%d selected", length), length);

          if (active_collection != NULL)
            {
              primary =  g_markup_printf_escaped ("<b>%s</b> (%s)",
                                                  photos_base_item_get_name (active_collection),
                                                  label);
            }
          else
            {
              primary = label;
              label = NULL;
            }

          g_free (label);
        }
    }
  else if (window_mode == PHOTOS_WINDOW_MODE_EDIT || window_mode == PHOTOS_WINDOW_MODE_PREVIEW)
    {
      GObject *item;

      item = photos_base_manager_get_active_object (self->item_mngr);
      if (item != NULL)
        primary = g_strdup (photos_base_item_get_name_with_fallback (PHOTOS_BASE_ITEM (item)));
    }

  if (selection_mode)
    {
      if (primary != NULL)
        {
          GtkWidget *label;

          gtk_button_set_label (GTK_BUTTON (self->selection_menu), primary);
          label = gtk_bin_get_child (GTK_BIN (self->selection_menu));
          gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
        }
    }
  else
    gtk_header_bar_set_title (GTK_HEADER_BAR (self->toolbar), primary);

  g_free (primary);
}


static GtkWidget *
photos_main_toolbar_add_back_button (PhotosMainToolbar *self)
{
  GtkWidget *back_button;

  back_button = gtk_button_new_from_icon_name (PHOTOS_ICON_GO_PREVIOUS_SYMBOLIC, GTK_ICON_SIZE_BUTTON);
  gtk_widget_set_tooltip_text (back_button, _("Back"));
  gtk_header_bar_pack_start (GTK_HEADER_BAR (self->toolbar), back_button);

  return back_button;
}


static void
photos_main_toolbar_remote_display_button_clicked (PhotosMainToolbar *self)
{
  photos_remote_display_manager_stop (self->remote_mngr);
}


static void
photos_main_toolbar_add_remote_display_button (PhotosMainToolbar *self)
{
  PhotosDlnaRenderer *renderer;
  GtkLabel *label;
  gchar *text;
  const gchar *name;

  g_clear_pointer (&self->remote_display_button, (GDestroyNotify) gtk_widget_destroy);

  renderer = photos_remote_display_manager_get_renderer (self->remote_mngr);
  name = photos_dlna_renderer_get_friendly_name (renderer);
  text = g_markup_printf_escaped ("Displaying on <b>%s</b>", name);

  self->remote_display_button = gtk_button_new_with_label (text);
  label = GTK_LABEL (gtk_bin_get_child (GTK_BIN (self->remote_display_button)));
  gtk_label_set_ellipsize (label, PANGO_ELLIPSIZE_MIDDLE);
  gtk_label_set_use_markup (label, TRUE);
  gtk_widget_set_margin_end (self->remote_display_button, 12);
  gtk_header_bar_pack_start (GTK_HEADER_BAR (self->toolbar), self->remote_display_button);
  gtk_widget_show_all (self->remote_display_button);

  g_signal_connect_swapped (self->remote_display_button,
                            "clicked",
                            G_CALLBACK (photos_main_toolbar_remote_display_button_clicked),
                            self);
  g_free (text);
}


static void
photos_main_toolbar_update_remote_display_button (PhotosMainToolbar *self)
{
  PhotosWindowMode window_mode;
  gboolean selection_mode, active;

  selection_mode = photos_selection_controller_get_selection_mode (self->sel_cntrlr);
  window_mode = photos_mode_controller_get_window_mode (self->mode_cntrlr);
  active = photos_remote_display_manager_is_active (self->remote_mngr);

  if (active && !selection_mode && window_mode != PHOTOS_WINDOW_MODE_PREVIEW)
    photos_main_toolbar_add_remote_display_button (self);
  else
    g_clear_pointer (&self->remote_display_button, (GDestroyNotify) gtk_widget_destroy);
}


static void
photos_main_toolbar_coll_back_button_clicked (PhotosMainToolbar *self)
{
  photos_item_manager_activate_previous_collection (PHOTOS_ITEM_MANAGER (self->item_mngr));
}


static void
photos_main_toolbar_col_active_changed (PhotosMainToolbar *self, PhotosBaseItem *collection)
{
  PhotosHeaderBarMode mode;

  if (collection != NULL)
    {
      mode = PHOTOS_HEADER_BAR_MODE_STANDALONE;
      if (self->coll_back_button == NULL)
        {
          self->coll_back_button = photos_main_toolbar_add_back_button (self);
          gtk_widget_show (self->coll_back_button);

          g_signal_connect_swapped (self->coll_back_button,
                                    "clicked",
                                    G_CALLBACK (photos_main_toolbar_coll_back_button_clicked),
                                    self);
        }
    }
  else
    {
      mode = PHOTOS_HEADER_BAR_MODE_NORMAL;
      g_clear_pointer (&self->coll_back_button, (GDestroyNotify) gtk_widget_destroy);
    }

  photos_header_bar_set_mode (PHOTOS_HEADER_BAR (self->toolbar), mode);
  photos_main_toolbar_update_remote_display_button (self);
  photos_main_toolbar_set_toolbar_title (self);
}


static void
photos_main_toolbar_item_active_changed (PhotosMainToolbar *self, GObject *object)
{
  PhotosWindowMode window_mode;
  gboolean favorite;

  if (object == NULL)
    return;

  window_mode = photos_mode_controller_get_window_mode (self->mode_cntrlr);
  if (window_mode != PHOTOS_WINDOW_MODE_PREVIEW)
    return;

  photos_main_toolbar_set_toolbar_title (self);

  favorite = photos_base_item_is_favorite (PHOTOS_BASE_ITEM (object));
  photos_main_toolbar_favorite_button_update (self, favorite);
}


static void
photos_main_toolbar_select_button_clicked (PhotosMainToolbar *self)
{
  photos_selection_controller_set_selection_mode (self->sel_cntrlr, TRUE);
}


static GtkWidget *
photos_main_toolbar_add_search_button (PhotosMainToolbar *self)
{
  GtkWidget *image;
  GtkWidget *search_button;

  image = gtk_image_new_from_icon_name (PHOTOS_ICON_EDIT_FIND_SYMBOLIC, GTK_ICON_SIZE_BUTTON);
  search_button = gtk_toggle_button_new ();
  gtk_widget_set_tooltip_text (search_button, _("Search"));
  gtk_button_set_image (GTK_BUTTON (search_button), image);
  gtk_actionable_set_action_name (GTK_ACTIONABLE (search_button), "app.search");
  gtk_header_bar_pack_end (GTK_HEADER_BAR (self->toolbar), search_button);

  return search_button;
}


static GtkWidget *
photos_main_toolbar_add_selection_button (PhotosMainToolbar *self)
{
  GtkWidget *selection_button;

  selection_button = gtk_button_new_from_icon_name (PHOTOS_ICON_OBJECT_SELECT_SYMBOLIC, GTK_ICON_SIZE_BUTTON);
  gtk_widget_set_tooltip_text (selection_button, _("Select Items"));
  gtk_header_bar_pack_end (GTK_HEADER_BAR (self->toolbar), selection_button);
  g_signal_connect_swapped (selection_button,
                            "clicked",
                            G_CALLBACK (photos_main_toolbar_select_button_clicked),
                            self);

  g_signal_connect_object (self->item_mngr,
                           "active-collection-changed",
                           G_CALLBACK (photos_main_toolbar_col_active_changed),
                           self,
                           G_CONNECT_SWAPPED);

  return selection_button;
}


static void
photos_main_toolbar_back_button_clicked (PhotosMainToolbar *self)
{
  photos_mode_controller_go_back (self->mode_cntrlr);
}


static void
photos_main_toolbar_clear_state_data (PhotosMainToolbar *self)
{
  g_clear_pointer (&self->coll_back_button, (GDestroyNotify) gtk_widget_destroy);
  g_clear_pointer (&self->remote_display_button, (GDestroyNotify) gtk_widget_destroy);
  g_clear_pointer (&self->selection_button, (GDestroyNotify) gtk_widget_destroy);

  if (self->searchbar != NULL && gtk_widget_get_parent (self->searchbar) == GTK_WIDGET (self))
    {
      GVariant *state;

      state = g_action_get_state (self->search);
      if (!g_variant_get_boolean (state))
        gtk_container_remove (GTK_CONTAINER (self), self->searchbar);

      g_variant_unref (state);
    }

  if (self->item_mngr != NULL)
    {
      g_signal_handlers_disconnect_by_func (self->item_mngr, photos_main_toolbar_col_active_changed, self);
      g_signal_handlers_disconnect_by_func (self->item_mngr, photos_main_toolbar_item_active_changed, self);
    }

  if (self->sel_cntrlr != NULL)
    g_signal_handlers_disconnect_by_func (self->sel_cntrlr, photos_main_toolbar_set_toolbar_title, self);
}


static void
photos_main_toolbar_clear_toolbar (PhotosMainToolbar *self)
{
  photos_main_toolbar_clear_state_data (self);
  gtk_header_bar_set_show_close_button (GTK_HEADER_BAR (self->toolbar), FALSE);
  photos_header_bar_clear (PHOTOS_HEADER_BAR (self->toolbar));
  g_simple_action_set_enabled (self->gear_menu, FALSE);
}


static GtkWidget *
photos_main_toolbar_create_overview_searchbar (PhotosMainToolbar *self)
{
  return photos_overview_searchbar_new ();
}


static GMenu *
photos_main_toolbar_create_preview_menu (PhotosMainToolbar *self)
{
  GMenu *menu;
  GtkBuilder *builder;
  PhotosBaseItem *item;

  builder = gtk_builder_new_from_resource ("/org/gnome/Photos/preview-menu.ui");

  menu = G_MENU (g_object_ref (gtk_builder_get_object (builder, "preview-menu")));
  item = PHOTOS_BASE_ITEM (photos_base_manager_get_active_object (self->item_mngr));
  if (item != NULL)
    {
      const gchar *default_app_name;

      default_app_name = photos_base_item_get_default_app_name (item);
      if (default_app_name != NULL)
        {
          GMenu *section;
          gchar *label;

          section = G_MENU (gtk_builder_get_object (builder, "open-section"));
          g_menu_remove (section, 0);

          label = g_strdup_printf (_("Open with %s"), default_app_name);
          g_menu_prepend (section, label, "app.open-current");
          g_free (label);
        }
    }

  g_object_unref (builder);
  return menu;
}


static void
photos_main_toolbar_done_button_clicked (PhotosMainToolbar *self)
{
  photos_selection_controller_set_selection_mode (self->sel_cntrlr, FALSE);
}


static void
photos_main_toolbar_favorite_button_clicked (PhotosMainToolbar *self)
{
  PhotosBaseItem *item;
  gboolean favorite;

  item = PHOTOS_BASE_ITEM (photos_base_manager_get_active_object (self->item_mngr));
  favorite = photos_base_item_is_favorite (item);
  photos_base_item_set_favorite (item, !favorite);

  photos_main_toolbar_favorite_button_update (self, !favorite);
}


static void
photos_main_toolbar_favorite_button_update (PhotosMainToolbar *self, gboolean favorite)
{
  GtkWidget *image;
  gchar *favorite_label;

  if (favorite)
    {
      favorite_label = g_strdup (_("Remove from favorites"));
      image = gtk_image_new_from_icon_name (PHOTOS_ICON_FAVORITE_SYMBOLIC, GTK_ICON_SIZE_BUTTON);
    }
  else
    {
      favorite_label = g_strdup (_("Add to favorites"));
      image = gtk_image_new_from_icon_name (PHOTOS_ICON_NOT_FAVORITE_SYMBOLIC, GTK_ICON_SIZE_BUTTON);
    }

  gtk_button_set_image (GTK_BUTTON (self->favorite_button), image);
  gtk_widget_set_tooltip_text (self->favorite_button, favorite_label);
  g_free (favorite_label);
}


static void
photos_main_toolbar_populate_for_collections (PhotosMainToolbar *self)
{
  PhotosBaseItem *collection;

  gtk_header_bar_set_show_close_button (GTK_HEADER_BAR (self->toolbar), TRUE);
  photos_header_bar_set_mode (PHOTOS_HEADER_BAR (self->toolbar), PHOTOS_HEADER_BAR_MODE_NORMAL);
  self->selection_button = photos_main_toolbar_add_selection_button (self);
  photos_main_toolbar_add_search_button (self);

  if (gtk_widget_get_parent (self->searchbar) == NULL)
    gtk_container_add (GTK_CONTAINER (self), self->searchbar);

  collection = photos_item_manager_get_active_collection (PHOTOS_ITEM_MANAGER (self->item_mngr));
  photos_main_toolbar_col_active_changed (self, collection);
}


static void
photos_main_toolbar_populate_for_edit (PhotosMainToolbar *self)
{
  GtkStyleContext *context;
  GtkWidget *cancel_button;
  GtkWidget *done_button;

  photos_header_bar_set_mode (PHOTOS_HEADER_BAR (self->toolbar), PHOTOS_HEADER_BAR_MODE_STANDALONE);

  cancel_button = gtk_button_new_with_label (_("Cancel"));
  gtk_actionable_set_action_name (GTK_ACTIONABLE (cancel_button), "app.edit-cancel");
  gtk_header_bar_pack_start (GTK_HEADER_BAR (self->toolbar), cancel_button);

  done_button = gtk_button_new_with_label (_("Done"));
  gtk_actionable_set_action_name (GTK_ACTIONABLE (done_button), "app.edit-done");
  gtk_header_bar_pack_end (GTK_HEADER_BAR (self->toolbar), done_button);
  context = gtk_widget_get_style_context (done_button);
  gtk_style_context_add_class (context, "suggested-action");
}


static void
photos_main_toolbar_populate_for_favorites (PhotosMainToolbar *self)
{
  PhotosBaseItem *collection;

  gtk_header_bar_set_show_close_button (GTK_HEADER_BAR (self->toolbar), TRUE);
  photos_header_bar_set_mode (PHOTOS_HEADER_BAR (self->toolbar), PHOTOS_HEADER_BAR_MODE_NORMAL);
  self->selection_button = photos_main_toolbar_add_selection_button (self);
  photos_main_toolbar_add_search_button (self);

  if (gtk_widget_get_parent (self->searchbar) == NULL)
    gtk_container_add (GTK_CONTAINER (self), self->searchbar);

  collection = photos_item_manager_get_active_collection (PHOTOS_ITEM_MANAGER (self->item_mngr));
  photos_main_toolbar_col_active_changed (self, collection);
}


static void
photos_main_toolbar_populate_for_overview (PhotosMainToolbar *self)
{
  PhotosBaseItem *collection;

  gtk_header_bar_set_show_close_button (GTK_HEADER_BAR (self->toolbar), TRUE);
  photos_header_bar_set_mode (PHOTOS_HEADER_BAR (self->toolbar), PHOTOS_HEADER_BAR_MODE_NORMAL);
  self->selection_button = photos_main_toolbar_add_selection_button (self);
  photos_main_toolbar_add_search_button (self);

  if (gtk_widget_get_parent (self->searchbar) == NULL)
    gtk_container_add (GTK_CONTAINER (self), self->searchbar);

  collection = photos_item_manager_get_active_collection (PHOTOS_ITEM_MANAGER (self->item_mngr));
  photos_main_toolbar_col_active_changed (self, collection);
}


static void
photos_main_toolbar_populate_for_preview (PhotosMainToolbar *self)
{
  GMenu *preview_menu;
  GtkWidget *back_button;
  GtkWidget *edit_button;
  GtkWidget *image;
  GtkWidget *menu_button;
  GtkWidget *share_button;
  GApplication *app;
  PhotosBaseItem *item;
  gboolean favorite;
  gboolean remote_display_available;
  GAction *remote_display_action;

  gtk_header_bar_set_show_close_button (GTK_HEADER_BAR (self->toolbar), TRUE);
  photos_header_bar_set_mode (PHOTOS_HEADER_BAR (self->toolbar), PHOTOS_HEADER_BAR_MODE_STANDALONE);

  back_button = photos_main_toolbar_add_back_button (self);
  g_signal_connect_swapped (back_button, "clicked", G_CALLBACK (photos_main_toolbar_back_button_clicked), self);

  preview_menu = photos_main_toolbar_create_preview_menu (self);
  image = gtk_image_new_from_icon_name (PHOTOS_ICON_SYSTEM_SYMBOLIC, GTK_ICON_SIZE_BUTTON);
  menu_button = gtk_menu_button_new ();
  gtk_actionable_set_action_name (GTK_ACTIONABLE (menu_button), "app.gear-menu");
  gtk_button_set_image (GTK_BUTTON (menu_button), image);
  gtk_menu_button_set_menu_model (GTK_MENU_BUTTON (menu_button), G_MENU_MODEL (preview_menu));
  gtk_header_bar_pack_end (GTK_HEADER_BAR (self->toolbar), menu_button);

  g_simple_action_set_enabled (self->gear_menu, TRUE);

  share_button = gtk_button_new_from_icon_name (PHOTOS_ICON_IMAGE_SHARE_SYMBOLIC, GTK_ICON_SIZE_BUTTON);
  gtk_actionable_set_action_name (GTK_ACTIONABLE (share_button), "app.share-current");
  gtk_header_bar_pack_end (GTK_HEADER_BAR (self->toolbar), share_button);

  edit_button = gtk_button_new_from_icon_name (PHOTOS_ICON_IMAGE_EDIT_SYMBOLIC, GTK_ICON_SIZE_BUTTON);
  gtk_actionable_set_action_name (GTK_ACTIONABLE (edit_button), "app.edit-current");
  gtk_header_bar_pack_end (GTK_HEADER_BAR (self->toolbar), edit_button);

  self->favorite_button = gtk_button_new ();
  gtk_header_bar_pack_end (GTK_HEADER_BAR (self->toolbar), self->favorite_button);
  g_signal_connect_swapped (self->favorite_button,
                            "clicked",
                            G_CALLBACK (photos_main_toolbar_favorite_button_clicked),
                            self);

  item = PHOTOS_BASE_ITEM (photos_base_manager_get_active_object (self->item_mngr));
  favorite = photos_base_item_is_favorite (item);
  photos_main_toolbar_favorite_button_update (self, favorite);

  /* Disable the remote-display-current action if the dLeyna services are not
   * available */
  app = g_application_get_default ();
  remote_display_action = g_action_map_lookup_action (G_ACTION_MAP (app), "remote-display-current");
  remote_display_available = photos_dlna_renderers_manager_is_available ();
  g_simple_action_set_enabled (G_SIMPLE_ACTION (remote_display_action), remote_display_available);

  g_signal_connect_object (self->item_mngr,
                           "active-changed",
                           G_CALLBACK (photos_main_toolbar_item_active_changed),
                           self,
                           G_CONNECT_SWAPPED);
}


static void
photos_main_toolbar_populate_for_search (PhotosMainToolbar *self)
{
  PhotosBaseItem *collection;

  gtk_header_bar_set_show_close_button (GTK_HEADER_BAR (self->toolbar), TRUE);
  photos_header_bar_set_mode (PHOTOS_HEADER_BAR (self->toolbar), PHOTOS_HEADER_BAR_MODE_NORMAL);
  self->selection_button = photos_main_toolbar_add_selection_button (self);
  photos_main_toolbar_add_search_button (self);

  if (gtk_widget_get_parent (self->searchbar) == NULL)
    gtk_container_add (GTK_CONTAINER (self), self->searchbar);

  collection = photos_item_manager_get_active_collection (PHOTOS_ITEM_MANAGER (self->item_mngr));
  photos_main_toolbar_col_active_changed (self, collection);
}


static void
photos_main_toolbar_populate_for_selection_mode (PhotosMainToolbar *self)
{
  GtkWidget *selection_button;

  photos_header_bar_set_mode (PHOTOS_HEADER_BAR (self->toolbar), PHOTOS_HEADER_BAR_MODE_SELECTION);

  selection_button = gtk_button_new_with_label (_("Cancel"));
  gtk_header_bar_pack_end (GTK_HEADER_BAR (self->toolbar), selection_button);
  g_signal_connect_swapped (selection_button,
                            "clicked",
                            G_CALLBACK (photos_main_toolbar_done_button_clicked),
                            self);

  g_signal_connect_object (self->sel_cntrlr,
                           "selection-changed",
                           G_CALLBACK (photos_main_toolbar_set_toolbar_title),
                           self,
                           G_CONNECT_SWAPPED);

  photos_main_toolbar_add_search_button (self);

  if (gtk_widget_get_parent (self->searchbar) == NULL)
    gtk_container_add (GTK_CONTAINER (self), self->searchbar);
}


static void
photos_main_toolbar_constructed (GObject *object)
{
  PhotosMainToolbar *self = PHOTOS_MAIN_TOOLBAR (object);

  G_OBJECT_CLASS (photos_main_toolbar_parent_class)->constructed (object);

  self->searchbar = photos_main_toolbar_create_overview_searchbar (self);
  g_object_ref (self->searchbar);

  photos_main_toolbar_reset_toolbar_mode (self);
}


static void
photos_main_toolbar_dispose (GObject *object)
{
  PhotosMainToolbar *self = PHOTOS_MAIN_TOOLBAR (object);

  photos_main_toolbar_clear_state_data (self);

  g_clear_object (&self->searchbar);
  g_clear_object (&self->item_mngr);
  g_clear_object (&self->mode_cntrlr);
  g_clear_object (&self->remote_mngr);
  g_clear_object (&self->sel_cntrlr);

  G_OBJECT_CLASS (photos_main_toolbar_parent_class)->dispose (object);
}


static void
photos_main_toolbar_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
  PhotosMainToolbar *self = PHOTOS_MAIN_TOOLBAR (object);

  switch (prop_id)
    {
    case PROP_OVERLAY:
      self->overlay = GTK_WIDGET (g_value_dup_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
photos_main_toolbar_share_changed_cb (PhotosMainToolbar          *self,
                                      PhotosDlnaRenderer         *renderer,
                                      PhotosBaseItem             *item,
                                      PhotosRemoteDisplayManager *remote_mngr)
{
  photos_main_toolbar_update_remote_display_button (self);
}


static void
photos_main_toolbar_share_error_cb (PhotosMainToolbar          *self,
                                    PhotosDlnaRenderer         *renderer,
                                    PhotosBaseItem             *item,
                                    GError                     *error,
                                    PhotosRemoteDisplayManager *remote_mngr)
{
  photos_main_toolbar_update_remote_display_button (self);

  g_warning ("Error sharing item with remote display: %s", error->message);
}


static void
photos_main_toolbar_init (PhotosMainToolbar *self)
{
  GMenu *selection_menu;
  GApplication *app;
  GtkBuilder *builder;
  PhotosSearchContextState *state;

  app = g_application_get_default ();
  state = photos_search_context_get_state (PHOTOS_SEARCH_CONTEXT (app));

  gtk_orientable_set_orientation (GTK_ORIENTABLE (self), GTK_ORIENTATION_VERTICAL);
  gtk_widget_show (GTK_WIDGET (self));

  self->toolbar = photos_header_bar_new ();
  gtk_container_add (GTK_CONTAINER (self), self->toolbar);
  gtk_widget_show (self->toolbar);

  self->gear_menu = G_SIMPLE_ACTION (g_action_map_lookup_action (G_ACTION_MAP (app), "gear-menu"));
  self->search = g_action_map_lookup_action (G_ACTION_MAP (app), "search");

  builder = gtk_builder_new_from_resource ("/org/gnome/Photos/selection-menu.ui");

  selection_menu = G_MENU (gtk_builder_get_object (builder, "selection-menu"));
  self->selection_menu = gtk_menu_button_new ();
  gtk_menu_button_set_menu_model (GTK_MENU_BUTTON (self->selection_menu), G_MENU_MODEL (selection_menu));
  g_object_unref (builder);

  photos_header_bar_set_selection_menu (PHOTOS_HEADER_BAR (self->toolbar), GTK_BUTTON (self->selection_menu));

  self->item_mngr = g_object_ref (state->item_mngr);

  self->mode_cntrlr = g_object_ref (state->mode_cntrlr);

  self->sel_cntrlr = photos_selection_controller_dup_singleton ();
  g_signal_connect_object (self->sel_cntrlr,
                           "selection-mode-changed",
                           G_CALLBACK (photos_main_toolbar_reset_toolbar_mode),
                           self,
                           G_CONNECT_SWAPPED);

  self->remote_mngr = photos_remote_display_manager_dup_singleton ();
  g_signal_connect_object (self->remote_mngr,
                           "share-began",
                           G_CALLBACK (photos_main_toolbar_share_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (self->remote_mngr,
                           "share-ended",
                           G_CALLBACK (photos_main_toolbar_share_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (self->remote_mngr,
                           "share-error",
                           G_CALLBACK (photos_main_toolbar_share_error_cb),
                           self,
                           G_CONNECT_SWAPPED);
}


static void
photos_main_toolbar_class_init (PhotosMainToolbarClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->constructed = photos_main_toolbar_constructed;
  object_class->dispose = photos_main_toolbar_dispose;
  object_class->set_property = photos_main_toolbar_set_property;

  g_object_class_install_property (object_class,
                                   PROP_OVERLAY,
                                   g_param_spec_object ("overlay",
                                                        "GtkOverlay object",
                                                        "The stack overlay widget",
                                                        GTK_TYPE_OVERLAY,
                                                        G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE));
}


GtkWidget *
photos_main_toolbar_new (GtkOverlay *overlay)
{
  return g_object_new (PHOTOS_TYPE_MAIN_TOOLBAR, "overlay", overlay, NULL);
}


PhotosSearchbar *
photos_main_toolbar_get_searchbar (PhotosMainToolbar *self)
{
  return PHOTOS_SEARCHBAR (self->searchbar);
}


gboolean
photos_main_toolbar_handle_event (PhotosMainToolbar *self, GdkEventKey *event)
{
  gboolean ret_val = FALSE;

  ret_val = photos_searchbar_handle_event (PHOTOS_SEARCHBAR (self->searchbar), event);
  return ret_val;
}


gboolean
photos_main_toolbar_is_focus (PhotosMainToolbar *self)
{
  return photos_searchbar_is_focus (PHOTOS_SEARCHBAR (self->searchbar));
}


void
photos_main_toolbar_reset_toolbar_mode (PhotosMainToolbar *self)
{
  PhotosWindowMode window_mode;
  gboolean selection_mode;

  photos_main_toolbar_clear_toolbar (self);
  selection_mode = photos_selection_controller_get_selection_mode (self->sel_cntrlr);
  window_mode = photos_mode_controller_get_window_mode (self->mode_cntrlr);

  if (selection_mode)
    photos_main_toolbar_populate_for_selection_mode (self);
  else if (window_mode == PHOTOS_WINDOW_MODE_COLLECTIONS)
    photos_main_toolbar_populate_for_collections (self);
  else if (window_mode == PHOTOS_WINDOW_MODE_EDIT)
    photos_main_toolbar_populate_for_edit (self);
  else if (window_mode == PHOTOS_WINDOW_MODE_FAVORITES)
    photos_main_toolbar_populate_for_favorites (self);
  else if (window_mode == PHOTOS_WINDOW_MODE_OVERVIEW)
    photos_main_toolbar_populate_for_overview (self);
  else if (window_mode == PHOTOS_WINDOW_MODE_PREVIEW)
    photos_main_toolbar_populate_for_preview (self);
  else if (window_mode == PHOTOS_WINDOW_MODE_SEARCH)
    photos_main_toolbar_populate_for_search (self);

  photos_main_toolbar_update_remote_display_button (self);

  photos_main_toolbar_set_toolbar_title (self);
  gtk_widget_show_all (self->toolbar);
}


void
photos_main_toolbar_set_stack (PhotosMainToolbar *self, GtkStack *stack)
{
  photos_header_bar_set_stack (PHOTOS_HEADER_BAR (self->toolbar), stack);
}


void
photos_main_toolbar_set_view_model (PhotosMainToolbar *self, PhotosViewModel *model)
{
  gboolean is_empty;

  is_empty = (gtk_tree_model_iter_n_children (GTK_TREE_MODEL (model), NULL) == 0);
  g_simple_action_set_enabled (G_SIMPLE_ACTION (self->search), !is_empty);
  if (self->selection_button != NULL)
    gtk_widget_set_sensitive (self->selection_button, !is_empty);
}
