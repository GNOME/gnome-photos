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

#include "photos-create-collection-job.h"
#include "photos-query.h"
#include "photos-query-builder.h"
#include "photos-tracker-queue.h"


struct _PhotosCreateCollectionJobPrivate
{
  PhotosCreateCollectionJobCallback callback;
  PhotosTrackerQueue *queue;
  const gchar *created_urn;
  gchar *name;
  gpointer user_data;
};

enum
{
  PROP_0,
  PROP_NAME
};


G_DEFINE_TYPE_WITH_PRIVATE (PhotosCreateCollectionJob, photos_create_collection_job, G_TYPE_OBJECT);


static void
photos_create_collection_job_query_executed (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosCreateCollectionJob *self = PHOTOS_CREATE_COLLECTION_JOB (user_data);
  PhotosCreateCollectionJobPrivate *priv = self->priv;
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
      g_warning ("Unable to create collection: %s", error->message);
      g_error_free (error);
      goto out;
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
    priv->created_urn = val;

 out:
  if (priv->callback != NULL)
    (*priv->callback) (priv->created_urn, priv->user_data);

  priv->created_urn = NULL;
  g_free (val);
  g_free (key);
}


static void
photos_create_collection_job_dispose (GObject *object)
{
  PhotosCreateCollectionJob *self = PHOTOS_CREATE_COLLECTION_JOB (object);

  g_clear_object (&self->priv->queue);

  G_OBJECT_CLASS (photos_create_collection_job_parent_class)->dispose (object);
}


static void
photos_create_collection_job_finalize (GObject *object)
{
  PhotosCreateCollectionJob *self = PHOTOS_CREATE_COLLECTION_JOB (object);

  g_free (self->priv->name);

  G_OBJECT_CLASS (photos_create_collection_job_parent_class)->finalize (object);
}


static void
photos_create_collection_job_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
  PhotosCreateCollectionJob *self = PHOTOS_CREATE_COLLECTION_JOB (object);

  switch (prop_id)
    {
    case PROP_NAME:
      self->priv->name = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
photos_create_collection_job_init (PhotosCreateCollectionJob *self)
{
  PhotosCreateCollectionJobPrivate *priv;

  self->priv = photos_create_collection_job_get_instance_private (self);
  priv = self->priv;

  priv->queue = photos_tracker_queue_dup_singleton (NULL, NULL);
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


void
photos_create_collection_job_run (PhotosCreateCollectionJob *self,
                                  PhotosCreateCollectionJobCallback callback,
                                  gpointer user_data)
{
  PhotosCreateCollectionJobPrivate *priv = self->priv;
  PhotosQuery *query;

  if (G_UNLIKELY (priv->queue == NULL))
    {
      if (callback != NULL)
        (*callback) (NULL, user_data);
      return;
    }

  priv->callback = callback;
  priv->user_data = user_data;

  query = photos_query_builder_create_collection_query (priv->name);
  photos_tracker_queue_update_blank (priv->queue,
                                     query->sparql,
                                     NULL,
                                     photos_create_collection_job_query_executed,
                                     g_object_ref (self),
                                     g_object_unref);
  photos_query_free (query);
}
