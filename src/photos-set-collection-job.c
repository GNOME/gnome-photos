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
#include "photos-selection-controller.h"
#include "photos-set-collection-job.h"
#include "photos-tracker-queue.h"
#include "photos-update-mtime-job.h"


struct _PhotosSetCollectionJobPrivate
{
  PhotosSelectionController *sel_cntrlr;
  PhotosSetCollectionJobCallback callback;
  PhotosTrackerQueue *queue;
  gboolean setting;
  gchar *collection_urn;
  gint running_jobs;
  gpointer user_data;
};

enum
{
  PROP_0,
  PROP_COLLECTION_URN,
  PROP_SETTING
};


G_DEFINE_TYPE (PhotosSetCollectionJob, photos_set_collection_job, G_TYPE_OBJECT);


static void
photos_set_collection_job_update_mtime (gpointer user_data)
{
  PhotosSetCollectionJob *self = PHOTOS_SET_COLLECTION_JOB (user_data);
  PhotosSetCollectionJobPrivate *priv = self->priv;

  if (priv->callback != NULL)
    (*priv->callback) (priv->user_data);

  g_object_unref (self);
}


static void
photos_set_collection_job_job_collector (PhotosSetCollectionJob *self)
{
  PhotosSetCollectionJobPrivate *priv = self->priv;

  priv->running_jobs--;
  if (priv->running_jobs == 0)
    {
      PhotosUpdateMtimeJob *job;

      job = photos_update_mtime_job_new (priv->collection_urn);
      photos_update_mtime_job_run (job, photos_set_collection_job_update_mtime, g_object_ref (self));
      g_object_unref (job);
    }
}


static void
photos_set_collection_job_query_executed (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosSetCollectionJob *self = PHOTOS_SET_COLLECTION_JOB (user_data);
  TrackerSparqlConnection *connection = TRACKER_SPARQL_CONNECTION (source_object);
  GError *error;

  error = NULL;
  tracker_sparql_connection_update_finish (connection, res, &error);
  if (error != NULL)
    {
      g_warning ("Unable to add item to collection: %s", error->message);
      g_error_free (error);
      return;
    }

  photos_set_collection_job_job_collector (self);
}


static void
photos_set_collection_job_dispose (GObject *object)
{
  PhotosSetCollectionJob *self = PHOTOS_SET_COLLECTION_JOB (object);
  PhotosSetCollectionJobPrivate *priv = self->priv;

  g_clear_object (&priv->sel_cntrlr);
  g_clear_object (&priv->queue);

  G_OBJECT_CLASS (photos_set_collection_job_parent_class)->dispose (object);
}


static void
photos_set_collection_job_finalize (GObject *object)
{
  PhotosSetCollectionJob *self = PHOTOS_SET_COLLECTION_JOB (object);

  g_free (self->priv->collection_urn);

  G_OBJECT_CLASS (photos_set_collection_job_parent_class)->finalize (object);
}


static void
photos_set_collection_job_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
  PhotosSetCollectionJob *self = PHOTOS_SET_COLLECTION_JOB (object);
  PhotosSetCollectionJobPrivate *priv = self->priv;

  switch (prop_id)
    {
    case PROP_COLLECTION_URN:
      priv->collection_urn = g_value_dup_string (value);
      break;

    case PROP_SETTING:
      priv->setting = g_value_get_boolean (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
photos_set_collection_job_init (PhotosSetCollectionJob *self)
{
  PhotosSetCollectionJobPrivate *priv = self->priv;

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, PHOTOS_TYPE_SET_COLLECTION_JOB, PhotosSetCollectionJobPrivate);
  priv = self->priv;

  priv->sel_cntrlr = photos_selection_controller_new ();
  priv->queue = photos_tracker_queue_new ();
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

  g_type_class_add_private (class, sizeof (PhotosSetCollectionJobPrivate));
}


PhotosSetCollectionJob *
photos_set_collection_job_new (const gchar *collection_urn, gboolean setting)
{
  return g_object_new (PHOTOS_TYPE_SET_COLLECTION_JOB,
                       "collection-urn", collection_urn,
                       "setting", setting,
                       NULL);
}


void
photos_set_collection_job_run (PhotosSetCollectionJob *self,
                               PhotosSetCollectionJobCallback callback,
                               gpointer user_data)
{
  PhotosSetCollectionJobPrivate *priv = self->priv;
  GList *l;
  GList *urns;

  priv->callback = callback;
  priv->user_data = user_data;

  urns = photos_selection_controller_get_selection (priv->sel_cntrlr);
  for (l = urns; l != NULL; l = l->next)
    {
      PhotosQuery *query;
      const gchar *urn = (gchar *) l->data;

      if (g_strcmp0 (priv->collection_urn, urn) == 0)
        continue;

      priv->running_jobs++;
      query = photos_query_builder_set_collection_query (urn, priv->collection_urn, priv->setting);
      photos_tracker_queue_update (priv->queue,
                                   query->sparql,
                                   NULL,
                                   photos_set_collection_job_query_executed,
                                   g_object_ref (self),
                                   g_object_unref);
      photos_query_free (query);
    }
}
