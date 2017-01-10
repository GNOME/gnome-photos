/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2012 – 2017 Red Hat, Inc.
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

#include <cairo-gobject.h>
#include <gio/gio.h>

#include "photos-base-manager.h"
#include "photos-enums.h"
#include "photos-filterable.h"
#include "photos-offset-controller.h"
#include "photos-search-context.h"
#include "photos-tracker-controller.h"
#include "photos-utils.h"
#include "photos-view-model.h"


struct _PhotosViewModel
{
  GtkListStore parent_instance;
  GHashTable *info_updated_ids;
  PhotosBaseManager *item_mngr;
  PhotosModeController *mode_cntrlr;
  PhotosOffsetController *offset_cntrlr;
  PhotosTrackerController *trk_cntrlr;
  PhotosWindowMode mode;
  gchar *row_ref_key;
  gint n_rows;
  gint64 oldest_mtime;
  guint reset_count_id;
};

enum
{
  PROP_0,
  PROP_MODE
};


G_DEFINE_TYPE (PhotosViewModel, photos_view_model, GTK_TYPE_LIST_STORE);


enum
{
  RESET_COUNT_TIMEOUT = 500 /* ms */
};


static void
photos_view_model_info_set (PhotosViewModel *self, PhotosBaseItem *item, GtkTreeIter *iter)
{
  gtk_list_store_set (GTK_LIST_STORE (self),
                      iter,
                      PHOTOS_VIEW_MODEL_URN, photos_filterable_get_id (PHOTOS_FILTERABLE (item)),
                      PHOTOS_VIEW_MODEL_URI, photos_base_item_get_uri (item),
                      PHOTOS_VIEW_MODEL_NAME, photos_base_item_get_name (item),
                      PHOTOS_VIEW_MODEL_AUTHOR, photos_base_item_get_author (item),
                      PHOTOS_VIEW_MODEL_ICON, photos_base_item_get_surface (item),
                      PHOTOS_VIEW_MODEL_MTIME, photos_base_item_get_mtime (item),
                      -1);
}


static gboolean
photos_view_model_reset_count_timeout (gpointer user_data)
{
  PhotosViewModel *self = PHOTOS_VIEW_MODEL (user_data);

  self->reset_count_id = 0;
  photos_offset_controller_reset_count (self->offset_cntrlr);
  return G_SOURCE_REMOVE;
}


static void
photos_view_model_reset_count (PhotosViewModel *self)
{
  if (self->reset_count_id == 0)
    self->reset_count_id = g_timeout_add (RESET_COUNT_TIMEOUT, photos_view_model_reset_count_timeout, self);
}


static void
photos_view_model_add_item (PhotosViewModel *self, PhotosBaseItem *item)
{
  GtkTreeIter iter;
  GtkTreePath *path;
  GtkTreeRowReference *row_ref;
  gint offset;
  gint step;
  gint64 mtime;

  /* Update the count so that PhotosOffsetController has the correct
   * values. Otherwise things like loading more items and "No
   * Results" page will not work correctly.
   */
  photos_view_model_reset_count (self);

  offset = photos_offset_controller_get_offset (self->offset_cntrlr);
  step = photos_offset_controller_get_step (self->offset_cntrlr);
  mtime = photos_base_item_get_mtime (item);
  if (self->n_rows >= offset + step && mtime < self->oldest_mtime)
    return;

  gtk_list_store_append (GTK_LIST_STORE (self), &iter);
  photos_view_model_info_set (self, item, &iter);

  self->n_rows++;
  if (mtime < self->oldest_mtime)
    self->oldest_mtime = mtime;

  path = gtk_tree_model_get_path (GTK_TREE_MODEL (self), &iter);
  row_ref = gtk_tree_row_reference_new (GTK_TREE_MODEL (self), path);
  gtk_tree_path_free (path);

  g_object_set_data_full (G_OBJECT (item),
                          self->row_ref_key,
                          row_ref,
                          (GDestroyNotify) gtk_tree_row_reference_free);
}


static void
photos_view_model_clear (PhotosViewModel *self)
{
  guint i;
  guint n_items;

  g_return_if_fail (self->item_mngr != NULL);

  n_items = g_list_model_get_n_items (G_LIST_MODEL (self->item_mngr));
  for (i = 0; i < n_items; i++)
    {
      PhotosBaseItem *item;

      item = PHOTOS_BASE_ITEM (g_list_model_get_object (G_LIST_MODEL (self->item_mngr), i));
      g_object_set_data (G_OBJECT (item), self->row_ref_key, NULL);
      g_object_unref (item);
    }

  gtk_list_store_clear (GTK_LIST_STORE (self));

  self->n_rows = 0;
  self->oldest_mtime = G_MAXINT64;
}


static gboolean
photos_view_model_item_removed_foreach (GtkTreeModel *model,
                                        GtkTreePath *path,
                                        GtkTreeIter *iter,
                                        gpointer user_data)
{
  PhotosViewModel *self = PHOTOS_VIEW_MODEL (model);
  PhotosBaseItem *item = PHOTOS_BASE_ITEM (user_data);
  gboolean ret_val = FALSE;
  const gchar *id;
  gchar *value;
  gint64 mtime;

  id = photos_filterable_get_id (PHOTOS_FILTERABLE (item));
  gtk_tree_model_get (model, iter, PHOTOS_VIEW_MODEL_URN, &value, PHOTOS_VIEW_MODEL_MTIME, &mtime, -1);

  if (g_strcmp0 (id, value) == 0)
    {
      GtkTreeIter tmp;

      tmp = *iter;
      if (!gtk_tree_model_iter_next (model, &tmp))
        ret_val = TRUE;

      gtk_list_store_remove (GTK_LIST_STORE (model), iter);
      gtk_tree_path_next (path); /* Ensure that path in sync with iter. */
      self->n_rows--;
    }
  else if (mtime < self->oldest_mtime)
    self->oldest_mtime = mtime;

  g_free (value);
  return ret_val;
}


static void
photos_view_model_remove_item (PhotosViewModel *self, PhotosBaseItem *item)
{
  /* Update the count so that PhotosOffsetController has the correct
   * values. Otherwise things like loading more items and "No
   * Results" page will not work correctly.
   */
  photos_view_model_reset_count (self);

  self->oldest_mtime = G_MAXINT64;
  gtk_tree_model_foreach (GTK_TREE_MODEL (self), photos_view_model_item_removed_foreach, item);
  g_object_set_data (G_OBJECT (item), self->row_ref_key, NULL);
}


static void
photos_view_model_info_updated (PhotosBaseItem *item, gpointer user_data)
{
  PhotosViewModel *self = PHOTOS_VIEW_MODEL (user_data);
  GtkTreeIter iter;
  GtkTreePath *path;
  GtkTreeRowReference *row_ref;

  row_ref = (GtkTreeRowReference *) g_object_get_data (G_OBJECT (item), self->row_ref_key);
  if (row_ref != NULL)
    {
      path = gtk_tree_row_reference_get_path (row_ref);
      if (path == NULL)
        return;

      gtk_tree_model_get_iter (GTK_TREE_MODEL (self), &iter, path);
      photos_view_model_info_set (self, item, &iter);
      gtk_tree_path_free (path);
    }
}


static void
photos_view_model_object_added (PhotosViewModel *self, GObject *object)
{
  PhotosBaseItem *item = PHOTOS_BASE_ITEM (object);
  GtkTreeRowReference *row_ref;
  const gchar *id;
  guint info_updated_id;

  g_return_if_fail (self->item_mngr != NULL);
  g_return_if_fail (self->mode_cntrlr != NULL);

  row_ref = (GtkTreeRowReference *) g_object_get_data (G_OBJECT (item), self->row_ref_key);
  if (row_ref != NULL)
    return;

  photos_view_model_add_item (self, item);

  info_updated_id = (guint) g_signal_connect_object (item,
                                                     "info-updated",
                                                     G_CALLBACK (photos_view_model_info_updated),
                                                     self,
                                                     0);

  id = photos_filterable_get_id (PHOTOS_FILTERABLE (item));
  g_hash_table_insert (self->info_updated_ids, g_strdup (id), GUINT_TO_POINTER (info_updated_id));
}


static void
photos_view_model_object_removed (PhotosViewModel *self, GObject *object)
{
  PhotosBaseItem *item = PHOTOS_BASE_ITEM (object);
  const gchar *id;
  gpointer data;
  guint info_updated_id;

  photos_view_model_remove_item (self, item);

  id = photos_filterable_get_id (PHOTOS_FILTERABLE (item));
  data = g_hash_table_lookup (self->info_updated_ids, id);

  g_return_if_fail (data != NULL);

  info_updated_id = GPOINTER_TO_UINT (data);
  g_signal_handler_disconnect (item, (gulong) info_updated_id);
  g_hash_table_remove (self->info_updated_ids, id);
}


static void
photos_view_model_constructed (GObject *object)
{
  PhotosViewModel *self = PHOTOS_VIEW_MODEL (object);
  PhotosBaseManager *item_mngr_chld;

  G_OBJECT_CLASS (photos_view_model_parent_class)->constructed (object);

  photos_utils_get_controller (self->mode, &self->offset_cntrlr, &self->trk_cntrlr);

  item_mngr_chld = photos_item_manager_get_for_mode (PHOTOS_ITEM_MANAGER (self->item_mngr), self->mode);
  g_signal_connect_object (item_mngr_chld,
                           "object-added",
                           G_CALLBACK (photos_view_model_object_added),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (item_mngr_chld,
                           "object-removed",
                           G_CALLBACK (photos_view_model_object_removed),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (item_mngr_chld, "clear", G_CALLBACK (photos_view_model_clear), self, G_CONNECT_SWAPPED);
}


static void
photos_view_model_dispose (GObject *object)
{
  PhotosViewModel *self = PHOTOS_VIEW_MODEL (object);

  if (self->reset_count_id != 0)
    {
      g_source_remove (self->reset_count_id);
      self->reset_count_id = 0;
    }

  g_clear_object (&self->offset_cntrlr);
  g_clear_object (&self->trk_cntrlr);

  G_OBJECT_CLASS (photos_view_model_parent_class)->dispose (object);
}


static void
photos_view_model_finalize (GObject *object)
{
  PhotosViewModel *self = PHOTOS_VIEW_MODEL (object);

  if (self->item_mngr != NULL)
    g_object_remove_weak_pointer (G_OBJECT (self->item_mngr), (gpointer *) &self->item_mngr);

  if (self->mode_cntrlr != NULL)
    g_object_remove_weak_pointer (G_OBJECT (self->mode_cntrlr), (gpointer *) &self->mode_cntrlr);

  g_hash_table_unref (self->info_updated_ids);
  g_free (self->row_ref_key);

  G_OBJECT_CLASS (photos_view_model_parent_class)->finalize (object);
}


static void
photos_view_model_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
  PhotosViewModel *self = PHOTOS_VIEW_MODEL (object);

  switch (prop_id)
    {
    case PROP_MODE:
      self->mode = (PhotosWindowMode) g_value_get_enum (value);
      self->row_ref_key = g_strdup_printf ("row-ref-%d", self->mode);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
photos_view_model_init (PhotosViewModel *self)
{
  GApplication *app;
  GType columns[] = {G_TYPE_STRING,    /* URN */
                     G_TYPE_STRING,    /* URI */
                     G_TYPE_STRING,    /* NAME */
                     G_TYPE_STRING,    /* AUTHOR */
                     CAIRO_GOBJECT_TYPE_SURFACE,  /* ICON */
                     G_TYPE_INT64,     /* MTIME */
                     G_TYPE_BOOLEAN,   /* STATE */
                     G_TYPE_UINT};     /* PULSE (unused) */
  PhotosSearchContextState *state;

  app = g_application_get_default ();
  state = photos_search_context_get_state (PHOTOS_SEARCH_CONTEXT (app));

  gtk_list_store_set_column_types (GTK_LIST_STORE (self), G_N_ELEMENTS (columns), columns);
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (self), PHOTOS_VIEW_MODEL_MTIME, GTK_SORT_DESCENDING);

  self->info_updated_ids = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

  self->item_mngr = state->item_mngr;
  g_object_add_weak_pointer (G_OBJECT (self->item_mngr), (gpointer *) &self->item_mngr);

  self->mode_cntrlr = state->mode_cntrlr;
  g_object_add_weak_pointer (G_OBJECT (self->mode_cntrlr), (gpointer *) &self->mode_cntrlr);

  self->oldest_mtime = G_MAXINT64;
}


static void
photos_view_model_class_init (PhotosViewModelClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->constructed = photos_view_model_constructed;
  object_class->dispose = photos_view_model_dispose;
  object_class->finalize = photos_view_model_finalize;
  object_class->set_property = photos_view_model_set_property;

  g_object_class_install_property (object_class,
                                   PROP_MODE,
                                   g_param_spec_enum ("mode",
                                                      "PhotosWindowMode enum",
                                                      "The mode for which the model holds the data",
                                                      PHOTOS_TYPE_WINDOW_MODE,
                                                      PHOTOS_WINDOW_MODE_NONE,
                                                      G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE));
}


GtkListStore *
photos_view_model_new (PhotosWindowMode mode)
{
  return g_object_new (PHOTOS_TYPE_VIEW_MODEL, "mode", mode, NULL);
}
