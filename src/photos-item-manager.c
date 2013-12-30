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

#include "photos-collection-manager.h"
#include "photos-item-manager.h"
#include "photos-local-item.h"
#include "photos-facebook-item.h"
#include "photos-flickr-item.h"
#include "photos-query.h"
#include "photos-single-item-job.h"
#include "photos-tracker-change-event.h"
#include "photos-tracker-change-monitor.h"


struct _PhotosItemManagerPrivate
{
  GQueue *collection_path;
  PhotosBaseManager *col_mngr;
  PhotosTrackerChangeMonitor *monitor;
};


G_DEFINE_TYPE_WITH_PRIVATE (PhotosItemManager, photos_item_manager, PHOTOS_TYPE_BASE_MANAGER);


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
  PhotosSingleItemJob *job;

  job = photos_single_item_job_new (urn);
  photos_single_item_job_run (job,
                              PHOTOS_QUERY_FLAGS_NONE,
                              photos_item_manager_item_created_executed,
                              g_object_ref (self));
  g_object_unref (job);
}


static void
photos_item_manager_changes_pending_foreach (gpointer key, gpointer value, gpointer user_data)
{
  PhotosItemManager *self = PHOTOS_ITEM_MANAGER (user_data);
  PhotosItemManagerPrivate *priv = self->priv;
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
          if (photos_base_item_is_collection (PHOTOS_BASE_ITEM (object)))
            photos_base_manager_remove_object_by_id (priv->col_mngr, change_urn);
        }
    }
}


static void
photos_item_manager_changes_pending (PhotosTrackerChangeMonitor *monitor, GHashTable *changes, gpointer user_data)
{
  PhotosItemManager *self = PHOTOS_ITEM_MANAGER (user_data);
  g_hash_table_foreach (changes, photos_item_manager_changes_pending_foreach, self);
}


static void
photos_item_manager_collection_path_free_foreach (gpointer data, gpointer user_data)
{
  g_clear_object (&data);
}


static gboolean
photos_item_manager_set_active_object (PhotosBaseManager *manager, GObject *object)
{
  PhotosItemManager *self = PHOTOS_ITEM_MANAGER (manager);
  PhotosItemManagerPrivate *priv = self->priv;
  GtkRecentManager *recent;
  gboolean ret_val;
  const gchar *uri;

  g_return_val_if_fail (PHOTOS_IS_BASE_ITEM (object) || object == NULL, FALSE);

  ret_val = PHOTOS_BASE_MANAGER_CLASS (photos_item_manager_parent_class)->set_active_object (manager, object);

  if (!ret_val)
    goto out;

  if (object == NULL)
    goto out;

  if (photos_base_item_is_collection (PHOTOS_BASE_ITEM (object)))
    {
      GObject *collection;

      collection = photos_base_manager_get_active_object (priv->col_mngr);
      g_queue_push_head (priv->collection_path, (collection != NULL) ? g_object_ref (collection) : NULL);
      photos_base_manager_set_active_object (priv->col_mngr, object);
      goto out;
    }

  recent = gtk_recent_manager_get_default ();
  uri = photos_base_item_get_uri (PHOTOS_BASE_ITEM (object));
  gtk_recent_manager_add_item (recent, uri);

 out:
  return ret_val;
}


static GObject *
photos_item_manager_constructor (GType                  type,
                                 guint                  n_construct_params,
                                 GObjectConstructParam *construct_params)
{
  static GObject *self = NULL;

  if (self == NULL)
    {
      self = G_OBJECT_CLASS (photos_item_manager_parent_class)->constructor (type,
                                                                             n_construct_params,
                                                                             construct_params);
      g_object_add_weak_pointer (self, (gpointer) &self);
      return self;
    }

  return g_object_ref (self);
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

  g_clear_object (&priv->col_mngr);
  g_clear_object (&priv->monitor);

  G_OBJECT_CLASS (photos_item_manager_parent_class)->dispose (object);
}


static void
photos_item_manager_init (PhotosItemManager *self)
{
  PhotosItemManagerPrivate *priv = self->priv;

  self->priv = photos_item_manager_get_instance_private (self);
  priv = self->priv;

  priv->collection_path = g_queue_new ();
  priv->col_mngr = photos_collection_manager_dup_singleton ();

  priv->monitor = photos_tracker_change_monitor_dup_singleton (NULL, NULL);
  if (G_LIKELY (priv->monitor != NULL))
    g_signal_connect (priv->monitor, "changes-pending", G_CALLBACK (photos_item_manager_changes_pending), self);
}


static void
photos_item_manager_class_init (PhotosItemManagerClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  PhotosBaseManagerClass *base_manager_class = PHOTOS_BASE_MANAGER_CLASS (class);

  object_class->constructor = photos_item_manager_constructor;
  object_class->dispose = photos_item_manager_dispose;
  base_manager_class->set_active_object = photos_item_manager_set_active_object;
}


PhotosBaseManager *
photos_item_manager_dup_singleton (void)
{
  return g_object_new (PHOTOS_TYPE_ITEM_MANAGER, NULL);
}


void
photos_item_manager_activate_previous_collection (PhotosItemManager *self)
{
  PhotosItemManagerPrivate *priv = self->priv;
  GObject *collection;

  collection = G_OBJECT (g_queue_pop_head (priv->collection_path));
  photos_base_manager_set_active_object (priv->col_mngr, collection);
  g_clear_object (&collection);
}


PhotosBaseItem *
photos_item_manager_add_item (PhotosItemManager *self, TrackerSparqlCursor *cursor)
{
  PhotosBaseItem *item;

  item = photos_item_manager_create_item (self, cursor);
  photos_base_manager_add_object (PHOTOS_BASE_MANAGER (self), G_OBJECT (item));

  if (photos_base_item_is_collection (item))
    photos_base_manager_add_object (self->priv->col_mngr, G_OBJECT (item));

  return item;
}


PhotosBaseItem *
photos_item_manager_create_item (PhotosItemManager *self, TrackerSparqlCursor *cursor)
{
  PhotosBaseItem *ret_val = NULL;
  gchar *identifier = NULL;

  identifier = g_strdup (tracker_sparql_cursor_get_string (cursor, PHOTOS_QUERY_COLUMNS_IDENTIFIER, NULL));

  if (identifier != NULL)
    {
      if (g_str_has_prefix (identifier, "flickr:") || g_str_has_prefix (identifier, "photos:collection:flickr:"))
        ret_val = photos_flickr_item_new (cursor);
      else if (g_str_has_prefix (identifier, "facebook:")
               || g_str_has_prefix (identifier, "photos:collection:facebook:"))
        ret_val = photos_facebook_item_new (cursor);
    }

  if (ret_val == NULL)
    ret_val = photos_local_item_new (cursor);

  g_free (identifier);
  return ret_val;
}
