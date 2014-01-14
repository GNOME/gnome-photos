/*
 * Photos - access, organize and share your photos on GNOME
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

#include "photos-base-view.h"
#include "photos-base-model.h"


struct _PhotosBaseViewPrivate
{
  GtkCellRenderer *renderer_heading;
  GtkCellRenderer *renderer_radio;
  GtkCellRenderer *renderer_text;
  GtkListStore *model;
  PhotosBaseManager *mngr;
};

enum
{
  PROP_0,
  PROP_MANAGER
};

enum
{
  ITEM_ACTIVATED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };


G_DEFINE_TYPE_WITH_PRIVATE (PhotosBaseView, photos_base_view, GTK_TYPE_TREE_VIEW);


typedef void (*PhotosBaseViewRendererCellFunc) (PhotosBaseView *,
                                                GtkTreeViewColumn *,
                                                GtkCellRenderer *,
                                                GtkTreeIter *);


static void
photos_base_view_visibility_for_heading (PhotosBaseView *self,
                                         GtkTreeViewColumn *col,
                                         GtkCellRenderer *cell,
                                         GtkTreeIter *iter,
                                         gboolean visible,
                                         PhotosBaseViewRendererCellFunc additional_func)
{
  gboolean cell_visible;
  gchar *heading;

  gtk_tree_model_get (GTK_TREE_MODEL (self->priv->model), iter, PHOTOS_BASE_MODEL_HEADING_TEXT, &heading, -1);
  cell_visible = (visible && heading != NULL && heading[0] != '\0')
                 || (!visible && heading != NULL && heading[0] == '\0');
  gtk_cell_renderer_set_visible (cell, cell_visible);
  g_free (heading);

  if (additional_func != NULL)
    (*additional_func) (self, col, cell, iter);
}


static void
photos_base_view_renderer_heading_cell_data (GtkTreeViewColumn *col,
                                             GtkCellRenderer *cell,
                                             GtkTreeModel *tree_model,
                                             GtkTreeIter *iter,
                                             gpointer user_data)
{
  PhotosBaseView *self = PHOTOS_BASE_VIEW (user_data);
  photos_base_view_visibility_for_heading (self, col, cell, iter, TRUE, NULL);
}


static void
photos_base_view_renderer_radio_cell_func (PhotosBaseView *self,
                                           GtkTreeViewColumn *col,
                                           GtkCellRenderer *cell,
                                           GtkTreeIter *iter)
{
  PhotosBaseViewPrivate *priv = self->priv;
  GObject *object;
  gboolean active;
  gchar *active_id = NULL;
  gchar *id;

  gtk_tree_model_get (GTK_TREE_MODEL (priv->model), iter, PHOTOS_BASE_MODEL_ID, &id, -1);
  object = photos_base_manager_get_active_object (priv->mngr);
  if (object != NULL)
    g_object_get (object, "id", &active_id, NULL);

  active = g_strcmp0 (id, active_id) == 0;
  gtk_cell_renderer_toggle_set_active (GTK_CELL_RENDERER_TOGGLE (cell), active);

  g_free (active_id);
  g_free (id);
}


static void
photos_base_view_renderer_radio_cell_data (GtkTreeViewColumn *col,
                                           GtkCellRenderer *cell,
                                           GtkTreeModel *tree_model,
                                           GtkTreeIter *iter,
                                           gpointer user_data)
{
  PhotosBaseView *self = PHOTOS_BASE_VIEW (user_data);
  photos_base_view_visibility_for_heading (self, col, cell, iter, FALSE, photos_base_view_renderer_radio_cell_func);
}


static void
photos_base_view_renderer_text_cell_data (GtkTreeViewColumn *col,
                                          GtkCellRenderer *cell,
                                          GtkTreeModel *tree_model,
                                          GtkTreeIter *iter,
                                          gpointer user_data)
{
  PhotosBaseView *self = PHOTOS_BASE_VIEW (user_data);
  photos_base_view_visibility_for_heading (self, col, cell, iter, FALSE, NULL);
}


static void
photos_base_view_row_activated (GtkTreeView *tree_view, GtkTreePath *path, GtkTreeViewColumn *column)
{
  PhotosBaseView *self = PHOTOS_BASE_VIEW (tree_view);
  PhotosBaseViewPrivate *priv = self->priv;
  GtkTreeIter iter;
  gchar *id;

  gtk_tree_model_get_iter (GTK_TREE_MODEL (priv->model), &iter, path);
  gtk_tree_model_get (GTK_TREE_MODEL (priv->model), &iter, PHOTOS_BASE_MODEL_ID, &id, -1);

  g_signal_emit (self, signals[ITEM_ACTIVATED], 0);
  photos_base_manager_set_active_object_by_id (priv->mngr, id);

  g_free (id);
}


static void
photos_base_view_constructed (GObject *object)
{
  PhotosBaseView *self = PHOTOS_BASE_VIEW (object);
  PhotosBaseViewPrivate *priv = self->priv;
  GtkTreeSelection *selection;
  GtkTreeViewColumn *col;

  G_OBJECT_CLASS (photos_base_view_parent_class)->constructed (object);

  priv->model = photos_base_model_new (priv->mngr);
  gtk_tree_view_set_model (GTK_TREE_VIEW (self), GTK_TREE_MODEL (priv->model));

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (self));
  gtk_tree_selection_set_mode (selection, GTK_SELECTION_NONE);

  g_signal_connect (self, "row-activated", G_CALLBACK (photos_base_view_row_activated), NULL);

  col = gtk_tree_view_column_new ();
  gtk_tree_view_append_column (GTK_TREE_VIEW (self), col);

  priv->renderer_heading = gtk_cell_renderer_text_new ();
  g_object_set (priv->renderer_heading, "weight", PANGO_WEIGHT_BOLD, "weight-set", TRUE, NULL);
  gtk_tree_view_column_pack_start (col, priv->renderer_heading, FALSE);
  gtk_tree_view_column_add_attribute (col, priv->renderer_heading, "text", PHOTOS_BASE_MODEL_HEADING_TEXT);
  gtk_tree_view_column_set_cell_data_func (col,
                                           priv->renderer_heading,
                                           photos_base_view_renderer_heading_cell_data,
                                           self,
                                           NULL);

  priv->renderer_radio = gtk_cell_renderer_toggle_new ();
  gtk_cell_renderer_toggle_set_radio (GTK_CELL_RENDERER_TOGGLE (priv->renderer_radio), TRUE);
  g_object_set (priv->renderer_radio, "mode", GTK_CELL_RENDERER_MODE_INERT, NULL);
  gtk_tree_view_column_pack_start (col, priv->renderer_radio, FALSE);
  gtk_tree_view_column_set_cell_data_func (col,
                                           priv->renderer_radio,
                                           photos_base_view_renderer_radio_cell_data,
                                           self,
                                           NULL);

  priv->renderer_text = gtk_cell_renderer_text_new ();
  gtk_tree_view_column_pack_start (col, priv->renderer_text, TRUE);
  gtk_tree_view_column_add_attribute (col, priv->renderer_text, "text", PHOTOS_BASE_MODEL_NAME);
  gtk_tree_view_column_set_cell_data_func (col,
                                           priv->renderer_text,
                                           photos_base_view_renderer_text_cell_data,
                                           self,
                                           NULL);

  gtk_widget_show_all (GTK_WIDGET (self));
}


static void
photos_base_view_dispose (GObject *object)
{
  PhotosBaseView *self = PHOTOS_BASE_VIEW (object);
  PhotosBaseViewPrivate *priv = self->priv;

  g_clear_object (&priv->model);
  g_clear_object (&priv->mngr);

  G_OBJECT_CLASS (photos_base_view_parent_class)->dispose (object);
}


static void
photos_base_view_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
  PhotosBaseView *self = PHOTOS_BASE_VIEW (object);

  switch (prop_id)
    {
    case PROP_MANAGER:
      self->priv->mngr = PHOTOS_BASE_MANAGER (g_value_dup_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
photos_base_view_init (PhotosBaseView *self)
{
  self->priv = photos_base_view_get_instance_private (self);
}


static void
photos_base_view_class_init (PhotosBaseViewClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->constructed = photos_base_view_constructed;
  object_class->dispose = photos_base_view_dispose;
  object_class->set_property = photos_base_view_set_property;

  g_object_class_install_property (object_class,
                                   PROP_MANAGER,
                                   g_param_spec_object ("manager",
                                                        "PhotosBaseManager object",
                                                        "The manager whose data is being rendered by this view",
                                                        PHOTOS_TYPE_BASE_MANAGER,
                                                        G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE));

  signals[ITEM_ACTIVATED] = g_signal_new ("item-activated",
                                          G_TYPE_FROM_CLASS (class),
                                          G_SIGNAL_RUN_LAST,
                                          G_STRUCT_OFFSET (PhotosBaseViewClass,
                                                           item_activated),
                                          NULL, /*accumulator */
                                          NULL, /*accu_data */
                                          g_cclosure_marshal_VOID__VOID,
                                          G_TYPE_NONE,
                                          0);
}


GtkWidget *
photos_base_view_new (PhotosBaseManager *mngr)
{
  return g_object_new (PHOTOS_TYPE_BASE_VIEW,
                       "activate-on-single-click", TRUE,
                       "enable-search", FALSE,
                       "headers-visible", FALSE,
                       "manager", mngr,
                       NULL);
}
