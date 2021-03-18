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

#include <glib.h>

#include "photos-query.h"
#include "photos-query-builder.h"
#include "photos-single-item-job.h"
#include "photos-tracker-queue.h"


struct _PhotosSingleItemJob
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


G_DEFINE_TYPE (PhotosSingleItemJob, photos_single_item_job, G_TYPE_OBJECT);


static void
photos_single_item_job_cursor_next (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  GTask *task = G_TASK (user_data);
  TrackerSparqlCursor *cursor = TRACKER_SPARQL_CURSOR (source_object);
  GDestroyNotify result_destroy = NULL;
  GError *error;
  gboolean success;
  gpointer result = NULL;

  error = NULL;
  /* Note that tracker_sparql_cursor_next_finish can return FALSE even
   * without an error.
   */
  success = tracker_sparql_cursor_next_finish (cursor, res, &error);
  if (error != NULL)
    {
      g_task_return_error (task, error);
      goto out;
    }

  if (success)
    {
      result = g_object_ref (cursor);
      result_destroy = g_object_unref;
    }

  g_task_return_pointer (task, result, result_destroy);

 out:
  g_object_unref (task);
}


static void
photos_single_item_job_query_executed (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  GTask *task = G_TASK (user_data);
  TrackerSparqlConnection *connection = TRACKER_SPARQL_CONNECTION (source_object);
  TrackerSparqlCursor *cursor;
  GError *error;

  error = NULL;
  cursor = tracker_sparql_connection_query_finish (connection, res, &error);
  if (error != NULL)
    {
      g_task_return_error (task, error);
      return;
    }

  tracker_sparql_cursor_next_async (cursor,
                                    g_task_get_cancellable (task),
                                    photos_single_item_job_cursor_next,
                                    g_object_ref (task));
  g_object_unref (cursor);
}


static void
photos_single_item_job_dispose (GObject *object)
{
  PhotosSingleItemJob *self = PHOTOS_SINGLE_ITEM_JOB (object);

  g_clear_error (&self->queue_error);
  g_clear_object (&self->queue);

  G_OBJECT_CLASS (photos_single_item_job_parent_class)->dispose (object);
}


static void
photos_single_item_job_finalize (GObject *object)
{
  PhotosSingleItemJob *self = PHOTOS_SINGLE_ITEM_JOB (object);

  g_free (self->urn);

  G_OBJECT_CLASS (photos_single_item_job_parent_class)->finalize (object);
}


static void
photos_single_item_job_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
  PhotosSingleItemJob *self = PHOTOS_SINGLE_ITEM_JOB (object);

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
photos_single_item_job_init (PhotosSingleItemJob *self)
{
  self->queue = photos_tracker_queue_dup_singleton (NULL, &self->queue_error);
}


static void
photos_single_item_job_class_init (PhotosSingleItemJobClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->dispose = photos_single_item_job_dispose;
  object_class->finalize = photos_single_item_job_finalize;
  object_class->set_property = photos_single_item_job_set_property;

  g_object_class_install_property (object_class,
                                   PROP_URN,
                                   g_param_spec_string ("urn",
                                                        "Uniform Resource Name",
                                                        "An unique ID associated with this item",
                                                        NULL,
                                                        G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE));
}


PhotosSingleItemJob *
photos_single_item_job_new (const gchar *urn)
{
  return g_object_new (PHOTOS_TYPE_SINGLE_ITEM_JOB, "urn", urn, NULL);
}


TrackerSparqlCursor *
photos_single_item_job_finish (PhotosSingleItemJob *self, GAsyncResult *res, GError **error)
{
  GTask *task = G_TASK (res);

  g_return_val_if_fail (g_task_is_valid (res, self), NULL);
  g_return_val_if_fail (g_task_get_source_tag (task) == photos_single_item_job_run, NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  return g_task_propagate_pointer (task, error);
}


void
photos_single_item_job_run (PhotosSingleItemJob *self,
                            PhotosSearchContextState *state,
                            gint flags,
                            GCancellable *cancellable,
                            GAsyncReadyCallback callback,
                            gpointer user_data)
{
  GTask *task;
  PhotosQuery *query = NULL;

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, photos_single_item_job_run);

  if (G_UNLIKELY (self->queue == NULL))
    {
      g_task_return_error (task, g_error_copy (self->queue_error));
      goto out;
    }

  query = photos_query_builder_single_query (state, flags, self->urn);
  photos_tracker_queue_select (self->queue,
                               query,
                               cancellable,
                               photos_single_item_job_query_executed,
                               g_object_ref (task),
                               g_object_unref);

 out:
  g_clear_object (&query);
  g_object_unref (task);
}
