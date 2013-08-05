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
#include <tracker-sparql.h>

#include "photos-offset-controller.h"
#include "photos-query-builder.h"
#include "photos-tracker-queue.h"


struct _PhotosOffsetControllerPrivate
{
  PhotosTrackerQueue *queue;
  gint count;
  gint offset;
};

enum
{
  COUNT_CHANGED,
  OFFSET_CHANGED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };


G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (PhotosOffsetController, photos_offset_controller, G_TYPE_OBJECT);


enum
{
  OFFSET_STEP = 50
};


static void
photos_offset_controller_cursor_next (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosOffsetController *self = PHOTOS_OFFSET_CONTROLLER (user_data);
  PhotosOffsetControllerPrivate *priv = self->priv;
  TrackerSparqlCursor *cursor = TRACKER_SPARQL_CURSOR (source_object);
  gboolean valid;

  valid = tracker_sparql_cursor_next_finish (cursor, res, NULL);
  if (valid)
    {
      priv->count = (gint) tracker_sparql_cursor_get_integer (cursor, 0);
      g_signal_emit (self, signals[COUNT_CHANGED], 0, priv->count);
    }

  tracker_sparql_cursor_close (cursor);
  g_object_unref (self);
}


static void
photos_offset_controller_reset_count_query_executed (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosOffsetController *self = PHOTOS_OFFSET_CONTROLLER (user_data);
  TrackerSparqlConnection *connection = TRACKER_SPARQL_CONNECTION (source_object);
  TrackerSparqlCursor *cursor;
  GError *error;

  error = NULL;
  cursor = tracker_sparql_connection_query_finish (connection, res, &error);
  if (error != NULL)
    {
      g_error_free (error);
      return;
    }

  tracker_sparql_cursor_next_async (cursor, NULL, photos_offset_controller_cursor_next, g_object_ref (self));
  g_object_unref (cursor);
}


static void
photos_offset_controller_dispose (GObject *object)
{
  PhotosOffsetController *self = PHOTOS_OFFSET_CONTROLLER (object);

  g_clear_object (&self->priv->queue);

  G_OBJECT_CLASS (photos_offset_controller_parent_class)->dispose (object);
}


static void
photos_offset_controller_init (PhotosOffsetController *self)
{
  PhotosOffsetControllerPrivate *priv;

  self->priv = photos_offset_controller_get_instance_private (self);
  priv = self->priv;

  priv->queue = photos_tracker_queue_new ();
}


static void
photos_offset_controller_class_init (PhotosOffsetControllerClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->dispose = photos_offset_controller_dispose;

  signals[COUNT_CHANGED] = g_signal_new ("count-changed",
                                         G_TYPE_FROM_CLASS (class),
                                         G_SIGNAL_RUN_LAST,
                                         G_STRUCT_OFFSET (PhotosOffsetControllerClass,
                                                          count_changed),
                                         NULL, /*accumulator */
                                         NULL, /*accu_data */
                                         g_cclosure_marshal_VOID__INT,
                                         G_TYPE_NONE,
                                         1,
                                         G_TYPE_INT);

  signals[OFFSET_CHANGED] = g_signal_new ("offset-changed",
                                          G_TYPE_FROM_CLASS (class),
                                          G_SIGNAL_RUN_LAST,
                                          G_STRUCT_OFFSET (PhotosOffsetControllerClass,
                                                           offset_changed),
                                          NULL, /*accumulator */
                                          NULL, /*accu_data */
                                          g_cclosure_marshal_VOID__INT,
                                          G_TYPE_NONE,
                                          1,
                                          G_TYPE_INT);
}


PhotosOffsetController *
photos_offset_controller_new (void)
{
  return g_object_new (PHOTOS_TYPE_OFFSET_CONTROLLER, NULL);
}


gint
photos_offset_controller_get_count (PhotosOffsetController *self)
{
  return self->priv->count;
}


gint
photos_offset_controller_get_offset (PhotosOffsetController *self)
{
  return self->priv->offset;
}


gint
photos_offset_controller_get_remaining (PhotosOffsetController *self)
{
  PhotosOffsetControllerPrivate *priv = self->priv;
  return priv->count - (priv->offset + OFFSET_STEP);
}


gint
photos_offset_controller_get_step (PhotosOffsetController *self)
{
  return OFFSET_STEP;
}


void
photos_offset_controller_increase_offset (PhotosOffsetController *self)
{
  PhotosOffsetControllerPrivate *priv = self->priv;

  priv->offset += OFFSET_STEP;
  g_signal_emit (self, signals[OFFSET_CHANGED], 0, priv->offset);
}


void
photos_offset_controller_reset_count (PhotosOffsetController *self)
{
  PhotosOffsetControllerPrivate *priv = self->priv;
  PhotosQuery *query;

  query = PHOTOS_OFFSET_CONTROLLER_GET_CLASS (self)->get_query (self);
  photos_tracker_queue_select (priv->queue,
                               query->sparql,
                               NULL,
                               photos_offset_controller_reset_count_query_executed,
                               g_object_ref (self),
                               g_object_unref);
  photos_query_free (query);
}


void
photos_offset_controller_reset_offset (PhotosOffsetController *self)
{
  self->priv->offset = 0;
}
