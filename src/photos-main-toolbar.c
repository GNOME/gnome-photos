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

#include "photos-collection-manager.h"
#include "photos-item-manager.h"
#include "photos-main-toolbar.h"
#include "photos-mode-controller.h"
#include "photos-selection-controller.h"
#include "photos-source-manager.h"


struct _PhotosMainToolbarPrivate
{
  GtkWidget *coll_back_button;
  GtkWidget *toolbar;
  PhotosBaseManager *col_mngr;
  PhotosBaseManager *item_mngr;
  PhotosBaseManager *src_mngr;
  PhotosModeController *mode_cntrlr;
  PhotosSelectionController *sel_cntrlr;
  PhotosWindowMode old_mode;
  gulong collection_id;
  gulong search_source_id;
  gulong selection_changed_id;
  gulong selection_mode_id;
  gulong window_mode_id;
};


G_DEFINE_TYPE (PhotosMainToolbar, photos_main_toolbar, GTK_TYPE_BOX);


static void
photos_main_toolbar_favorites_toggled (GtkToggleButton *toggle_button, gpointer user_data)
{
  PhotosMainToolbar *self = PHOTOS_MAIN_TOOLBAR (user_data);

  if (gtk_toggle_button_get_active (toggle_button))
    photos_mode_controller_set_window_mode (self->priv->mode_cntrlr, PHOTOS_WINDOW_MODE_FAVORITES);
}


static void
photos_main_toolbar_overview_toggled (GtkToggleButton *toggle_button, gpointer user_data)
{
  PhotosMainToolbar *self = PHOTOS_MAIN_TOOLBAR (user_data);

  if (gtk_toggle_button_get_active (toggle_button))
    photos_mode_controller_set_window_mode (self->priv->mode_cntrlr, PHOTOS_WINDOW_MODE_OVERVIEW);
}


static void
photos_main_toolbar_add_modes (PhotosMainToolbar *self, PhotosWindowMode window_mode)
{
  PhotosMainToolbarPrivate *priv = self->priv;
  GtkWidget *button;

  button = gd_main_toolbar_add_mode (GD_MAIN_TOOLBAR (priv->toolbar), _("Photos"));
  if (window_mode == PHOTOS_WINDOW_MODE_OVERVIEW)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
  g_signal_connect (button, "toggled", G_CALLBACK (photos_main_toolbar_overview_toggled), self);

  button = gd_main_toolbar_add_mode (GD_MAIN_TOOLBAR (priv->toolbar), _("Favorites"));
  if (window_mode == PHOTOS_WINDOW_MODE_FAVORITES)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
  g_signal_connect (button, "toggled", G_CALLBACK (photos_main_toolbar_favorites_toggled), self);
}


static void
photos_main_toolbar_set_toolbar_title (PhotosMainToolbar *self)
{
  PhotosMainToolbarPrivate *priv = self->priv;
  GObject *active_collection;
  PhotosWindowMode window_mode;
  gboolean selection_mode;
  gchar *detail = NULL;
  gchar *primary = NULL;

  active_collection = photos_base_manager_get_active_object (priv->col_mngr);
  selection_mode = photos_selection_controller_get_selection_mode (priv->sel_cntrlr);
  window_mode = photos_mode_controller_get_window_mode (priv->mode_cntrlr);

  if (window_mode == PHOTOS_WINDOW_MODE_OVERVIEW || window_mode == PHOTOS_WINDOW_MODE_FAVORITES)
    {
      if (!selection_mode)
        {
          if (active_collection != NULL)
            primary = g_strdup (photos_base_item_get_name (PHOTOS_BASE_ITEM (active_collection)));
          else
            {
            }
        }
      else
        {
          GList *selection;
          guint length;

          selection = photos_selection_controller_get_selection (priv->sel_cntrlr);
          length = g_list_length (selection);
          if (length == 0)
            detail = g_strdup(_("Click on items to select them"));
          else
            detail = g_strdup_printf (_("%d selected"), length);

          if (active_collection != NULL)
            {
              primary = g_strdup (photos_base_item_get_name (PHOTOS_BASE_ITEM (active_collection)));
            }
          else
            {
              primary = detail;
              detail = NULL;
            }
        }
    }
  else if (window_mode == PHOTOS_WINDOW_MODE_PREVIEW)
    {
      GObject *item;

      item = photos_base_manager_get_active_object (priv->item_mngr);
      primary = g_strdup (photos_base_item_get_name (PHOTOS_BASE_ITEM (item)));
    }

  if (detail != NULL)
    {
      gchar *tmp;

      tmp = detail;
      detail = g_strconcat ("(", detail, ")", NULL);
      g_free (tmp);
    }

  gd_main_toolbar_set_labels (GD_MAIN_TOOLBAR (priv->toolbar), primary, detail);
  g_free (primary);
  g_free (detail);
}


static void
photos_main_toolbar_coll_back_button_clicked (GtkButton *button, gpointer user_data)
{
  PhotosMainToolbar *self = PHOTOS_MAIN_TOOLBAR (user_data);
  photos_base_manager_set_active_object (self->priv->col_mngr, NULL);
}


static void
photos_main_toolbar_active_changed (PhotosBaseManager *manager, GObject *object, gpointer user_data)
{
  PhotosMainToolbar *self = PHOTOS_MAIN_TOOLBAR (user_data);
  PhotosMainToolbarPrivate *priv = self->priv;
  GObject *item;

  item = photos_base_manager_get_active_object (priv->col_mngr);
  if (item != NULL && priv->coll_back_button == NULL)
    {
      priv->coll_back_button = gd_main_toolbar_add_button (GD_MAIN_TOOLBAR (priv->toolbar),
                                                           "go-previous-symbolic",
                                                           _("Back"),
                                                           TRUE);
      g_signal_connect (priv->coll_back_button,
                        "clicked",
                        G_CALLBACK (photos_main_toolbar_coll_back_button_clicked),
                        self);
    }
  else if (item == NULL && priv->coll_back_button != NULL)
    {
      gtk_widget_destroy (priv->coll_back_button);
      priv->coll_back_button = NULL;
    }

  photos_main_toolbar_set_toolbar_title (self);
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

  if (priv->collection_id != 0)
    {
      g_signal_handler_disconnect (priv->col_mngr, priv->collection_id);
      priv->collection_id = 0;
    }

  if (priv->selection_changed_id != 0)
    {
      g_signal_handler_disconnect (priv->sel_cntrlr, priv->selection_changed_id);
      priv->selection_changed_id = 0;
    }
}


static void
photos_main_toolbar_clear_toolbar (PhotosMainToolbar *self)
{
  PhotosMainToolbarPrivate *priv = self->priv;
  GtkStyleContext *context;

  gd_main_toolbar_set_labels_menu (GD_MAIN_TOOLBAR (priv->toolbar), NULL);
  photos_main_toolbar_clear_state_data (self);
  context = gtk_widget_get_style_context (priv->toolbar);
  gtk_style_context_remove_class (context, "selection-mode");
  gtk_widget_reset_style (priv->toolbar);
  gd_main_toolbar_clear (GD_MAIN_TOOLBAR (priv->toolbar));
}


static void
photos_main_toolbar_done_button_clicked (GtkButton *button, gpointer user_data)
{
  PhotosMainToolbar *self = PHOTOS_MAIN_TOOLBAR (user_data);
  photos_selection_controller_set_selection_mode (self->priv->sel_cntrlr, FALSE);
}


static void
photos_main_toolbar_select_button_clicked (GtkButton *button, gpointer user_data)
{
  PhotosMainToolbar *self = PHOTOS_MAIN_TOOLBAR (user_data);
  photos_selection_controller_set_selection_mode (self->priv->sel_cntrlr, TRUE);
}


static void
photos_main_toolbar_populate_for_favorites (PhotosMainToolbar *self)
{
  PhotosMainToolbarPrivate *priv = self->priv;
  GObject *object;
  GtkWidget *selection_button;

  gd_main_toolbar_set_show_modes (GD_MAIN_TOOLBAR (priv->toolbar), TRUE);
  photos_main_toolbar_add_modes (self, PHOTOS_WINDOW_MODE_FAVORITES);

  selection_button = gd_main_toolbar_add_button (GD_MAIN_TOOLBAR (priv->toolbar),
                                                 "object-select-symbolic",
                                                 _("Select Items"),
                                                 FALSE);
  g_signal_connect (selection_button, "clicked", G_CALLBACK (photos_main_toolbar_select_button_clicked), self);

  priv->collection_id = g_signal_connect (priv->col_mngr,
                                          "active-changed",
                                          G_CALLBACK (photos_main_toolbar_active_changed),
                                          self);

  object = photos_base_manager_get_active_object (priv->col_mngr);
  photos_main_toolbar_active_changed (priv->col_mngr, object, self);
}


static void
photos_main_toolbar_populate_for_overview (PhotosMainToolbar *self)
{
  PhotosMainToolbarPrivate *priv = self->priv;
  GObject *object;
  GtkWidget *selection_button;

  gd_main_toolbar_set_show_modes (GD_MAIN_TOOLBAR (priv->toolbar), TRUE);
  photos_main_toolbar_add_modes (self, PHOTOS_WINDOW_MODE_OVERVIEW);

  selection_button = gd_main_toolbar_add_button (GD_MAIN_TOOLBAR (priv->toolbar),
                                                 "object-select-symbolic",
                                                 _("Select Items"),
                                                 FALSE);
  g_signal_connect (selection_button, "clicked", G_CALLBACK (photos_main_toolbar_select_button_clicked), self);

  priv->collection_id = g_signal_connect (priv->col_mngr,
                                          "active-changed",
                                          G_CALLBACK (photos_main_toolbar_active_changed),
                                          self);

  object = photos_base_manager_get_active_object (priv->col_mngr);
  photos_main_toolbar_active_changed (priv->col_mngr, object, self);
}


static void
photos_main_toolbar_populate_for_preview (PhotosMainToolbar *self)
{
  PhotosMainToolbarPrivate *priv = self->priv;
  GtkTextDirection direction;
  GtkWidget *back_button;
  const gchar *icon_name;

  gd_main_toolbar_set_show_modes (GD_MAIN_TOOLBAR (priv->toolbar), FALSE);

  direction = gtk_widget_get_direction (GTK_WIDGET (priv->toolbar));
  icon_name = (direction == GTK_TEXT_DIR_RTL) ? "go-next-symbolic" : "go-previous-symbolic";
  back_button = gd_main_toolbar_add_button (GD_MAIN_TOOLBAR (priv->toolbar), icon_name, _("Back"), TRUE);
  g_signal_connect (back_button, "clicked", G_CALLBACK (photos_main_toolbar_back_button_clicked), self);
}


static void
photos_main_toolbar_populate_for_selection_mode (PhotosMainToolbar *self)
{
  PhotosMainToolbarPrivate *priv = self->priv;
  GMenu *selection_menu;
  GtkBuilder *builder;
  GtkStyleContext *context;
  GtkWidget *selection_button;

  gd_main_toolbar_set_show_modes (GD_MAIN_TOOLBAR (priv->toolbar), FALSE);

  context = gtk_widget_get_style_context (priv->toolbar);
  gtk_style_context_add_class (context, "selection-mode");
  gtk_widget_reset_style (priv->toolbar);

  builder = gtk_builder_new ();
  gtk_builder_add_from_resource (builder, "/org/gnome/photos/selection-menu.ui", NULL);

  selection_menu = G_MENU (gtk_builder_get_object (builder, "selection-menu"));
  gd_main_toolbar_set_labels_menu (GD_MAIN_TOOLBAR (priv->toolbar), G_MENU_MODEL (selection_menu));
  g_object_unref (builder);

  selection_button = gd_main_toolbar_add_button (GD_MAIN_TOOLBAR (priv->toolbar), NULL, _("Done"), FALSE);
  context = gtk_widget_get_style_context (selection_button);
  gtk_style_context_add_class (context, "suggested-action");
  g_signal_connect (selection_button, "clicked", G_CALLBACK (photos_main_toolbar_done_button_clicked), self);

  priv->selection_changed_id = g_signal_connect_swapped (priv->sel_cntrlr,
                                                         "selection-changed",
                                                         G_CALLBACK (photos_main_toolbar_set_toolbar_title),
                                                         self);
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
  else if (window_mode == PHOTOS_WINDOW_MODE_FAVORITES)
    photos_main_toolbar_populate_for_favorites (self);
  else if (window_mode == PHOTOS_WINDOW_MODE_OVERVIEW)
    photos_main_toolbar_populate_for_overview (self);
  else if (window_mode == PHOTOS_WINDOW_MODE_PREVIEW)
    photos_main_toolbar_populate_for_preview (self);

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

  if (priv->window_mode_id != 0)
    {
      g_signal_handler_disconnect (priv->mode_cntrlr, priv->window_mode_id);
      priv->window_mode_id = 0;
    }

  if (priv->selection_mode_id != 0)
    {
      g_signal_handler_disconnect (priv->sel_cntrlr, priv->selection_mode_id);
      priv->selection_mode_id = 0;
    }

  if (priv->search_source_id != 0)
    {
      g_signal_handler_disconnect (priv->src_mngr, priv->search_source_id);
      priv->search_source_id = 0;
    }

  g_clear_object (&priv->col_mngr);
  g_clear_object (&priv->item_mngr);
  g_clear_object (&priv->src_mngr);
  g_clear_object (&priv->mode_cntrlr);
  g_clear_object (&priv->sel_cntrlr);

  G_OBJECT_CLASS (photos_main_toolbar_parent_class)->dispose (object);
}


static void
photos_main_toolbar_init (PhotosMainToolbar *self)
{
  PhotosMainToolbarPrivate *priv;
  GtkStyleContext *context;

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, PHOTOS_TYPE_MAIN_TOOLBAR, PhotosMainToolbarPrivate);
  priv = self->priv;

  gtk_orientable_set_orientation (GTK_ORIENTABLE (self), GTK_ORIENTATION_VERTICAL);
  gtk_widget_show (GTK_WIDGET (self));

  priv->toolbar = gd_main_toolbar_new ();
  gtk_toolbar_set_icon_size (GTK_TOOLBAR (priv->toolbar), GTK_ICON_SIZE_MENU);
  context = gtk_widget_get_style_context (priv->toolbar);
  gtk_style_context_add_class (context, GTK_STYLE_CLASS_MENUBAR);
  gtk_container_add (GTK_CONTAINER (self), priv->toolbar);
  gtk_widget_show (priv->toolbar);

  priv->col_mngr = photos_collection_manager_new ();
  priv->item_mngr = photos_item_manager_new ();

  priv->src_mngr = photos_source_manager_new ();
  priv->search_source_id = g_signal_connect_swapped (priv->src_mngr,
                                                     "active-changed",
                                                     G_CALLBACK (photos_main_toolbar_set_toolbar_title),
                                                     self);

  priv->mode_cntrlr = photos_mode_controller_new ();
  priv->old_mode = PHOTOS_WINDOW_MODE_NONE;
  priv->window_mode_id = g_signal_connect_swapped (priv->mode_cntrlr,
                                                   "window-mode-changed",
                                                   G_CALLBACK (photos_main_toolbar_window_mode_changed),
                                                   self);

  priv->sel_cntrlr = photos_selection_controller_new ();
  priv->selection_mode_id = g_signal_connect_swapped (priv->sel_cntrlr,
                                                      "selection-mode-changed",
                                                      G_CALLBACK (photos_main_toolbar_reset_toolbar_mode),
                                                      self);

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
