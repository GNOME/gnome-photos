/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2012, 2013 Red Hat, Inc.
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
#include <libgd/gd.h>

#include "photos-application.h"
#include "photos-collection-manager.h"
#include "photos-header-bar.h"
#include "photos-item-manager.h"
#include "photos-main-toolbar.h"
#include "photos-mode-controller.h"
#include "photos-remote-display-manager.h"
#include "photos-selection-controller.h"
#include "photos-source-manager.h"


struct _PhotosMainToolbarPrivate
{
  GSimpleAction *gear_menu;
  GtkWidget *coll_back_button;
  GtkWidget *remote_display_button;
  GtkWidget *selection_menu;
  GtkWidget *toolbar;
  PhotosBaseManager *col_mngr;
  PhotosBaseManager *item_mngr;
  PhotosBaseManager *src_mngr;
  PhotosModeController *mode_cntrlr;
  PhotosRemoteDisplayManager *remote_mngr;
  PhotosSelectionController *sel_cntrlr;
  PhotosWindowMode old_mode;
};


G_DEFINE_TYPE (PhotosMainToolbar, photos_main_toolbar, GTK_TYPE_BOX);


static void
photos_main_toolbar_set_toolbar_title (PhotosMainToolbar *self)
{
  PhotosMainToolbarPrivate *priv = self->priv;
  GObject *active_collection;
  PhotosWindowMode window_mode;
  gboolean selection_mode;
  gchar *primary = NULL;

  active_collection = photos_base_manager_get_active_object (priv->col_mngr);
  selection_mode = photos_selection_controller_get_selection_mode (priv->sel_cntrlr);
  window_mode = photos_mode_controller_get_window_mode (priv->mode_cntrlr);

  if (window_mode == PHOTOS_WINDOW_MODE_OVERVIEW
      || window_mode == PHOTOS_WINDOW_MODE_COLLECTIONS
      || window_mode == PHOTOS_WINDOW_MODE_FAVORITES)
    {
      if (!selection_mode)
        {
          if (active_collection != NULL)
            primary = g_markup_printf_escaped ("%s", photos_base_item_get_name (PHOTOS_BASE_ITEM (active_collection)));
          else
            {
            }
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
                                                  photos_base_item_get_name (PHOTOS_BASE_ITEM (active_collection)),
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
      primary = g_markup_printf_escaped ("%s", photos_base_item_get_name (PHOTOS_BASE_ITEM (item)));
    }

  if (selection_mode)
    gd_header_button_set_label (GD_HEADER_BUTTON (priv->selection_menu), primary);
  else
    gd_header_bar_set_title (GD_HEADER_BAR (priv->toolbar), primary);

  g_free (primary);
}


static GtkWidget *
photos_main_toolbar_add_back_button (PhotosMainToolbar *self)
{
  PhotosMainToolbarPrivate *priv = self->priv;
  GtkTextDirection direction;
  GtkWidget *back_button;
  const gchar *icon_name;

  direction = gtk_widget_get_direction (GTK_WIDGET (priv->toolbar));
  icon_name = (direction == GTK_TEXT_DIR_RTL) ? "go-next-symbolic" : "go-previous-symbolic";

  back_button = gd_header_simple_button_new ();
  gd_header_button_set_label (GD_HEADER_BUTTON (back_button), _("Back"));
  gd_header_button_set_symbolic_icon_name (GD_HEADER_BUTTON (back_button), icon_name);
  gd_header_bar_pack_start (GD_HEADER_BAR (priv->toolbar), back_button);

  return back_button;
}


static void
photos_main_toolbar_coll_back_button_clicked (GtkButton *button, gpointer user_data)
{
  PhotosMainToolbar *self = PHOTOS_MAIN_TOOLBAR (user_data);
  photos_item_manager_activate_previous_collection (PHOTOS_ITEM_MANAGER (self->priv->item_mngr));
}


static void
photos_main_toolbar_active_changed (PhotosBaseManager *manager, GObject *object, gpointer user_data)
{
  PhotosMainToolbar *self = PHOTOS_MAIN_TOOLBAR (user_data);
  PhotosMainToolbarPrivate *priv = self->priv;
  GObject *item;
  PhotosHeaderBarMode mode;

  item = photos_base_manager_get_active_object (priv->col_mngr);
  if (item != NULL)
    {
      mode = PHOTOS_HEADER_BAR_MODE_STANDALONE;
      if (priv->coll_back_button == NULL)
        {
          priv->coll_back_button = photos_main_toolbar_add_back_button (self);
          gtk_widget_show (priv->coll_back_button);

          g_signal_connect (priv->coll_back_button,
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
photos_main_toolbar_select_button_clicked (GtkButton *button, gpointer user_data)
{
  PhotosMainToolbar *self = PHOTOS_MAIN_TOOLBAR (user_data);
  photos_selection_controller_set_selection_mode (self->priv->sel_cntrlr, TRUE);
}


static void
photos_main_toolbar_remote_display_button_clicked (GtkButton *button,
                                                   gpointer user_data)
{
  PhotosMainToolbar *self = PHOTOS_MAIN_TOOLBAR (user_data);
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

  priv->remote_display_button = gd_header_simple_button_new ();
  gd_header_button_set_label (GD_HEADER_BUTTON (priv->remote_display_button), text);
  gd_header_button_set_use_markup (GD_HEADER_BUTTON (priv->remote_display_button), TRUE);
  label = GTK_LABEL (gtk_bin_get_child (GTK_BIN (priv->remote_display_button)));
  gtk_label_set_ellipsize (label, PANGO_ELLIPSIZE_MIDDLE);
  gtk_widget_set_margin_right (priv->remote_display_button, 12);
  gd_header_bar_pack_start (GD_HEADER_BAR (priv->toolbar), priv->remote_display_button);
  gtk_widget_show_all (priv->remote_display_button);

  g_signal_connect (priv->remote_display_button, "clicked",
                    G_CALLBACK (photos_main_toolbar_remote_display_button_clicked), self);
  g_free (text);
}


static void
photos_main_toolbar_add_selection_button (PhotosMainToolbar *self)
{
  PhotosMainToolbarPrivate *priv = self->priv;
  GtkWidget *selection_button;

  selection_button = gd_header_simple_button_new ();
  gd_header_button_set_label (GD_HEADER_BUTTON (selection_button), _("Select Items"));
  gd_header_button_set_symbolic_icon_name (GD_HEADER_BUTTON (selection_button), "object-select-symbolic");
  gd_header_bar_pack_end (GD_HEADER_BAR (priv->toolbar), selection_button);
  g_signal_connect (selection_button, "clicked", G_CALLBACK (photos_main_toolbar_select_button_clicked), self);

  g_signal_connect_object (priv->col_mngr,
                           "active-changed",
                           G_CALLBACK (photos_main_toolbar_active_changed),
                           self,
                           0);
}


static void
photos_main_toolbar_back_button_clicked (GtkButton *button, gpointer user_data)
{
  PhotosMainToolbar *self = PHOTOS_MAIN_TOOLBAR (user_data);
  PhotosMainToolbarPrivate *priv = self->priv;

  photos_base_manager_set_active_object (priv->item_mngr, NULL);
  photos_mode_controller_set_window_mode (priv->mode_cntrlr, priv->old_mode);
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

  if (priv->col_mngr != NULL)
    g_signal_handlers_disconnect_by_func (priv->col_mngr, photos_main_toolbar_active_changed, self);

  if (priv->sel_cntrlr != NULL)
    g_signal_handlers_disconnect_by_func (priv->sel_cntrlr, photos_main_toolbar_set_toolbar_title, self);
}


static void
photos_main_toolbar_clear_toolbar (PhotosMainToolbar *self)
{
  PhotosMainToolbarPrivate *priv = self->priv;

  photos_main_toolbar_clear_state_data (self);
  photos_header_bar_clear (PHOTOS_HEADER_BAR (priv->toolbar));
  g_simple_action_set_enabled (priv->gear_menu, FALSE);
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
photos_main_toolbar_done_button_clicked (GtkButton *button, gpointer user_data)
{
  PhotosMainToolbar *self = PHOTOS_MAIN_TOOLBAR (user_data);
  photos_selection_controller_set_selection_mode (self->priv->sel_cntrlr, FALSE);
}


static void
photos_main_toolbar_populate_for_collections (PhotosMainToolbar *self)
{
  PhotosMainToolbarPrivate *priv = self->priv;
  GObject *object;

  photos_header_bar_set_mode (PHOTOS_HEADER_BAR (priv->toolbar), PHOTOS_HEADER_BAR_MODE_NORMAL);
  photos_main_toolbar_add_selection_button (self);

  object = photos_base_manager_get_active_object (priv->col_mngr);
  photos_main_toolbar_active_changed (priv->col_mngr, object, self);
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
  GObject *object;

  photos_header_bar_set_mode (PHOTOS_HEADER_BAR (priv->toolbar), PHOTOS_HEADER_BAR_MODE_NORMAL);
  photos_main_toolbar_add_selection_button (self);

  object = photos_base_manager_get_active_object (priv->col_mngr);
  photos_main_toolbar_active_changed (priv->col_mngr, object, self);
}


static void
photos_main_toolbar_populate_for_overview (PhotosMainToolbar *self)
{
  PhotosMainToolbarPrivate *priv = self->priv;
  GObject *object;

  photos_header_bar_set_mode (PHOTOS_HEADER_BAR (priv->toolbar), PHOTOS_HEADER_BAR_MODE_NORMAL);
  photos_main_toolbar_add_selection_button (self);

  object = photos_base_manager_get_active_object (priv->col_mngr);
  photos_main_toolbar_active_changed (priv->col_mngr, object, self);
}


static void
photos_main_toolbar_populate_for_preview (PhotosMainToolbar *self)
{
  PhotosMainToolbarPrivate *priv = self->priv;
  GMenu *preview_menu;
  GtkWidget *back_button;
  GtkWidget *menu_button;
  GtkApplication *app;
  gboolean remote_display_available;
  GAction *remote_display_action;

  photos_header_bar_set_mode (PHOTOS_HEADER_BAR (priv->toolbar), PHOTOS_HEADER_BAR_MODE_STANDALONE);

  back_button = photos_main_toolbar_add_back_button (self);
  g_signal_connect (back_button, "clicked", G_CALLBACK (photos_main_toolbar_back_button_clicked), self);

  preview_menu = photos_main_toolbar_create_preview_menu (self);
  menu_button = gd_header_menu_button_new ();
  gtk_actionable_set_action_name (GTK_ACTIONABLE (menu_button), "app.gear-menu");
  gtk_menu_button_set_menu_model (GTK_MENU_BUTTON (menu_button), G_MENU_MODEL (preview_menu));
  gd_header_button_set_symbolic_icon_name (GD_HEADER_BUTTON (menu_button), "emblem-system-symbolic");
  gd_header_bar_pack_end (GD_HEADER_BAR (priv->toolbar), menu_button);

  g_simple_action_set_enabled (priv->gear_menu, TRUE);

  /* Disable the remote-display-current action if the dLeyna services are not
   * available */
  app = photos_application_new ();
  remote_display_action = g_action_map_lookup_action (G_ACTION_MAP (app), "remote-display-current");
  remote_display_available = photos_dlna_renderers_manager_is_available ();
  g_simple_action_set_enabled (G_SIMPLE_ACTION (remote_display_action), remote_display_available);
}


static void
photos_main_toolbar_populate_for_selection_mode (PhotosMainToolbar *self)
{
  PhotosMainToolbarPrivate *priv = self->priv;
  GtkStyleContext *context;
  GtkWidget *selection_button;

  photos_header_bar_set_mode (PHOTOS_HEADER_BAR (priv->toolbar), PHOTOS_HEADER_BAR_MODE_SELECTION);

  selection_button = gd_header_simple_button_new ();
  gd_header_button_set_label (GD_HEADER_BUTTON (selection_button), _("Done"));
  gd_header_bar_pack_end (GD_HEADER_BAR (priv->toolbar), selection_button);
  context = gtk_widget_get_style_context (selection_button);
  gtk_style_context_add_class (context, "suggested-action");
  g_signal_connect (selection_button, "clicked", G_CALLBACK (photos_main_toolbar_done_button_clicked), self);

  g_signal_connect_object (priv->sel_cntrlr,
                           "selection-changed",
                           G_CALLBACK (photos_main_toolbar_set_toolbar_title),
                           self,
                           G_CONNECT_SWAPPED);
}


static void
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

  photos_main_toolbar_update_remote_display_button (self);

  photos_main_toolbar_set_toolbar_title (self);
  gtk_widget_show_all (priv->toolbar);
}


static void
photos_main_toolbar_window_mode_changed (PhotosMainToolbar *self, PhotosWindowMode mode, PhotosWindowMode old_mode)
{
  self->priv->old_mode = old_mode;
  photos_main_toolbar_reset_toolbar_mode (self);
}


static void
photos_main_toolbar_dispose (GObject *object)
{
  PhotosMainToolbar *self = PHOTOS_MAIN_TOOLBAR (object);
  PhotosMainToolbarPrivate *priv = self->priv;

  photos_main_toolbar_clear_state_data (self);

  g_clear_object (&priv->col_mngr);
  g_clear_object (&priv->item_mngr);
  g_clear_object (&priv->src_mngr);
  g_clear_object (&priv->mode_cntrlr);
  g_clear_object (&priv->remote_mngr);
  g_clear_object (&priv->sel_cntrlr);

  G_OBJECT_CLASS (photos_main_toolbar_parent_class)->dispose (object);
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
  GtkApplication *app;
  GtkBuilder *builder;

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, PHOTOS_TYPE_MAIN_TOOLBAR, PhotosMainToolbarPrivate);
  priv = self->priv;

  gtk_orientable_set_orientation (GTK_ORIENTABLE (self), GTK_ORIENTATION_VERTICAL);
  gtk_widget_show (GTK_WIDGET (self));

  priv->toolbar = photos_header_bar_new ();
  gtk_container_add (GTK_CONTAINER (self), priv->toolbar);
  gtk_widget_show (priv->toolbar);

  app = photos_application_new ();
  priv->gear_menu = G_SIMPLE_ACTION (g_action_map_lookup_action (G_ACTION_MAP (app), "gear-menu"));
  g_object_unref (app);

  builder = gtk_builder_new ();
  gtk_builder_add_from_resource (builder, "/org/gnome/photos/selection-menu.ui", NULL);

  selection_menu = G_MENU (gtk_builder_get_object (builder, "selection-menu"));
  priv->selection_menu = gd_header_menu_button_new ();
  gtk_menu_button_set_menu_model (GTK_MENU_BUTTON (priv->selection_menu), G_MENU_MODEL (selection_menu));
  gd_header_button_set_use_markup (GD_HEADER_BUTTON (priv->selection_menu), TRUE);
  g_object_unref (builder);

  photos_header_bar_set_selection_menu (PHOTOS_HEADER_BAR (priv->toolbar),
                                        GD_HEADER_BUTTON (priv->selection_menu));

  priv->col_mngr = photos_collection_manager_new ();
  priv->item_mngr = photos_item_manager_new ();

  priv->src_mngr = photos_source_manager_new ();
  g_signal_connect_object (priv->src_mngr,
                           "active-changed",
                           G_CALLBACK (photos_main_toolbar_set_toolbar_title),
                           self,
                           G_CONNECT_SWAPPED);

  priv->mode_cntrlr = photos_mode_controller_new ();
  priv->old_mode = PHOTOS_WINDOW_MODE_NONE;
  g_signal_connect_object (priv->mode_cntrlr,
                           "window-mode-changed",
                           G_CALLBACK (photos_main_toolbar_window_mode_changed),
                           self,
                           G_CONNECT_SWAPPED);

  priv->sel_cntrlr = photos_selection_controller_new ();
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

  photos_main_toolbar_reset_toolbar_mode (self);
}


static void
photos_main_toolbar_class_init (PhotosMainToolbarClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->dispose = photos_main_toolbar_dispose;

  g_type_class_add_private (class, sizeof (PhotosMainToolbarPrivate));
}


GtkWidget *
photos_main_toolbar_new (void)
{
  return g_object_new (PHOTOS_TYPE_MAIN_TOOLBAR, NULL);
}


void
photos_main_toolbar_set_stack (PhotosMainToolbar *self, GdStack *stack)
{
  photos_header_bar_set_stack (PHOTOS_HEADER_BAR (self->priv->toolbar), stack);
}
