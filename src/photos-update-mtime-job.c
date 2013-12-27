/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2013 Red Hat, Inc.
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

#include "photos-query.h"
#include "photos-query-builder.h"
#include "photos-update-mtime-job.h"
#include "photos-tracker-queue.h"


struct _PhotosUpdateMtimeJobPrivate
{
  PhotosUpdateMtimeJobCallback callback;
  PhotosTrackerQueue *queue;
  gchar *urn;
  gpointer user_data;
};

enum
{
  PROP_0,
  PROP_URN
};


G_DEFINE_TYPE_WITH_PRIVATE (PhotosUpdateMtimeJob, photos_update_mtime_job, G_TYPE_OBJECT);


static void
photos_update_mtime_job_query_executed (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosUpdateMtimeJob *self = PHOTOS_UPDATE_MTIME_JOB (user_data);
  PhotosUpdateMtimeJobPrivate *priv = self->priv;
  TrackerSparqlConnection *connection = TRACKER_SPARQL_CONNECTION (source_object);
  GError *error;

  error = NULL;
  tracker_sparql_connection_update_finish (connection, res, &error);
  if (error != NULL)
    {
      g_warning ("Unable to update mtime: %s", error->message);
      g_error_free (error);
      goto out;
    }

 out:
  if (priv->callback != NULL)
    (*priv->callback) (priv->user_data);
}


static void
photos_update_mtime_job_dispose (GObject *object)
{
  PhotosUpdateMtimeJob *self = PHOTOS_UPDATE_MTIME_JOB (object);
  PhotosUpdateMtimeJobPrivate *priv = self->priv;

  g_clear_object (&priv->queue);

  G_OBJECT_CLASS (photos_update_mtime_job_parent_class)->dispose (object);
}


static void
photos_update_mtime_job_finalize (GObject *object)
{
  PhotosUpdateMtimeJob *self = PHOTOS_UPDATE_MTIME_JOB (object);

  g_free (self->priv->urn);

  G_OBJECT_CLASS (photos_update_mtime_job_parent_class)->finalize (object);
}


static void
photos_update_mtime_job_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
  PhotosUpdateMtimeJob *self = PHOTOS_UPDATE_MTIME_JOB (object);

  switch (prop_id)
    {
    case PROP_URN:
      self->priv->urn = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
photos_update_mtime_job_init (PhotosUpdateMtimeJob *self)
{
  PhotosUpdateMtimeJobPrivate *priv = self->priv;

  self->priv = photos_update_mtime_job_get_instance_private (self);
  priv = self->priv;

  priv->queue = photos_tracker_queue_dup_singleton (NULL, NULL);
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


void
photos_update_mtime_job_run (PhotosUpdateMtimeJob *self,
                             PhotosUpdateMtimeJobCallback callback,
                             gpointer user_data)
{
  PhotosUpdateMtimeJobPrivate *priv = self->priv;
  PhotosQuery *query;

  priv->callback = callback;
  priv->user_data = user_data;

  query = photos_query_builder_update_mtime_query (priv->urn);
  photos_tracker_queue_update (priv->queue,
                               query->sparql,
                               NULL,
                               photos_update_mtime_job_query_executed,
                               g_object_ref (self),
                               g_object_unref);
  photos_query_free (query);
}
