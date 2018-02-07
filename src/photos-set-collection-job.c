/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2013 – 2017 Red Hat, Inc.
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

#include "photos-query.h"
#include "photos-query-builder.h"
#include "photos-search-context.h"
#include "photos-selection-controller.h"
#include "photos-set-collection-job.h"
#include "photos-tracker-queue.h"
#include "photos-update-mtime-job.h"


struct _PhotosSetCollectionJob
{
  GObject parent_instance;
  GError *queue_error;
  PhotosSelectionController *sel_cntrlr;
  PhotosTrackerQueue *queue;
  gboolean setting;
  gchar *collection_urn;
  gint running_jobs;
};

enum
{
  PROP_0,
  PROP_COLLECTION_URN,
  PROP_SETTING
};


G_DEFINE_TYPE (PhotosSetCollectionJob, photos_set_collection_job, G_TYPE_OBJECT);


static void
photos_set_collection_job_update_mtime (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  GTask *task = G_TASK (user_data);
  PhotosUpdateMtimeJob *job = PHOTOS_UPDATE_MTIME_JOB (source_object);
  GError *error = NULL;

  photos_update_mtime_job_finish (job, res, &error);
  if (error != NULL)
    {
      g_task_return_error (task, error);
      goto out;
    }

  g_task_return_boolean (task, TRUE);

 out:
  g_object_unref (task);
}


static void
photos_set_collection_job_job_collector (PhotosSetCollectionJob *self, GTask *task)
{
  GCancellable *cancellable;

  cancellable = g_task_get_cancellable (task);

  self->running_jobs--;
  if (self->running_jobs == 0)
    {
      PhotosUpdateMtimeJob *job;

      job = photos_update_mtime_job_new (self->collection_urn);
      photos_update_mtime_job_run (job, cancellable, photos_set_collection_job_update_mtime, g_object_ref (task));
      g_object_unref (job);
    }
}


static void
photos_set_collection_job_query_executed (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosSetCollectionJob *self;
  GTask *task = G_TASK (user_data);
  TrackerSparqlConnection *connection = TRACKER_SPARQL_CONNECTION (source_object);
  GError *error;

  self = PHOTOS_SET_COLLECTION_JOB (g_task_get_source_object (task));

  error = NULL;
  tracker_sparql_connection_update_finish (connection, res, &error);
  if (error != NULL)
    {
      g_task_return_error (task, error);
      return;
    }

  photos_set_collection_job_job_collector (self, task);
}


static void
photos_set_collection_job_dispose (GObject *object)
{
  PhotosSetCollectionJob *self = PHOTOS_SET_COLLECTION_JOB (object);

  g_clear_object (&self->sel_cntrlr);
  g_clear_object (&self->queue);

  G_OBJECT_CLASS (photos_set_collection_job_parent_class)->dispose (object);
}


static void
photos_set_collection_job_finalize (GObject *object)
{
  PhotosSetCollectionJob *self = PHOTOS_SET_COLLECTION_JOB (object);

  g_clear_error (&self->queue_error);
  g_free (self->collection_urn);

  G_OBJECT_CLASS (photos_set_collection_job_parent_class)->finalize (object);
}


static void
photos_set_collection_job_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
  PhotosSetCollectionJob *self = PHOTOS_SET_COLLECTION_JOB (object);

  switch (prop_id)
    {
    case PROP_COLLECTION_URN:
      self->collection_urn = g_value_dup_string (value);
      break;

    case PROP_SETTING:
      self->setting = g_value_get_boolean (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
photos_set_collection_job_init (PhotosSetCollectionJob *self)
{
  self->sel_cntrlr = photos_selection_controller_dup_singleton ();
  self->queue = photos_tracker_queue_dup_singleton (NULL, &self->queue_error);
}


static void
photos_set_collection_job_class_init (PhotosSetCollectionJobClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->dispose = photos_set_collection_job_dispose;
  object_class->finalize = photos_set_collection_job_finalize;
  object_class->set_property = photos_set_collection_job_set_property;

  g_object_class_install_property (object_class,
                                   PROP_COLLECTION_URN,
                                   g_param_spec_string ("collection-urn",
                                                        "Uniform Resource Name",
                                                        "An unique ID associated with this collection",
                                                        NULL,
                                                        G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE));

  g_object_class_install_property (object_class,
                                   PROP_SETTING,
                                   g_param_spec_boolean ("setting",
                                                         "Insert or delete",
                                                         "Whether to insert or delete an item from this "
                                                         "collection",
                                                         FALSE,
                                                         G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE));
}


PhotosSetCollectionJob *
photos_set_collection_job_new (const gchar *collection_urn, gboolean setting)
{
  return g_object_new (PHOTOS_TYPE_SET_COLLECTION_JOB,
                       "collection-urn", collection_urn,
                       "setting", setting,
                       NULL);
}


gboolean
photos_set_collection_job_finish (PhotosSetCollectionJob *self, GAsyncResult *res, GError **error)
{
  GTask *task;

  g_return_val_if_fail (PHOTOS_IS_SET_COLLECTION_JOB (self), FALSE);

  g_return_val_if_fail (g_task_is_valid (res, self), FALSE);
  task = G_TASK (res);

  g_return_val_if_fail (g_task_get_source_tag (task) == photos_set_collection_job_run, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  return g_task_propagate_boolean (task, error);
}


void
photos_set_collection_job_run (PhotosSetCollectionJob *self,
                               GCancellable *cancellable,
                               GAsyncReadyCallback callback,
                               gpointer user_data)
{
  GApplication *app;
  GList *l;
  GList *urns;
  GTask *task = NULL;
  PhotosSearchContextState *state;

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, photos_set_collection_job_run);

  if (G_UNLIKELY (self->queue == NULL))
    {
      g_task_return_error (task, g_error_copy (self->queue_error));
      goto out;
    }

  app = g_application_get_default ();
  state = photos_search_context_get_state (PHOTOS_SEARCH_CONTEXT (app));

  urns = photos_selection_controller_get_selection (self->sel_cntrlr);
  for (l = urns; l != NULL; l = l->next)
    {
      PhotosQuery *query = NULL;
      const gchar *urn = (gchar *) l->data;

      if (g_strcmp0 (self->collection_urn, urn) == 0)
        continue;

      self->running_jobs++;
      query = photos_query_builder_set_collection_query (state, urn, self->collection_urn, self->setting);
      photos_tracker_queue_update (self->queue,
                                   query,
                                   cancellable,
                                   photos_set_collection_job_query_executed,
                                   g_object_ref (task),
                                   g_object_unref);
      g_object_unref (query);
    }

 out:
  g_clear_object (&task);
}
