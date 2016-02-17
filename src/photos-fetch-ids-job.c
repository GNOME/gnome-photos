/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2014 – 2016 Red Hat, Inc.
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

#include "photos-fetch-ids-job.h"
#include "photos-query.h"
#include "photos-query-builder.h"
#include "photos-search-controller.h"
#include "photos-tracker-queue.h"


struct _PhotosFetchIdsJob
{
  GObject parent_instance;
  GCancellable *cancellable;
  GPtrArray *ids;
  PhotosFetchIdsJobCallback callback;
  PhotosTrackerQueue *queue;
  gchar **terms;
  gpointer user_data;
};

struct _PhotosFetchIdsJobClass
{
  GObjectClass parent_class;
};

enum
{
  PROP_0,
  PROP_TERMS
};


G_DEFINE_TYPE (PhotosFetchIdsJob, photos_fetch_ids_job, G_TYPE_OBJECT);


static void
photos_fetch_ids_job_emit_callback (PhotosFetchIdsJob *self)
{
  if (self->callback == NULL)
    return;

  g_ptr_array_add (self->ids, NULL);
  (*self->callback) ((const gchar *const *) self->ids->pdata, self->user_data);
}


static void
photos_fetch_ids_job_cursor_next (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosFetchIdsJob *self = PHOTOS_FETCH_IDS_JOB (user_data);
  TrackerSparqlCursor *cursor = TRACKER_SPARQL_CURSOR (source_object);
  GError *error;
  gboolean valid;
  const gchar *id;

  error = NULL;
  valid = tracker_sparql_cursor_next_finish (cursor, res, &error);
  if (error != NULL)
    {
      g_warning ("Unable to read results of FetchIdsJob: %s", error->message);
      g_error_free (error);
      goto end;
    }
  if(!valid)
    goto end;

  id = tracker_sparql_cursor_get_string (cursor, PHOTOS_QUERY_COLUMNS_URN, NULL);
  g_ptr_array_add (self->ids, g_strdup (id));

  tracker_sparql_cursor_next_async (cursor, self->cancellable, photos_fetch_ids_job_cursor_next, self);
  return;

 end:
  photos_fetch_ids_job_emit_callback (self);
  tracker_sparql_cursor_close (cursor);
  g_object_unref (self);
}


static void
photos_fetch_ids_job_query_executed (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosFetchIdsJob *self = PHOTOS_FETCH_IDS_JOB (user_data);
  TrackerSparqlConnection *connection = TRACKER_SPARQL_CONNECTION (source_object);
  TrackerSparqlCursor *cursor;
  GError *error;

  error = NULL;
  cursor = tracker_sparql_connection_query_finish (connection, res, &error);
  if (error != NULL)
    {
      g_warning ("Unable to run FetchIdsJob: %s", error->message);
      g_error_free (error);
      photos_fetch_ids_job_emit_callback (self);
      return;
    }

  tracker_sparql_cursor_next_async (cursor,
                                    self->cancellable,
                                    photos_fetch_ids_job_cursor_next,
                                    g_object_ref (self));
  g_object_unref (cursor);
}


static void
photos_fetch_ids_job_dispose (GObject *object)
{
  PhotosFetchIdsJob *self = PHOTOS_FETCH_IDS_JOB (object);

  g_clear_pointer (&self->ids, (GDestroyNotify) g_ptr_array_unref);
  g_clear_object (&self->cancellable);
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
  self->queue = photos_tracker_queue_dup_singleton (NULL, NULL);
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


void
photos_fetch_ids_job_run (PhotosFetchIdsJob *self,
                          PhotosSearchContextState *state,
                          GCancellable *cancellable,
                          PhotosFetchIdsJobCallback callback,
                          gpointer user_data)
{
  PhotosQuery *query;
  gchar *str;

  self->callback = callback;
  self->user_data = user_data;

  if (G_UNLIKELY (self->queue == NULL))
    {
      photos_fetch_ids_job_emit_callback (self);
      return;
    }

  g_clear_object (&self->cancellable);
  if (cancellable != NULL)
    self->cancellable = g_object_ref (cancellable);

  str = g_strjoinv (" ", (gchar **) self->terms);
  photos_search_controller_set_string (state->srch_cntrlr, str);
  g_free (str);

  query = photos_query_builder_global_query (state, PHOTOS_QUERY_FLAGS_SEARCH, NULL);
  photos_tracker_queue_select (self->queue,
                               query->sparql,
                               self->cancellable,
                               photos_fetch_ids_job_query_executed,
                               g_object_ref (self),
                               g_object_unref);
  photos_query_free (query);
}
