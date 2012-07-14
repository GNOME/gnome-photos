/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2012 Red Hat, Inc.
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

#include <clutter-gtk/clutter-gtk.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "gd-main-toolbar.h"
#include "photos-collection-manager.h"
#include "photos-item-manager.h"
#include "photos-main-toolbar.h"
#include "photos-mode-controller.h"
#include "photos-selection-controller.h"
#include "photos-source-manager.h"


struct _PhotosMainToolbarPrivate
{
  ClutterActor *actor;
  ClutterLayoutManager *layout;
  GtkWidget *coll_back_button;
  GtkWidget *widget;
  PhotosBaseManager *col_mngr;
  PhotosBaseManager *item_mngr;
  PhotosBaseManager *src_mngr;
  PhotosModeController *mode_cntrlr;
  PhotosSelectionController *sel_cntrlr;
  gulong collection_id;
  gulong search_source_id;
  gulong selection_changed_id;
  gulong selection_mode_id;
  gulong window_mode_id;
};


G_DEFINE_TYPE (PhotosMainToolbar, photos_main_toolbar, G_TYPE_OBJECT);


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

  if (window_mode == PHOTOS_WINDOW_MODE_OVERVIEW)
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

  gd_main_toolbar_set_labels (GD_MAIN_TOOLBAR (priv->widget), primary, detail);
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
      priv->coll_back_button = gd_main_toolbar_add_button (GD_MAIN_TOOLBAR (priv->widget),
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
  photos_mode_controller_set_window_mode (self->priv->mode_cntrlr, PHOTOS_WINDOW_MODE_OVERVIEW);
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

  photos_main_toolbar_clear_state_data (self);
  context = gtk_widget_get_style_context (priv->widget);
  gtk_style_context_remove_class (context, "documents-selection-mode");
  gtk_widget_reset_style (priv->widget);
  gd_main_toolbar_clear (GD_MAIN_TOOLBAR (priv->widget));
}


static void
photos_main_toolbar_destroy (GtkWidget *widget, gpointer user_data)
{
  PhotosMainToolbar *self = PHOTOS_MAIN_TOOLBAR (user_data);
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
photos_main_toolbar_populate_for_overview (PhotosMainToolbar *self)
{
  PhotosMainToolbarPrivate *priv = self->priv;
  GObject *object;
  GtkWidget *selection_button;

  selection_button = gd_main_toolbar_add_button (GD_MAIN_TOOLBAR (priv->widget),
                                                 "emblem-default-symbolic",
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
  GtkWidget *back_button;

  back_button = gd_main_toolbar_add_button (GD_MAIN_TOOLBAR (priv->widget),
                                            "go-previous-symbolic",
                                            _("Back"),
                                            TRUE);
  g_signal_connect (back_button, "clicked", G_CALLBACK (photos_main_toolbar_back_button_clicked), self);
}


static void
photos_main_toolbar_populate_for_selection_mode (PhotosMainToolbar *self)
{
  PhotosMainToolbarPrivate *priv = self->priv;
  GtkStyleContext *context;
  GtkWidget *selection_button;

  context = gtk_widget_get_style_context (priv->widget);
  gtk_style_context_add_class (context, "documents-selection-mode");
  gtk_widget_reset_style (priv->widget);

  selection_button = gd_main_toolbar_add_button (GD_MAIN_TOOLBAR (priv->widget), NULL, _("Done"), FALSE);
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

  photos_main_toolbar_clear_toolbar (self);
  window_mode = photos_mode_controller_get_window_mode (priv->mode_cntrlr);

  if (window_mode == PHOTOS_WINDOW_MODE_OVERVIEW)
    {
      gboolean selection_mode;

      selection_mode = photos_selection_controller_get_selection_mode (priv->sel_cntrlr);
      if (selection_mode)
        photos_main_toolbar_populate_for_selection_mode (self);
      else
        photos_main_toolbar_populate_for_overview (self);
    }
  else if (window_mode == PHOTOS_WINDOW_MODE_PREVIEW)
    photos_main_toolbar_populate_for_preview (self);

  photos_main_toolbar_set_toolbar_title (self);
  gtk_widget_show_all (priv->widget);
}


static void
photos_main_toolbar_dispose (GObject *object)
{
  PhotosMainToolbar *self = PHOTOS_MAIN_TOOLBAR (object);
  PhotosMainToolbarPrivate *priv = self->priv;

  g_clear_object (&priv->col_mngr);
  g_clear_object (&priv->item_mngr);
  g_clear_object (&priv->src_mngr);

  if (priv->mode_cntrlr != NULL)
    {
      g_object_unref (priv->mode_cntrlr);
      priv->mode_cntrlr = NULL;
    }

  if (priv->sel_cntrlr != NULL)
    {
      g_object_unref (priv->sel_cntrlr);
      priv->sel_cntrlr = NULL;
    }

  G_OBJECT_CLASS (photos_main_toolbar_parent_class)->dispose (object);
}


static void
photos_main_toolbar_init (PhotosMainToolbar *self)
{
  PhotosMainToolbarPrivate *priv;
  ClutterActor *actor;
  GtkStyleContext *context;

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, PHOTOS_TYPE_MAIN_TOOLBAR, PhotosMainToolbarPrivate);
  priv = self->priv;

  priv->widget = gd_main_toolbar_new ();
  gtk_toolbar_set_icon_size (GTK_TOOLBAR (priv->widget), GTK_ICON_SIZE_MENU);
  context = gtk_widget_get_style_context (priv->widget);
  gtk_style_context_add_class (context, GTK_STYLE_CLASS_MENUBAR);
  gtk_widget_show (priv->widget);

  priv->layout = clutter_box_layout_new ();
  clutter_box_layout_set_orientation (CLUTTER_BOX_LAYOUT (priv->layout), CLUTTER_ORIENTATION_VERTICAL);

  priv->actor = clutter_actor_new ();
  clutter_actor_set_layout_manager (priv->actor, priv->layout);

  actor = gtk_clutter_actor_new_with_contents (priv->widget);
  clutter_box_layout_pack (CLUTTER_BOX_LAYOUT (priv->layout),
                           actor,
                           FALSE,
                           TRUE,
                           FALSE,
                           CLUTTER_BOX_ALIGNMENT_CENTER,
                           CLUTTER_BOX_ALIGNMENT_START);

  priv->col_mngr = photos_collection_manager_new ();
  priv->item_mngr = photos_item_manager_new ();

  priv->src_mngr = photos_source_manager_new ();
  priv->search_source_id = g_signal_connect_swapped (priv->src_mngr,
                                                     "active-changed",
                                                     G_CALLBACK (photos_main_toolbar_set_toolbar_title),
                                                     self);

  priv->mode_cntrlr = photos_mode_controller_new ();
  priv->window_mode_id = g_signal_connect_swapped (priv->mode_cntrlr,
                                                   "window-mode-changed",
                                                   G_CALLBACK (photos_main_toolbar_reset_toolbar_mode),
                                                   self);

  priv->sel_cntrlr = photos_selection_controller_new ();
  priv->selection_mode_id = g_signal_connect_swapped (priv->sel_cntrlr,
                                                      "selection-mode-changed",
                                                      G_CALLBACK (photos_main_toolbar_reset_toolbar_mode),
                                                      self);

  photos_main_toolbar_reset_toolbar_mode (self);

  g_signal_connect (priv->widget, "destroy", G_CALLBACK (photos_main_toolbar_destroy), self);
}


static void
photos_main_toolbar_class_init (PhotosMainToolbarClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->dispose = photos_main_toolbar_dispose;

  g_type_class_add_private (class, sizeof (PhotosMainToolbarPrivate));
}


PhotosMainToolbar *
photos_main_toolbar_new (void)
{
  return g_object_new (PHOTOS_TYPE_MAIN_TOOLBAR, NULL);
}


ClutterActor *
photos_main_toolbar_get_actor (PhotosMainToolbar *self)
{
  return self->priv->actor;
}
