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


#include "config.h"

#include <glib.h>

#include "photos-item-manager.h"
#include "photos-item-model.h"
#include "photos-local-item.h"
#include "photos-query.h"
#include "photos-single-item-job.h"
#include "photos-tracker-change-event.h"
#include "photos-tracker-change-monitor.h"


struct _PhotosItemManagerPrivate
{
  GtkListStore *model;
  PhotosTrackerChangeMonitor *monitor;
};


G_DEFINE_TYPE (PhotosItemManager, photos_item_manager, PHOTOS_TYPE_BASE_MANAGER);


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
        }
    }
}


static void
photos_item_manager_changes_pending (PhotosTrackerChangeMonitor *monitor, GHashTable *changes, gpointer user_data)
{
  PhotosItemManager *self = PHOTOS_ITEM_MANAGER (user_data);
  g_hash_table_foreach (changes, photos_item_manager_changes_pending_foreach, self);
}


static gboolean
photos_item_manager_set_active_object (PhotosBaseManager *manager, GObject *object)
{
  gboolean ret_val;

  g_return_val_if_fail (PHOTOS_IS_BASE_ITEM (object) || object == NULL, FALSE);

  ret_val = PHOTOS_BASE_MANAGER_CLASS (photos_item_manager_parent_class)->set_active_object (manager, object);

  if (!ret_val)
    goto out;

  if (object != NULL)
    {
      GtkRecentManager *recent;
      const gchar *uri;

      recent = gtk_recent_manager_get_default ();
      uri = photos_base_item_get_uri (PHOTOS_BASE_ITEM (object));
      gtk_recent_manager_add_item (recent, uri);
    }

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

  g_clear_object (&priv->model);
  g_clear_object (&priv->monitor);

  G_OBJECT_CLASS (photos_item_manager_parent_class)->dispose (object);
}


static void
photos_item_manager_init (PhotosItemManager *self)
{
  PhotosItemManagerPrivate *priv = self->priv;

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, PHOTOS_TYPE_ITEM_MANAGER, PhotosItemManagerPrivate);
  priv = self->priv;

  priv->model = photos_item_model_new ();

  priv->monitor = photos_tracker_change_monitor_new ();
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

  g_type_class_add_private (class, sizeof (PhotosItemManagerPrivate));
}


PhotosBaseManager *
photos_item_manager_new (void)
{
  return g_object_new (PHOTOS_TYPE_ITEM_MANAGER, NULL);
}


PhotosBaseItem *
photos_item_manager_add_item (PhotosItemManager *self, TrackerSparqlCursor *cursor)
{
  PhotosBaseItem *item;

  item = photos_item_manager_create_item (self, cursor);
  photos_base_manager_add_object (PHOTOS_BASE_MANAGER (self), G_OBJECT (item));
  photos_item_model_item_added (PHOTOS_ITEM_MODEL (self->priv->model), item);

  /* TODO: add to collection_manager */

  return item;
}


void
photos_item_manager_clear (PhotosItemManager *self)
{
  photos_base_manager_clear (PHOTOS_BASE_MANAGER (self));
  gtk_list_store_clear (self->priv->model);
}


PhotosBaseItem *
photos_item_manager_create_item (PhotosItemManager *self, TrackerSparqlCursor *cursor)
{
  return photos_local_item_new (cursor);
}


GtkListStore *
photos_item_manager_get_model (PhotosItemManager *self)
{
  return self->priv->model;
}
