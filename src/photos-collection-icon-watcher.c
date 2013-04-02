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

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <glib.h>
#include <tracker-sparql.h>

#include "photos-collection-icon-watcher.h"
#include "photos-item-manager.h"
#include "photos-query.h"
#include "photos-query-builder.h"
#include "photos-single-item-job.h"
#include "photos-tracker-queue.h"
#include "photos-utils.h"


struct _PhotosCollectionIconWatcherPrivate
{
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
  GIcon *icon;
  GList *icons = NULL;
  GList *l;
  gint size;

  for (l = self->priv->items; l != NULL; l = l->next)
    {
      GdkPixbuf *pristine_icon;
      PhotosBaseItem *item = PHOTOS_BASE_ITEM (l->data);

      pristine_icon = photos_base_item_get_pristine_icon (item);
      icons = g_list_prepend (icons, g_object_ref (pristine_icon));
    }
  icons = g_list_reverse (icons);

  size = photos_utils_get_icon_size ();
  icon = photos_utils_create_collection_icon (size, icons);

  g_signal_emit (self, signals[ICON_UPDATED], 0, icon);

  g_list_free_full (icons, g_object_unref);
  g_clear_object (&icon);
}


static void
photos_collection_icon_watcher_all_items_ready (PhotosCollectionIconWatcher *self)
{
  PhotosCollectionIconWatcherPrivate *priv = self->priv;
  GList *l;

  for (l = priv->items; l != NULL; l = l->next)
    {
      PhotosBaseItem *item = PHOTOS_BASE_ITEM (l->data);
      gulong update_id;

      update_id = g_signal_connect_swapped (item,
                                            "info-updated",
                                            G_CALLBACK (photos_collection_icon_watcher_create_collection_icon),
                                            self);
      g_hash_table_insert (priv->item_connections, GUINT_TO_POINTER ((guint) update_id), g_object_ref (item));
    }

  photos_collection_icon_watcher_create_collection_icon (self);
}


static void
photos_collection_icon_watcher_clear (PhotosCollectionIconWatcher *self)
{
  PhotosCollectionIconWatcherPrivate *priv = self->priv;

  g_hash_table_remove_all (priv->item_connections);

  g_list_free_full (priv->urns, g_free);
  priv->urns = NULL;

  g_list_free_full (priv->items, g_object_unref);
  priv->items = NULL;
}


static void
photos_collection_icon_watcher_destroy (PhotosCollectionIconWatcher *self)
{
  PhotosCollectionIconWatcherPrivate *priv = self->priv;
  GHashTableIter iter;
  PhotosBaseItem *item;
  gpointer key;

  g_hash_table_iter_init (&iter, priv->item_connections);
  while (g_hash_table_iter_next (&iter, (gpointer) &key, (gpointer) &item))
    {
      guint id = GPOINTER_TO_UINT (key);
      g_signal_handler_disconnect (item, id);
    }
}


static void
photos_collection_icon_watcher_to_query_collector (PhotosCollectionIconWatcher *self)
{
  PhotosCollectionIconWatcherPrivate *priv = self->priv;

  priv->to_query_remaining--;
  if (priv->to_query_remaining == 0)
    photos_collection_icon_watcher_all_items_ready (self);
}


static void
photos_collection_icon_watcher_to_query_executed (TrackerSparqlCursor *cursor, gpointer user_data)
{
  PhotosCollectionIconWatcher *self = PHOTOS_COLLECTION_ICON_WATCHER (user_data);
  PhotosCollectionIconWatcherPrivate *priv = self->priv;

  if (cursor != NULL)
    {
      PhotosBaseItem *item;

      item = photos_item_manager_create_item (PHOTOS_ITEM_MANAGER (priv->item_mngr), cursor);
      priv->items = g_list_prepend (priv->items, item);
    }

  photos_collection_icon_watcher_to_query_collector (self);
  g_object_unref (self);
}


static void
photos_collection_icon_watcher_finished (PhotosCollectionIconWatcher *self)
{
  PhotosCollectionIconWatcherPrivate *priv = self->priv;
  GList *l;
  GList *to_query = NULL;

  if (priv->urns == NULL)
    return;

  priv->to_query_remaining = 0;

  for (l = priv->urns; l != NULL; l = l->next)
    {
      GObject *item;
      const gchar *urn = (gchar *) l->data;

      item = photos_base_manager_get_object_by_id (priv->item_mngr, urn);
      if (item != NULL)
        priv->items = g_list_prepend (priv->items, g_object_ref (item));
      else
        {
          to_query = g_list_prepend (to_query, g_strdup (urn));
          priv->to_query_remaining++;
        }
    }

  if (priv->to_query_remaining == 0)
    {
      photos_collection_icon_watcher_all_items_ready (self);
      return;
    }

  for (l = to_query; l != NULL; l = l->next)
    {
      PhotosSingleItemJob *job;
      const gchar *urn = (gchar *) l->data;

      job = photos_single_item_job_new (urn);
      photos_single_item_job_run (job,
                                  PHOTOS_QUERY_FLAGS_UNFILTERED,
                                  photos_collection_icon_watcher_to_query_executed,
                                  g_object_ref (self));
      g_object_unref (job);
    }

  g_list_free_full (to_query, g_free);
}


static void
photos_collection_icon_watcher_cursor_next (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosCollectionIconWatcher *self = PHOTOS_COLLECTION_ICON_WATCHER (user_data);
  PhotosCollectionIconWatcherPrivate *priv = self->priv;
  TrackerSparqlCursor *cursor = TRACKER_SPARQL_CURSOR (source_object);
  GError *error;
  gboolean valid;
  gchar *urn;

  error = NULL;
  valid = tracker_sparql_cursor_next_finish (cursor, res, &error);
  if (error != NULL)
    {
      g_warning ("Unable to query collection items: %s", error->message);
      g_error_free (error);
      goto end;
    }
  else if (!valid)
    goto end;

  urn = g_strdup (tracker_sparql_cursor_get_string (cursor, 0, NULL));
  priv->urns = g_list_prepend (priv->urns, urn);

  tracker_sparql_cursor_next_async (cursor,
                                    NULL,
                                    photos_collection_icon_watcher_cursor_next,
                                    self);
  return;

 end:
  priv->urns = g_list_reverse (priv->urns);
  photos_collection_icon_watcher_finished (self);
  tracker_sparql_cursor_close (cursor);
  g_object_unref (self);
}


static void
photos_collection_icon_watcher_query_executed (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosCollectionIconWatcher *self = PHOTOS_COLLECTION_ICON_WATCHER (user_data);
  TrackerSparqlConnection *connection = TRACKER_SPARQL_CONNECTION (source_object);
  TrackerSparqlCursor *cursor;
  GError *error;

  error = NULL;
  cursor = tracker_sparql_connection_query_finish (connection, res, &error);
  if (error != NULL)
    {
      g_warning ("Unable to query collection items: %s", error->message);
      g_error_free (error);
      return;
    }

  tracker_sparql_cursor_next_async (cursor,
                                    NULL,
                                    photos_collection_icon_watcher_cursor_next,
                                    g_object_ref (self));
  g_object_unref (cursor);
}


static void
photos_collection_icon_watcher_start (PhotosCollectionIconWatcher *self)
{
  PhotosCollectionIconWatcherPrivate *priv = self->priv;
  PhotosQuery *query;
  const gchar *id;

  photos_collection_icon_watcher_clear (self);

  id = photos_base_item_get_id (priv->collection);
  query = photos_query_builder_collection_icon_query (id);
  photos_tracker_queue_select (priv->queue,
                               query->sparql,
                               NULL,
                               photos_collection_icon_watcher_query_executed,
                               g_object_ref (self),
                               g_object_unref);
  photos_query_free (query);
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
  PhotosCollectionIconWatcherPrivate *priv = self->priv;

  if (priv->item_connections != NULL)
    {
      photos_collection_icon_watcher_destroy (self);
      g_hash_table_unref (priv->item_connections);
      priv->item_connections = NULL;
    }

  g_list_free_full (priv->items, g_object_unref);
  priv->items = NULL;

  g_clear_object (&priv->item_mngr);
  g_clear_object (&priv->queue);

  G_OBJECT_CLASS (photos_collection_icon_watcher_parent_class)->dispose (object);
}


static void
photos_collection_icon_watcher_finalize (GObject *object)
{
  PhotosCollectionIconWatcher *self = PHOTOS_COLLECTION_ICON_WATCHER (object);

  g_list_free_full (self->priv->urns, g_free);

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
      self->priv->collection = PHOTOS_BASE_ITEM (g_value_get_object (value)); /* self is owned by collection */
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
photos_collection_icon_watcher_init (PhotosCollectionIconWatcher *self)
{
  PhotosCollectionIconWatcherPrivate *priv = self->priv;

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                            PHOTOS_TYPE_COLLECTION_ICON_WATCHER,
                                            PhotosCollectionIconWatcherPrivate);
  priv = self->priv;

  priv->item_connections = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, g_object_unref);
  priv->item_mngr = photos_item_manager_new ();
  priv->queue = photos_tracker_queue_new ();
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
                                        G_STRUCT_OFFSET (PhotosCollectionIconWatcherClass,
                                                         icon_updated),
                                        NULL, /* accumulator */
                                        NULL, /* accu_data */
                                        g_cclosure_marshal_VOID__OBJECT,
                                        G_TYPE_NONE,
                                        1,
                                        G_TYPE_ICON);

  g_type_class_add_private (class, sizeof (PhotosCollectionIconWatcherPrivate));
}


PhotosCollectionIconWatcher *
photos_collection_icon_watcher_new (PhotosBaseItem *collection)
{
  return g_object_new (PHOTOS_TYPE_COLLECTION_ICON_WATCHER, "collection", collection, NULL);
}


void
photos_collection_icon_watcher_refresh (PhotosCollectionIconWatcher *self)
{
  photos_collection_icon_watcher_destroy (self);
  photos_collection_icon_watcher_start (self);
}
