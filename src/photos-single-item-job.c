/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2012 Red Hat, Inc.
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

#include "photos-query.h"
#include "photos-query-builder.h"
#include "photos-single-item-job.h"
#include "photos-tracker-queue.h"


struct _PhotosSingleItemJobPrivate
{
  PhotosSingleItemJobCallback callback;
  PhotosTrackerQueue *queue;
  TrackerSparqlCursor *cursor;
  gchar *urn;
  gpointer user_data;
};

enum
{
  PROP_0,
  PROP_URN
};


G_DEFINE_TYPE (PhotosSingleItemJob, photos_single_item_job, G_TYPE_OBJECT);


static void
photos_single_item_job_emit_callback (PhotosSingleItemJob *self)
{
  PhotosSingleItemJobPrivate *priv = self->priv;

  if (priv->callback == NULL)
    return;

  (*priv->callback) (priv->cursor, priv->user_data);
}


static void
photos_single_item_job_cursor_next (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosSingleItemJob *self = PHOTOS_SINGLE_ITEM_JOB (user_data);
  PhotosSingleItemJobPrivate *priv = self->priv;
  GError *error;
  gboolean valid;

  error = NULL;
  valid = tracker_sparql_cursor_next_finish (priv->cursor, res, &error);
  if (!valid)
    {
      g_warning ("Unable to query single item: %s", error->message);
      g_error_free (error);
    }

  photos_single_item_job_emit_callback (self);
  tracker_sparql_cursor_close (priv->cursor);
  g_clear_object (&priv->cursor);
  g_object_unref (self);
}


static void
photos_single_item_job_query_executed (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosSingleItemJob *self = PHOTOS_SINGLE_ITEM_JOB (user_data);
  PhotosSingleItemJobPrivate *priv = self->priv;
  TrackerSparqlConnection *connection = TRACKER_SPARQL_CONNECTION (source_object);
  GError *error;

  error = NULL;
  priv->cursor = tracker_sparql_connection_query_finish (connection, res, &error);
  if (error != NULL)
    {
      g_warning ("Unable to query single item: %s", error->message);
      g_error_free (error);
      photos_single_item_job_emit_callback (self);
      return;
    }

  tracker_sparql_cursor_next_async (priv->cursor,
                                    NULL,
                                    photos_single_item_job_cursor_next,
                                    g_object_ref (self));
}


static void
photos_single_item_job_dispose (GObject *object)
{
  PhotosSingleItemJob *self = PHOTOS_SINGLE_ITEM_JOB (object);
  PhotosSingleItemJobPrivate *priv = self->priv;

  g_clear_object (&priv->cursor);
  g_clear_object (&priv->queue);

  G_OBJECT_CLASS (photos_single_item_job_parent_class)->dispose (object);
}


static void
photos_single_item_job_finalize (GObject *object)
{
  PhotosSingleItemJob *self = PHOTOS_SINGLE_ITEM_JOB (object);

  g_free (self->priv->urn);

  G_OBJECT_CLASS (photos_single_item_job_parent_class)->finalize (object);
}


static void
photos_single_item_job_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
  PhotosSingleItemJob *self = PHOTOS_SINGLE_ITEM_JOB (object);

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
photos_single_item_job_init (PhotosSingleItemJob *self)
{
  PhotosSingleItemJobPrivate *priv = self->priv;

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, PHOTOS_TYPE_SINGLE_ITEM_JOB, PhotosSingleItemJobPrivate);
  priv = self->priv;

  priv->queue = photos_tracker_queue_new ();
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

  g_type_class_add_private (class, sizeof (PhotosSingleItemJobPrivate));
}


PhotosSingleItemJob *
photos_single_item_job_new (const gchar *urn)
{
  return g_object_new (PHOTOS_TYPE_SINGLE_ITEM_JOB, "urn", urn, NULL);
}


void
photos_single_item_job_run (PhotosSingleItemJob *self,
                            gint flags,
                            PhotosSingleItemJobCallback callback,
                            gpointer user_data)
{
  PhotosSingleItemJobPrivate *priv = self->priv;
  PhotosQuery *query;

  priv->callback = callback;
  priv->user_data = user_data;

  query = photos_query_builder_single_query (flags, priv->urn);
  photos_tracker_queue_select (priv->queue,
                               query->sparql,
                               NULL,
                               photos_single_item_job_query_executed,
                               g_object_ref (self),
                               g_object_unref);
  photos_query_free (query);
}
