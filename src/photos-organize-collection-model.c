/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2012, 2013, 2014 Red Hat, Inc.
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

#include <gio/gio.h>

#include "photos-base-item.h"
#include "photos-base-manager.h"
#include "photos-fetch-collection-state-job.h"
#include "photos-filterable.h"
#include "photos-organize-collection-model.h"
#include "photos-search-context.h"


struct _PhotosOrganizeCollectionModelPrivate
{
  GtkTreePath *coll_path;
  GtkTreeRowReference *placeholder_ref;
  PhotosBaseManager *manager;
};


G_DEFINE_TYPE_WITH_PRIVATE (PhotosOrganizeCollectionModel, photos_organize_collection_model, GTK_TYPE_LIST_STORE);


static gboolean
photos_organize_collection_model_foreach (GtkTreeModel *model,
                                          GtkTreePath *path,
                                          GtkTreeIter *iter,
                                          gpointer user_data)
{
  PhotosOrganizeCollectionModel *self = PHOTOS_ORGANIZE_COLLECTION_MODEL (model);
  PhotosBaseItem *collection = PHOTOS_BASE_ITEM (user_data);
  gboolean ret_val = FALSE;
  gchar *id;

  gtk_tree_model_get (GTK_TREE_MODEL (self), iter, PHOTOS_ORGANIZE_MODEL_ID, &id, -1);
  if (g_strcmp0 (photos_filterable_get_id (PHOTOS_FILTERABLE (collection)), id) == 0)
    {
      self->priv->coll_path = gtk_tree_path_copy (path);
      ret_val = TRUE;
      goto out;
    }

 out:
  g_free (id);
  return ret_val;
}


static GtkTreeIter *
photos_organize_collection_model_find_collection_iter (PhotosOrganizeCollectionModel *self,
                                                       PhotosBaseItem *collection)
{
  PhotosOrganizeCollectionModelPrivate *priv = self->priv;

  gtk_tree_model_foreach (GTK_TREE_MODEL (self), photos_organize_collection_model_foreach, collection);
  if (priv->coll_path != NULL)
    {
      GtkTreeIter iter;

      gtk_tree_model_get_iter (GTK_TREE_MODEL (self), &iter, priv->coll_path);
      g_clear_pointer (&priv->coll_path, (GDestroyNotify) gtk_tree_path_free);
      return gtk_tree_iter_copy (&iter);
    }

  return NULL;
}


static void
photos_organize_collection_model_fetch_collection_state_executed (GHashTable *collection_state, gpointer user_data)
{
  PhotosOrganizeCollectionModel *self = PHOTOS_ORGANIZE_COLLECTION_MODEL (user_data);
  GHashTableIter iter;
  const gchar *idx;
  gpointer value;

  photos_organize_collection_model_remove_placeholder (self);

  g_hash_table_iter_init (&iter, collection_state);
  while (g_hash_table_iter_next (&iter, (gpointer) &idx, (gpointer) &value))
    {
      GtkTreeIter *iter;
      PhotosBaseItem *collection;
      gint state = GPOINTER_TO_INT (value);

      if (state & PHOTOS_COLLECTION_STATE_HIDDEN)
        continue;

      collection = PHOTOS_BASE_ITEM (photos_base_manager_get_object_by_id (self->priv->manager, idx));
      iter = photos_organize_collection_model_find_collection_iter (self, collection);
      if (iter == NULL)
        {
          GtkTreeIter tmp;

          gtk_list_store_append (GTK_LIST_STORE (self), &tmp);
          iter = gtk_tree_iter_copy (&tmp);
        }

      gtk_list_store_set (GTK_LIST_STORE (self),
                          iter,
                          PHOTOS_ORGANIZE_MODEL_ID, idx,
                          PHOTOS_ORGANIZE_MODEL_NAME, photos_base_item_get_name (collection),
                          PHOTOS_ORGANIZE_MODEL_STATE, state,
                          -1);
      gtk_tree_iter_free (iter);
    }

  g_object_unref (self);
}


static void
photos_organize_collection_model_refresh_state (PhotosOrganizeCollectionModel *self)
{
  PhotosFetchCollectionStateJob *job;

  job = photos_fetch_collection_state_job_new ();
  photos_fetch_collection_state_job_run (job,
                                         photos_organize_collection_model_fetch_collection_state_executed,
                                         g_object_ref (self));
}


static void
photos_organize_collection_model_object_added (PhotosBaseManager *manager, GObject *object, gpointer user_data)
{
  PhotosOrganizeCollectionModel *self = PHOTOS_ORGANIZE_COLLECTION_MODEL (user_data);
  photos_organize_collection_model_refresh_state (self);
}


static void
photos_organize_collection_model_object_removed (PhotosBaseManager *manager, GObject *object, gpointer user_data)
{
  PhotosOrganizeCollectionModel *self = PHOTOS_ORGANIZE_COLLECTION_MODEL (user_data);
  GtkTreeIter *iter;

  iter = photos_organize_collection_model_find_collection_iter (self, PHOTOS_BASE_ITEM (object));
  if (iter == NULL)
    return;

  gtk_list_store_remove (GTK_LIST_STORE (self), iter);
  gtk_tree_iter_free (iter);
}


static void
photos_organize_collection_model_dispose (GObject *object)
{
  PhotosOrganizeCollectionModel *self = PHOTOS_ORGANIZE_COLLECTION_MODEL (object);
  PhotosOrganizeCollectionModelPrivate *priv = self->priv;

  g_clear_object (&priv->manager);

  G_OBJECT_CLASS (photos_organize_collection_model_parent_class)->dispose (object);
}


static void
photos_organize_collection_model_finalize (GObject *object)
{
  PhotosOrganizeCollectionModel *self = PHOTOS_ORGANIZE_COLLECTION_MODEL (object);
  PhotosOrganizeCollectionModelPrivate *priv = self->priv;

  gtk_tree_path_free (priv->coll_path);
  gtk_tree_row_reference_free (priv->placeholder_ref);

  G_OBJECT_CLASS (photos_organize_collection_model_parent_class)->finalize (object);
}


static void
photos_organize_collection_model_init (PhotosOrganizeCollectionModel *self)
{
  PhotosOrganizeCollectionModelPrivate *priv;
  GApplication *app;
  GType columns[] = {G_TYPE_STRING,  /* ID */
                     G_TYPE_STRING,  /* NAME */
                     G_TYPE_INT};    /* STATE */
  PhotosSearchContextState *state;

  self->priv = photos_organize_collection_model_get_instance_private (self);
  priv = self->priv;

  app = g_application_get_default ();
  state = photos_search_context_get_state (PHOTOS_SEARCH_CONTEXT (app));

  gtk_list_store_set_column_types (GTK_LIST_STORE (self), sizeof (columns) / sizeof (columns[0]), columns);

  priv->manager = g_object_ref (state->item_mngr);

  g_signal_connect_object (priv->manager,
                           "object-added",
                           G_CALLBACK (photos_organize_collection_model_object_added),
                           self,
                           0);
  g_signal_connect_object (priv->manager,
                           "object-removed",
                           G_CALLBACK (photos_organize_collection_model_object_removed),
                           self,
                           0);

  /* Populate the model. */
  photos_organize_collection_model_refresh_state (self);
}


static void
photos_organize_collection_model_class_init (PhotosOrganizeCollectionModelClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->dispose = photos_organize_collection_model_dispose;
  object_class->finalize = photos_organize_collection_model_finalize;
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
                      PHOTOS_ORGANIZE_MODEL_STATE, PHOTOS_COLLECTION_STATE_ACTIVE,
                      -1);

  placeholder_path = gtk_tree_model_get_path (GTK_TREE_MODEL (self), &iter);
  if (placeholder_path != NULL)
    priv->placeholder_ref = gtk_tree_row_reference_new (GTK_TREE_MODEL (self), placeholder_path);

  return placeholder_path;
}


GtkTreePath *
photos_organize_collection_model_get_placeholder (PhotosOrganizeCollectionModel *self, gboolean forget)
{
  PhotosOrganizeCollectionModelPrivate *priv = self->priv;
  GtkTreePath *ret_val = NULL;

  if (priv->placeholder_ref != NULL)
    ret_val = gtk_tree_row_reference_get_path (priv->placeholder_ref);

  if (forget)
    {
      gtk_tree_row_reference_free (priv->placeholder_ref);
      priv->placeholder_ref = NULL;
    }

  return ret_val;
}


void
photos_organize_collection_model_refresh_collection_state (PhotosOrganizeCollectionModel *self)
{
  photos_organize_collection_model_refresh_state (self);
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

  gtk_tree_path_free (placeholder_path);
  gtk_tree_row_reference_free (priv->placeholder_ref);
  priv->placeholder_ref = NULL;
}
