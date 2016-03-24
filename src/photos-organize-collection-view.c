/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2012 – 2016 Red Hat, Inc.
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
#include <glib.h>
#include <libgd/gd.h>

#include "photos-base-item.h"
#include "photos-base-manager.h"
#include "photos-create-collection-job.h"
#include "photos-fetch-collection-state-job.h"
#include "photos-organize-collection-model.h"
#include "photos-organize-collection-view.h"
#include "photos-query.h"
#include "photos-search-context.h"
#include "photos-set-collection-job.h"
#include "photos-utils.h"


struct _PhotosOrganizeCollectionViewPrivate
{
  GCancellable *cancellable;
  GtkCellRenderer *renderer_check;
  GtkCellRenderer *renderer_detail;
  GtkCellRenderer *renderer_text;
  GtkListStore *model;
  GtkTreeViewColumn *view_col;
  PhotosBaseManager *col_mngr;
  PhotosBaseManager *src_mngr;
  gboolean choice_confirmed;
};


G_DEFINE_TYPE_WITH_PRIVATE (PhotosOrganizeCollectionView, photos_organize_collection_view, GTK_TYPE_TREE_VIEW);


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
                                       state & PHOTOS_COLLECTION_STATE_ACTIVE);
  g_object_set (cell_renderer,
                "inconsistent", (state & PHOTOS_COLLECTION_STATE_INCONSISTENT) != 0,
                NULL);
  gtk_cell_renderer_set_visible (cell_renderer, g_strcmp0 (id, PHOTOS_COLLECTION_PLACEHOLDER_ID));
}


static void
photos_organize_collection_view_set_collection_executed (gpointer user_data)
{
  PhotosOrganizeCollectionView *self = PHOTOS_ORGANIZE_COLLECTION_VIEW (user_data);

  photos_organize_collection_model_refresh_collection_state (PHOTOS_ORGANIZE_COLLECTION_MODEL (self->priv->model));
  g_object_unref (self);
}


static void
photos_organize_collection_view_check_toggled (PhotosOrganizeCollectionView *self, gchar *path)
{
  PhotosOrganizeCollectionViewPrivate *priv = self->priv;
  GtkTreeIter iter;
  GtkTreePath *tree_path;
  PhotosSetCollectionJob *job;
  gboolean state;
  gchar *coll_urn;

  tree_path = gtk_tree_path_new_from_string (path);
  gtk_tree_model_get_iter (GTK_TREE_MODEL (priv->model), &iter, tree_path);
  gtk_tree_model_get (GTK_TREE_MODEL (priv->model), &iter, PHOTOS_ORGANIZE_MODEL_ID, &coll_urn, -1);
  state = gtk_cell_renderer_toggle_get_active (GTK_CELL_RENDERER_TOGGLE (priv->renderer_check));

  job = photos_set_collection_job_new (coll_urn, !state);
  photos_set_collection_job_run (job, photos_organize_collection_view_set_collection_executed, g_object_ref (self));
  g_object_unref (job);

  g_free (coll_urn);
}


static void
photos_organize_collection_view_create_collection_executed (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosOrganizeCollectionView *self;
  PhotosOrganizeCollectionViewPrivate *priv;
  PhotosCreateCollectionJob *col_job = PHOTOS_CREATE_COLLECTION_JOB (source_object);
  PhotosSetCollectionJob *set_job = NULL;
  GtkTreeIter iter;
  GtkTreePath *path = NULL;
  GError *error = NULL;
  const gchar *created_urn;

  created_urn = photos_create_collection_job_finish (col_job, res, &error);
  if (error != NULL)
    {
      if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        {
          g_error_free (error);
          goto out;
        }
      else
        {
          g_warning ("Unable to create collection: %s", error->message);
          g_error_free (error);
        }
    }

  self = PHOTOS_ORGANIZE_COLLECTION_VIEW (user_data);
  priv = self->priv;

  if (created_urn == NULL)
    {
      photos_organize_collection_model_remove_placeholder (PHOTOS_ORGANIZE_COLLECTION_MODEL (priv->model));
      goto out;
    }

  path = photos_organize_collection_model_get_placeholder (PHOTOS_ORGANIZE_COLLECTION_MODEL (priv->model), TRUE);
  if (path == NULL)
    goto out;

  gtk_tree_model_get_iter (GTK_TREE_MODEL (priv->model), &iter, path);
  gtk_list_store_set (priv->model, &iter, PHOTOS_ORGANIZE_MODEL_ID, created_urn, -1);

  set_job = photos_set_collection_job_new (created_urn, TRUE);
  photos_set_collection_job_run (set_job, NULL, NULL);

 out:
  g_clear_object (&set_job);
  gtk_tree_path_free (path);
}


static void
photos_organize_collection_view_detail_cell (GtkTreeViewColumn *tree_column,
                                             GtkCellRenderer *cell_renderer,
                                             GtkTreeModel *tree_model,
                                             GtkTreeIter *iter,
                                             gpointer user_data)
{
  PhotosOrganizeCollectionView *self = PHOTOS_ORGANIZE_COLLECTION_VIEW (user_data);
  PhotosOrganizeCollectionViewPrivate *priv = self->priv;
  GObject *object;
  gchar *id;

  gtk_tree_model_get (GTK_TREE_MODEL (priv->model), iter, PHOTOS_ORGANIZE_MODEL_ID, &id, -1);
  object = photos_base_manager_get_object_by_id (priv->col_mngr, id);

  if (object != NULL)
    {
      const gchar *identifier;

      identifier = photos_base_item_get_identifier (PHOTOS_BASE_ITEM (object));
      if (identifier != NULL && !g_str_has_prefix (identifier, PHOTOS_QUERY_LOCAL_COLLECTIONS_IDENTIFIER))
        {
          PhotosSource *source;
          const gchar *name;
          const gchar *resource_urn;

          resource_urn = photos_base_item_get_resource_urn (PHOTOS_BASE_ITEM (object));
          source = PHOTOS_SOURCE (photos_base_manager_get_object_by_id (priv->src_mngr, resource_urn));
          name = photos_source_get_name (source);
          g_object_set (cell_renderer, "text", name, NULL);
          gtk_cell_renderer_set_visible (cell_renderer, TRUE);
        }
    }
  else
    {
      g_object_set (cell_renderer, "text", "", NULL);
      gtk_cell_renderer_set_visible (cell_renderer, FALSE);
    }

  g_free (id);
}


static void
photos_organize_collection_view_text_edited_real (PhotosOrganizeCollectionView *self,
                                                  GtkCellRendererText *cell_renderer,
                                                  GtkTreePath *path,
                                                  const gchar *new_text)
{
  PhotosOrganizeCollectionViewPrivate *priv = self->priv;
  GtkTreeIter iter;
  PhotosCreateCollectionJob *job;

  g_object_set (cell_renderer, "editable", FALSE, NULL);

  if (new_text == NULL || new_text[0] == '\0')
    {
      /* Don't insert collections with empty names. */
      photos_organize_collection_model_remove_placeholder (PHOTOS_ORGANIZE_COLLECTION_MODEL (priv->model));
      return;
    }

  gtk_tree_model_get_iter (GTK_TREE_MODEL (priv->model), &iter, path);
  gtk_list_store_set (priv->model, &iter, PHOTOS_ORGANIZE_MODEL_NAME, new_text, -1);

  job = photos_create_collection_job_new (new_text);
  photos_create_collection_job_run (job,
                                    priv->cancellable,
                                    photos_organize_collection_view_create_collection_executed,
                                    self);
  g_object_unref (job);
}


static void
photos_organize_collection_view_text_edited (PhotosOrganizeCollectionView *self, gchar *path, gchar *new_text)
{
  PhotosOrganizeCollectionViewPrivate *priv = self->priv;
  GtkTreePath *tree_path;

  tree_path = gtk_tree_path_new_from_string (path);
  photos_organize_collection_view_text_edited_real (self,
                                                    GTK_CELL_RENDERER_TEXT (priv->renderer_text),
                                                    tree_path,
                                                    new_text);
  gtk_tree_path_free (tree_path);
}


static void
photos_organize_collection_view_text_editing_canceled (PhotosOrganizeCollectionView *self)
{
  PhotosOrganizeCollectionViewPrivate *priv = self->priv;

  if (priv->choice_confirmed)
    {
      GtkCellArea *cell_area;
      GtkCellEditable *entry;
      GtkTreePath *path;

      priv->choice_confirmed = FALSE;

      g_object_get (priv->view_col, "cell-area", &cell_area, NULL);
      entry = gtk_cell_area_get_edit_widget (cell_area);
      g_object_unref (cell_area);

      path = photos_organize_collection_model_get_placeholder (PHOTOS_ORGANIZE_COLLECTION_MODEL (priv->model),
                                                               FALSE);

      if (entry != NULL && path != NULL)
        {
          const gchar *text;

          text = gtk_entry_get_text (GTK_ENTRY (entry));
          photos_organize_collection_view_text_edited_real (self,
                                                            GTK_CELL_RENDERER_TEXT (priv->renderer_text),
                                                            path,
                                                            text);
        }

      gtk_tree_path_free (path);
    }
  else
    photos_organize_collection_model_remove_placeholder (PHOTOS_ORGANIZE_COLLECTION_MODEL (priv->model));
}


static void
photos_organize_collection_view_dispose (GObject *object)
{
  PhotosOrganizeCollectionView *self = PHOTOS_ORGANIZE_COLLECTION_VIEW (object);
  PhotosOrganizeCollectionViewPrivate *priv = self->priv;

  if (priv->cancellable != NULL)
    {
      g_cancellable_cancel (priv->cancellable);
      g_clear_object (&priv->cancellable);
    }

  g_clear_object (&priv->model);
  g_clear_object (&priv->col_mngr);
  g_clear_object (&priv->src_mngr);

  G_OBJECT_CLASS (photos_organize_collection_view_parent_class)->dispose (object);
}


static void
photos_organize_collection_view_init (PhotosOrganizeCollectionView *self)
{
  PhotosOrganizeCollectionViewPrivate *priv;
  GApplication *app;
  PhotosSearchContextState *state;

  self->priv = photos_organize_collection_view_get_instance_private (self);
  priv = self->priv;

  app = g_application_get_default ();
  state = photos_search_context_get_state (PHOTOS_SEARCH_CONTEXT (app));

  priv->cancellable = g_cancellable_new ();

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
  g_signal_connect_swapped (priv->renderer_check,
                            "toggled",
                            G_CALLBACK (photos_organize_collection_view_check_toggled),
                            self);

  priv->renderer_text = gtk_cell_renderer_text_new ();
  gtk_tree_view_column_pack_start (priv->view_col, priv->renderer_text, TRUE);
  gtk_tree_view_column_add_attribute (priv->view_col, priv->renderer_text, "text", PHOTOS_ORGANIZE_MODEL_NAME);
  g_signal_connect_swapped (priv->renderer_text,
                            "edited",
                            G_CALLBACK (photos_organize_collection_view_text_edited),
                            self);
  g_signal_connect_swapped (priv->renderer_text,
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

  priv->col_mngr = g_object_ref (state->src_mngr);
  priv->src_mngr = g_object_ref (state->src_mngr);

  gtk_widget_show (GTK_WIDGET (self));
}


static void
photos_organize_collection_view_class_init (PhotosOrganizeCollectionViewClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->dispose = photos_organize_collection_view_dispose;
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
