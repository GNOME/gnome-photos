/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2013 – 2021 Red Hat, Inc.
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

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <glib.h>
#include <tracker-sparql.h>

#include "photos-collection-icon-watcher.h"
#include "photos-filterable.h"
#include "photos-item-manager.h"
#include "photos-query.h"
#include "photos-query-builder.h"
#include "photos-search-context.h"
#include "photos-single-item-job.h"
#include "photos-tracker-queue.h"
#include "photos-utils.h"


struct _PhotosCollectionIconWatcher
{
  GObject parent_instance;
  GCancellable *cancellable;
  GHashTable *item_connections;
  GList *items;
  GList *urns;
  PhotosBaseItem *collection;
  PhotosBaseManager *item_mngr;
  PhotosTrackerQueue *queue;
  guint to_query_remaining;
};

enum
{
  PROP_0,
  PROP_COLLECTION
};

enum
{
  ICON_UPDATED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };


G_DEFINE_TYPE (PhotosCollectionIconWatcher, photos_collection_icon_watcher, G_TYPE_OBJECT);


static void
photos_collection_icon_watcher_create_collection_icon (PhotosCollectionIconWatcher *self)
{
  g_autoptr (GIcon) icon = NULL;
  g_autolist (GdkPixbuf) icons = NULL;
  GList *l;
  gint size;

  for (l = self->items; l != NULL; l = l->next)
    {
      GdkPixbuf *original_icon;
      PhotosBaseItem *item = PHOTOS_BASE_ITEM (l->data);

      original_icon = photos_base_item_get_original_icon (item);
      if (original_icon != NULL)
        icons = g_list_prepend (icons, g_object_ref (original_icon));
    }
  icons = g_list_reverse (icons);

  size = photos_utils_get_icon_size ();
  icon = photos_utils_create_collection_icon (size, icons);

  if (self->collection != NULL)
    g_signal_emit (self, signals[ICON_UPDATED], 0, icon);
}


static void
photos_collection_icon_watcher_all_items_ready (PhotosCollectionIconWatcher *self)
{
  GList *l;

  for (l = self->items; l != NULL; l = l->next)
    {
      PhotosBaseItem *item = PHOTOS_BASE_ITEM (l->data);
      gulong update_id;

      update_id = g_signal_connect_swapped (item,
                                            "info-updated",
                                            G_CALLBACK (photos_collection_icon_watcher_create_collection_icon),
                                            self);
      g_hash_table_insert (self->item_connections, GUINT_TO_POINTER ((guint) update_id), g_object_ref (item));
    }

  photos_collection_icon_watcher_create_collection_icon (self);
}


static void
photos_collection_icon_watcher_clear (PhotosCollectionIconWatcher *self)
{
  g_hash_table_remove_all (self->item_connections);

  g_list_free_full (self->urns, g_free);
  self->urns = NULL;

  g_list_free_full (self->items, g_object_unref);
  self->items = NULL;
}


static void
photos_collection_icon_watcher_destroy (PhotosCollectionIconWatcher *self)
{
  GHashTableIter iter;
  PhotosBaseItem *item;
  gpointer key;

  g_hash_table_iter_init (&iter, self->item_connections);
  while (g_hash_table_iter_next (&iter, (gpointer *) &key, (gpointer *) &item))
    {
      guint id = GPOINTER_TO_UINT (key);
      g_signal_handler_disconnect (item, id);
    }
}


static void
photos_collection_icon_watcher_to_query_collector (PhotosCollectionIconWatcher *self)
{
  self->to_query_remaining--;
  if (self->to_query_remaining == 0)
    photos_collection_icon_watcher_all_items_ready (self);
}


static void
photos_collection_icon_watcher_to_query_executed (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  g_autoptr (PhotosCollectionIconWatcher) self = PHOTOS_COLLECTION_ICON_WATCHER (user_data);
  PhotosSingleItemJob *job = PHOTOS_SINGLE_ITEM_JOB (source_object);
  g_autoptr (TrackerSparqlCursor) cursor = NULL;

  {
    g_autoptr (GError) error = NULL;

    cursor = photos_single_item_job_finish (job, res, &error);
    if (error != NULL)
      g_warning ("Unable to query single item: %s", error->message);
  }

  if (cursor != NULL && self->item_mngr != NULL)
    {
      g_autoptr (PhotosBaseItem) item = NULL;

      item = photos_item_manager_create_item (PHOTOS_ITEM_MANAGER (self->item_mngr), G_TYPE_NONE, cursor, TRUE);
      self->items = g_list_prepend (self->items, g_object_ref (item));
    }

  photos_collection_icon_watcher_to_query_collector (self);
}


static void
photos_collection_icon_watcher_finished (PhotosCollectionIconWatcher *self)
{
  GApplication *app;
  GList *l;
  GList *to_query = NULL;
  PhotosSearchContextState *state;

  g_return_if_fail (self->item_mngr != NULL);

  if (self->urns == NULL)
    return;

  app = g_application_get_default ();
  state = photos_search_context_get_state (PHOTOS_SEARCH_CONTEXT (app));

  self->to_query_remaining = 0;

  for (l = self->urns; l != NULL; l = l->next)
    {
      GObject *item;
      const gchar *urn = (gchar *) l->data;

      item = photos_base_manager_get_object_by_id (self->item_mngr, urn);
      if (item != NULL)
        self->items = g_list_prepend (self->items, g_object_ref (item));
      else
        {
          to_query = g_list_prepend (to_query, g_strdup (urn));
          self->to_query_remaining++;
        }
    }

  if (self->to_query_remaining == 0)
    {
      photos_collection_icon_watcher_all_items_ready (self);
      return;
    }

  for (l = to_query; l != NULL; l = l->next)
    {
      g_autoptr (PhotosSingleItemJob) job = NULL;
      const gchar *urn = (gchar *) l->data;

      job = photos_single_item_job_new (urn);
      photos_single_item_job_run (job,
                                  state,
                                  PHOTOS_QUERY_FLAGS_UNFILTERED,
                                  NULL,
                                  photos_collection_icon_watcher_to_query_executed,
                                  g_object_ref (self));
    }

  g_list_free_full (to_query, g_free);
}


static void
photos_collection_icon_watcher_cursor_next (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosCollectionIconWatcher *self;
  TrackerSparqlCursor *cursor = TRACKER_SPARQL_CURSOR (source_object);
  gboolean success;

  {
    g_autoptr (GError) error = NULL;

    /* Note that tracker_sparql_cursor_next_finish can return FALSE even
     * without an error.
     */
    success = tracker_sparql_cursor_next_finish (cursor, res, &error);
    if (error != NULL)
      {
        if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
          return;
        else
          g_warning ("Unable to query collection items: %s", error->message);
      }
  }

  self = PHOTOS_COLLECTION_ICON_WATCHER (user_data);

  if (success)
    {
      const gchar *urn;

      urn = tracker_sparql_cursor_get_string (cursor, 0, NULL);
      self->urns = g_list_prepend (self->urns, g_strdup (urn));

      tracker_sparql_cursor_next_async (cursor,
                                        self->cancellable,
                                        photos_collection_icon_watcher_cursor_next,
                                        self);
    }
  else
    {
      self->urns = g_list_reverse (self->urns);
      photos_collection_icon_watcher_finished (self);
      tracker_sparql_cursor_close (cursor);
    }
}


static void
photos_collection_icon_watcher_query_executed (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosCollectionIconWatcher *self = PHOTOS_COLLECTION_ICON_WATCHER (user_data);
  TrackerSparqlConnection *connection = TRACKER_SPARQL_CONNECTION (source_object);
  g_autoptr (TrackerSparqlCursor) cursor = NULL;

  {
    g_autoptr (GError) error = NULL;

    cursor = tracker_sparql_connection_query_finish (connection, res, &error);
    if (error != NULL)
      {
        g_warning ("Unable to query collection items: %s", error->message);
        goto out;
      }
  }

  tracker_sparql_cursor_next_async (cursor,
                                    self->cancellable,
                                    photos_collection_icon_watcher_cursor_next,
                                    self);

 out:
  return;
}


static void
photos_collection_icon_watcher_start (PhotosCollectionIconWatcher *self)
{
  GApplication *app;
  g_autoptr (PhotosQuery) query = NULL;
  PhotosSearchContextState *state;
  const gchar *id;

  photos_collection_icon_watcher_clear (self);

  if (G_UNLIKELY (self->queue == NULL))
    return;

  if (self->collection == NULL)
    return;

  app = g_application_get_default ();
  state = photos_search_context_get_state (PHOTOS_SEARCH_CONTEXT (app));

  id = photos_filterable_get_id (PHOTOS_FILTERABLE (self->collection));
  query = photos_query_builder_collection_icon_query (state, id);
  photos_tracker_queue_select (self->queue,
                               query,
                               NULL,
                               photos_collection_icon_watcher_query_executed,
                               g_object_ref (self),
                               g_object_unref);
}


static void
photos_collection_icon_watcher_constructed (GObject *object)
{
  PhotosCollectionIconWatcher *self = PHOTOS_COLLECTION_ICON_WATCHER (object);

  G_OBJECT_CLASS (photos_collection_icon_watcher_parent_class)->constructed (object);
  photos_collection_icon_watcher_start (self);
}


static void
photos_collection_icon_watcher_dispose (GObject *object)
{
  PhotosCollectionIconWatcher *self = PHOTOS_COLLECTION_ICON_WATCHER (object);

  if (self->cancellable != NULL)
    {
      g_cancellable_cancel (self->cancellable);
      g_clear_object (&self->cancellable);
    }

  if (self->item_connections != NULL)
    {
      photos_collection_icon_watcher_destroy (self);
      g_clear_pointer (&self->item_connections, g_hash_table_unref);
    }

  g_list_free_full (self->items, g_object_unref);
  self->items = NULL;

  g_clear_object (&self->queue);

  G_OBJECT_CLASS (photos_collection_icon_watcher_parent_class)->dispose (object);
}


static void
photos_collection_icon_watcher_finalize (GObject *object)
{
  PhotosCollectionIconWatcher *self = PHOTOS_COLLECTION_ICON_WATCHER (object);

  g_list_free_full (self->urns, g_free);

  if (self->collection != NULL)
    g_object_remove_weak_pointer (G_OBJECT (self->collection), (gpointer *) &self->collection);

  if (self->item_mngr != NULL)
    g_object_remove_weak_pointer (G_OBJECT (self->item_mngr), (gpointer *) &self->item_mngr);

  G_OBJECT_CLASS (photos_collection_icon_watcher_parent_class)->finalize (object);
}


static void
photos_collection_icon_watcher_set_property (GObject *object,
                                             guint prop_id,
                                             const GValue *value,
                                             GParamSpec *pspec)
{
  PhotosCollectionIconWatcher *self = PHOTOS_COLLECTION_ICON_WATCHER (object);

  switch (prop_id)
    {
    case PROP_COLLECTION:
      self->collection = PHOTOS_BASE_ITEM (g_value_get_object (value)); /* self is owned by collection */
      if (self->collection != NULL)
        g_object_add_weak_pointer (G_OBJECT (self->collection), (gpointer *) &self->collection);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
photos_collection_icon_watcher_init (PhotosCollectionIconWatcher *self)
{
  GApplication *app;
  PhotosSearchContextState *state;

  app = g_application_get_default ();
  state = photos_search_context_get_state (PHOTOS_SEARCH_CONTEXT (app));

  self->cancellable = g_cancellable_new ();
  self->item_connections = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, g_object_unref);

  self->item_mngr = state->item_mngr;
  g_object_add_weak_pointer (G_OBJECT (self->item_mngr), (gpointer *) &self->item_mngr);

  self->queue = photos_tracker_queue_dup_singleton (NULL, NULL);
}


static void
photos_collection_icon_watcher_class_init (PhotosCollectionIconWatcherClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->constructed = photos_collection_icon_watcher_constructed;
  object_class->dispose = photos_collection_icon_watcher_dispose;
  object_class->finalize = photos_collection_icon_watcher_finalize;
  object_class->set_property = photos_collection_icon_watcher_set_property;

  g_object_class_install_property (object_class,
                                   PROP_COLLECTION,
                                   g_param_spec_object ("collection",
                                                        "PhotosBaseItem object",
                                                        "The collection to watch.",
                                                        PHOTOS_TYPE_BASE_ITEM,
                                                        G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE));

  signals[ICON_UPDATED] = g_signal_new ("icon-updated",
                                        G_TYPE_FROM_CLASS (class),
                                        G_SIGNAL_RUN_LAST,
                                        0,
                                        NULL, /* accumulator */
                                        NULL, /* accu_data */
                                        g_cclosure_marshal_VOID__OBJECT,
                                        G_TYPE_NONE,
                                        1,
                                        G_TYPE_ICON);
}


PhotosCollectionIconWatcher *
photos_collection_icon_watcher_new (PhotosBaseItem *collection)
{
  return g_object_new (PHOTOS_TYPE_COLLECTION_ICON_WATCHER, "collection", collection, NULL);
}


void
photos_collection_icon_watcher_refresh (PhotosCollectionIconWatcher *self)
{
  g_return_if_fail (PHOTOS_IS_COLLECTION_ICON_WATCHER (self));

  photos_collection_icon_watcher_destroy (self);
  photos_collection_icon_watcher_start (self);
}
