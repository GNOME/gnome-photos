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

#include "photos-debug.h"
#include "photos-filterable.h"
#include "photos-source.h"
#include "photos-tracker-queue.h"


struct _PhotosTrackerQueue
{
  GObject parent_instance;
  GError *initialization_error;
  GQueue *queue;
  TrackerSparqlConnection *connection;
  gboolean is_initialized;
  gboolean running;
};

static void photos_tracker_queue_initable_iface_init (GInitableIface *iface);


G_DEFINE_TYPE_WITH_CODE (PhotosTrackerQueue, photos_tracker_queue, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, photos_tracker_queue_initable_iface_init));


typedef enum
{
  PHOTOS_TRACKER_QUERY_SELECT,
  PHOTOS_TRACKER_QUERY_UPDATE,
  PHOTOS_TRACKER_QUERY_UPDATE_BLANK
} PhotosTrackerQueryType;

typedef struct _PhotosTrackerQueueData PhotosTrackerQueueData;

struct _PhotosTrackerQueueData
{
  GAsyncReadyCallback callback;
  GCancellable *cancellable;
  GDestroyNotify destroy_data;
  PhotosQuery *query;
  PhotosTrackerQueryType query_type;
  gpointer user_data;
};


G_LOCK_DEFINE_STATIC (init_lock);


static void photos_tracker_queue_check (PhotosTrackerQueue *self);


static void
photos_tracker_queue_data_free (PhotosTrackerQueueData *data)
{
  g_clear_object (&data->query);
  g_clear_object (&data->cancellable);

  if (data->destroy_data != NULL)
    (*data->destroy_data) (data->user_data);

  g_slice_free (PhotosTrackerQueueData, data);
}


static PhotosTrackerQueueData *
photos_tracker_queue_data_new (PhotosQuery *query,
                               PhotosTrackerQueryType query_type,
                               GCancellable *cancellable,
                               GAsyncReadyCallback callback,
                               gpointer user_data,
                               GDestroyNotify destroy_data)
{
  PhotosTrackerQueueData *data;

  data = g_slice_new0 (PhotosTrackerQueueData);

  data->query = g_object_ref (query);
  data->query_type = query_type;
  data->cancellable = cancellable;
  data->callback = callback;
  data->user_data = user_data;
  data->destroy_data = destroy_data;

  return data;
}


static void
photos_tracker_queue_log_query (PhotosQuery *query)
{
  PhotosSource *source;
  const gchar *sparql;
  const gchar *tag;

  tag = photos_query_get_tag (query);
  if (tag != NULL && tag[0] != '\0')
    photos_debug (PHOTOS_DEBUG_TRACKER, "%s", tag);

  source = photos_query_get_source (query);
  if (source != NULL)
    {
      const gchar *id;
      const gchar *name;

      id = photos_filterable_get_id (PHOTOS_FILTERABLE (source));
      name = photos_source_get_name (source);
      photos_debug (PHOTOS_DEBUG_TRACKER, "Source (%s): %s", id, name);
    }

  sparql = photos_query_get_sparql (query);
  photos_debug (PHOTOS_DEBUG_TRACKER, "%s", sparql);
}


static void
photos_tracker_queue_collector (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosTrackerQueue *self = PHOTOS_TRACKER_QUEUE (user_data);
  PhotosTrackerQueueData *data;

  data = g_queue_pop_head (self->queue);
  if (data->callback != NULL)
    (*data->callback) (source_object, res, data->user_data);
  self->running = FALSE;
  photos_tracker_queue_data_free (data);

  photos_tracker_queue_check (self);
  g_object_unref (self);
}


static void
photos_tracker_queue_check (PhotosTrackerQueue *self)
{
  PhotosTrackerQueueData *data;
  const gchar *sparql;

  if (self->running)
    return;

  if (self->queue->length == 0)
    return;

  data = g_queue_peek_head (self->queue);
  self->running = TRUE;

  photos_tracker_queue_log_query (data->query);
  sparql = photos_query_get_sparql (data->query);

  switch (data->query_type)
    {
    case PHOTOS_TRACKER_QUERY_SELECT:
      tracker_sparql_connection_query_async (self->connection,
                                             sparql,
                                             data->cancellable,
                                             photos_tracker_queue_collector,
                                             g_object_ref (self));
      break;

    case PHOTOS_TRACKER_QUERY_UPDATE:
      tracker_sparql_connection_update_async (self->connection,
                                              sparql,
                                              G_PRIORITY_DEFAULT,
                                              data->cancellable,
                                              photos_tracker_queue_collector,
                                              g_object_ref (self));
      break;

    case PHOTOS_TRACKER_QUERY_UPDATE_BLANK:
      tracker_sparql_connection_update_blank_async (self->connection,
                                                    sparql,
                                                    G_PRIORITY_DEFAULT,
                                                    data->cancellable,
                                                    photos_tracker_queue_collector,
                                                    g_object_ref (self));
      break;

    default:
      g_assert_not_reached ();
      break;
    }
}


static GObject *
photos_tracker_queue_constructor (GType type, guint n_construct_params, GObjectConstructParam *construct_params)
{
  static GObject *self = NULL;

  if (self == NULL)
    {
      self = G_OBJECT_CLASS (photos_tracker_queue_parent_class)->constructor (type,
                                                                              n_construct_params,
                                                                              construct_params);
      g_object_add_weak_pointer (self, (gpointer) &self);
      return self;
    }

  return g_object_ref (self);
}


static void
photos_tracker_queue_dispose (GObject *object)
{
  PhotosTrackerQueue *self = PHOTOS_TRACKER_QUEUE (object);

  g_clear_object (&self->connection);

  G_OBJECT_CLASS (photos_tracker_queue_parent_class)->dispose (object);
}


static void
photos_tracker_queue_finalize (GObject *object)
{
  PhotosTrackerQueue *self = PHOTOS_TRACKER_QUEUE (object);

  g_clear_error (&self->initialization_error);
  g_queue_free (self->queue);

  G_OBJECT_CLASS (photos_tracker_queue_parent_class)->finalize (object);
}


static void
photos_tracker_queue_init (PhotosTrackerQueue *self)
{
  self->queue = g_queue_new ();
}


static void
photos_tracker_queue_class_init (PhotosTrackerQueueClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->constructor = photos_tracker_queue_constructor;
  object_class->dispose = photos_tracker_queue_dispose;
  object_class->finalize = photos_tracker_queue_finalize;
}


static gboolean
photos_tracker_queue_initable_init (GInitable *initable, GCancellable *cancellable, GError **error)
{
  PhotosTrackerQueue *self = PHOTOS_TRACKER_QUEUE (initable);
  gboolean ret_val = FALSE;

  G_LOCK (init_lock);

  if (self->is_initialized)
    {
      if (self->connection != NULL)
        ret_val = TRUE;
      else
        g_assert_nonnull (self->initialization_error);

      goto out;
    }

  g_assert_no_error (self->initialization_error);

  self->connection = tracker_sparql_connection_get (cancellable, &self->initialization_error);
  if (G_UNLIKELY (self->initialization_error != NULL))
    goto out;

  ret_val = TRUE;

 out:
  self->is_initialized = TRUE;
  if (!ret_val)
    {
      g_assert_nonnull (self->initialization_error);
      g_propagate_error (error, g_error_copy (self->initialization_error));
    }

  G_UNLOCK (init_lock);
  return ret_val;
}


static void
photos_tracker_queue_initable_iface_init (GInitableIface *iface)
{
  iface->init = photos_tracker_queue_initable_init;
}


PhotosTrackerQueue *
photos_tracker_queue_dup_singleton (GCancellable *cancellable, GError **error)
{
  g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  return g_initable_new (PHOTOS_TYPE_TRACKER_QUEUE, cancellable, error, NULL);
}


void
photos_tracker_queue_select (PhotosTrackerQueue *self,
                             PhotosQuery *query,
                             GCancellable *cancellable,
                             GAsyncReadyCallback callback,
                             gpointer user_data,
                             GDestroyNotify destroy_data)
{
  PhotosTrackerQueueData *data;

  if (cancellable != NULL)
    g_object_ref (cancellable);

  data = photos_tracker_queue_data_new (query,
                                        PHOTOS_TRACKER_QUERY_SELECT,
                                        cancellable,
                                        callback,
                                        user_data,
                                        destroy_data);

  g_queue_push_tail (self->queue, data);
  photos_tracker_queue_check (self);
}


void
photos_tracker_queue_update (PhotosTrackerQueue *self,
                             PhotosQuery *query,
                             GCancellable *cancellable,
                             GAsyncReadyCallback callback,
                             gpointer user_data,
                             GDestroyNotify destroy_data)
{
  PhotosTrackerQueueData *data;

  if (cancellable != NULL)
    g_object_ref (cancellable);

  data = photos_tracker_queue_data_new (query,
                                        PHOTOS_TRACKER_QUERY_UPDATE,
                                        cancellable,
                                        callback,
                                        user_data,
                                        destroy_data);

  g_queue_push_tail (self->queue, data);
  photos_tracker_queue_check (self);
}


void
photos_tracker_queue_update_blank (PhotosTrackerQueue *self,
                                   PhotosQuery *query,
                                   GCancellable *cancellable,
                                   GAsyncReadyCallback callback,
                                   gpointer user_data,
                                   GDestroyNotify destroy_data)
{
  PhotosTrackerQueueData *data;

  if (cancellable != NULL)
    g_object_ref (cancellable);

  data = photos_tracker_queue_data_new (query,
                                        PHOTOS_TRACKER_QUERY_UPDATE_BLANK,
                                        cancellable,
                                        callback,
                                        user_data,
                                        destroy_data);

  g_queue_push_tail (self->queue, data);
  photos_tracker_queue_check (self);
}
