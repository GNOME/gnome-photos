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

#include "photos-item-manager.h"
#include "photos-selection-controller.h"
#include "photos-tracker-controller.h"
#include "photos-utils.h"
#include "photos-view.h"


struct _PhotosViewPrivate
{
  GtkListStore *model;
  PhotosBaseManager *item_mngr;
  PhotosSelectionController *sel_cntrlr;
  PhotosTrackerController *trk_cntrlr;
};


G_DEFINE_TYPE (PhotosView, photos_view, GD_TYPE_MAIN_VIEW);


static void
photos_view_item_activated (GdMainView *main_view, const gchar * id, const GtkTreePath *path, gpointer user_data)
{
  /* TODO: DocumentManager */
}


static void
photos_view_query_status_changed (PhotosTrackerController *trk_cntrlr, gboolean query_status, gpointer user_data)
{
  PhotosView *self = PHOTOS_VIEW (user_data);
  PhotosViewPrivate *priv = self->priv;

  if (!query_status)
    {
      priv->model = photos_item_manager_get_model (PHOTOS_ITEM_MANAGER (priv->item_mngr));
      gd_main_view_set_model (GD_MAIN_VIEW (self), GTK_TREE_MODEL (priv->model));
      photos_selection_controller_freeze_selection (priv->sel_cntrlr, FALSE);
      /* TODO: update selection */
    }
  else
    {
      photos_selection_controller_freeze_selection (priv->sel_cntrlr, TRUE);
      priv->model = NULL;
      gd_main_view_set_model (GD_MAIN_VIEW (self), NULL);
    }
}


static void
photos_view_selection_mode_changed (PhotosSelectionController *sel_cntrlr, gboolean mode, gpointer user_data)
{
  gd_main_view_set_selection_mode (GD_MAIN_VIEW (user_data), mode);
}


static void
photos_view_selection_mode_request (GdMainView *main_view, gpointer user_data)
{
  PhotosView *self = PHOTOS_VIEW (main_view);
  photos_selection_controller_set_selection_mode (self->priv->sel_cntrlr, TRUE);
}


static void
photos_view_view_selection_changed (GdMainView *main_view, gpointer user_data)
{
  PhotosView *self = PHOTOS_VIEW (main_view);
  GtkTreeModel *model;
  GList *selected_urns;
  GList *selection;

  selection = gd_main_view_get_selection (main_view);
  model = gd_main_view_get_model (main_view);
  selected_urns = photos_utils_get_urns_from_paths (selection, model);
  photos_selection_controller_set_selection (self->priv->sel_cntrlr, selected_urns);

  if (selection != NULL)
    g_list_free_full (selection, (GDestroyNotify) gtk_tree_path_free);
}


static void
photos_view_dispose (GObject *object)
{
  PhotosView *self = PHOTOS_VIEW (object);
  PhotosViewPrivate *priv = self->priv;

  g_clear_object (&priv->item_mngr);
  g_clear_object (&priv->sel_cntrlr);
  g_clear_object (&priv->trk_cntrlr);

  G_OBJECT_CLASS (photos_view_parent_class)->dispose (object);
}


static void
photos_view_init (PhotosView *self)
{
  PhotosViewPrivate *priv;

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                            PHOTOS_TYPE_VIEW,
                                            PhotosViewPrivate);
  priv = self->priv;

  g_signal_connect (self, "item-activated", G_CALLBACK (photos_view_item_activated), NULL);
  g_signal_connect (self, "selection-mode-request", G_CALLBACK (photos_view_selection_mode_request), NULL);
  g_signal_connect (self, "view-selection-changed", G_CALLBACK (photos_view_view_selection_changed), NULL);

  priv->item_mngr = photos_item_manager_new ();

  priv->sel_cntrlr = photos_selection_controller_new ();
  g_signal_connect (priv->sel_cntrlr,
                    "selection-mode-changed",
                    G_CALLBACK (photos_view_selection_mode_changed),
                    self);
  photos_view_selection_mode_changed (priv->sel_cntrlr,
                                      photos_selection_controller_get_selection_mode (priv->sel_cntrlr),
                                      self);

  priv->trk_cntrlr = photos_tracker_controller_new ();
  g_signal_connect (priv->trk_cntrlr, "query-status-changed", G_CALLBACK (photos_view_query_status_changed), self);
  photos_tracker_controller_start (priv->trk_cntrlr);

  gtk_widget_show (GTK_WIDGET (self));
}


static void
photos_view_class_init (PhotosViewClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->dispose = photos_view_dispose;

  g_type_class_add_private (class, sizeof (PhotosViewPrivate));
}


GtkWidget *
photos_view_new (void)
{
  return g_object_new (PHOTOS_TYPE_VIEW, "view-type", GD_MAIN_VIEW_ICON, NULL);
}
