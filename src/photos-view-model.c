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

#include "photos-item-manager.h"
#include "photos-view-model.h"


struct _PhotosViewModelPrivate
{
  PhotosBaseManager *item_mngr;
};


G_DEFINE_TYPE (PhotosViewModel, photos_view_model, GTK_TYPE_LIST_STORE);


static void
photos_view_model_info_set (PhotosViewModel *self, PhotosBaseItem *item, GtkTreeIter *iter)
{
  gtk_list_store_set (GTK_LIST_STORE (self),
                      iter,
                      PHOTOS_VIEW_MODEL_URN, photos_base_item_get_id (item),
                      PHOTOS_VIEW_MODEL_URI, photos_base_item_get_uri (item),
                      PHOTOS_VIEW_MODEL_NAME, photos_base_item_get_name (item),
                      PHOTOS_VIEW_MODEL_AUTHOR, photos_base_item_get_author (item),
                      PHOTOS_VIEW_MODEL_ICON, photos_base_item_get_icon (item),
                      PHOTOS_VIEW_MODEL_MTIME, photos_base_item_get_mtime (item),
                      -1);
}


static void
photos_view_model_info_updated (PhotosBaseItem *item, gpointer user_data)
{
  PhotosViewModel *self = PHOTOS_VIEW_MODEL (user_data);
  GtkTreeIter iter;
  GtkTreePath *path;
  GtkTreeRowReference *row_ref;

  row_ref = (GtkTreeRowReference *) g_object_get_data (G_OBJECT (item), "row-ref");
  path = gtk_tree_row_reference_get_path (row_ref);
  if (path == NULL)
    return;

  gtk_tree_model_get_iter (GTK_TREE_MODEL (self), &iter, path);
  photos_view_model_info_set (self, item, &iter);
}


static gboolean
photos_view_model_item_removed_foreach (GtkTreeModel *model,
                                        GtkTreePath *path,
                                        GtkTreeIter *iter,
                                        gpointer user_data)
{
  PhotosBaseItem *item = PHOTOS_BASE_ITEM (user_data);
  gboolean ret_val = FALSE;
  const gchar *id;
  gchar *value;

  id = photos_base_item_get_id (item);
  gtk_tree_model_get (model, iter, PHOTOS_VIEW_MODEL_URN, &value, -1);

  if (g_strcmp0 (id, value) == 0)
    {
      gtk_list_store_remove (GTK_LIST_STORE (model), iter);
      ret_val = TRUE;
    }

  g_free (value);
  return ret_val;
}


static void
photos_view_model_object_added (PhotosViewModel *self, GObject *object)
{
  GtkTreeIter iter;
  GtkTreePath *path;
  GtkTreeRowReference *row_ref;
  PhotosBaseItem *item = PHOTOS_BASE_ITEM (object);

  gtk_list_store_append (GTK_LIST_STORE (self), &iter);
  photos_view_model_info_set (self, item, &iter);

  path = gtk_tree_model_get_path (GTK_TREE_MODEL (self), &iter);
  row_ref = gtk_tree_row_reference_new (GTK_TREE_MODEL (self), path);
  gtk_tree_path_free (path);

  g_object_set_data_full (G_OBJECT (item), "row-ref", row_ref, (GDestroyNotify) gtk_tree_row_reference_free);
  g_signal_connect (item, "info-updated", G_CALLBACK (photos_view_model_info_updated), self);
}


static void
photos_view_model_object_removed (PhotosViewModel *self, GObject *object)
{
  PhotosBaseItem *item = PHOTOS_BASE_ITEM (object);

  gtk_tree_model_foreach (GTK_TREE_MODEL (self), photos_view_model_item_removed_foreach, item);
  g_object_set_data (object, "row-ref", NULL);
}


static void
photos_view_model_dispose (GObject *object)
{
  PhotosViewModel *self = PHOTOS_VIEW_MODEL (object);

  g_clear_object (&self->priv->item_mngr);

  G_OBJECT_CLASS (photos_view_model_parent_class)->dispose (object);
}


static void
photos_view_model_init (PhotosViewModel *self)
{
  PhotosViewModelPrivate *priv;
  GType columns[] = {G_TYPE_STRING,    /* URN */
                     G_TYPE_STRING,    /* URI */
                     G_TYPE_STRING,    /* NAME */
                     G_TYPE_STRING,    /* AUTHOR */
                     GDK_TYPE_PIXBUF,  /* ICON */
                     G_TYPE_INT64,     /* MTIME */
                     G_TYPE_BOOLEAN};  /* STATE */

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, PHOTOS_TYPE_VIEW_MODEL, PhotosViewModelPrivate);
  priv = self->priv;

  gtk_list_store_set_column_types (GTK_LIST_STORE (self), sizeof (columns) / sizeof (columns[0]), columns);
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (self), PHOTOS_VIEW_MODEL_MTIME, GTK_SORT_DESCENDING);

  priv->item_mngr = photos_item_manager_new ();
  g_signal_connect_swapped (priv->item_mngr, "object-added", G_CALLBACK (photos_view_model_object_added), self);
  g_signal_connect_swapped (priv->item_mngr, "object-removed", G_CALLBACK (photos_view_model_object_removed), self);
  g_signal_connect_swapped (priv->item_mngr, "clear", G_CALLBACK (gtk_list_store_clear), self);
}


static void
photos_view_model_class_init (PhotosViewModelClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->dispose = photos_view_model_dispose;

  g_type_class_add_private (class, sizeof (PhotosViewModelPrivate));
}


GtkListStore *
photos_view_model_new (void)
{
  return g_object_new (PHOTOS_TYPE_VIEW_MODEL, NULL);
}
