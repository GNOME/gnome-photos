/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2012, 2013, 2014 Red Hat, Inc.
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

#include "photos-filterable.h"
#include "photos-item-manager.h"
#include "photos-local-item.h"
#include "photos-query.h"
#include "photos-search-context.h"
#include "photos-single-item-job.h"
#include "photos-tracker-change-event.h"
#include "photos-tracker-change-monitor.h"
#include "photos-utils.h"


struct _PhotosItemManagerPrivate
{
  GHashTable *collections;
  GIOExtensionPoint *extension_point;
  GQueue *collection_path;
  PhotosBaseItem *active_collection;
  PhotosTrackerChangeMonitor *monitor;
};

enum
{
  ACTIVE_COLLECTION_CHANGED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };


G_DEFINE_TYPE_WITH_PRIVATE (PhotosItemManager, photos_item_manager, PHOTOS_TYPE_BASE_MANAGER);


static void
photos_item_manager_add_object (PhotosBaseManager *mngr, GObject *object)
{
  PhotosItemManager *self = PHOTOS_ITEM_MANAGER (mngr);
  PhotosItemManagerPrivate *priv = self->priv;
  PhotosBaseItem *item;
  const gchar *id;
  gpointer *old_collection;

  g_return_if_fail (PHOTOS_IS_BASE_ITEM (object));

  item = PHOTOS_BASE_ITEM (object);

  if (!photos_base_item_is_collection (item))
    goto end;

  id = photos_filterable_get_id (PHOTOS_FILTERABLE (item));
  if (id == NULL)
    goto end;

  old_collection = g_hash_table_lookup (priv->collections, id);
  if (old_collection != NULL)
    goto end;

  g_hash_table_insert (priv->collections, g_strdup (id), g_object_ref (item));

 end:
  PHOTOS_BASE_MANAGER_CLASS (photos_item_manager_parent_class)->add_object (mngr, object);
}


static void
photos_item_manager_item_created_executed (TrackerSparqlCursor *cursor, gpointer user_data)
{
  PhotosItemManager *self = PHOTOS_ITEM_MANAGER (user_data);

  if (cursor == NULL)
    goto out;

  photos_item_manager_add_item (self, cursor);

 out:
  g_object_unref (self);
}


static void
photos_item_manager_item_created (PhotosItemManager *self, const gchar *urn)
{
  GApplication *app;
  PhotosSearchContextState *state;
  PhotosSingleItemJob *job;

  app = g_application_get_default ();
  state = photos_search_context_get_state (PHOTOS_SEARCH_CONTEXT (app));

  job = photos_single_item_job_new (urn);
  photos_single_item_job_run (job,
                              state,
                              PHOTOS_QUERY_FLAGS_NONE,
                              photos_item_manager_item_created_executed,
                              g_object_ref (self));
  g_object_unref (job);
}


static void
photos_item_manager_changes_pending_foreach (gpointer key, gpointer value, gpointer user_data)
{
  PhotosItemManager *self = PHOTOS_ITEM_MANAGER (user_data);
  PhotosTrackerChangeEvent *change_event = (PhotosTrackerChangeEvent *) value;
  PhotosTrackerChangeEventType change_type;
  const gchar *change_urn;

  change_type = photos_tracker_change_event_get_type (change_event);
  change_urn = photos_tracker_change_event_get_urn (change_event);

  if (change_type == PHOTOS_TRACKER_CHANGE_EVENT_CHANGED)
    {
      GObject *object;

      object = photos_base_manager_get_object_by_id (PHOTOS_BASE_MANAGER (self), change_urn);
      if (object != NULL)
        photos_base_item_refresh (PHOTOS_BASE_ITEM (object));
    }
  else if (change_type == PHOTOS_TRACKER_CHANGE_EVENT_CREATED)
    {
      photos_item_manager_item_created (self, change_urn);
    }
  else if (change_type == PHOTOS_TRACKER_CHANGE_EVENT_DELETED)
    {
      GObject *object;

      object = photos_base_manager_get_object_by_id (PHOTOS_BASE_MANAGER (self), change_urn);
      if (object != NULL)
        {
          photos_base_item_destroy (PHOTOS_BASE_ITEM (object));
          photos_base_manager_remove_object_by_id (PHOTOS_BASE_MANAGER (self), change_urn);
        }
    }
}


static void
photos_item_manager_changes_pending (PhotosItemManager *self, GHashTable *changes)
{
  g_hash_table_foreach (changes, photos_item_manager_changes_pending_foreach, self);
}


static void
photos_item_manager_collection_path_free_foreach (gpointer data, gpointer user_data)
{
  g_clear_object (&data);
}


static gchar *
photos_item_manager_get_where (PhotosBaseManager *mngr, gint flags)
{
  PhotosItemManager *self = PHOTOS_ITEM_MANAGER (mngr);
  PhotosItemManagerPrivate *priv = self->priv;

  if (priv->active_collection == NULL)
    return g_strdup ("");

  return photos_base_item_get_where (priv->active_collection);
}


static void
photos_item_manager_remove_object_by_id (PhotosBaseManager *mngr, const gchar *id)
{
  PhotosItemManager *self = PHOTOS_ITEM_MANAGER (mngr);
  PhotosItemManagerPrivate *priv = self->priv;
  gpointer *collection;

  if (id == NULL)
    goto end;

  collection = g_hash_table_lookup (priv->collections, id);
  if (collection == NULL)
    goto end;

  g_hash_table_remove (priv->collections, id);

 end:
  PHOTOS_BASE_MANAGER_CLASS (photos_item_manager_parent_class)->remove_object_by_id (mngr, id);
}


static gboolean
photos_item_manager_set_active_object (PhotosBaseManager *manager, GObject *object)
{
  PhotosItemManager *self = PHOTOS_ITEM_MANAGER (manager);
  PhotosItemManagerPrivate *priv = self->priv;
  GObject *active_item;
  GtkRecentManager *recent;
  gboolean active_collection_changed = FALSE;
  gboolean ret_val = FALSE;
  const gchar *uri;

  g_return_val_if_fail (PHOTOS_IS_BASE_ITEM (object) || object == NULL, FALSE);

  active_item = photos_base_manager_get_active_object (manager);

  /* Passing NULL is a way to go back to the current collection or
   * overview from the preview. However, you can't do that when you
   * are looking at a collection.
   */
  if (object == NULL)
    {
      if (active_item != (GObject *) priv->active_collection)
        object = (GObject *) priv->active_collection;
      else
        goto out;
    }

  /* This is when we are going back to the overview from the preview. */
  if (object == NULL)
    goto end;

  if (photos_base_item_is_collection (PHOTOS_BASE_ITEM (object)))
    {
      /* This is when we are going back to the collection from the
       * preview.
       */
      if (object == (GObject *) priv->active_collection)
        goto end;

      g_queue_push_head (priv->collection_path,
                         (priv->active_collection != NULL) ? g_object_ref (priv->active_collection) : NULL);

      g_clear_object (&priv->active_collection);
      priv->active_collection = g_object_ref (object);
      active_collection_changed = TRUE;
      goto end;
    }

  recent = gtk_recent_manager_get_default ();
  uri = photos_base_item_get_uri (PHOTOS_BASE_ITEM (object));
  gtk_recent_manager_add_item (recent, uri);

 end:
  ret_val = PHOTOS_BASE_MANAGER_CLASS (photos_item_manager_parent_class)->set_active_object (manager, object);
  if (active_collection_changed)
    g_signal_emit (self, signals[ACTIVE_COLLECTION_CHANGED], 0, priv->active_collection);

 out:
  return ret_val;
}


static void
photos_item_manager_dispose (GObject *object)
{
  PhotosItemManager *self = PHOTOS_ITEM_MANAGER (object);
  PhotosItemManagerPrivate *priv = self->priv;

  if (priv->collection_path != NULL)
    {
      g_queue_foreach (priv->collection_path, photos_item_manager_collection_path_free_foreach, NULL);
      g_queue_free (priv->collection_path);
      priv->collection_path = NULL;
    }

  g_clear_pointer (&priv->collections, (GDestroyNotify) g_hash_table_unref);
  g_clear_object (&priv->active_collection);
  g_clear_object (&priv->monitor);

  G_OBJECT_CLASS (photos_item_manager_parent_class)->dispose (object);
}


static void
photos_item_manager_init (PhotosItemManager *self)
{
  PhotosItemManagerPrivate *priv = self->priv;

  self->priv = photos_item_manager_get_instance_private (self);
  priv = self->priv;

  priv->collections = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
  priv->extension_point = g_io_extension_point_lookup (PHOTOS_BASE_ITEM_EXTENSION_POINT_NAME);
  priv->collection_path = g_queue_new ();

  priv->monitor = photos_tracker_change_monitor_dup_singleton (NULL, NULL);
  if (G_LIKELY (priv->monitor != NULL))
    g_signal_connect_swapped (priv->monitor,
                              "changes-pending",
                              G_CALLBACK (photos_item_manager_changes_pending),
                              self);
}


static void
photos_item_manager_class_init (PhotosItemManagerClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  PhotosBaseManagerClass *base_manager_class = PHOTOS_BASE_MANAGER_CLASS (class);

  object_class->dispose = photos_item_manager_dispose;
  base_manager_class->add_object = photos_item_manager_add_object;
  base_manager_class->get_where = photos_item_manager_get_where;
  base_manager_class->set_active_object = photos_item_manager_set_active_object;
  base_manager_class->remove_object_by_id = photos_item_manager_remove_object_by_id;

  signals[ACTIVE_COLLECTION_CHANGED] = g_signal_new ("active-collection-changed",
                                                     G_TYPE_FROM_CLASS (class),
                                                     G_SIGNAL_RUN_LAST,
                                                     G_STRUCT_OFFSET (PhotosItemManagerClass,
                                                                      active_collection_changed),
                                                     NULL, /*accumulator */
                                                     NULL, /*accu_data */
                                                     g_cclosure_marshal_VOID__OBJECT,
                                                     G_TYPE_NONE,
                                                     1,
                                                     PHOTOS_TYPE_BASE_ITEM);
}


PhotosBaseManager *
photos_item_manager_new (void)
{
  return g_object_new (PHOTOS_TYPE_ITEM_MANAGER, NULL);
}


void
photos_item_manager_activate_previous_collection (PhotosItemManager *self)
{
  PhotosItemManagerPrivate *priv = self->priv;
  gpointer *collection;

  collection = g_queue_pop_head (priv->collection_path);
  g_assert (collection == NULL || PHOTOS_IS_BASE_ITEM (collection));

  g_clear_object (&priv->active_collection);

  if (collection != NULL)
    g_object_ref (collection);
  priv->active_collection = PHOTOS_BASE_ITEM (collection);

  PHOTOS_BASE_MANAGER_CLASS (photos_item_manager_parent_class)
    ->set_active_object (PHOTOS_BASE_MANAGER (self), (GObject *) collection);

  g_signal_emit (self, signals[ACTIVE_COLLECTION_CHANGED], 0, priv->active_collection);

  g_clear_object (&collection);
}


void
photos_item_manager_add_item (PhotosItemManager *self, TrackerSparqlCursor *cursor)
{
  PhotosBaseItem *item = NULL;
  GObject *object;
  const gchar *id;

  id = tracker_sparql_cursor_get_string (cursor, PHOTOS_QUERY_COLUMNS_URN, NULL);
  object = photos_base_manager_get_object_by_id (PHOTOS_BASE_MANAGER (self), id);
  if (object != NULL)
    {
      g_signal_emit_by_name (self, "object-added", object);
      goto out;
    }

  item = photos_item_manager_create_item (self, cursor);
  photos_base_manager_add_object (PHOTOS_BASE_MANAGER (self), G_OBJECT (item));

 out:
  g_clear_object (&item);
}


PhotosBaseItem *
photos_item_manager_create_item (PhotosItemManager *self, TrackerSparqlCursor *cursor)
{
  PhotosBaseItem *ret_val = NULL;
  GIOExtension *extension;
  GType type;
  const gchar *extension_name = "local";
  gchar *identifier = NULL;
  gchar **split_identifier = NULL;

  identifier = g_strdup (tracker_sparql_cursor_get_string (cursor, PHOTOS_QUERY_COLUMNS_IDENTIFIER, NULL));
  if (identifier == NULL)
    goto final;

  split_identifier = g_strsplit (identifier, ":", 4);

  if (g_str_has_prefix (identifier, "photos:collection:"))
    {
      /* Its a collection. */
      extension_name = split_identifier[2];
    }
  else
    {
      /* Its a normal photo item. */
      if (g_strv_length (split_identifier) > 1)
        extension_name = split_identifier[0];
    }

 final:
  extension = g_io_extension_point_get_extension_by_name (self->priv->extension_point, extension_name);
  if (G_UNLIKELY (extension == NULL))
    {
      g_warning ("Unable to find extension %s for identifier: %s", extension_name, identifier);
      goto out;
    }

  type = g_io_extension_get_type (extension);
  ret_val = PHOTOS_BASE_ITEM (g_object_new (type,
                                            "cursor", cursor,
                                            "failed-thumbnailing", FALSE,
                                            NULL));

 out:
  g_strfreev (split_identifier);
  g_free (identifier);
  return ret_val;
}


PhotosBaseItem *
photos_item_manager_get_active_collection (PhotosItemManager *self)
{
  return self->priv->active_collection;
}


GHashTable *
photos_item_manager_get_collections (PhotosItemManager *self)
{
  return self->priv->collections;
}
