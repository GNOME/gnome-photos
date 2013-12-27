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

#include "photos-fetch-collections-job.h"
#include "photos-query.h"
#include "photos-query-builder.h"
#include "photos-tracker-queue.h"


struct _PhotosFetchCollectionsJobPrivate
{
  GList *collections;
  PhotosFetchCollectionsJobCallback callback;
  PhotosTrackerQueue *queue;
  gchar *urn;
  gpointer user_data;
};

enum
{
  PROP_0,
  PROP_URN
};


G_DEFINE_TYPE_WITH_PRIVATE (PhotosFetchCollectionsJob, photos_fetch_collections_job, G_TYPE_OBJECT);


static void
photos_fetch_collections_job_emit_callback (PhotosFetchCollectionsJob *self)
{
  PhotosFetchCollectionsJobPrivate *priv = self->priv;

  if (priv->callback == NULL)
    return;

  (*priv->callback) (priv->collections, priv->user_data);
}


static void
photos_fetch_collections_job_cursor_next (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosFetchCollectionsJob *self = PHOTOS_FETCH_COLLECTIONS_JOB (user_data);
  PhotosFetchCollectionsJobPrivate *priv = self->priv;
  TrackerSparqlCursor *cursor = TRACKER_SPARQL_CURSOR (source_object);
  GError *error;
  gboolean valid;
  gchar *urn;

  error = NULL;
  valid = tracker_sparql_cursor_next_finish (cursor, res, &error);
  if (error != NULL)
    {
      g_warning ("Unable to fetch collections: %s", error->message);
      g_error_free (error);
      goto end;
    }
  if (!valid)
    goto end;

  urn = g_strdup (tracker_sparql_cursor_get_string (cursor, 0, NULL));
  priv->collections = g_list_prepend (priv->collections, urn);

  tracker_sparql_cursor_next_async (cursor,
                                    NULL,
                                    photos_fetch_collections_job_cursor_next,
                                    self);
  return;

 end:
  priv->collections = g_list_reverse (priv->collections);
  photos_fetch_collections_job_emit_callback (self);
  tracker_sparql_cursor_close (cursor);
  g_object_unref (self);
}


static void
photos_fetch_collections_job_query_executed (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosFetchCollectionsJob *self = PHOTOS_FETCH_COLLECTIONS_JOB (user_data);
  TrackerSparqlConnection *connection = TRACKER_SPARQL_CONNECTION (source_object);
  TrackerSparqlCursor *cursor;
  GError *error;

  error = NULL;
  cursor = tracker_sparql_connection_query_finish (connection, res, &error);
  if (error != NULL)
    {
      g_warning ("Unable to fetch collections: %s", error->message);
      g_error_free (error);
      photos_fetch_collections_job_emit_callback (self);
      return;
    }

  tracker_sparql_cursor_next_async (cursor,
                                    NULL,
                                    photos_fetch_collections_job_cursor_next,
                                    g_object_ref (self));
  g_object_unref (cursor);
}


static void
photos_fetch_collections_job_dispose (GObject *object)
{
  PhotosFetchCollectionsJob *self = PHOTOS_FETCH_COLLECTIONS_JOB (object);

  g_clear_object (&self->priv->queue);

  G_OBJECT_CLASS (photos_fetch_collections_job_parent_class)->dispose (object);
}


static void
photos_fetch_collections_job_finalize (GObject *object)
{
  PhotosFetchCollectionsJob *self = PHOTOS_FETCH_COLLECTIONS_JOB (object);
  PhotosFetchCollectionsJobPrivate *priv = self->priv;

  g_list_free_full (priv->collections, g_free);
  g_free (priv->urn);

  G_OBJECT_CLASS (photos_fetch_collections_job_parent_class)->finalize (object);
}


static void
photos_fetch_collections_job_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
  PhotosFetchCollectionsJob *self = PHOTOS_FETCH_COLLECTIONS_JOB (object);

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
photos_fetch_collections_job_init (PhotosFetchCollectionsJob *self)
{
  PhotosFetchCollectionsJobPrivate *priv = self->priv;

  self->priv = photos_fetch_collections_job_get_instance_private (self);
  priv = self->priv;

  priv->queue = photos_tracker_queue_dup_singleton (NULL, NULL);
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


void
photos_fetch_collections_job_run (PhotosFetchCollectionsJob *self,
                                  PhotosFetchCollectionsJobCallback callback,
                                  gpointer user_data)
{
  PhotosFetchCollectionsJobPrivate *priv = self->priv;
  PhotosQuery *query;

  priv->callback = callback;
  priv->user_data = user_data;

  query = photos_query_builder_fetch_collections_query (priv->urn);
  photos_tracker_queue_select (priv->queue,
                               query->sparql,
                               NULL,
                               photos_fetch_collections_job_query_executed,
                               g_object_ref (self),
                               g_object_unref);
  photos_query_free (query);
}
