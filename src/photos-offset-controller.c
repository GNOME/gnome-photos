/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2012 – 2017 Red Hat, Inc.
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
#include <tracker-sparql.h>
#include <dazzle.h>

#include "photos-debug.h"
#include "photos-offset-controller.h"
#include "photos-query-builder.h"
#include "photos-tracker-queue.h"


struct _PhotosOffsetControllerPrivate
{
  GCancellable *cancellable;
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
DZL_DEFINE_COUNTER (instances,
                    "PhotosOffsetController",
                    "Instances",
                    "Number of PhotosOffsetController instances")


enum
{
  OFFSET_STEP = 60
};


static void
photos_offset_controller_cursor_next (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosOffsetController *self;
  PhotosOffsetControllerPrivate *priv;
  GError *error;
  TrackerSparqlCursor *cursor = TRACKER_SPARQL_CURSOR (source_object);
  gboolean success;

  error = NULL;
  /* Note that tracker_sparql_cursor_next_finish can return FALSE even without
   * an error
   */
  success = tracker_sparql_cursor_next_finish (cursor, res, &error);
  if (error != NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Unable to query the item count: %s", error->message);
      g_error_free (error);
      goto out;
    }

  self = PHOTOS_OFFSET_CONTROLLER (user_data);
  priv = photos_offset_controller_get_instance_private (self);

  if (success)
    {
      const gchar *type_name;

      priv->count = (gint) tracker_sparql_cursor_get_integer (cursor, 0);

      type_name = G_OBJECT_TYPE_NAME (self);
      photos_debug (PHOTOS_DEBUG_TRACKER, "%s has %d items", type_name, priv->count);

      g_signal_emit (self, signals[COUNT_CHANGED], 0, priv->count);
    }

 out:
  tracker_sparql_cursor_close (cursor);
}


static void
photos_offset_controller_reset_count_query_executed (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosOffsetController *self = PHOTOS_OFFSET_CONTROLLER (user_data);
  PhotosOffsetControllerPrivate *priv;
  TrackerSparqlConnection *connection = TRACKER_SPARQL_CONNECTION (source_object);
  TrackerSparqlCursor *cursor;
  GError *error;

  priv = photos_offset_controller_get_instance_private (self);

  error = NULL;
  cursor = tracker_sparql_connection_query_finish (connection, res, &error);
  if (error != NULL)
    {
      g_error_free (error);
      return;
    }

  tracker_sparql_cursor_next_async (cursor,
                                    priv->cancellable,
                                    photos_offset_controller_cursor_next,
                                    self);
  g_object_unref (cursor);
}


static void
photos_offset_controller_dispose (GObject *object)
{
  PhotosOffsetController *self = PHOTOS_OFFSET_CONTROLLER (object);
  PhotosOffsetControllerPrivate *priv;

  priv = photos_offset_controller_get_instance_private (self);

  if (priv->cancellable != NULL)
    {
      g_cancellable_cancel (priv->cancellable);
      g_clear_object (&priv->cancellable);
    }

  g_clear_object (&priv->queue);

  G_OBJECT_CLASS (photos_offset_controller_parent_class)->dispose (object);
}


static void
photos_offset_controller_finalize (GObject *object)
{
  G_OBJECT_CLASS (photos_offset_controller_parent_class)->finalize (object);

  DZL_COUNTER_DEC (instances);
}


static void
photos_offset_controller_init (PhotosOffsetController *self)
{
  PhotosOffsetControllerPrivate *priv;

  DZL_COUNTER_INC (instances);

  priv = photos_offset_controller_get_instance_private (self);

  priv->cancellable = g_cancellable_new ();
  priv->queue = photos_tracker_queue_dup_singleton (NULL, NULL);
}


static void
photos_offset_controller_class_init (PhotosOffsetControllerClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->dispose = photos_offset_controller_dispose;
  object_class->finalize = photos_offset_controller_finalize;

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
  PhotosOffsetControllerPrivate *priv;

  priv = photos_offset_controller_get_instance_private (self);
  return priv->count;
}


gint
photos_offset_controller_get_offset (PhotosOffsetController *self)
{
  PhotosOffsetControllerPrivate *priv;

  priv = photos_offset_controller_get_instance_private (self);
  return priv->offset;
}


gint
photos_offset_controller_get_remaining (PhotosOffsetController *self)
{
  PhotosOffsetControllerPrivate *priv;

  priv = photos_offset_controller_get_instance_private (self);
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
  PhotosOffsetControllerPrivate *priv;
  gint remaining;

  priv = photos_offset_controller_get_instance_private (self);

  remaining = photos_offset_controller_get_remaining (self);
  if (remaining <= 0)
    return;

  priv->offset += OFFSET_STEP;
  g_signal_emit (self, signals[OFFSET_CHANGED], 0, priv->offset);
}


void
photos_offset_controller_reset_count (PhotosOffsetController *self)
{
  PhotosOffsetControllerPrivate *priv;
  PhotosQuery *query = NULL;
  const gchar *type_name;
  gchar *tag = NULL;

  priv = photos_offset_controller_get_instance_private (self);

  if (G_UNLIKELY (priv->queue == NULL))
    goto out;

  query = PHOTOS_OFFSET_CONTROLLER_GET_CLASS (self)->get_query (self);
  g_return_if_fail (query != NULL);

  type_name = G_OBJECT_TYPE_NAME (self);
  tag = g_strdup_printf ("%s: %s", type_name, G_STRFUNC);
  photos_query_set_tag (query, tag);

  photos_tracker_queue_select (priv->queue,
                               query,
                               NULL,
                               photos_offset_controller_reset_count_query_executed,
                               g_object_ref (self),
                               g_object_unref);

 out:
  g_clear_object (&query);
  g_free (tag);
}


void
photos_offset_controller_reset_offset (PhotosOffsetController *self)
{
  PhotosOffsetControllerPrivate *priv;

  priv = photos_offset_controller_get_instance_private (self);
  priv->offset = 0;
}
