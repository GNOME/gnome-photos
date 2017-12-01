/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2012 – 2017 Red Hat, Inc.
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

#include "photos-tracker-change-event.h"
#include "photos-tracker-change-monitor.h"
#include "photos-tracker-queue.h"
#include "photos-tracker-resources.h"
#include "photos-query.h"


struct _PhotosTrackerChangeMonitor
{
  GObject parent_instance;
  GHashTable *pending_changes;
  GHashTable *unresolved_ids;
  GQueue *pending_events;
  PhotosTrackerQueue *queue;
  TrackerResources *resource_service;
  guint outstanding_ops;
  guint pending_events_id;
};

enum
{
  CHANGES_PENDING,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static void photos_tracker_change_monitor_initable_iface_init (GInitableIface *iface);


G_DEFINE_TYPE_EXTENDED (PhotosTrackerChangeMonitor, photos_tracker_change_monitor, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, photos_tracker_change_monitor_initable_iface_init));


enum
{
  CHANGE_MONITOR_TIMEOUT = 500, /* ms */
  CHANGE_MONITOR_MAX_ITEMS = 500
};


typedef struct _PhotosTrackerChangeMonitorQueryData PhotosTrackerChangeMonitorQueryData;
typedef struct _TrackerResourcesEvent TrackerResourcesEvent;

struct _PhotosTrackerChangeMonitorQueryData
{
  PhotosTrackerChangeMonitor *self;
  GHashTable *id_table;
  GQueue *events;
};

struct _TrackerResourcesEvent
{
  gint32 graph;
  gint32 subject;
  gint32 predicate;
  gint32 object;
};


static void
photos_tracker_change_monitor_query_data_free (PhotosTrackerChangeMonitorQueryData *data)
{
  g_clear_object (&data->self);

  if (data->id_table != NULL)
    g_hash_table_unref (data->id_table);

  if (data->events != NULL)
    g_queue_free_full (data->events, (GDestroyNotify) photos_tracker_change_event_free);

  g_slice_free (PhotosTrackerChangeMonitorQueryData, data);
}


static PhotosTrackerChangeMonitorQueryData *
photos_tracker_change_monitor_query_data_new (PhotosTrackerChangeMonitor *self,
                                              GHashTable *id_table,
                                              GQueue *events)
{
  PhotosTrackerChangeMonitorQueryData *data;

  data = g_slice_new0 (PhotosTrackerChangeMonitorQueryData);
  data->self = g_object_ref (self);
  data->id_table = id_table;
  data->events = events;

  return data;
}


static void
photos_tracker_change_monitor_add_event (PhotosTrackerChangeMonitor *self, PhotosTrackerChangeEvent *change_event)
{
  PhotosTrackerChangeEvent *old_change_event;
  const gchar *urn;

  urn = photos_tracker_change_event_get_urn (change_event);
  old_change_event = (PhotosTrackerChangeEvent *) g_hash_table_lookup (self->pending_changes, urn);

  if (old_change_event != NULL)
    photos_tracker_change_event_merge (old_change_event, change_event);
  else
    g_hash_table_insert (self->pending_changes, g_strdup (urn), photos_tracker_change_event_copy (change_event));
}


static void
photos_tracker_change_monitor_remove_timeout (PhotosTrackerChangeMonitor *self)
{
  if (self->pending_events_id != 0)
    {
      g_source_remove (self->pending_events_id);
      self->pending_events_id = 0;
    }
}


static void
photos_tracker_change_monitor_send_events (PhotosTrackerChangeMonitor *self, GHashTable *id_table, GQueue *events)
{
  GList *l;

  for (l = events->head; l != NULL; l = l->next)
    {
      PhotosTrackerChangeEvent *change_event = (PhotosTrackerChangeEvent *) l->data;
      const gchar *predicate;
      const gchar *urn;
      gint32 predicate_id;
      gint32 urn_id;

      predicate_id = photos_tracker_change_event_get_predicate_id (change_event);
      urn_id = photos_tracker_change_event_get_urn_id (change_event);

      predicate = (gchar *) g_hash_table_lookup (id_table, GINT_TO_POINTER (predicate_id));
      if (G_UNLIKELY (predicate == NULL))
        continue;

      urn = (gchar *) g_hash_table_lookup (id_table, GINT_TO_POINTER (urn_id));
      if (G_UNLIKELY (urn == NULL))
        continue;

      photos_tracker_change_event_set_resolved_values (change_event, urn, predicate);
      photos_tracker_change_monitor_add_event (self, change_event);
    }

  g_signal_emit (self, signals[CHANGES_PENDING], 0, self->pending_changes);
  g_hash_table_remove_all (self->pending_changes);
}


static void
photos_tracker_change_monitor_cursor_next (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosTrackerChangeMonitorQueryData *data = (PhotosTrackerChangeMonitorQueryData *) user_data;
  PhotosTrackerChangeMonitor *self = data->self;
  TrackerSparqlCursor *cursor = TRACKER_SPARQL_CURSOR (source_object);
  GError *error;
  GHashTableIter iter;
  gboolean valid;

  error = NULL;
  valid = tracker_sparql_cursor_next_finish (cursor, res, &error);
  if (error != NULL)
    {
      g_warning ("Unable to resolve item URNs for graph changes: %s", error->message);
      g_error_free (error);
    }

  if (valid)
    {
      guint idx;

      idx = 0;
      g_hash_table_iter_init (&iter, data->id_table);
      while (g_hash_table_iter_next (&iter, NULL, NULL))
        {
          const gchar *str;

          str = tracker_sparql_cursor_get_string (cursor, idx, NULL);
          g_hash_table_iter_replace (&iter, g_strdup (str));
          idx++;
        }

      photos_tracker_change_monitor_send_events (self, data->id_table, data->events);
    }

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
      g_warning ("Unable to resolve item URNs for graph changes: %s", error->message);
      g_error_free (error);
      photos_tracker_change_monitor_query_data_free (data);
      return;
    }

  tracker_sparql_cursor_next_async (cursor, NULL, photos_tracker_change_monitor_cursor_next, data);
  g_object_unref (cursor);
}


static gboolean
photos_tracker_change_monitor_process_events (PhotosTrackerChangeMonitor *self)
{
  GHashTable *id_table;
  GHashTableIter iter;
  GQueue *events;
  GString *sparql;
  PhotosTrackerChangeMonitorQueryData *data;
  PhotosQuery *query = NULL;
  gpointer id;

  events = self->pending_events;
  self->pending_events = g_queue_new ();

  id_table = self->unresolved_ids;
  self->unresolved_ids = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, g_free);

  self->pending_events_id = 0;

  sparql = g_string_new ("SELECT");

  g_hash_table_iter_init (&iter, id_table);
  while (g_hash_table_iter_next (&iter, &id, NULL))
    g_string_append_printf (sparql, " tracker:uri(%d)", GPOINTER_TO_INT (id));

  g_string_append (sparql, " {}");

  query = photos_query_new (NULL, sparql->str);

  data = photos_tracker_change_monitor_query_data_new (self, id_table, events);
  photos_tracker_queue_select (self->queue,
                               query,
                               NULL,
                               photos_tracker_change_monitor_query_executed,
                               data,
                               NULL);

  g_string_free (sparql, TRUE);
  g_object_unref (query);
  return G_SOURCE_REMOVE;
}


static void
photos_tracker_change_monitor_add_pending_event (PhotosTrackerChangeMonitor *self,
                                                 const TrackerResourcesEvent *event,
                                                 gboolean is_delete)
{
  PhotosTrackerChangeEvent *change_event;

  photos_tracker_change_monitor_remove_timeout (self);

  g_hash_table_insert (self->unresolved_ids, GINT_TO_POINTER (event->subject), NULL);
  g_hash_table_insert (self->unresolved_ids, GINT_TO_POINTER (event->predicate), NULL);

  change_event = photos_tracker_change_event_new (event->subject, event->predicate, is_delete);
  g_queue_push_tail (self->pending_events, change_event);

  if (self->pending_events->length >= CHANGE_MONITOR_MAX_ITEMS)
    photos_tracker_change_monitor_process_events (self);
  else
    self->pending_events_id = g_timeout_add (CHANGE_MONITOR_TIMEOUT,
                                             (GSourceFunc) photos_tracker_change_monitor_process_events,
                                             self);
}


static void
photos_tracker_change_monitor_graph_updated (TrackerResources *resource_service,
                                             const gchar *class_name,
                                             GVariant *delete_events,
                                             GVariant *insert_events,
                                             gpointer user_data)
{
  PhotosTrackerChangeMonitor *self = PHOTOS_TRACKER_CHANGE_MONITOR (user_data);
  const TrackerResourcesEvent *events;
  gsize i;
  gsize n_elements;

  events = (const TrackerResourcesEvent *) g_variant_get_fixed_array (delete_events,
                                                                      &n_elements,
                                                                      sizeof (TrackerResourcesEvent));
  for (i = 0; i < n_elements; i++)
    photos_tracker_change_monitor_add_pending_event (self, &events[i], TRUE);

  events = (const TrackerResourcesEvent *) g_variant_get_fixed_array (insert_events,
                                                                      &n_elements,
                                                                      sizeof (TrackerResourcesEvent));
  for (i = 0; i < n_elements; i++)
    photos_tracker_change_monitor_add_pending_event (self, &events[i], FALSE);
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

  photos_tracker_change_monitor_remove_timeout (self);

  g_clear_object (&self->queue);
  g_clear_object (&self->resource_service);

  G_OBJECT_CLASS (photos_tracker_change_monitor_parent_class)->dispose (object);
}


static void
photos_tracker_change_monitor_finalize (GObject *object)
{
  PhotosTrackerChangeMonitor *self = PHOTOS_TRACKER_CHANGE_MONITOR (object);

  g_hash_table_unref (self->pending_changes);
  g_hash_table_unref (self->unresolved_ids);

  g_queue_free_full (self->pending_events, (GDestroyNotify) photos_tracker_change_event_free);

  G_OBJECT_CLASS (photos_tracker_change_monitor_parent_class)->finalize (object);
}


static void
photos_tracker_change_monitor_init (PhotosTrackerChangeMonitor *self)
{
  self->pending_changes = g_hash_table_new_full (g_str_hash,
                                                 g_str_equal,
                                                 g_free,
                                                 (GDestroyNotify) photos_tracker_change_event_free);

  self->unresolved_ids = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, g_free);

  self->pending_events = g_queue_new ();
}


static void
photos_tracker_change_monitor_class_init (PhotosTrackerChangeMonitorClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->constructor = photos_tracker_change_monitor_constructor;
  object_class->dispose = photos_tracker_change_monitor_dispose;
  object_class->finalize = photos_tracker_change_monitor_finalize;

  signals[CHANGES_PENDING] = g_signal_new ("changes-pending",
                                           G_TYPE_FROM_CLASS (class),
                                           G_SIGNAL_RUN_LAST,
                                           0,
                                           NULL, /*accumulator */
                                           NULL, /*accu_data */
                                           g_cclosure_marshal_VOID__BOXED,
                                           G_TYPE_NONE,
                                           1,
                                           G_TYPE_HASH_TABLE);
}


static gboolean
photos_tracker_change_monitor_initable_init (GInitable *initable, GCancellable *cancellable, GError **error)
{
  PhotosTrackerChangeMonitor *self = PHOTOS_TRACKER_CHANGE_MONITOR (initable);
  gboolean ret_val = TRUE;

  if (G_LIKELY (self->queue != NULL && self->resource_service != NULL))
    goto out;

  self->queue = photos_tracker_queue_dup_singleton (cancellable, error);
  if (G_UNLIKELY (self->queue == NULL))
    {
      ret_val = FALSE;
      goto out;
    }

  self->resource_service = tracker_resources_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                                                     G_DBUS_PROXY_FLAGS_NONE,
                                                                     "org.freedesktop.Tracker1",
                                                                     "/org/freedesktop/Tracker1/Resources",
                                                                     cancellable,
                                                                     error);
  if (G_UNLIKELY (self->resource_service == NULL))
    {
      ret_val = FALSE;
      goto out;
    }

  g_signal_connect (self->resource_service,
                    "graph-updated",
                    G_CALLBACK (photos_tracker_change_monitor_graph_updated),
                    self);

 out:
  return ret_val;
}


static void
photos_tracker_change_monitor_initable_iface_init (GInitableIface *iface)
{
  iface->init = photos_tracker_change_monitor_initable_init;
}


PhotosTrackerChangeMonitor *
photos_tracker_change_monitor_dup_singleton (GCancellable *cancellable, GError **error)
{
  return g_initable_new (PHOTOS_TYPE_TRACKER_CHANGE_MONITOR, cancellable, error, NULL);
}
