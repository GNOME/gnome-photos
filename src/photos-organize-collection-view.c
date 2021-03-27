/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2012 – 2021 Red Hat, Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
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
#include "photos-selection-controller.h"
#include "photos-set-collection-job.h"
#include "photos-utils.h"


struct _PhotosOrganizeCollectionView
{
  GtkTreeView parent_instance;
  GCancellable *cancellable;
  GtkCellRenderer *renderer_check;
  GtkCellRenderer *renderer_detail;
  GtkCellRenderer *renderer_text;
  GtkListStore *model;
  GtkTreeViewColumn *view_col;
  PhotosBaseManager *item_mngr;
  PhotosBaseManager *src_mngr;
  PhotosSelectionController *sel_cntrlr;
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
  g_autofree gchar *id = NULL;
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
photos_organize_collection_view_set_collection_executed (GObject *source_object,
                                                         GAsyncResult *res,
                                                         gpointer user_data)
{
  PhotosOrganizeCollectionView *self;
  PhotosSetCollectionJob *job = PHOTOS_SET_COLLECTION_JOB (source_object);

  {
    g_autoptr (GError) error = NULL;

    if (!photos_set_collection_job_finish (job, res, &error))
      {
        if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
          goto out;

        g_warning ("Unable to set collection: %s", error->message);
      }
  }

  self = PHOTOS_ORGANIZE_COLLECTION_VIEW (user_data);

  photos_organize_collection_model_refresh_collection_state (PHOTOS_ORGANIZE_COLLECTION_MODEL (self->model));

 out:
  return;
}


static void
photos_organize_collection_view_check_toggled (PhotosOrganizeCollectionView *self, gchar *path)
{
  GApplication *app;
  GList *urns;
  GtkTreeIter iter;
  g_autoptr (GtkTreePath) tree_path = NULL;
  PhotosSearchContextState *state;
  g_autoptr (PhotosSetCollectionJob) job = NULL;
  gboolean active;
  g_autofree gchar *coll_urn = NULL;

  app = g_application_get_default ();
  state = photos_search_context_get_state (PHOTOS_SEARCH_CONTEXT (app));

  tree_path = gtk_tree_path_new_from_string (path);
  gtk_tree_model_get_iter (GTK_TREE_MODEL (self->model), &iter, tree_path);
  gtk_tree_model_get (GTK_TREE_MODEL (self->model), &iter, PHOTOS_ORGANIZE_MODEL_ID, &coll_urn, -1);
  active = gtk_cell_renderer_toggle_get_active (GTK_CELL_RENDERER_TOGGLE (self->renderer_check));

  job = photos_set_collection_job_new (coll_urn, !active);
  urns = photos_selection_controller_get_selection (self->sel_cntrlr);
  photos_set_collection_job_run (job,
                                 state,
                                 urns,
                                 self->cancellable,
                                 photos_organize_collection_view_set_collection_executed,
                                 self);
}


static void
photos_organize_collection_view_create_collection_executed (GObject *source_object,
                                                            GAsyncResult *res,
                                                            gpointer user_data)
{
  PhotosOrganizeCollectionView *self;
  GApplication *app;
  GList *urns;
  GtkTreeIter iter;
  g_autoptr (GtkTreePath) path = NULL;
  PhotosCreateCollectionJob *col_job = PHOTOS_CREATE_COLLECTION_JOB (source_object);
  PhotosSearchContextState *state;
  g_autoptr (PhotosSetCollectionJob) set_job = NULL;
  g_autofree gchar *created_urn = NULL;

  {
    g_autoptr (GError) error = NULL;

    created_urn = photos_create_collection_job_finish (col_job, res, &error);
    if (error != NULL)
      {
        if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
          goto out;

        g_warning ("Unable to create collection: %s", error->message);
      }
  }

  self = PHOTOS_ORGANIZE_COLLECTION_VIEW (user_data);

  if (created_urn == NULL)
    {
      photos_organize_collection_model_remove_placeholder (PHOTOS_ORGANIZE_COLLECTION_MODEL (self->model));
      goto out;
    }

  path = photos_organize_collection_model_get_placeholder (PHOTOS_ORGANIZE_COLLECTION_MODEL (self->model), TRUE);
  if (path == NULL)
    goto out;

  app = g_application_get_default ();
  state = photos_search_context_get_state (PHOTOS_SEARCH_CONTEXT (app));

  gtk_tree_model_get_iter (GTK_TREE_MODEL (self->model), &iter, path);
  gtk_list_store_set (self->model, &iter, PHOTOS_ORGANIZE_MODEL_ID, created_urn, -1);

  set_job = photos_set_collection_job_new (created_urn, TRUE);
  urns = photos_selection_controller_get_selection (self->sel_cntrlr);
  photos_set_collection_job_run (set_job, state, urns, NULL, NULL, NULL);

 out:
  return;
}


static void
photos_organize_collection_view_detail_cell (GtkTreeViewColumn *tree_column,
                                             GtkCellRenderer *cell_renderer,
                                             GtkTreeModel *tree_model,
                                             GtkTreeIter *iter,
                                             gpointer user_data)
{
  PhotosOrganizeCollectionView *self = PHOTOS_ORGANIZE_COLLECTION_VIEW (user_data);
  GObject *object;
  const gchar *identifier = NULL;
  g_autofree gchar *id = NULL;

  gtk_tree_model_get (GTK_TREE_MODEL (self->model), iter, PHOTOS_ORGANIZE_MODEL_ID, &id, -1);
  object = photos_base_manager_get_object_by_id (self->item_mngr, id);

  if (object != NULL)
    identifier = photos_base_item_get_identifier (PHOTOS_BASE_ITEM (object));

  if (identifier != NULL && !g_str_has_prefix (identifier, PHOTOS_QUERY_LOCAL_COLLECTIONS_IDENTIFIER))
    {
      PhotosSource *source;
      const gchar *name;
      const gchar *resource_urn;

      resource_urn = photos_base_item_get_resource_urn (PHOTOS_BASE_ITEM (object));
      source = PHOTOS_SOURCE (photos_base_manager_get_object_by_id (self->src_mngr, resource_urn));
      name = photos_source_get_name (source);
      g_object_set (cell_renderer, "text", name, NULL);
      gtk_cell_renderer_set_visible (cell_renderer, TRUE);
    }
  else
    {
      g_object_set (cell_renderer, "text", "", NULL);
      gtk_cell_renderer_set_visible (cell_renderer, FALSE);
    }
}


static void
photos_organize_collection_view_text_edited_real (PhotosOrganizeCollectionView *self,
                                                  GtkCellRendererText *cell_renderer,
                                                  GtkTreePath *path,
                                                  const gchar *new_text)
{
  GtkTreeIter iter;
  g_autoptr (PhotosCreateCollectionJob) job = NULL;

  g_object_set (cell_renderer, "editable", FALSE, NULL);

  if (new_text == NULL || new_text[0] == '\0')
    {
      /* Don't insert collections with empty names. */
      photos_organize_collection_model_remove_placeholder (PHOTOS_ORGANIZE_COLLECTION_MODEL (self->model));
      goto out;
    }

  gtk_tree_model_get_iter (GTK_TREE_MODEL (self->model), &iter, path);
  gtk_list_store_set (self->model, &iter, PHOTOS_ORGANIZE_MODEL_NAME, new_text, -1);

  job = photos_create_collection_job_new (new_text, NULL);
  photos_create_collection_job_run (job,
                                    self->cancellable,
                                    photos_organize_collection_view_create_collection_executed,
                                    self);

 out:
  return;
}


static void
photos_organize_collection_view_text_edited (PhotosOrganizeCollectionView *self, gchar *path, gchar *new_text)
{
  g_autoptr (GtkTreePath) tree_path = NULL;

  tree_path = gtk_tree_path_new_from_string (path);
  photos_organize_collection_view_text_edited_real (self,
                                                    GTK_CELL_RENDERER_TEXT (self->renderer_text),
                                                    tree_path,
                                                    new_text);
}


static void
photos_organize_collection_view_text_editing_canceled (PhotosOrganizeCollectionView *self)
{
  if (self->choice_confirmed)
    {
      g_autoptr (GtkCellArea) cell_area = NULL;
      GtkCellEditable *entry;
      g_autoptr (GtkTreePath) path = NULL;

      self->choice_confirmed = FALSE;

      g_object_get (self->view_col, "cell-area", &cell_area, NULL);
      entry = gtk_cell_area_get_edit_widget (cell_area);

      path = photos_organize_collection_model_get_placeholder (PHOTOS_ORGANIZE_COLLECTION_MODEL (self->model),
                                                               FALSE);

      if (entry != NULL && path != NULL)
        {
          const gchar *text;

          text = gtk_entry_get_text (GTK_ENTRY (entry));
          photos_organize_collection_view_text_edited_real (self,
                                                            GTK_CELL_RENDERER_TEXT (self->renderer_text),
                                                            path,
                                                            text);
        }
    }
  else
    photos_organize_collection_model_remove_placeholder (PHOTOS_ORGANIZE_COLLECTION_MODEL (self->model));
}


static void
photos_organize_collection_view_dispose (GObject *object)
{
  PhotosOrganizeCollectionView *self = PHOTOS_ORGANIZE_COLLECTION_VIEW (object);

  if (self->cancellable != NULL)
    {
      g_cancellable_cancel (self->cancellable);
      g_clear_object (&self->cancellable);
    }

  g_clear_object (&self->model);
  g_clear_object (&self->item_mngr);
  g_clear_object (&self->src_mngr);
  g_clear_object (&self->sel_cntrlr);

  G_OBJECT_CLASS (photos_organize_collection_view_parent_class)->dispose (object);
}


static void
photos_organize_collection_view_init (PhotosOrganizeCollectionView *self)
{
  GApplication *app;
  PhotosSearchContextState *state;

  app = g_application_get_default ();
  state = photos_search_context_get_state (PHOTOS_SEARCH_CONTEXT (app));

  self->cancellable = g_cancellable_new ();

  self->model = photos_organize_collection_model_new ();
  gtk_tree_view_set_model (GTK_TREE_VIEW (self), GTK_TREE_MODEL (self->model));

  self->view_col = gtk_tree_view_column_new ();
  gtk_tree_view_append_column (GTK_TREE_VIEW (self), self->view_col);

  self->renderer_check = gtk_cell_renderer_toggle_new ();
  gtk_tree_view_column_pack_start (self->view_col, self->renderer_check, FALSE);
  gtk_tree_view_column_set_cell_data_func (self->view_col,
                                           self->renderer_check,
                                           photos_organize_collection_view_check_cell,
                                           self,
                                           NULL);
  g_signal_connect_swapped (self->renderer_check,
                            "toggled",
                            G_CALLBACK (photos_organize_collection_view_check_toggled),
                            self);

  self->renderer_text = gtk_cell_renderer_text_new ();
  gtk_tree_view_column_pack_start (self->view_col, self->renderer_text, TRUE);
  gtk_tree_view_column_add_attribute (self->view_col, self->renderer_text, "text", PHOTOS_ORGANIZE_MODEL_NAME);
  g_signal_connect_swapped (self->renderer_text,
                            "edited",
                            G_CALLBACK (photos_organize_collection_view_text_edited),
                            self);
  g_signal_connect_swapped (self->renderer_text,
                            "editing-canceled",
                            G_CALLBACK (photos_organize_collection_view_text_editing_canceled),
                            self);

  self->renderer_detail = gd_styled_text_renderer_new ();
  gtk_cell_renderer_set_padding (self->renderer_detail, 16, 0);
  gd_styled_text_renderer_add_class (GD_STYLED_TEXT_RENDERER (self->renderer_detail), "dim-label");
  gtk_tree_view_column_pack_start (self->view_col, self->renderer_detail, FALSE);
  gtk_tree_view_column_set_cell_data_func (self->view_col,
                                           self->renderer_detail,
                                           photos_organize_collection_view_detail_cell,
                                           self,
                                           NULL);

  self->item_mngr = g_object_ref (state->item_mngr);
  self->src_mngr = g_object_ref (state->src_mngr);
  self->sel_cntrlr = photos_selection_controller_dup_singleton ();

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
  g_autoptr (GtkTreePath) path = NULL;

  path = photos_organize_collection_model_add_placeholder (PHOTOS_ORGANIZE_COLLECTION_MODEL (self->model));
  if (path == NULL)
    goto out;

  g_object_set (self->renderer_text, "editable", TRUE, NULL);
  gtk_tree_view_set_cursor_on_cell (GTK_TREE_VIEW (self), path, self->view_col, self->renderer_text, TRUE);

 out:
  return;
}


void
photos_organize_collection_view_confirmed_choice (PhotosOrganizeCollectionView *self)
{
  self->choice_confirmed = TRUE;
}
