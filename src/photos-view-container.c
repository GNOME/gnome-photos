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
#include <libgd/gd.h>

#include "photos-application.h"
#include "photos-enums.h"
#include "photos-item-manager.h"
#include "photos-load-more-button.h"
#include "photos-remote-display-manager.h"
#include "photos-selection-controller.h"
#include "photos-tracker-collections-controller.h"
#include "photos-tracker-favorites-controller.h"
#include "photos-tracker-overview-controller.h"
#include "photos-utils.h"
#include "photos-view-container.h"
#include "photos-view-model.h"


struct _PhotosViewContainerPrivate
{
  GdMainView *view;
  GtkListStore *model;
  GtkWidget *load_more;
  PhotosBaseManager *item_mngr;
  PhotosModeController *mode_cntrlr;
  PhotosRemoteDisplayManager *remote_mngr;
  PhotosSelectionController *sel_cntrlr;
  PhotosTrackerController *trk_cntrlr;
  PhotosWindowMode mode;
  gboolean disposed;
};

enum
{
  PROP_0,
  PROP_MODE
};


G_DEFINE_TYPE (PhotosViewContainer, photos_view_container, GTK_TYPE_GRID);


static void
photos_view_container_view_changed (PhotosViewContainer *self)
{
  PhotosViewContainerPrivate *priv = self->priv;
  GtkAdjustment *vadjustment;
  GtkWidget *vscrollbar;
  gboolean end = FALSE;
  gdouble page_size;
  gdouble upper;
  gdouble value;
  gint reveal_area_height = 32;

  vscrollbar = gtk_scrolled_window_get_vscrollbar (GTK_SCROLLED_WINDOW (priv->view));
  if (vscrollbar == NULL || !gtk_widget_get_visible (GTK_WIDGET (vscrollbar)))
    {
      photos_load_more_button_set_block (PHOTOS_LOAD_MORE_BUTTON (priv->load_more), TRUE);
      return;
    }

  vadjustment = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (priv->view));
  page_size = gtk_adjustment_get_page_size (vadjustment);
  upper = gtk_adjustment_get_upper (vadjustment);
  value = gtk_adjustment_get_value (vadjustment);

  /* Special case these values which happen at construction */
  if ((gint) value == 0 && (gint) upper == 1 && (gint) page_size == 1)
    end = FALSE;
  else
    end = !(value < (upper - page_size - reveal_area_height));

  photos_load_more_button_set_block (PHOTOS_LOAD_MORE_BUTTON (priv->load_more), !end);
}


static void
photos_view_container_connect_view (PhotosViewContainer *self)
{
  PhotosViewContainerPrivate *priv = self->priv;
  GtkAdjustment *vadjustment;
  GtkWidget *vscrollbar;

  vadjustment = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (priv->view));
  g_signal_connect_object (vadjustment,
                           "changed",
                           G_CALLBACK (photos_view_container_view_changed),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (vadjustment,
                           "value-changed",
                           G_CALLBACK (photos_view_container_view_changed),
                           self,
                           G_CONNECT_SWAPPED);

  vscrollbar = gtk_scrolled_window_get_vscrollbar (GTK_SCROLLED_WINDOW (priv->view));
  g_signal_connect_object (vscrollbar,
                           "notify::visible",
                           G_CALLBACK (photos_view_container_view_changed),
                           self,
                           G_CONNECT_SWAPPED);

  photos_view_container_view_changed (self);
}


static void
photos_view_container_disconnect_view (PhotosViewContainer *self)
{
  PhotosViewContainerPrivate *priv = self->priv;
  GtkAdjustment *vadjustment;
  GtkWidget *vscrollbar;

  vadjustment = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (priv->view));
  vscrollbar = gtk_scrolled_window_get_vscrollbar (GTK_SCROLLED_WINDOW (priv->view));

  g_signal_handlers_disconnect_by_func (vadjustment, photos_view_container_view_changed, self);
  g_signal_handlers_disconnect_by_func (vscrollbar, photos_view_container_view_changed, self);
}


static void
photos_view_container_item_activated (GdMainView *main_view,
                                      const gchar * id,
                                      const GtkTreePath *path,
                                      gpointer user_data)
{
  PhotosViewContainer *self = PHOTOS_VIEW_CONTAINER (user_data);
  PhotosViewContainerPrivate *priv = self->priv;
  GObject *object;

  object = photos_base_manager_get_object_by_id (priv->item_mngr, id);

  if (!photos_base_item_is_collection (PHOTOS_BASE_ITEM (object)) &&
      photos_remote_display_manager_is_active (priv->remote_mngr))
    photos_remote_display_manager_render (priv->remote_mngr, PHOTOS_BASE_ITEM (object));
  else
    photos_base_manager_set_active_object (priv->item_mngr, object);
}


static void
photos_view_container_query_status_changed (PhotosTrackerController *trk_cntrlr,
                                            gboolean query_status,
                                            gpointer user_data)
{
  PhotosViewContainer *self = PHOTOS_VIEW_CONTAINER (user_data);
  PhotosViewContainerPrivate *priv = self->priv;

  if (!query_status)
    {
      gd_main_view_set_model (priv->view, GTK_TREE_MODEL (priv->model));
      photos_selection_controller_freeze_selection (priv->sel_cntrlr, FALSE);
      /* TODO: update selection */
    }
  else
    {
      photos_selection_controller_freeze_selection (priv->sel_cntrlr, TRUE);
      gd_main_view_set_model (priv->view, NULL);
    }
}


static void
photos_view_container_selection_mode_changed (PhotosSelectionController *sel_cntrlr,
                                              gboolean mode,
                                              gpointer user_data)
{
  PhotosViewContainer *self = PHOTOS_VIEW_CONTAINER (user_data);
  gd_main_view_set_selection_mode (self->priv->view, mode);
}


static void
photos_view_container_selection_mode_request (GdMainView *main_view, gpointer user_data)
{
  PhotosViewContainer *self = PHOTOS_VIEW_CONTAINER (user_data);
  photos_selection_controller_set_selection_mode (self->priv->sel_cntrlr, TRUE);
}


static void
photos_view_container_view_selection_changed (GdMainView *main_view, gpointer user_data)
{
  PhotosViewContainer *self = PHOTOS_VIEW_CONTAINER (user_data);
  PhotosViewContainerPrivate *priv = self->priv;
  GList *selected_urns;
  GList *selection;

  selection = gd_main_view_get_selection (main_view);
  selected_urns = photos_utils_get_urns_from_paths (selection, GTK_TREE_MODEL (priv->model));
  photos_selection_controller_set_selection (priv->sel_cntrlr, selected_urns);

  if (selection != NULL)
    g_list_free_full (selection, (GDestroyNotify) gtk_tree_path_free);
}


static void
photos_view_container_window_mode_changed (PhotosModeController *mode_cntrlr,
                                           PhotosWindowMode mode,
                                           PhotosWindowMode old_mode,
                                           gpointer user_data)
{
  PhotosViewContainer *self = PHOTOS_VIEW_CONTAINER (user_data);

  photos_view_container_disconnect_view (self);

  if (mode == PHOTOS_WINDOW_MODE_COLLECTIONS
      || mode == PHOTOS_WINDOW_MODE_FAVORITES
      || mode == PHOTOS_WINDOW_MODE_OVERVIEW)
    photos_view_container_connect_view (self);
}


static void
photos_view_container_constructed (GObject *object)
{
  PhotosViewContainer *self = PHOTOS_VIEW_CONTAINER (object);
  PhotosViewContainerPrivate *priv = self->priv;
  GAction *action;
  GtkApplication *app;
  gboolean status;

  G_OBJECT_CLASS (photos_view_container_parent_class)->constructed (object);

  priv->model = photos_view_model_new (priv->mode);

  gtk_orientable_set_orientation (GTK_ORIENTABLE (self), GTK_ORIENTATION_VERTICAL);

  priv->view = gd_main_view_new (GD_MAIN_VIEW_ICON);
  gtk_container_add (GTK_CONTAINER (self), GTK_WIDGET (priv->view));

  priv->load_more = photos_load_more_button_new (priv->mode);
  gtk_container_add (GTK_CONTAINER (self), priv->load_more);

  gtk_widget_show_all (GTK_WIDGET (self));

  g_signal_connect (priv->view, "item-activated", G_CALLBACK (photos_view_container_item_activated), self);
  g_signal_connect (priv->view,
                    "selection-mode-request",
                    G_CALLBACK (photos_view_container_selection_mode_request),
                    self);
  g_signal_connect (priv->view,
                    "view-selection-changed",
                    G_CALLBACK (photos_view_container_view_selection_changed),
                    self);

  priv->item_mngr = photos_item_manager_new ();

  priv->sel_cntrlr = photos_selection_controller_new ();
  g_signal_connect (priv->sel_cntrlr,
                    "selection-mode-changed",
                    G_CALLBACK (photos_view_container_selection_mode_changed),
                    self);
  photos_view_container_selection_mode_changed (priv->sel_cntrlr,
                                                photos_selection_controller_get_selection_mode (priv->sel_cntrlr),
                                                self);

  priv->mode_cntrlr = photos_mode_controller_new ();
  g_signal_connect (priv->mode_cntrlr,
                    "window-mode-changed",
                    G_CALLBACK (photos_view_container_window_mode_changed),
                    self);

  priv->remote_mngr = photos_remote_display_manager_dup_singleton ();

  switch (priv->mode)
    {
    case PHOTOS_WINDOW_MODE_COLLECTIONS:
      priv->trk_cntrlr = photos_tracker_collections_controller_new ();
      break;

    case PHOTOS_WINDOW_MODE_FAVORITES:
      priv->trk_cntrlr = photos_tracker_favorites_controller_new ();
      break;

    case PHOTOS_WINDOW_MODE_OVERVIEW:
      priv->trk_cntrlr = photos_tracker_overview_controller_new ();
      break;

    default:
      g_assert_not_reached ();
      break;
    }

  app = photos_application_new ();

  action = g_action_map_lookup_action (G_ACTION_MAP (app), "select-all");
  g_signal_connect_swapped (action, "activate", G_CALLBACK (gd_main_view_select_all), priv->view);

  action = g_action_map_lookup_action (G_ACTION_MAP (app), "select-none");
  g_signal_connect_swapped (action, "activate", G_CALLBACK (gd_main_view_unselect_all), priv->view);

  g_object_unref (app);

  g_signal_connect (priv->trk_cntrlr,
                    "query-status-changed",
                    G_CALLBACK (photos_view_container_query_status_changed),
                    self);
  photos_tracker_controller_start (priv->trk_cntrlr);

  status = photos_tracker_controller_get_query_status (priv->trk_cntrlr);
  photos_view_container_query_status_changed (priv->trk_cntrlr, status, self);
}


static void
photos_view_container_dispose (GObject *object)
{
  PhotosViewContainer *self = PHOTOS_VIEW_CONTAINER (object);
  PhotosViewContainerPrivate *priv = self->priv;

  if (!priv->disposed)
    {
      photos_view_container_disconnect_view (self);
      priv->disposed = TRUE;
    }

  g_clear_object (&priv->model);
  g_clear_object (&priv->item_mngr);
  g_clear_object (&priv->mode_cntrlr);
  g_clear_object (&priv->remote_mngr);
  g_clear_object (&priv->sel_cntrlr);
  g_clear_object (&priv->trk_cntrlr);

  G_OBJECT_CLASS (photos_view_container_parent_class)->dispose (object);
}


static void
photos_view_container_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
  PhotosViewContainer *self = PHOTOS_VIEW_CONTAINER (object);

  switch (prop_id)
    {
    case PROP_MODE:
      self->priv->mode = (PhotosWindowMode) g_value_get_enum (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
photos_view_container_init (PhotosViewContainer *self)
{
  PhotosViewContainerPrivate *priv;

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                            PHOTOS_TYPE_VIEW_CONTAINER,
                                            PhotosViewContainerPrivate);
}


static void
photos_view_container_class_init (PhotosViewContainerClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->constructed = photos_view_container_constructed;
  object_class->dispose = photos_view_container_dispose;
  object_class->set_property = photos_view_container_set_property;

  g_object_class_install_property (object_class,
                                   PROP_MODE,
                                   g_param_spec_enum ("mode",
                                                      "PhotosWindowMode enum",
                                                      "The mode for which the widget is a view",
                                                      PHOTOS_TYPE_WINDOW_MODE,
                                                      PHOTOS_WINDOW_MODE_NONE,
                                                      G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE));

  g_type_class_add_private (class, sizeof (PhotosViewContainerPrivate));
}


GtkWidget *
photos_view_container_new (PhotosWindowMode mode)
{
  return g_object_new (PHOTOS_TYPE_VIEW_CONTAINER, "mode", mode, NULL);
}
