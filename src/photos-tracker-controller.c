/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2012, 2013 Red Hat, Inc.
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
#include <glib/gi18n.h>

#include "photos-collection-manager.h"
#include "photos-item-manager.h"
#include "photos-marshalers.h"
#include "photos-query-builder.h"
#include "photos-source-manager.h"
#include "photos-tracker-controller.h"
#include "photos-tracker-queue.h"


struct _PhotosTrackerControllerPrivate
{
  GCancellable *cancellable;
  PhotosBaseManager *col_mngr;
  PhotosBaseManager *item_mngr;
  PhotosBaseManager *src_mngr;
  PhotosOffsetController *offset_cntrlr;
  PhotosQuery *current_query;
  PhotosTrackerQueue *queue;
  gboolean is_started;
  gboolean query_queued;
  gboolean querying;
  gint query_queued_flags;
  gint64 last_query_time;
};

enum
{
  QUERY_ERROR,
  QUERY_STATUS_CHANGED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };


G_DEFINE_ABSTRACT_TYPE (PhotosTrackerController, photos_tracker_controller, G_TYPE_OBJECT);


typedef enum
{
  PHOTOS_TRACKER_REFRESH_FLAGS_NONE = 0,
  PHOTOS_TRACKER_REFRESH_FLAGS_RESET_OFFSET = 1 << 0
} PhotosTrackerRefreshFlags;

static void photos_tracker_controller_refresh_internal (PhotosTrackerController *self, gint flags);
static void photos_tracker_controller_set_query_status (PhotosTrackerController *self, gboolean query_status);


static void
photos_tracker_controller_query_error (PhotosTrackerController *self, GError *error)
{
  const gchar *primary = _("Unable to fetch the list of photos");

  if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    return;

  g_signal_emit (self, signals[QUERY_ERROR], 0, primary, error->message);
}


static void
photos_tracker_controller_query_finished (PhotosTrackerController *self, GError *error)
{
  PhotosTrackerControllerPrivate *priv = self->priv;

  photos_tracker_controller_set_query_status (self, FALSE);

  if (error != NULL)
    photos_tracker_controller_query_error (self, error);
  else
    photos_offset_controller_reset_count (priv->offset_cntrlr);

  if (priv->query_queued)
    {
      priv->query_queued = FALSE;
      photos_tracker_controller_refresh_internal (self, priv->query_queued_flags);
    }
}


static void
photos_tracker_controller_cursor_next (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosTrackerController *self = PHOTOS_TRACKER_CONTROLLER (user_data);
  PhotosTrackerControllerPrivate *priv = self->priv;
  TrackerSparqlCursor *cursor = TRACKER_SPARQL_CURSOR (source_object);
  gboolean valid;
  gint64 now;

  valid = tracker_sparql_cursor_next_finish (cursor, res, NULL); /* TODO: use GError */
  if (!valid)
    {
      tracker_sparql_cursor_close (cursor);
      photos_tracker_controller_query_finished (self, NULL);
      g_object_unref (self);
      return;
    }

  now = g_get_monotonic_time ();
  g_debug ("Query Cursor: %" G_GINT64_FORMAT, (now - priv->last_query_time) / 1000000);

  photos_item_manager_add_item (PHOTOS_ITEM_MANAGER (priv->item_mngr), cursor);
  tracker_sparql_cursor_next_async (cursor,
                                    priv->cancellable,
                                    photos_tracker_controller_cursor_next,
                                    self);
}


static void
photos_tracker_controller_query_executed (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosTrackerController *self = PHOTOS_TRACKER_CONTROLLER (user_data);
  TrackerSparqlConnection *connection = TRACKER_SPARQL_CONNECTION (source_object);
  GError *error;
  TrackerSparqlCursor *cursor;

  error = NULL;
  cursor = tracker_sparql_connection_query_finish (connection, res, &error);
  if (error != NULL)
    {
      photos_tracker_controller_query_finished (self, error);
      g_error_free (error);
      return;
    }

  tracker_sparql_cursor_next_async (cursor,
                                    self->priv->cancellable,
                                    photos_tracker_controller_cursor_next,
                                    g_object_ref (self));
  g_object_unref (cursor);
}


static void
photos_tracker_controller_perform_current_query (PhotosTrackerController *self)
{
  PhotosTrackerControllerPrivate *priv = self->priv;

  if (priv->current_query != NULL)
    photos_query_free (priv->current_query);

  priv->current_query = PHOTOS_TRACKER_CONTROLLER_GET_CLASS (self)->get_query ();
  g_cancellable_reset (priv->cancellable);

  photos_tracker_queue_select (priv->queue,
                               priv->current_query->sparql,
                               priv->cancellable,
                               photos_tracker_controller_query_executed,
                               g_object_ref (self),
                               g_object_unref);
}


static void
photos_tracker_controller_offset_changed (PhotosOffsetController *offset_cntrlr, gint offset, gpointer user_data)
{
  PhotosTrackerController *self = PHOTOS_TRACKER_CONTROLLER (user_data);
  photos_tracker_controller_perform_current_query (self);
}


static void
photos_tracker_controller_refresh_for_object (PhotosBaseManager *manager, GObject *object, gpointer user_data)
{
}


static void
photos_tracker_controller_set_query_status (PhotosTrackerController *self, gboolean query_status)
{
  PhotosTrackerControllerPrivate *priv = self->priv;
  gint64 now;

  if (priv->querying == query_status)
    return;

  now = g_get_monotonic_time ();
  if (query_status)
    priv->last_query_time = now;
  else
    {
      g_debug ("Query Elapsed: %" G_GINT64_FORMAT, (now - priv->last_query_time) / 1000000);
      priv->last_query_time = 0;
    }

  priv->querying = query_status;
  g_signal_emit (self, signals[QUERY_STATUS_CHANGED], 0, priv->querying);
}


static void
photos_tracker_controller_refresh_internal (PhotosTrackerController *self, gint flags)
{
  PhotosTrackerControllerPrivate *priv = self->priv;

  priv->is_started = TRUE;

  if (flags & PHOTOS_TRACKER_REFRESH_FLAGS_RESET_OFFSET)
    photos_offset_controller_reset_offset (priv->offset_cntrlr);

  if (photos_tracker_controller_get_query_status (self))
    {
      g_cancellable_cancel (priv->cancellable);
      priv->query_queued = TRUE;
      priv->query_queued_flags = flags;
      return;
    }

  photos_tracker_controller_set_query_status (self, TRUE);
  photos_base_manager_clear (priv->item_mngr);
  photos_tracker_controller_perform_current_query (self);
}


static void
photos_tracker_controller_source_object_added (PhotosBaseManager *manager, GObject *object, gpointer user_data)
{
}


static void
photos_tracker_controller_source_object_removed (PhotosBaseManager *manager, GObject *object, gpointer user_data)
{
}


static void
photos_tracker_controller_constructed (GObject *object)
{
  PhotosTrackerController *self = PHOTOS_TRACKER_CONTROLLER (object);
  PhotosTrackerControllerPrivate *priv = self->priv;

  G_OBJECT_CLASS (photos_tracker_controller_parent_class)->constructed (object);

  priv->offset_cntrlr = PHOTOS_TRACKER_CONTROLLER_GET_CLASS (self)->get_offset_controller ();
  g_signal_connect (priv->offset_cntrlr,
                    "offset-changed",
                    G_CALLBACK (photos_tracker_controller_offset_changed),
                    self);
}


static void
photos_tracker_controller_dispose (GObject *object)
{
  PhotosTrackerController *self = PHOTOS_TRACKER_CONTROLLER (object);
  PhotosTrackerControllerPrivate *priv = self->priv;

  g_clear_object (&priv->col_mngr);
  g_clear_object (&priv->item_mngr);
  g_clear_object (&priv->src_mngr);
  g_clear_object (&priv->offset_cntrlr);
  g_clear_object (&priv->queue);

  G_OBJECT_CLASS (photos_tracker_controller_parent_class)->dispose (object);
}


static void
photos_tracker_controller_finalize (GObject *object)
{
  PhotosTrackerController *self = PHOTOS_TRACKER_CONTROLLER (object);
  PhotosTrackerControllerPrivate *priv = self->priv;

  if (priv->current_query != NULL)
    photos_query_free (priv->current_query);

  G_OBJECT_CLASS (photos_tracker_controller_parent_class)->finalize (object);
}


static void
photos_tracker_controller_init (PhotosTrackerController *self)
{
  PhotosTrackerControllerPrivate *priv;

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, PHOTOS_TYPE_TRACKER_CONTROLLER, PhotosTrackerControllerPrivate);
  priv = self->priv;

  priv->cancellable = g_cancellable_new ();
  priv->item_mngr = photos_item_manager_new ();

  priv->col_mngr = photos_collection_manager_new ();
  g_signal_connect (priv->col_mngr,
                    "active-changed",
                    G_CALLBACK (photos_tracker_controller_refresh_for_object),
                    self);

  priv->src_mngr = photos_source_manager_new ();
  g_signal_connect (priv->src_mngr,
                    "object-added",
                    G_CALLBACK (photos_tracker_controller_source_object_added),
                    self);
  g_signal_connect (priv->src_mngr,
                    "object-removed",
                    G_CALLBACK (photos_tracker_controller_source_object_removed),
                    self);
  g_signal_connect (priv->src_mngr,
                    "active-changed",
                    G_CALLBACK (photos_tracker_controller_refresh_for_object),
                    self);

  priv->queue = photos_tracker_queue_new ();
}


static void
photos_tracker_controller_class_init (PhotosTrackerControllerClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->constructed = photos_tracker_controller_constructed;
  object_class->dispose = photos_tracker_controller_dispose;
  object_class->finalize = photos_tracker_controller_finalize;

  signals[QUERY_ERROR] = g_signal_new ("query-error",
                                       G_TYPE_FROM_CLASS (class),
                                       G_SIGNAL_RUN_LAST,
                                       G_STRUCT_OFFSET (PhotosTrackerControllerClass,
                                                        query_error),
                                       NULL, /*accumulator */
                                       NULL, /*accu_data */
                                       _photos_marshal_VOID__STRING_STRING,
                                       G_TYPE_NONE,
                                       2,
                                       G_TYPE_STRING,
                                       G_TYPE_STRING);

  signals[QUERY_STATUS_CHANGED] = g_signal_new ("query-status-changed",
                                                G_TYPE_FROM_CLASS (class),
                                                G_SIGNAL_RUN_LAST,
                                                G_STRUCT_OFFSET (PhotosTrackerControllerClass,
                                                                 query_status_changed),
                                                NULL, /*accumulator */
                                                NULL, /*accu_data */
                                                g_cclosure_marshal_VOID__BOOLEAN,
                                                G_TYPE_NONE,
                                                1,
                                                G_TYPE_BOOLEAN);

  g_type_class_add_private (class, sizeof (PhotosTrackerControllerPrivate));
}


void
photos_tracker_controller_start (PhotosTrackerController *self)
{
  PhotosTrackerControllerPrivate *priv = self->priv;

  if (priv->is_started)
    return;

  photos_tracker_controller_refresh_internal (self, PHOTOS_TRACKER_REFRESH_FLAGS_NONE);
}


gboolean
photos_tracker_controller_get_query_status (PhotosTrackerController *self)
{
  return self->priv->querying;
}
