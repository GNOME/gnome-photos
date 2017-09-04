/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2013 – 2017 Red Hat, Inc.
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

#include "photos-create-collection-job.h"
#include "photos-error.h"
#include "photos-query.h"
#include "photos-query-builder.h"
#include "photos-search-context.h"
#include "photos-tracker-queue.h"
#include "photos-utils.h"


struct _PhotosCreateCollectionJob
{
  GObject parent_instance;
  GError *queue_error;
  PhotosTrackerQueue *queue;
  gchar *name;
};

enum
{
  PROP_0,
  PROP_NAME
};


G_DEFINE_TYPE (PhotosCreateCollectionJob, photos_create_collection_job, G_TYPE_OBJECT);


static void
photos_create_collection_job_query_executed (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  GTask *task = G_TASK (user_data);
  TrackerSparqlConnection *connection = TRACKER_SPARQL_CONNECTION (source_object);
  GError *error;
  GVariant *variant;
  GVariant *child;
  gchar *key = NULL;
  gchar *val = NULL;

  error = NULL;
  variant = tracker_sparql_connection_update_blank_finish (connection, res, &error);
  if (error != NULL)
    {
      g_task_return_error (task, error);
      return;
    }

  child = g_variant_get_child_value (variant, 0); /* variant is now aa{ss} */
  g_variant_unref (variant);
  variant = child;

  child = g_variant_get_child_value (variant, 0); /* variant is now s{ss} */
  g_variant_unref (variant);
  variant = child;

  child = g_variant_get_child_value (variant, 0); /* variant is now {ss} */
  g_variant_unref (variant);
  variant = child;

  child = g_variant_get_child_value (variant, 0);
  key = g_variant_dup_string (child, NULL);
  g_variant_unref (child);

  child = g_variant_get_child_value (variant, 1);
  val = g_variant_dup_string (child, NULL);
  g_variant_unref (child);

  g_variant_unref (variant);

  if (g_strcmp0 (key, "res") == 0)
    g_task_return_pointer (task, g_strdup (val), g_free);
  else
    g_task_return_new_error (task, PHOTOS_ERROR, 0, "Failed to parse GVariant");

  g_free (val);
  g_free (key);
}


static void
photos_create_collection_job_dispose (GObject *object)
{
  PhotosCreateCollectionJob *self = PHOTOS_CREATE_COLLECTION_JOB (object);

  g_clear_object (&self->queue);

  G_OBJECT_CLASS (photos_create_collection_job_parent_class)->dispose (object);
}


static void
photos_create_collection_job_finalize (GObject *object)
{
  PhotosCreateCollectionJob *self = PHOTOS_CREATE_COLLECTION_JOB (object);

  g_clear_error (&self->queue_error);
  g_free (self->name);

  G_OBJECT_CLASS (photos_create_collection_job_parent_class)->finalize (object);
}


static void
photos_create_collection_job_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
  PhotosCreateCollectionJob *self = PHOTOS_CREATE_COLLECTION_JOB (object);

  switch (prop_id)
    {
    case PROP_NAME:
      self->name = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
photos_create_collection_job_init (PhotosCreateCollectionJob *self)
{
  self->queue = photos_tracker_queue_dup_singleton (NULL, &self->queue_error);
}


static void
photos_create_collection_job_class_init (PhotosCreateCollectionJobClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->dispose = photos_create_collection_job_dispose;
  object_class->finalize = photos_create_collection_job_finalize;
  object_class->set_property = photos_create_collection_job_set_property;

  g_object_class_install_property (object_class,
                                   PROP_NAME,
                                   g_param_spec_string ("name",
                                                        "Name",
                                                        "The name of the collection to be created",
                                                        NULL,
                                                        G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE));
}


PhotosCreateCollectionJob *
photos_create_collection_job_new (const gchar *name)
{
  return g_object_new (PHOTOS_TYPE_CREATE_COLLECTION_JOB, "name", name, NULL);
}


gchar *
photos_create_collection_job_finish (PhotosCreateCollectionJob *self, GAsyncResult *res, GError **error)
{
  GTask *task = G_TASK (res);

  g_return_val_if_fail (g_task_is_valid (res, self), NULL);
  g_return_val_if_fail (g_task_get_source_tag (task) == photos_create_collection_job_run, NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  return g_task_propagate_pointer (task, error);
}


void
photos_create_collection_job_run (PhotosCreateCollectionJob *self,
                                  GCancellable *cancellable,
                                  GAsyncReadyCallback callback,
                                  gpointer user_data)
{
  GApplication *app;
  GTask *task;
  PhotosQuery *query;
  PhotosSearchContextState *state;

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, photos_create_collection_job_run);

  if (G_UNLIKELY (self->queue == NULL))
    {
      g_task_return_error (task, g_error_copy (self->queue_error));
      goto out;
    }

  app = g_application_get_default ();
  state = photos_search_context_get_state (PHOTOS_SEARCH_CONTEXT (app));

  query = photos_query_builder_create_collection_query (state, self->name);
  photos_tracker_queue_update_blank (self->queue,
                                     query,
                                     cancellable,
                                     photos_create_collection_job_query_executed,
                                     g_object_ref (task),
                                     g_object_unref);
  photos_query_free (query);

 out:
  g_object_unref (task);
}
