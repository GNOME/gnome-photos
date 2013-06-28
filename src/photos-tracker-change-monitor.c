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
#include <glib/gi18n.h>
#include <tracker-sparql.h>

#include "photos-tracker-change-event.h"
#include "photos-tracker-change-monitor.h"
#include "photos-tracker-queue.h"
#include "photos-tracker-resources.h"


struct _PhotosTrackerChangeMonitorPrivate
{
  GHashTable *pending;
  PhotosTrackerQueue *queue;
  TrackerResources *resource_service;
  guint outstanding_ops;
};

enum
{
  CHANGES_PENDING,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };


G_DEFINE_TYPE_WITH_PRIVATE (PhotosTrackerChangeMonitor, photos_tracker_change_monitor, G_TYPE_OBJECT);


typedef struct _PhotosTrackerChangeMonitorQueryData PhotosTrackerChangeMonitorQueryData;
typedef struct _TrackerResourcesEvent TrackerResourcesEvent;

struct _PhotosTrackerChangeMonitorQueryData
{
  PhotosTrackerChangeMonitor *self;
  gboolean is_delete;
};

struct _TrackerResourcesEvent
{
  gint32 first;
  gint32 second;
  gint32 third;
  gint32 fourth;
} __attribute__ ((packed));


static PhotosTrackerChangeMonitorQueryData *
photos_tracker_change_monitor_query_data_copy (PhotosTrackerChangeMonitorQueryData *data)
{
  PhotosTrackerChangeMonitorQueryData *copy;

  copy = g_slice_new0 (PhotosTrackerChangeMonitorQueryData);
  copy->self = g_object_ref (data->self);
  copy->is_delete = data->is_delete;

  return copy;
}


static void
photos_tracker_change_monitor_query_data_free (PhotosTrackerChangeMonitorQueryData *data)
{
  g_clear_object (&data->self);
  g_slice_free (PhotosTrackerChangeMonitorQueryData, data);
}


static PhotosTrackerChangeMonitorQueryData *
photos_tracker_change_monitor_query_data_new (PhotosTrackerChangeMonitor *self, gboolean is_delete)
{
  PhotosTrackerChangeMonitorQueryData *data;

  data = g_slice_new0 (PhotosTrackerChangeMonitorQueryData);
  data->self = g_object_ref (self);
  data->is_delete = is_delete;

  return data;
}


static void
photos_tracker_change_monitor_add_event (PhotosTrackerChangeMonitor *self,
                                         const gchar *subject,
                                         const gchar *predicate,
                                         gboolean is_delete)
{
  PhotosTrackerChangeMonitorPrivate *priv = self->priv;
  PhotosTrackerChangeEvent *event;
  PhotosTrackerChangeEvent *old_event;

  event = photos_tracker_change_event_new (subject, predicate, is_delete);
  old_event = (PhotosTrackerChangeEvent *) g_hash_table_lookup (priv->pending, subject);

  if (old_event != NULL)
    {
      photos_tracker_change_event_merge (old_event, event);
      photos_tracker_change_event_free (event);
    }
  else
    g_hash_table_insert (priv->pending, (gpointer) g_strdup (subject), (gpointer) event);
}


static void
photos_tracker_change_monitor_update_collector (PhotosTrackerChangeMonitor *self)
{
  PhotosTrackerChangeMonitorPrivate *priv = self->priv;

  priv->outstanding_ops--;
  if (priv->outstanding_ops != 0)
    return;

  g_signal_emit (self, signals[CHANGES_PENDING], 0, priv->pending);
  g_hash_table_remove_all (priv->pending);
}


static void
photos_tracker_change_monitor_cursor_next (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosTrackerChangeMonitorQueryData *data = (PhotosTrackerChangeMonitorQueryData *) user_data;
  TrackerSparqlCursor *cursor = TRACKER_SPARQL_CURSOR (source_object);
  gboolean valid;

  valid = tracker_sparql_cursor_next_finish (cursor, res, NULL);
  if (valid)
    {
      const gchar *predicate;
      const gchar *subject;

      subject = tracker_sparql_cursor_get_string (cursor, 0, NULL);
      predicate = tracker_sparql_cursor_get_string (cursor, 1, NULL);
      if (subject != NULL && predicate != NULL)
        photos_tracker_change_monitor_add_event (data->self, subject, predicate, data->is_delete);
    }

  photos_tracker_change_monitor_update_collector (data->self);

  tracker_sparql_cursor_close (cursor);
  photos_tracker_change_monitor_query_data_free (data);
}


static void
photos_tracker_change_monitor_query_executed (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosTrackerChangeMonitorQueryData *data = (PhotosTrackerChangeMonitorQueryData *) user_data;
  TrackerSparqlConnection *connection = TRACKER_SPARQL_CONNECTION (source_object);
  GError *error;
  TrackerSparqlCursor *cursor;

  error = NULL;
  cursor = tracker_sparql_connection_query_finish (connection, res, &error);
  if (error != NULL)
    {
      g_error_free (error);
      return;
    }

  tracker_sparql_cursor_next_async (cursor,
                                    NULL,
                                    photos_tracker_change_monitor_cursor_next,
                                    photos_tracker_change_monitor_query_data_copy (data));
  g_object_unref (cursor);
}


static void
photos_tracker_change_monitor_update_iterator (PhotosTrackerChangeMonitor *self,
                                               const TrackerResourcesEvent *event,
                                               gboolean is_delete)
{
  PhotosTrackerChangeMonitorQueryData *data;
  gchar *sparql;

  sparql = g_strdup_printf ("SELECT tracker:uri(%" G_GINT32_FORMAT ") tracker:uri(%" G_GINT32_FORMAT ") {}",
                            event->second,
                            event->third);

  data = photos_tracker_change_monitor_query_data_new (self, is_delete);

  photos_tracker_queue_select (self->priv->queue,
                               sparql,
                               NULL,
                               photos_tracker_change_monitor_query_executed,
                               data,
                               (GDestroyNotify) photos_tracker_change_monitor_query_data_free);
  g_free (sparql);
}


static void
photos_tracker_change_monitor_graph_updated (TrackerResources *resource_service,
                                             const gchar *class_name,
                                             GVariant *delete_events,
                                             GVariant *insert_events,
                                             gpointer user_data)
{
  PhotosTrackerChangeMonitor *self = PHOTOS_TRACKER_CHANGE_MONITOR (user_data);
  PhotosTrackerChangeMonitorPrivate *priv = self->priv;
  const TrackerResourcesEvent *events;
  gsize i;
  gsize n_elements;

  events = (const TrackerResourcesEvent *) g_variant_get_fixed_array (delete_events,
                                                                      &n_elements,
                                                                      sizeof (TrackerResourcesEvent));
  for (i = 0; i < n_elements; i++)
    {
      priv->outstanding_ops++;
      photos_tracker_change_monitor_update_iterator (self, &events[i], TRUE);
    }

  events = (const TrackerResourcesEvent *) g_variant_get_fixed_array (insert_events,
                                                                      &n_elements,
                                                                      sizeof (TrackerResourcesEvent));
  for (i = 0; i < n_elements; i++)
    {
      priv->outstanding_ops++;
      photos_tracker_change_monitor_update_iterator (self, &events[i], FALSE);
    }
}


static GObject *
photos_tracker_change_monitor_constructor (GType type,
                                           guint n_construct_params,
                                           GObjectConstructParam *construct_params)
{
  static GObject *self = NULL;

  if (self == NULL)
    {
      self = G_OBJECT_CLASS (photos_tracker_change_monitor_parent_class)->constructor (type,
                                                                                       n_construct_params,
                                                                                       construct_params);
      g_object_add_weak_pointer (self, (gpointer) &self);
      return self;
    }

  return g_object_ref (self);
}


static void
photos_tracker_change_monitor_dispose (GObject *object)
{
  PhotosTrackerChangeMonitor *self = PHOTOS_TRACKER_CHANGE_MONITOR (object);
  PhotosTrackerChangeMonitorPrivate *priv = self->priv;

  if (priv->pending != NULL)
    {
      g_hash_table_unref (priv->pending);
      priv->pending = NULL;
    }

  g_clear_object (&priv->queue);
  g_clear_object (&priv->resource_service);

  G_OBJECT_CLASS (photos_tracker_change_monitor_parent_class)->dispose (object);
}


static void
photos_tracker_change_monitor_init (PhotosTrackerChangeMonitor *self)
{
  PhotosTrackerChangeMonitorPrivate *priv;

  self->priv = photos_tracker_change_monitor_get_instance_private (self);
  priv = self->priv;

  priv->pending = g_hash_table_new_full (g_str_hash,
                                         g_str_equal,
                                         g_free,
                                         (GDestroyNotify) photos_tracker_change_event_free);

  priv->queue = photos_tracker_queue_dup_singleton ();
  priv->resource_service = tracker_resources_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                                                     G_DBUS_PROXY_FLAGS_NONE,
                                                                     "org.freedesktop.Tracker1",
                                                                     "/org/freedesktop/Tracker1/Resources",
                                                                     NULL,
                                                                     NULL);
  g_signal_connect (priv->resource_service,
                    "graph-updated",
                    G_CALLBACK (photos_tracker_change_monitor_graph_updated),
                    self);
}


static void
photos_tracker_change_monitor_class_init (PhotosTrackerChangeMonitorClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->constructor = photos_tracker_change_monitor_constructor;
  object_class->dispose = photos_tracker_change_monitor_dispose;

  signals[CHANGES_PENDING] = g_signal_new ("changes-pending",
                                           G_TYPE_FROM_CLASS (class),
                                           G_SIGNAL_RUN_LAST,
                                           G_STRUCT_OFFSET (PhotosTrackerChangeMonitorClass, changes_pending),
                                           NULL, /*accumulator */
                                           NULL, /*accu_data */
                                           g_cclosure_marshal_VOID__BOXED,
                                           G_TYPE_NONE,
                                           1,
                                           G_TYPE_HASH_TABLE);
}


PhotosTrackerChangeMonitor *
photos_tracker_change_monitor_dup_singleton (void)
{
  return g_object_new (PHOTOS_TYPE_TRACKER_CHANGE_MONITOR, NULL);
}
