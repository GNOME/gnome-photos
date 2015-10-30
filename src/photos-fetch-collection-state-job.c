/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2013, 2014, 2015 Red Hat, Inc.
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
#include <tracker-sparql.h>

#include "photos-fetch-collection-state-job.h"
#include "photos-fetch-collections-job.h"
#include "photos-filterable.h"
#include "photos-item-manager.h"
#include "photos-query.h"
#include "photos-query-builder.h"
#include "photos-search-context.h"
#include "photos-selection-controller.h"


struct _PhotosFetchCollectionStateJob
{
  GObject parent_instance;
  GHashTable *collections_for_items;
  PhotosBaseManager *item_mngr;
  PhotosSelectionController *sel_cntrlr;
  PhotosFetchCollectionStateJobCallback callback;
  gint running_jobs;
  gpointer user_data;
};

struct _PhotosFetchCollectionStateJobClass
{
  GObjectClass parent_class;
};


G_DEFINE_TYPE (PhotosFetchCollectionStateJob, photos_fetch_collection_state_job, G_TYPE_OBJECT);


typedef struct _PhotosFetchCollectionStateJobData PhotosFetchCollectionStateJobData;

struct _PhotosFetchCollectionStateJobData
{
  PhotosFetchCollectionStateJob *job;
  gchar *urn;
};


static void
photos_fetch_collection_state_job_data_free (PhotosFetchCollectionStateJobData *data)
{
  g_object_unref (data->job);
  g_free (data->urn);
  g_slice_free (PhotosFetchCollectionStateJobData, data);
}


static PhotosFetchCollectionStateJobData *
photos_fetch_collection_state_job_data_new (PhotosFetchCollectionStateJob *job, const gchar *urn)
{
  PhotosFetchCollectionStateJobData *data;

  data = g_slice_new0 (PhotosFetchCollectionStateJobData);
  data->job = g_object_ref (job);
  data->urn = g_strdup (urn);
  return data;
}


static void
photos_fetch_collection_state_job_emit_callback (PhotosFetchCollectionStateJob *self)
{
  GHashTable *collection_state;
  GHashTable *collections;
  GHashTableIter iter1;
  PhotosBaseItem *collection;
  const gchar *coll_idx;

  collection_state = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  collections = photos_item_manager_get_collections (PHOTOS_ITEM_MANAGER (self->item_mngr));

  /* For all the registered collections… */
  g_hash_table_iter_init (&iter1, collections);
  while (g_hash_table_iter_next (&iter1, (gpointer *) &coll_idx, (gpointer *) &collection))
    {
      GHashTableIter iter2;
      GList *collections_for_item;
      PhotosBaseItem *item;
      gboolean found = FALSE;
      gboolean hidden = FALSE;
      gboolean not_found = FALSE;
      const gchar *item_idx;
      gint state = PHOTOS_COLLECTION_STATE_NORMAL;
      gpointer *keys;
      guint length;

      /* If the only object we are fetching collection state for is a
       * collection itself, hide this if it is the same collection.
       */
      keys = g_hash_table_get_keys_as_array (self->collections_for_items, &length);
      if (length == 1)
        {
          item_idx = ((gchar **) keys)[0];
          item = PHOTOS_BASE_ITEM (photos_base_manager_get_object_by_id (self->item_mngr, item_idx));
          if (g_strcmp0 (photos_filterable_get_id (PHOTOS_FILTERABLE (item)),
                         photos_filterable_get_id (PHOTOS_FILTERABLE (collection))) == 0)
            hidden = TRUE;
        }
      g_free (keys);

      g_hash_table_iter_init (&iter2, self->collections_for_items);
      while (g_hash_table_iter_next (&iter2, (gpointer *) &item_idx, (gpointer *) &collections_for_item))
        {
          const gchar *identifier;

          item = PHOTOS_BASE_ITEM (photos_base_manager_get_object_by_id (self->item_mngr, item_idx));

          /* If one of the selected items is part of this collection… */
          if (g_list_find_custom (collections_for_item, coll_idx, (GCompareFunc) g_strcmp0) != NULL)
            found = TRUE;
          else
            not_found = TRUE;

          identifier = photos_base_item_get_identifier (collection);
          if (g_strcmp0 (photos_base_item_get_resource_urn (item),
                         photos_base_item_get_resource_urn (collection)) != 0
              && identifier != NULL
              && !g_str_has_prefix (identifier, PHOTOS_QUERY_LOCAL_COLLECTIONS_IDENTIFIER))
            hidden = TRUE;
        }

      if (found && not_found)
        state |= PHOTOS_COLLECTION_STATE_INCONSISTENT;
      else if (found)
        state |= PHOTOS_COLLECTION_STATE_ACTIVE;

      if (hidden)
        state |= PHOTOS_COLLECTION_STATE_HIDDEN;

      g_hash_table_insert (collection_state, g_strdup (coll_idx), GINT_TO_POINTER (state));
    }

  if (self->callback != NULL)
    (*self->callback) (collection_state, self->user_data);

  g_hash_table_unref (collection_state);
}


static void
photos_fetch_collection_state_job_job_collector (GList *collections_for_item, gpointer user_data)
{
  PhotosFetchCollectionStateJobData *data = user_data;
  PhotosFetchCollectionStateJob *self = data->job;
  const gchar *urn = data->urn;

  g_hash_table_insert (self->collections_for_items,
                       g_strdup (urn),
                       g_list_copy_deep (collections_for_item, (GCopyFunc) g_strdup, NULL));

  self->running_jobs--;
  if (self->running_jobs == 0)
    photos_fetch_collection_state_job_emit_callback (self);

  photos_fetch_collection_state_job_data_free (data);
}


static void
photos_fetch_collection_state_job_value_destroy_func (gpointer data)
{
  g_list_free_full ((GList *) data, g_free);
}


static void
photos_fetch_collection_state_job_dispose (GObject *object)
{
  PhotosFetchCollectionStateJob *self = PHOTOS_FETCH_COLLECTION_STATE_JOB (object);

  g_clear_object (&self->item_mngr);
  g_clear_object (&self->sel_cntrlr);

  G_OBJECT_CLASS (photos_fetch_collection_state_job_parent_class)->dispose (object);
}


static void
photos_fetch_collection_state_job_finalize (GObject *object)
{
  PhotosFetchCollectionStateJob *self = PHOTOS_FETCH_COLLECTION_STATE_JOB (object);

  g_hash_table_unref (self->collections_for_items);

  G_OBJECT_CLASS (photos_fetch_collection_state_job_parent_class)->finalize (object);
}


static void
photos_fetch_collection_state_job_init (PhotosFetchCollectionStateJob *self)
{
  GApplication *app;
  PhotosSearchContextState *state;

  app = g_application_get_default ();
  state = photos_search_context_get_state (PHOTOS_SEARCH_CONTEXT (app));

  self->collections_for_items = g_hash_table_new_full (g_str_hash,
                                                       g_str_equal,
                                                       g_free,
                                                       photos_fetch_collection_state_job_value_destroy_func);

  self->item_mngr = g_object_ref (state->item_mngr);
  self->sel_cntrlr = photos_selection_controller_dup_singleton ();
}


static void
photos_fetch_collection_state_job_class_init (PhotosFetchCollectionStateJobClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->dispose = photos_fetch_collection_state_job_dispose;
  object_class->finalize = photos_fetch_collection_state_job_finalize;
}


PhotosFetchCollectionStateJob *
photos_fetch_collection_state_job_new (void)
{
  return g_object_new (PHOTOS_TYPE_FETCH_COLLECTION_STATE_JOB, NULL);
}


void
photos_fetch_collection_state_job_run (PhotosFetchCollectionStateJob *self,
                                       PhotosFetchCollectionStateJobCallback callback,
                                       gpointer user_data)
{
  GList *l;
  GList *urns;

  self->callback = callback;
  self->user_data = user_data;

  urns = photos_selection_controller_get_selection (self->sel_cntrlr);
  for (l = urns; l != NULL; l = l->next)
    {
      PhotosFetchCollectionStateJobData *data;
      PhotosFetchCollectionsJob *job;
      const gchar *urn = (gchar *) l->data;

      self->running_jobs++;
      job = photos_fetch_collections_job_new (urn);
      data = photos_fetch_collection_state_job_data_new (self, urn);
      photos_fetch_collections_job_run (job, photos_fetch_collection_state_job_job_collector, data);
      g_object_unref (job);
    }
}
