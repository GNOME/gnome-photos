/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2012, 2013, 2014 Red Hat, Inc.
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
#include "photos-mode-controller.h"
#include "photos-overview-searchbar.h"
#include "photos-remote-display-manager.h"
#include "photos-search-context.h"
#include "photos-selection-controller.h"


struct _PhotosMainToolbarPrivate
{
  GAction *search;
  GCancellable *loader_cancellable;
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

enum
{
  PROP_0,
  PROP_OVERLAY
};


G_DEFINE_TYPE_WITH_PRIVATE (PhotosMainToolbar, photos_main_toolbar, GTK_TYPE_BOX);


static void photos_main_toolbar_favorite_button_update (PhotosMainToolbar *self, gboolean favorite);


static void
photos_main_toolbar_set_toolbar_title (PhotosMainToolbar *self)
{
  PhotosMainToolbarPrivate *priv = self->priv;
  PhotosBaseItem *active_collection;
  PhotosWindowMode window_mode;
  gboolean selection_mode;
  gchar *primary = NULL;

  active_collection = photos_item_manager_get_active_collection (PHOTOS_ITEM_MANAGER (priv->item_mngr));
  selection_mode = photos_selection_controller_get_selection_mode (priv->sel_cntrlr);
  window_mode = photos_mode_controller_get_window_mode (priv->mode_cntrlr);

  if (window_mode == PHOTOS_WINDOW_MODE_OVERVIEW
      || window_mode == PHOTOS_WINDOW_MODE_COLLECTIONS
      || window_mode == PHOTOS_WINDOW_MODE_FAVORITES
      || window_mode == PHOTOS_WINDOW_MODE_SEARCH)
    {
      if (!selection_mode)
        {
          if (active_collection != NULL)
            primary = g_markup_printf_escaped ("%s", photos_base_item_get_name (active_collection));
        }
      else
        {
          GList *selection;
          gchar *label;
          guint length;

          selection = photos_selection_controller_get_selection (priv->sel_cntrlr);
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
              primary = g_markup_printf_escaped ("%s", label);
            }

          g_free (label);
        }
    }
  else if (window_mode == PHOTOS_WINDOW_MODE_PREVIEW)
    {
      GObject *item;

      item = photos_base_manager_get_active_object (priv->item_mngr);
      if (item != NULL)
        primary = g_markup_printf_escaped ("%s", photos_base_item_get_name (PHOTOS_BASE_ITEM (item)));
    }

  if (selection_mode)
    {
      if (primary != NULL)
        {
          GtkWidget *label;

          gtk_button_set_label (GTK_BUTTON (priv->selection_menu), primary);
          label = gtk_bin_get_child (GTK_BIN (priv->selection_menu));
          gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
        }
    }
  else
    gtk_header_bar_set_title (GTK_HEADER_BAR (priv->toolbar), primary);

  g_free (primary);
}


static GtkWidget *
photos_main_toolbar_add_back_button (PhotosMainToolbar *self)
{
  PhotosMainToolbarPrivate *priv = self->priv;
  GtkWidget *back_button;

  back_button = gtk_button_new_from_icon_name (PHOTOS_ICON_GO_PREVIOUS_SYMBOLIC, GTK_ICON_SIZE_BUTTON);
  gtk_widget_set_tooltip_text (back_button, _("Back"));
  gtk_header_bar_pack_start (GTK_HEADER_BAR (priv->toolbar), back_button);

  return back_button;
}


static void
photos_main_toolbar_coll_back_button_clicked (PhotosMainToolbar *self)
{
  photos_item_manager_activate_previous_collection (PHOTOS_ITEM_MANAGER (self->priv->item_mngr));
}


static void
photos_main_toolbar_col_active_changed (PhotosMainToolbar *self, PhotosBaseItem *collection)
{
  PhotosMainToolbarPrivate *priv = self->priv;
  PhotosHeaderBarMode mode;

  if (collection != NULL)
    {
      mode = PHOTOS_HEADER_BAR_MODE_STANDALONE;
      if (priv->coll_back_button == NULL)
        {
          priv->coll_back_button = photos_main_toolbar_add_back_button (self);
          gtk_widget_show (priv->coll_back_button);

          g_signal_connect_swapped (priv->coll_back_button,
                                    "clicked",
                                    G_CALLBACK (photos_main_toolbar_coll_back_button_clicked),
                                    self);
        }
    }
  else
    {
      mode = PHOTOS_HEADER_BAR_MODE_NORMAL;
      if (priv->coll_back_button != NULL)
        {
          gtk_widget_destroy (priv->coll_back_button);
          priv->coll_back_button = NULL;
        }
    }

  photos_header_bar_set_mode (PHOTOS_HEADER_BAR (priv->toolbar), mode);
  photos_main_toolbar_set_toolbar_title (self);
}


static void
photos_main_toolbar_item_active_changed (PhotosMainToolbar *self, GObject *object)
{
  PhotosMainToolbarPrivate *priv = self->priv;
  PhotosWindowMode window_mode;
  gboolean favorite;

  if (object == NULL)
    return;

  window_mode = photos_mode_controller_get_window_mode (priv->mode_cntrlr);
  if (window_mode != PHOTOS_WINDOW_MODE_PREVIEW)
    return;

  photos_main_toolbar_set_toolbar_title (self);

  favorite = photos_base_item_is_favorite (PHOTOS_BASE_ITEM (object));
  photos_main_toolbar_favorite_button_update (self, favorite);
}


static void
photos_main_toolbar_select_button_clicked (PhotosMainToolbar *self)
{
  photos_selection_controller_set_selection_mode (self->priv->sel_cntrlr, TRUE);
}


static void
photos_main_toolbar_remote_display_button_clicked (PhotosMainToolbar *self)
{
  PhotosMainToolbarPrivate *priv = self->priv;

  photos_remote_display_manager_stop (priv->remote_mngr);
}


static void
photos_main_toolbar_add_remote_display_button (PhotosMainToolbar *self)
{
  PhotosMainToolbarPrivate *priv = self->priv;
  PhotosDlnaRenderer *renderer;
  GtkLabel *label;
  gchar *text;
  const gchar *name;

  if (priv->remote_display_button != NULL)
    gtk_widget_destroy (priv->remote_display_button);

  renderer = photos_remote_display_manager_get_renderer (priv->remote_mngr);
  name = photos_dlna_renderer_get_friendly_name (renderer);
  text = g_markup_printf_escaped ("Displaying on <b>%s</b>", name);

  priv->remote_display_button = gtk_button_new_with_label (text);
  label = GTK_LABEL (gtk_bin_get_child (GTK_BIN (priv->remote_display_button)));
  gtk_label_set_ellipsize (label, PANGO_ELLIPSIZE_MIDDLE);
  gtk_label_set_use_markup (label, TRUE);
  gtk_widget_set_margin_end (priv->remote_display_button, 12);
  gtk_header_bar_pack_start (GTK_HEADER_BAR (priv->toolbar), priv->remote_display_button);
  gtk_widget_show_all (priv->remote_display_button);

  g_signal_connect_swapped (priv->remote_display_button,
                            "clicked",
                            G_CALLBACK (photos_main_toolbar_remote_display_button_clicked),
                            self);
  g_free (text);
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
  gtk_header_bar_pack_end (GTK_HEADER_BAR (self->priv->toolbar), search_button);

  return search_button;
}


static GtkWidget *
photos_main_toolbar_add_selection_button (PhotosMainToolbar *self)
{
  PhotosMainToolbarPrivate *priv = self->priv;
  GtkWidget *selection_button;

  selection_button = gtk_button_new_from_icon_name (PHOTOS_ICON_OBJECT_SELECT_SYMBOLIC, GTK_ICON_SIZE_BUTTON);
  gtk_widget_set_tooltip_text (selection_button, _("Select Items"));
  gtk_header_bar_pack_end (GTK_HEADER_BAR (priv->toolbar), selection_button);
  g_signal_connect_swapped (selection_button,
                            "clicked",
                            G_CALLBACK (photos_main_toolbar_select_button_clicked),
                            self);

  g_signal_connect_object (priv->item_mngr,
                           "active-collection-changed",
                           G_CALLBACK (photos_main_toolbar_col_active_changed),
                           self,
                           G_CONNECT_SWAPPED);

  return selection_button;
}


static void
photos_main_toolbar_back_button_clicked (PhotosMainToolbar *self)
{
  PhotosMainToolbarPrivate *priv = self->priv;

  if (priv->loader_cancellable != NULL)
    g_cancellable_cancel (priv->loader_cancellable);

  photos_base_manager_set_active_object (priv->item_mngr, NULL);
  photos_mode_controller_go_back (priv->mode_cntrlr);
}


static void
photos_main_toolbar_clear_state_data (PhotosMainToolbar *self)
{
  PhotosMainToolbarPrivate *priv = self->priv;

  if (priv->coll_back_button != NULL)
    {
      gtk_widget_destroy (priv->coll_back_button);
      priv->coll_back_button = NULL;
    }

  if (priv->remote_display_button != NULL)
    {
      gtk_widget_destroy (priv->remote_display_button);
      priv->remote_display_button = NULL;
    }

  if (priv->selection_button != NULL)
    {
      gtk_widget_destroy (priv->selection_button);
      priv->selection_button = NULL;
    }

  if (priv->searchbar != NULL && gtk_widget_get_parent (priv->searchbar) == GTK_WIDGET (self))
    {
      GVariant *state;

      state = g_action_get_state (priv->search);
      if (!g_variant_get_boolean (state))
        gtk_container_remove (GTK_CONTAINER (self), priv->searchbar);

      g_variant_unref (state);
    }

  if (priv->item_mngr != NULL)
    {
      g_signal_handlers_disconnect_by_func (priv->item_mngr, photos_main_toolbar_col_active_changed, self);
      g_signal_handlers_disconnect_by_func (priv->item_mngr, photos_main_toolbar_item_active_changed, self);
    }

  if (priv->sel_cntrlr != NULL)
    g_signal_handlers_disconnect_by_func (priv->sel_cntrlr, photos_main_toolbar_set_toolbar_title, self);
}


static void
photos_main_toolbar_clear_toolbar (PhotosMainToolbar *self)
{
  PhotosMainToolbarPrivate *priv = self->priv;

  photos_main_toolbar_clear_state_data (self);
  gtk_header_bar_set_show_close_button (GTK_HEADER_BAR (priv->toolbar), FALSE);
  photos_header_bar_clear (PHOTOS_HEADER_BAR (priv->toolbar));
  g_simple_action_set_enabled (priv->gear_menu, FALSE);
}


static GtkWidget *
photos_main_toolbar_create_overview_searchbar (PhotosMainToolbar *self)
{
  GtkWidget *dropdown;
  GtkWidget *searchbar;

  dropdown = photos_dropdown_new ();
  gtk_overlay_add_overlay (GTK_OVERLAY (self->priv->overlay), dropdown);
  searchbar = photos_overview_searchbar_new (PHOTOS_DROPDOWN (dropdown));

  return searchbar;
}


static GMenu *
photos_main_toolbar_create_preview_menu (PhotosMainToolbar *self)
{
  GMenu *menu;
  GtkBuilder *builder;
  PhotosBaseItem *item;

  builder = gtk_builder_new ();
  gtk_builder_add_from_resource (builder, "/org/gnome/photos/preview-menu.ui", NULL);

  menu = G_MENU (g_object_ref (gtk_builder_get_object (builder, "preview-menu")));
  item = PHOTOS_BASE_ITEM (photos_base_manager_get_active_object (self->priv->item_mngr));
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
  photos_selection_controller_set_selection_mode (self->priv->sel_cntrlr, FALSE);
}


static void
photos_main_toolbar_favorite_button_clicked (PhotosMainToolbar *self)
{
  PhotosBaseItem *item;
  gboolean favorite;

  item = PHOTOS_BASE_ITEM (photos_base_manager_get_active_object (self->priv->item_mngr));
  favorite = photos_base_item_is_favorite (item);
  photos_base_item_set_favorite (item, !favorite);

  photos_main_toolbar_favorite_button_update (self, !favorite);
}


static void
photos_main_toolbar_favorite_button_update (PhotosMainToolbar *self, gboolean favorite)
{
  PhotosMainToolbarPrivate *priv = self->priv;
  GtkStyleContext *context;
  gchar *favorite_label;

  g_signal_handlers_block_by_func (priv->favorite_button, photos_main_toolbar_favorite_button_clicked, self);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->favorite_button), favorite);
  g_signal_handlers_unblock_by_func (priv->favorite_button, photos_main_toolbar_favorite_button_clicked, self);

  context = gtk_widget_get_style_context (priv->favorite_button);
  if (favorite)
    {
      favorite_label = g_strdup (_("Remove from favorites"));
      gtk_style_context_add_class (context, "photos-favorite");
    }
  else
    {
      favorite_label = g_strdup (_("Add to favorites"));
      gtk_style_context_remove_class (context, "photos-favorite");
    }

  gtk_widget_reset_style (priv->favorite_button);
  gtk_widget_set_tooltip_text (priv->favorite_button, favorite_label);
  g_free (favorite_label);
}


static void
photos_main_toolbar_populate_for_collections (PhotosMainToolbar *self)
{
  PhotosMainToolbarPrivate *priv = self->priv;
  PhotosBaseItem *collection;

  gtk_header_bar_set_show_close_button (GTK_HEADER_BAR (priv->toolbar), TRUE);
  photos_header_bar_set_mode (PHOTOS_HEADER_BAR (priv->toolbar), PHOTOS_HEADER_BAR_MODE_NORMAL);
  priv->selection_button = photos_main_toolbar_add_selection_button (self);
  photos_main_toolbar_add_search_button (self);

  if (gtk_widget_get_parent (priv->searchbar) == NULL)
    gtk_container_add (GTK_CONTAINER (self), priv->searchbar);

  collection = photos_item_manager_get_active_collection (PHOTOS_ITEM_MANAGER (priv->item_mngr));
  photos_main_toolbar_col_active_changed (self, collection);
}


static void
photos_main_toolbar_update_remote_display_button (PhotosMainToolbar *self)
{
  PhotosMainToolbarPrivate *priv = self->priv;
  PhotosWindowMode window_mode;
  gboolean selection_mode, active;

  selection_mode = photos_selection_controller_get_selection_mode (priv->sel_cntrlr);
  window_mode = photos_mode_controller_get_window_mode (priv->mode_cntrlr);
  active = photos_remote_display_manager_is_active (priv->remote_mngr);

  if (active && !selection_mode && window_mode != PHOTOS_WINDOW_MODE_PREVIEW)
    photos_main_toolbar_add_remote_display_button (self);
  else
    g_clear_pointer (&priv->remote_display_button, gtk_widget_destroy);
}


static void
photos_main_toolbar_populate_for_favorites (PhotosMainToolbar *self)
{
  PhotosMainToolbarPrivate *priv = self->priv;
  PhotosBaseItem *collection;

  gtk_header_bar_set_show_close_button (GTK_HEADER_BAR (priv->toolbar), TRUE);
  photos_header_bar_set_mode (PHOTOS_HEADER_BAR (priv->toolbar), PHOTOS_HEADER_BAR_MODE_NORMAL);
  priv->selection_button = photos_main_toolbar_add_selection_button (self);
  photos_main_toolbar_add_search_button (self);

  if (gtk_widget_get_parent (priv->searchbar) == NULL)
    gtk_container_add (GTK_CONTAINER (self), priv->searchbar);

  collection = photos_item_manager_get_active_collection (PHOTOS_ITEM_MANAGER (priv->item_mngr));
  photos_main_toolbar_col_active_changed (self, collection);
}


static void
photos_main_toolbar_populate_for_overview (PhotosMainToolbar *self)
{
  PhotosMainToolbarPrivate *priv = self->priv;
  PhotosBaseItem *collection;

  gtk_header_bar_set_show_close_button (GTK_HEADER_BAR (priv->toolbar), TRUE);
  photos_header_bar_set_mode (PHOTOS_HEADER_BAR (priv->toolbar), PHOTOS_HEADER_BAR_MODE_NORMAL);
  priv->selection_button = photos_main_toolbar_add_selection_button (self);
  photos_main_toolbar_add_search_button (self);

  if (gtk_widget_get_parent (priv->searchbar) == NULL)
    gtk_container_add (GTK_CONTAINER (self), priv->searchbar);

  collection = photos_item_manager_get_active_collection (PHOTOS_ITEM_MANAGER (priv->item_mngr));
  photos_main_toolbar_col_active_changed (self, collection);
}


static void
photos_main_toolbar_populate_for_preview (PhotosMainToolbar *self)
{
  PhotosMainToolbarPrivate *priv = self->priv;
  GMenu *preview_menu;
  GtkWidget *back_button;
  GtkWidget *image;
  GtkWidget *menu_button;
  GApplication *app;
  PhotosBaseItem *item;
  gboolean favorite;
  gboolean remote_display_available;
  GAction *remote_display_action;

  gtk_header_bar_set_show_close_button (GTK_HEADER_BAR (priv->toolbar), TRUE);
  photos_header_bar_set_mode (PHOTOS_HEADER_BAR (priv->toolbar), PHOTOS_HEADER_BAR_MODE_STANDALONE);

  back_button = photos_main_toolbar_add_back_button (self);
  g_signal_connect_swapped (back_button, "clicked", G_CALLBACK (photos_main_toolbar_back_button_clicked), self);

  preview_menu = photos_main_toolbar_create_preview_menu (self);
  image = gtk_image_new_from_icon_name (PHOTOS_ICON_SYSTEM_SYMBOLIC, GTK_ICON_SIZE_BUTTON);
  menu_button = gtk_menu_button_new ();
  gtk_actionable_set_action_name (GTK_ACTIONABLE (menu_button), "app.gear-menu");
  gtk_button_set_image (GTK_BUTTON (menu_button), image);
  gtk_menu_button_set_menu_model (GTK_MENU_BUTTON (menu_button), G_MENU_MODEL (preview_menu));
  gtk_header_bar_pack_end (GTK_HEADER_BAR (priv->toolbar), menu_button);

  g_simple_action_set_enabled (priv->gear_menu, TRUE);

  image = gtk_image_new_from_icon_name (PHOTOS_ICON_FAVORITE_SYMBOLIC, GTK_ICON_SIZE_BUTTON);
  priv->favorite_button = gtk_toggle_button_new ();
  gtk_button_set_image (GTK_BUTTON (priv->favorite_button), image);
  gtk_header_bar_pack_end (GTK_HEADER_BAR (priv->toolbar), priv->favorite_button);
  g_signal_connect_swapped (priv->favorite_button,
                            "clicked",
                            G_CALLBACK (photos_main_toolbar_favorite_button_clicked),
                            self);

  item = PHOTOS_BASE_ITEM (photos_base_manager_get_active_object (priv->item_mngr));
  favorite = photos_base_item_is_favorite (item);
  photos_main_toolbar_favorite_button_update (self, favorite);

  /* Disable the remote-display-current action if the dLeyna services are not
   * available */
  app = g_application_get_default ();
  remote_display_action = g_action_map_lookup_action (G_ACTION_MAP (app), "remote-display-current");
  remote_display_available = photos_dlna_renderers_manager_is_available ();
  g_simple_action_set_enabled (G_SIMPLE_ACTION (remote_display_action), remote_display_available);

  g_signal_connect_object (priv->item_mngr,
                           "active-changed",
                           G_CALLBACK (photos_main_toolbar_item_active_changed),
                           self,
                           G_CONNECT_SWAPPED);
}


static void
photos_main_toolbar_populate_for_search (PhotosMainToolbar *self)
{
  PhotosMainToolbarPrivate *priv = self->priv;
  PhotosBaseItem *collection;

  gtk_header_bar_set_show_close_button (GTK_HEADER_BAR (priv->toolbar), TRUE);
  photos_header_bar_set_mode (PHOTOS_HEADER_BAR (priv->toolbar), PHOTOS_HEADER_BAR_MODE_NORMAL);
  priv->selection_button = photos_main_toolbar_add_selection_button (self);
  photos_main_toolbar_add_search_button (self);

  if (gtk_widget_get_parent (priv->searchbar) == NULL)
    gtk_container_add (GTK_CONTAINER (self), priv->searchbar);

  collection = photos_item_manager_get_active_collection (PHOTOS_ITEM_MANAGER (priv->item_mngr));
  photos_main_toolbar_col_active_changed (self, collection);
}


static void
photos_main_toolbar_populate_for_selection_mode (PhotosMainToolbar *self)
{
  PhotosMainToolbarPrivate *priv = self->priv;
  GtkWidget *selection_button;

  photos_header_bar_set_mode (PHOTOS_HEADER_BAR (priv->toolbar), PHOTOS_HEADER_BAR_MODE_SELECTION);

  selection_button = gtk_button_new_with_label (_("Cancel"));
  gtk_header_bar_pack_end (GTK_HEADER_BAR (priv->toolbar), selection_button);
  g_signal_connect_swapped (selection_button,
                            "clicked",
                            G_CALLBACK (photos_main_toolbar_done_button_clicked),
                            self);

  g_signal_connect_object (priv->sel_cntrlr,
                           "selection-changed",
                           G_CALLBACK (photos_main_toolbar_set_toolbar_title),
                           self,
                           G_CONNECT_SWAPPED);

  photos_main_toolbar_add_search_button (self);

  if (gtk_widget_get_parent (priv->searchbar) == NULL)
    gtk_container_add (GTK_CONTAINER (self), priv->searchbar);
}


static void
photos_main_toolbar_constructed (GObject *object)
{
  PhotosMainToolbar *self = PHOTOS_MAIN_TOOLBAR (object);
  PhotosMainToolbarPrivate *priv = self->priv;

  G_OBJECT_CLASS (photos_main_toolbar_parent_class)->constructed (object);

  priv->searchbar = photos_main_toolbar_create_overview_searchbar (self);
  g_object_ref (priv->searchbar);

  photos_main_toolbar_reset_toolbar_mode (self);
}


static void
photos_main_toolbar_dispose (GObject *object)
{
  PhotosMainToolbar *self = PHOTOS_MAIN_TOOLBAR (object);
  PhotosMainToolbarPrivate *priv = self->priv;

  photos_main_toolbar_clear_state_data (self);

  g_clear_object (&priv->loader_cancellable);
  g_clear_object (&priv->searchbar);
  g_clear_object (&priv->item_mngr);
  g_clear_object (&priv->mode_cntrlr);
  g_clear_object (&priv->remote_mngr);
  g_clear_object (&priv->sel_cntrlr);

  G_OBJECT_CLASS (photos_main_toolbar_parent_class)->dispose (object);
}


static void
photos_main_toolbar_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
  PhotosMainToolbar *self = PHOTOS_MAIN_TOOLBAR (object);

  switch (prop_id)
    {
    case PROP_OVERLAY:
      self->priv->overlay = GTK_WIDGET (g_value_dup_object (value));
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
  PhotosMainToolbarPrivate *priv;
  GMenu *selection_menu;
  GApplication *app;
  GtkBuilder *builder;
  PhotosSearchContextState *state;

  self->priv = photos_main_toolbar_get_instance_private (self);
  priv = self->priv;

  app = g_application_get_default ();
  state = photos_search_context_get_state (PHOTOS_SEARCH_CONTEXT (app));

  gtk_orientable_set_orientation (GTK_ORIENTABLE (self), GTK_ORIENTATION_VERTICAL);
  gtk_widget_show (GTK_WIDGET (self));

  priv->toolbar = photos_header_bar_new ();
  gtk_container_add (GTK_CONTAINER (self), priv->toolbar);
  gtk_widget_show (priv->toolbar);

  priv->gear_menu = G_SIMPLE_ACTION (g_action_map_lookup_action (G_ACTION_MAP (app), "gear-menu"));
  priv->search = g_action_map_lookup_action (G_ACTION_MAP (app), "search");

  builder = gtk_builder_new ();
  gtk_builder_add_from_resource (builder, "/org/gnome/photos/selection-menu.ui", NULL);

  selection_menu = G_MENU (gtk_builder_get_object (builder, "selection-menu"));
  priv->selection_menu = gtk_menu_button_new ();
  gtk_menu_button_set_menu_model (GTK_MENU_BUTTON (priv->selection_menu), G_MENU_MODEL (selection_menu));
  g_object_unref (builder);

  photos_header_bar_set_selection_menu (PHOTOS_HEADER_BAR (priv->toolbar), GTK_BUTTON (priv->selection_menu));

  priv->item_mngr = g_object_ref (state->item_mngr);

  priv->mode_cntrlr = photos_mode_controller_dup_singleton ();

  priv->sel_cntrlr = photos_selection_controller_dup_singleton ();
  g_signal_connect_object (priv->sel_cntrlr,
                           "selection-mode-changed",
                           G_CALLBACK (photos_main_toolbar_reset_toolbar_mode),
                           self,
                           G_CONNECT_SWAPPED);

  priv->remote_mngr = photos_remote_display_manager_dup_singleton ();
  g_signal_connect_object (priv->remote_mngr,
                           "share-began",
                           G_CALLBACK (photos_main_toolbar_share_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (priv->remote_mngr,
                           "share-ended",
                           G_CALLBACK (photos_main_toolbar_share_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (priv->remote_mngr,
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
  return PHOTOS_SEARCHBAR (self->priv->searchbar);
}


gboolean
photos_main_toolbar_handle_event (PhotosMainToolbar *self, GdkEventKey *event)
{
  gboolean ret_val = FALSE;

  ret_val = photos_searchbar_handle_event (PHOTOS_SEARCHBAR (self->priv->searchbar), event);
  return ret_val;
}


gboolean
photos_main_toolbar_is_focus (PhotosMainToolbar *self)
{
  return photos_searchbar_is_focus (PHOTOS_SEARCHBAR (self->priv->searchbar));
}


void
photos_main_toolbar_reset_toolbar_mode (PhotosMainToolbar *self)
{
  PhotosMainToolbarPrivate *priv = self->priv;
  PhotosWindowMode window_mode;
  gboolean selection_mode;

  photos_main_toolbar_clear_toolbar (self);
  selection_mode = photos_selection_controller_get_selection_mode (priv->sel_cntrlr);
  window_mode = photos_mode_controller_get_window_mode (priv->mode_cntrlr);

  if (selection_mode)
    photos_main_toolbar_populate_for_selection_mode (self);
  else if (window_mode == PHOTOS_WINDOW_MODE_COLLECTIONS)
    photos_main_toolbar_populate_for_collections (self);
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
  gtk_widget_show_all (priv->toolbar);
}


void
photos_main_toolbar_set_cancellable (PhotosMainToolbar *self, GCancellable *cancellable)
{
  PhotosMainToolbarPrivate *priv = self->priv;

  if (cancellable == priv->loader_cancellable)
    return;

  g_clear_object (&priv->loader_cancellable);

  if (cancellable != NULL)
    g_object_ref (cancellable);

  priv->loader_cancellable = cancellable;
}


void
photos_main_toolbar_set_stack (PhotosMainToolbar *self, GtkStack *stack)
{
  photos_header_bar_set_stack (PHOTOS_HEADER_BAR (self->priv->toolbar), stack);
}


void
photos_main_toolbar_set_view_model (PhotosMainToolbar *self, PhotosViewModel *model)
{
  PhotosMainToolbarPrivate *priv = self->priv;
  gboolean is_empty;

  is_empty = (gtk_tree_model_iter_n_children (GTK_TREE_MODEL (model), NULL) == 0);
  g_simple_action_set_enabled (G_SIMPLE_ACTION (priv->search), !is_empty);
  if (priv->selection_button != NULL)
    gtk_widget_set_sensitive (priv->selection_button, !is_empty);
}
