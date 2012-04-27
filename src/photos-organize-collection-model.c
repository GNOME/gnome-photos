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

#include "photos-collection-manager.h"
#include "photos-organize-collection-model.h"


struct _PhotosOrganizeCollectionModelPrivate
{
  GtkTreeRowReference *placeholder_ref;
  PhotosBaseManager *manager;
  gulong coll_added_id;
  gulong coll_removed_id;
};


G_DEFINE_TYPE (PhotosOrganizeCollectionModel, photos_organize_collection_model, GTK_TYPE_LIST_STORE);


static void
photos_organize_collection_model_item_added (PhotosBaseManager *manager, GObject *item, gpointer user_data)
{
}


static void
photos_organize_collection_model_item_removed (PhotosBaseManager *manager, GObject *item, gpointer user_data)
{
}


static void
photos_organize_collection_model_dispose (GObject *object)
{
  PhotosOrganizeCollectionModel *self = PHOTOS_ORGANIZE_COLLECTION_MODEL (object);
  PhotosOrganizeCollectionModelPrivate *priv = self->priv;

  if (priv->manager != NULL)
    {
      g_object_unref (priv->manager);
      priv->manager = NULL;
    }

  G_OBJECT_CLASS (photos_organize_collection_model_parent_class)->dispose (object);
}


static void
photos_organize_collection_model_finalize (GObject *object)
{
  PhotosOrganizeCollectionModel *self = PHOTOS_ORGANIZE_COLLECTION_MODEL (object);

  gtk_tree_row_reference_free (self->priv->placeholder_ref);

  G_OBJECT_CLASS (photos_organize_collection_model_parent_class)->finalize (object);
}


static void
photos_organize_collection_model_init (PhotosOrganizeCollectionModel *self)
{
  PhotosOrganizeCollectionModelPrivate *priv;
  GType columns[] = {G_TYPE_STRING,  /* ID */
                     G_TYPE_STRING,  /* NAME */
                     G_TYPE_INT};    /* STATE */

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                            PHOTOS_TYPE_ORGANIZE_COLLECTION_MODEL,
                                            PhotosOrganizeCollectionModelPrivate);
  priv = self->priv;

  gtk_list_store_set_column_types (GTK_LIST_STORE (self), sizeof (columns) / sizeof (columns[0]), columns);

  priv->manager = photos_collection_manager_new ();
  priv->coll_added_id = g_signal_connect (priv->manager,
                                          "item-added",
                                          G_CALLBACK (photos_organize_collection_model_item_added),
                                          self);
  priv->coll_removed_id = g_signal_connect (priv->manager,
                                            "item-removed",
                                            G_CALLBACK (photos_organize_collection_model_item_removed),
                                            self);

  /* TODO: populate the model */
}


static void
photos_organize_collection_model_class_init (PhotosOrganizeCollectionModelClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->dispose = photos_organize_collection_model_dispose;
  object_class->finalize = photos_organize_collection_model_finalize;

  g_type_class_add_private (class, sizeof (PhotosOrganizeCollectionModelPrivate));
}


GtkListStore *
photos_organize_collection_model_new (void)
{
  return g_object_new (PHOTOS_TYPE_ORGANIZE_COLLECTION_MODEL, NULL);
}


GtkTreePath *
photos_organize_collection_model_add_placeholder (PhotosOrganizeCollectionModel *self)
{
  PhotosOrganizeCollectionModelPrivate *priv = self->priv;
  GtkTreeIter iter;
  GtkTreePath *placeholder_path;

  photos_organize_collection_model_remove_placeholder (self);

  gtk_list_store_append (GTK_LIST_STORE (self), &iter);
  gtk_list_store_set (GTK_LIST_STORE (self),
                      &iter,
                      PHOTOS_ORGANIZE_MODEL_ID, PHOTOS_COLLECTION_PLACEHOLDER_ID,
                      PHOTOS_ORGANIZE_MODEL_NAME, "",
                      PHOTOS_ORGANIZE_MODEL_STATE, PHOTOS_ORGANIZE_COLLECTION_STATE_ACTIVE,
                      -1);

  placeholder_path = gtk_tree_model_get_path (GTK_TREE_MODEL (self), &iter);
  if (placeholder_path != NULL)
    priv->placeholder_ref = gtk_tree_row_reference_new (GTK_TREE_MODEL (self), placeholder_path);

  return placeholder_path;
}


void
photos_organize_collection_model_destroy (PhotosOrganizeCollectionModel *self)
{
  PhotosOrganizeCollectionModelPrivate *priv = self->priv;

  if (priv->coll_added_id != 0)
    {
      g_signal_handler_disconnect (priv->manager, priv->coll_added_id);
      priv->coll_added_id = 0;
    }

  if (priv->coll_removed_id != 0)
    {
      g_signal_handler_disconnect (priv->manager, priv->coll_removed_id);
      priv->coll_removed_id = 0;
    }
}


GtkTreePath *
photos_organize_collection_model_forget_placeholder (PhotosOrganizeCollectionModel *self)
{
  PhotosOrganizeCollectionModelPrivate *priv = self->priv;
  GtkTreePath *path;

  path = gtk_tree_row_reference_get_path (priv->placeholder_ref);
  gtk_tree_row_reference_free (priv->placeholder_ref);
  priv->placeholder_ref = NULL;
  return path;
}


void
photos_organize_collection_model_remove_placeholder (PhotosOrganizeCollectionModel *self)
{
  PhotosOrganizeCollectionModelPrivate *priv = self->priv;
  GtkTreeIter placeholder_iter;
  GtkTreePath *placeholder_path;

  if (priv->placeholder_ref == NULL)
    return;

  placeholder_path = gtk_tree_row_reference_get_path (priv->placeholder_ref);
  if (gtk_tree_model_get_iter (GTK_TREE_MODEL (self), &placeholder_iter, placeholder_path))
    gtk_list_store_remove (GTK_LIST_STORE (self), &placeholder_iter);

  gtk_tree_row_reference_free (priv->placeholder_ref);
  priv->placeholder_ref = NULL;
}
