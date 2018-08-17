/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2014 – 2017 Red Hat, Inc.
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

#include <glib.h>
#include <tracker-sparql.h>

#include "photos-fetch-ids-job.h"
#include "photos-query.h"
#include "photos-query-builder.h"
#include "photos-search-controller.h"
#include "photos-tracker-queue.h"


struct _PhotosFetchIdsJob
{
  GObject parent_instance;
  GError *queue_error;
  GPtrArray *ids;
  PhotosTrackerQueue *queue;
  gchar **terms;
};

enum
{
  PROP_0,
  PROP_TERMS
};


G_DEFINE_TYPE (PhotosFetchIdsJob, photos_fetch_ids_job, G_TYPE_OBJECT);


static void
photos_fetch_ids_job_cursor_next (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosFetchIdsJob *self;
  GCancellable *cancellable;
  g_autoptr (GTask) task = G_TASK (user_data);
  TrackerSparqlCursor *cursor = TRACKER_SPARQL_CURSOR (source_object);
  GError *error;
  gboolean success;

  self = PHOTOS_FETCH_IDS_JOB (g_task_get_source_object (task));
  cancellable = g_task_get_cancellable (task);

  error = NULL;
  /* Note that tracker_sparql_cursor_next_finish can return FALSE even
   * without an error.
   */
  success = tracker_sparql_cursor_next_finish (cursor, res, &error);
  if (error != NULL)
    {
      g_task_return_error (task, error);
      goto end;
    }

  if (success)
    {
      const gchar *id;

      id = tracker_sparql_cursor_get_string (cursor, PHOTOS_QUERY_COLUMNS_URN, NULL);
      g_ptr_array_add (self->ids, g_strdup (id));

      tracker_sparql_cursor_next_async (cursor,
                                        cancellable,
                                        photos_fetch_ids_job_cursor_next,
                                        g_object_ref (task));
      return;
    }

  g_ptr_array_add (self->ids, NULL);
  g_task_return_pointer (task, self->ids->pdata, NULL);

 end:
  return;
}


static void
photos_fetch_ids_job_query_executed (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  GCancellable *cancellable;
  GTask *task = G_TASK (user_data);
  TrackerSparqlConnection *connection = TRACKER_SPARQL_CONNECTION (source_object);
  TrackerSparqlCursor *cursor; /* TODO: Use g_autoptr */
  GError *error;

  cancellable = g_task_get_cancellable (task);

  error = NULL;
  cursor = tracker_sparql_connection_query_finish (connection, res, &error);
  if (error != NULL)
    {
      g_task_return_error (task, error);
      return;
    }

  tracker_sparql_cursor_next_async (cursor,
                                    cancellable,
                                    photos_fetch_ids_job_cursor_next,
                                    g_object_ref (task));
  g_object_unref (cursor);
}


static void
photos_fetch_ids_job_dispose (GObject *object)
{
  PhotosFetchIdsJob *self = PHOTOS_FETCH_IDS_JOB (object);

  g_clear_pointer (&self->ids, g_ptr_array_unref);
  g_clear_object (&self->queue);

  G_OBJECT_CLASS (photos_fetch_ids_job_parent_class)->dispose (object);
}


static void
photos_fetch_ids_job_finalize (GObject *object)
{
  PhotosFetchIdsJob *self = PHOTOS_FETCH_IDS_JOB (object);

  g_strfreev (self->terms);

  G_OBJECT_CLASS (photos_fetch_ids_job_parent_class)->finalize (object);
}


static void
photos_fetch_ids_job_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
  PhotosFetchIdsJob *self = PHOTOS_FETCH_IDS_JOB (object);

  switch (prop_id)
    {
    case PROP_TERMS:
      self->terms = g_value_dup_boxed (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
photos_fetch_ids_job_init (PhotosFetchIdsJob *self)
{
  self->ids = g_ptr_array_new_with_free_func (g_free);
  self->queue = photos_tracker_queue_dup_singleton (NULL, &self->queue_error);
}


static void
photos_fetch_ids_job_class_init (PhotosFetchIdsJobClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->dispose = photos_fetch_ids_job_dispose;
  object_class->finalize = photos_fetch_ids_job_finalize;
  object_class->set_property = photos_fetch_ids_job_set_property;

  g_object_class_install_property (object_class,
                                   PROP_TERMS,
                                   g_param_spec_boxed ("terms",
                                                       "Search terms",
                                                       "Search terms entered by the user",
                                                       G_TYPE_STRV,
                                                       G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE));
}


PhotosFetchIdsJob *
photos_fetch_ids_job_new (const gchar *const *terms)
{
  return g_object_new (PHOTOS_TYPE_FETCH_IDS_JOB, "terms", terms, NULL);
}


const gchar *const *
photos_fetch_ids_job_finish (PhotosFetchIdsJob *self, GAsyncResult *res, GError **error)
{
  GTask *task = G_TASK (res);

  g_return_val_if_fail (g_task_is_valid (res, self), NULL);
  g_return_val_if_fail (g_task_get_source_tag (task) == photos_fetch_ids_job_run, NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  return g_task_propagate_pointer (task, error);
}


void
photos_fetch_ids_job_run (PhotosFetchIdsJob *self,
                          PhotosSearchContextState *state,
                          GCancellable *cancellable,
                          GAsyncReadyCallback callback,
                          gpointer user_data)
{
  g_autoptr (GTask) task = NULL;
  g_autoptr (PhotosQuery) query = NULL;
  g_autofree gchar *str = NULL;

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, photos_fetch_ids_job_run);

  if (G_UNLIKELY (self->queue == NULL))
    {
      g_task_return_error (task, g_error_copy (self->queue_error));
      goto out;
    }

  str = g_strjoinv (" ", (gchar **) self->terms);
  photos_search_controller_set_string (state->srch_cntrlr, str);

  query = photos_query_builder_global_query (state, PHOTOS_QUERY_FLAGS_SEARCH, NULL);
  photos_tracker_queue_select (self->queue,
                               query,
                               cancellable,
                               photos_fetch_ids_job_query_executed,
                               g_object_ref (task),
                               g_object_unref);

 out:
  return;
}
