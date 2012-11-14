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

/* Based on code from:
 *   + Documents
 */


#include "config.h"

#include <glib.h>
#include <libgd/gd.h>

#include "photos-organize-collection-model.h"
#include "photos-organize-collection-view.h"


struct _PhotosOrganizeCollectionViewPrivate
{
  GtkCellRenderer *renderer_check;
  GtkCellRenderer *renderer_detail;
  GtkCellRenderer *renderer_text;
  GtkListStore *model;
  GtkTreeViewColumn *view_col;
  gboolean choice_confirmed;
};


G_DEFINE_TYPE (PhotosOrganizeCollectionView, photos_organize_collection_view, GTK_TYPE_TREE_VIEW);


static void
photos_organize_collection_view_check_cell (GtkTreeViewColumn *tree_column,
                                            GtkCellRenderer *cell_renderer,
                                            GtkTreeModel *tree_model,
                                            GtkTreeIter *iter,
                                            gpointer user_data)
{
  gchar *id;
  gint state;

  gtk_tree_model_get (tree_model, iter, PHOTOS_ORGANIZE_MODEL_ID, &id, PHOTOS_ORGANIZE_MODEL_STATE, &state, -1);

  gtk_cell_renderer_toggle_set_active (GTK_CELL_RENDERER_TOGGLE (cell_renderer),
                                       state & PHOTOS_ORGANIZE_COLLECTION_STATE_ACTIVE);
  g_object_set (cell_renderer, "inconsistent", state & PHOTOS_ORGANIZE_COLLECTION_STATE_INCONSISTENT, NULL);
  gtk_cell_renderer_set_visible (cell_renderer, g_strcmp0 (id, PHOTOS_COLLECTION_PLACEHOLDER_ID));
}


static void
photos_organize_collection_view_check_toggled (GtkCellRendererToggle *cell_renderer,
                                               gchar *path,
                                               gpointer user_data)
{
}


static void
photos_organize_collection_view_detail_cell (GtkTreeViewColumn *tree_column,
                                             GtkCellRenderer *cell_renderer,
                                             GtkTreeModel *tree_model,
                                             GtkTreeIter *iter,
                                             gpointer user_data)
{
}


static void
photos_organize_collection_view_text_edited (GtkCellRendererText *cell_renderer,
                                             gchar *path,
                                             gchar *new_text,
                                             gpointer user_data)
{
}


static void
photos_organize_collection_view_text_editing_canceled (GObject *object, GParamSpec *pspec, gpointer user_data)
{
}


static void
photos_organize_collection_view_destroy (GtkWidget *widget)
{
  PhotosOrganizeCollectionView *self = PHOTOS_ORGANIZE_COLLECTION_VIEW (widget);
  photos_organize_collection_model_destroy (PHOTOS_ORGANIZE_COLLECTION_MODEL (self->priv->model));
}


static void
photos_organize_collection_view_dispose (GObject *object)
{
  PhotosOrganizeCollectionView *self = PHOTOS_ORGANIZE_COLLECTION_VIEW (object);
  PhotosOrganizeCollectionViewPrivate *priv = self->priv;

  if (priv->model != NULL)
    {
      g_object_unref (priv->model);
      priv->model = NULL;
    }

  G_OBJECT_CLASS (photos_organize_collection_view_parent_class)->dispose (object);
}


static void
photos_organize_collection_view_init (PhotosOrganizeCollectionView *self)
{
  PhotosOrganizeCollectionViewPrivate *priv;

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                            PHOTOS_TYPE_ORGANIZE_COLLECTION_VIEW,
                                            PhotosOrganizeCollectionViewPrivate);
  priv = self->priv;

  priv->model = photos_organize_collection_model_new ();
  gtk_tree_view_set_model (GTK_TREE_VIEW (self), GTK_TREE_MODEL (priv->model));

  priv->view_col = gtk_tree_view_column_new ();
  gtk_tree_view_append_column (GTK_TREE_VIEW (self), priv->view_col);

  priv->renderer_check = gtk_cell_renderer_toggle_new ();
  gtk_tree_view_column_pack_start (priv->view_col, priv->renderer_check, FALSE);
  gtk_tree_view_column_set_cell_data_func (priv->view_col,
                                           priv->renderer_check,
                                           photos_organize_collection_view_check_cell,
                                           self,
                                           NULL);
  g_signal_connect (priv->renderer_check,
                    "toggled",
                    G_CALLBACK (photos_organize_collection_view_check_toggled),
                    self);

  priv->renderer_text = gtk_cell_renderer_text_new ();
  gtk_tree_view_column_pack_start (priv->view_col, priv->renderer_text, TRUE);
  gtk_tree_view_column_add_attribute (priv->view_col, priv->renderer_text, "text", PHOTOS_ORGANIZE_MODEL_NAME);
  g_signal_connect (priv->renderer_text,
                    "edited",
                    G_CALLBACK (photos_organize_collection_view_text_edited),
                    self);
  g_signal_connect (priv->renderer_text,
                    "editing-canceled",
                    G_CALLBACK (photos_organize_collection_view_text_editing_canceled),
                    self);

  priv->renderer_detail = gd_styled_text_renderer_new ();
  gtk_cell_renderer_set_padding (priv->renderer_detail, 16, 0);
  gd_styled_text_renderer_add_class (GD_STYLED_TEXT_RENDERER (priv->renderer_detail), "dim-label");
  gtk_tree_view_column_pack_start (priv->view_col, priv->renderer_detail, FALSE);
  gtk_tree_view_column_set_cell_data_func (priv->view_col,
                                           priv->renderer_detail,
                                           photos_organize_collection_view_detail_cell,
                                           self,
                                           NULL);

  gtk_widget_show (GTK_WIDGET (self));
}


static void
photos_organize_collection_view_class_init (PhotosOrganizeCollectionViewClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->dispose = photos_organize_collection_view_dispose;
  widget_class->destroy = photos_organize_collection_view_destroy;

  g_type_class_add_private (class, sizeof (PhotosOrganizeCollectionViewPrivate));
}


GtkWidget *
photos_organize_collection_view_new (void)
{
  return g_object_new (PHOTOS_TYPE_ORGANIZE_COLLECTION_VIEW,
                       "headers-visible", FALSE,
                       "vexpand", TRUE,
                       "hexpand", TRUE,
                       NULL);
}


void
photos_organize_collection_view_add_collection (PhotosOrganizeCollectionView *self)
{
  PhotosOrganizeCollectionViewPrivate *priv = self->priv;
  GtkTreePath *path;

  path = photos_organize_collection_model_add_placeholder (PHOTOS_ORGANIZE_COLLECTION_MODEL (priv->model));
  if (path == NULL)
    return;

  g_object_set (priv->renderer_text, "editable", TRUE, NULL);
  gtk_tree_view_set_cursor_on_cell (GTK_TREE_VIEW (self), path, priv->view_col, priv->renderer_text, TRUE);
}


void
photos_organize_collection_view_confirmed_choice (PhotosOrganizeCollectionView *self)
{
  self->priv->choice_confirmed = TRUE;
}
