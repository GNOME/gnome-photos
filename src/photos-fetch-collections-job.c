/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2013 – 2016 Red Hat, Inc.
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
#include <tracker-sparql.h>

#include "photos-fetch-collections-job.h"
#include "photos-query.h"
#include "photos-query-builder.h"
#include "photos-search-context.h"
#include "photos-tracker-queue.h"


struct _PhotosFetchCollectionsJob
{
  GObject parent_instance;
  GError *queue_error;
  GList *collections;
  PhotosTrackerQueue *queue;
  gchar *urn;
};

struct _PhotosFetchCollectionsJobClass
{
  GObjectClass parent_class;
};

enum
{
  PROP_0,
  PROP_URN
};


G_DEFINE_TYPE (PhotosFetchCollectionsJob, photos_fetch_collections_job, G_TYPE_OBJECT);


static void
photos_fetch_collections_job_cursor_next (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  GTask *task = G_TASK (user_data);
  PhotosFetchCollectionsJob *self;
  TrackerSparqlCursor *cursor = TRACKER_SPARQL_CURSOR (source_object);
  GCancellable *cancellable;
  GError *error;
  gboolean success;

  self = g_task_get_source_object (task);
  cancellable = g_task_get_cancellable (task);

  error = NULL;
  success = tracker_sparql_cursor_next_finish (cursor, res, &error);
  if (error != NULL)
    {
      g_task_return_error (task, error);
      goto end;
    }

  if (success)
    {
      gchar *urn;

      urn = g_strdup (tracker_sparql_cursor_get_string (cursor, 0, NULL));
      self->collections = g_list_prepend (self->collections, urn);

      tracker_sparql_cursor_next_async (cursor,
                                        cancellable,
                                        photos_fetch_collections_job_cursor_next,
                                        g_object_ref (task));

      g_object_unref (task);
      return;
    }

  self->collections = g_list_reverse (self->collections);
  g_task_return_pointer (task, self->collections, NULL);

 end:
  tracker_sparql_cursor_close (cursor);
  g_object_unref (task);
}


static void
photos_fetch_collections_job_query_executed (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  GTask *task = G_TASK (user_data);
  TrackerSparqlConnection *connection = TRACKER_SPARQL_CONNECTION (source_object);
  GCancellable *cancellable;
  TrackerSparqlCursor *cursor;
  GError *error;

  cancellable = g_task_get_cancellable (task);

  error = NULL;
  cursor = tracker_sparql_connection_query_finish (connection, res, &error);
  if (error != NULL)
    {
      g_task_return_error (task, error);
      goto out;
    }

  tracker_sparql_cursor_next_async (cursor,
                                    cancellable,
                                    photos_fetch_collections_job_cursor_next,
                                    g_object_ref (task));

 out:
  g_object_unref (cursor);
}


static void
photos_fetch_collections_job_dispose (GObject *object)
{
  PhotosFetchCollectionsJob *self = PHOTOS_FETCH_COLLECTIONS_JOB (object);

  g_clear_object (&self->queue);

  G_OBJECT_CLASS (photos_fetch_collections_job_parent_class)->dispose (object);
}


static void
photos_fetch_collections_job_finalize (GObject *object)
{
  PhotosFetchCollectionsJob *self = PHOTOS_FETCH_COLLECTIONS_JOB (object);

  g_clear_error (&self->queue_error);
  g_list_free_full (self->collections, g_free);
  g_free (self->urn);

  G_OBJECT_CLASS (photos_fetch_collections_job_parent_class)->finalize (object);
}


static void
photos_fetch_collections_job_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
  PhotosFetchCollectionsJob *self = PHOTOS_FETCH_COLLECTIONS_JOB (object);

  switch (prop_id)
    {
    case PROP_URN:
      self->urn = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
photos_fetch_collections_job_init (PhotosFetchCollectionsJob *self)
{
  self->queue = photos_tracker_queue_dup_singleton (NULL, &self->queue_error);
}


static void
photos_fetch_collections_job_class_init (PhotosFetchCollectionsJobClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->dispose = photos_fetch_collections_job_dispose;
  object_class->finalize = photos_fetch_collections_job_finalize;
  object_class->set_property = photos_fetch_collections_job_set_property;

  g_object_class_install_property (object_class,
                                   PROP_URN,
                                   g_param_spec_string ("urn",
                                                        "Uniform Resource Name",
                                                        "An unique ID associated with this item",
                                                        NULL,
                                                        G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE));
}


PhotosFetchCollectionsJob *
photos_fetch_collections_job_new (const gchar *urn)
{
  return g_object_new (PHOTOS_TYPE_FETCH_COLLECTIONS_JOB, "urn", urn, NULL);
}


GList *
photos_fetch_collections_job_finish (PhotosFetchCollectionsJob *self, GAsyncResult *res, GError **error)
{
  GTask *task = G_TASK (res);

  g_return_val_if_fail (g_task_is_valid (res, self), NULL);
  g_return_val_if_fail (g_task_get_source_tag (task) == photos_fetch_collections_job_run, NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  return g_task_propagate_pointer (task, error);
}


void
photos_fetch_collections_job_run (PhotosFetchCollectionsJob *self,
                                  GCancellable *cancellable,
                                  GAsyncReadyCallback callback,
                                  gpointer user_data)
{
  GApplication *app;
  GTask *task;
  PhotosQuery *query;
  PhotosSearchContextState *state;

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, photos_fetch_collections_job_run);

  if (G_UNLIKELY (self->queue == NULL))
    {
      g_task_return_error (task, g_error_copy (self->queue_error));
      goto out;
    }

  app = g_application_get_default ();
  state = photos_search_context_get_state (PHOTOS_SEARCH_CONTEXT (app));

  query = photos_query_builder_fetch_collections_query (state, self->urn);
  photos_tracker_queue_select (self->queue,
                               query->sparql,
                               cancellable,
                               photos_fetch_collections_job_query_executed,
                               g_object_ref (task),
                               g_object_unref);
  photos_query_free (query);

 out:
  g_object_unref (task);
}
