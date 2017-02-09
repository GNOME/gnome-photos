/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2014 – 2017 Red Hat, Inc.
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

#include "photos-error.h"
#include "photos-query.h"
#include "photos-query-builder.h"
#include "photos-create-collection-icon-job.h"
#include "photos-tracker-queue.h"
#include "photos-utils.h"


struct _PhotosCreateCollectionIconJob
{
  GObject parent_instance;
  GError *queue_error;
  GIcon *icon;
  PhotosTrackerQueue *queue;
  gchar *urn;
};

enum
{
  PROP_0,
  PROP_URN
};


G_DEFINE_TYPE (PhotosCreateCollectionIconJob, photos_create_collection_icon_job, G_TYPE_OBJECT);


static void
photos_create_collection_icon_job_dispose (GObject *object)
{
  PhotosCreateCollectionIconJob *self = PHOTOS_CREATE_COLLECTION_ICON_JOB (object);

  g_clear_object (&self->icon);
  g_clear_object (&self->queue);

  G_OBJECT_CLASS (photos_create_collection_icon_job_parent_class)->dispose (object);
}


static void
photos_create_collection_icon_job_finalize (GObject *object)
{
  PhotosCreateCollectionIconJob *self = PHOTOS_CREATE_COLLECTION_ICON_JOB (object);

  g_clear_error (&self->queue_error);
  g_free (self->urn);

  G_OBJECT_CLASS (photos_create_collection_icon_job_parent_class)->finalize (object);
}


static void
photos_create_collection_icon_job_set_property (GObject *object,
                                                guint prop_id,
                                                const GValue *value,
                                                GParamSpec *pspec)
{
  PhotosCreateCollectionIconJob *self = PHOTOS_CREATE_COLLECTION_ICON_JOB (object);

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
photos_create_collection_icon_job_init (PhotosCreateCollectionIconJob *self)
{
  self->queue = photos_tracker_queue_dup_singleton (NULL, &self->queue_error);
}


static void
photos_create_collection_icon_job_class_init (PhotosCreateCollectionIconJobClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->dispose = photos_create_collection_icon_job_dispose;
  object_class->finalize = photos_create_collection_icon_job_finalize;
  object_class->set_property = photos_create_collection_icon_job_set_property;

  g_object_class_install_property (object_class,
                                   PROP_URN,
                                   g_param_spec_string ("urn",
                                                        "Uniform Resource Name",
                                                        "An unique ID associated with this item",
                                                        NULL,
                                                        G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE));
}


PhotosCreateCollectionIconJob *
photos_create_collection_icon_job_new (const gchar *urn)
{
  return g_object_new (PHOTOS_TYPE_CREATE_COLLECTION_ICON_JOB, "urn", urn, NULL);
}


GIcon *
photos_create_collection_icon_job_finish (PhotosCreateCollectionIconJob *self, GAsyncResult *res, GError **error)
{
  GTask *task = G_TASK (res);

  g_return_val_if_fail (g_task_is_valid (res, self), NULL);
  g_return_val_if_fail (g_task_get_source_tag (task) == photos_create_collection_icon_job_run, NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  return g_task_propagate_pointer (task, error);
}


void
photos_create_collection_icon_job_run (PhotosCreateCollectionIconJob *self,
                                       GCancellable *cancellable,
                                       GAsyncReadyCallback callback,
                                       gpointer user_data)
{
  GTask *task;

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, photos_create_collection_icon_job_run);

  if (G_UNLIKELY (self->queue == NULL))
    {
      g_task_return_error (task, g_error_copy (self->queue_error));
      return;
    }

  /* TODO: build collection icon query */
  g_task_return_new_error (task, PHOTOS_ERROR, 0, "Unable to create collection icon");

  g_object_unref (task);
}
