/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2013 – 2021 Red Hat, Inc.
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
#include <tracker-sparql.h>

#include "photos-query.h"
#include "photos-query-builder.h"
#include "photos-update-mtime-job.h"
#include "photos-search-context.h"
#include "photos-tracker-queue.h"


struct _PhotosUpdateMtimeJob
{
  GObject parent_instance;
  GError *queue_error;
  PhotosTrackerQueue *queue;
  gchar *urn;
};

enum
{
  PROP_0,
  PROP_URN
};


G_DEFINE_TYPE (PhotosUpdateMtimeJob, photos_update_mtime_job, G_TYPE_OBJECT);


static void
photos_update_mtime_job_query_executed (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  GTask *task = G_TASK (user_data);
  TrackerSparqlConnection *connection = TRACKER_SPARQL_CONNECTION (source_object);
  GError *error;

  error = NULL;
  tracker_sparql_connection_update_finish (connection, res, &error);
  if (error != NULL)
    {
      g_task_return_error (task, error);
      return;
    }

  g_task_return_boolean (task, TRUE);
}


static void
photos_update_mtime_job_dispose (GObject *object)
{
  PhotosUpdateMtimeJob *self = PHOTOS_UPDATE_MTIME_JOB (object);

  g_clear_object (&self->queue);

  G_OBJECT_CLASS (photos_update_mtime_job_parent_class)->dispose (object);
}


static void
photos_update_mtime_job_finalize (GObject *object)
{
  PhotosUpdateMtimeJob *self = PHOTOS_UPDATE_MTIME_JOB (object);

  g_clear_error (&self->queue_error);
  g_free (self->urn);

  G_OBJECT_CLASS (photos_update_mtime_job_parent_class)->finalize (object);
}


static void
photos_update_mtime_job_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
  PhotosUpdateMtimeJob *self = PHOTOS_UPDATE_MTIME_JOB (object);

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
photos_update_mtime_job_init (PhotosUpdateMtimeJob *self)
{
  self->queue = photos_tracker_queue_dup_singleton (NULL, &self->queue_error);
}


static void
photos_update_mtime_job_class_init (PhotosUpdateMtimeJobClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->dispose = photos_update_mtime_job_dispose;
  object_class->finalize = photos_update_mtime_job_finalize;
  object_class->set_property = photos_update_mtime_job_set_property;

  g_object_class_install_property (object_class,
                                   PROP_URN,
                                   g_param_spec_string ("urn",
                                                        "Uniform Resource Name",
                                                        "An unique ID associated with this item",
                                                        NULL,
                                                        G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE));
}


PhotosUpdateMtimeJob *
photos_update_mtime_job_new (const gchar *urn)
{
  return g_object_new (PHOTOS_TYPE_UPDATE_MTIME_JOB, "urn", urn, NULL);
}


gboolean
photos_update_mtime_job_finish (PhotosUpdateMtimeJob *self, GAsyncResult *res, GError **error)
{
  GTask *task = G_TASK (res);

  g_return_val_if_fail (g_task_is_valid (res, self), FALSE);
  g_return_val_if_fail (g_task_get_source_tag (task) == photos_update_mtime_job_run, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  return g_task_propagate_boolean (task, error);
}


void
photos_update_mtime_job_run (PhotosUpdateMtimeJob *self,
                             GCancellable *cancellable,
                             GAsyncReadyCallback callback,
                             gpointer user_data)
{
  GApplication *app;
  g_autoptr (GTask) task = NULL;
  g_autoptr (PhotosQuery) query = NULL;
  PhotosSearchContextState *state;

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, photos_update_mtime_job_run);

  if (G_UNLIKELY (self->queue == NULL))
    {
      g_task_return_error (task, g_error_copy (self->queue_error));
      goto out;
    }

  app = g_application_get_default ();
  state = photos_search_context_get_state (PHOTOS_SEARCH_CONTEXT (app));

  query = photos_query_builder_update_mtime_query (state, self->urn);
  photos_tracker_queue_update (self->queue,
                               query,
                               cancellable,
                               photos_update_mtime_job_query_executed,
                               g_object_ref (task),
                               g_object_unref);

 out:
  return;
}
