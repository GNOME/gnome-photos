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

#include "photos-base-manager.h"


struct _PhotosBaseManagerPrivate
{
  GHashTable *items;
  PhotosBaseItem *active_item;
};

enum
{
  ACTIVE_CHANGED,
  ITEM_ADDED,
  ITEM_REMOVED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };


G_DEFINE_TYPE (PhotosBaseManager, photos_base_manager, G_TYPE_OBJECT);


static void
photos_base_manager_dispose (GObject *object)
{
  PhotosBaseManager *self = PHOTOS_BASE_MANAGER (object);
  PhotosBaseManagerPrivate *priv = self->priv;

  if (priv->items != NULL)
    {
      g_hash_table_unref (priv->items);
      priv->items = NULL;
    }

  g_clear_object (&priv->active_item);

  G_OBJECT_CLASS (photos_base_manager_parent_class)->dispose (object);
}


static void
photos_base_manager_init (PhotosBaseManager *self)
{
  PhotosBaseManagerPrivate *priv;

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, PHOTOS_TYPE_BASE_MANAGER, PhotosBaseManagerPrivate);
  priv = self->priv;

  priv->items = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_object_unref);
}


static void
photos_base_manager_class_init (PhotosBaseManagerClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->dispose = photos_base_manager_dispose;

  signals[ACTIVE_CHANGED] = g_signal_new ("active-changed",
                                          G_TYPE_FROM_CLASS (class),
                                          G_SIGNAL_RUN_LAST,
                                          G_STRUCT_OFFSET (PhotosBaseManagerClass,
                                                           active_changed),
                                          NULL, /*accumulator */
                                          NULL, /*accu_data */
                                          g_cclosure_marshal_VOID__OBJECT,
                                          G_TYPE_NONE,
                                          1,
                                          PHOTOS_TYPE_BASE_ITEM);

  signals[ITEM_ADDED] = g_signal_new ("item-added",
                                      G_TYPE_FROM_CLASS (class),
                                      G_SIGNAL_RUN_LAST,
                                      G_STRUCT_OFFSET (PhotosBaseManagerClass,
                                                       item_added),
                                      NULL, /*accumulator */
                                      NULL, /* accu_data */
                                      g_cclosure_marshal_VOID__OBJECT,
                                      G_TYPE_NONE,
                                      1,
                                      PHOTOS_TYPE_BASE_ITEM);

  signals[ITEM_REMOVED] = g_signal_new ("item-removed",
                                        G_TYPE_FROM_CLASS (class),
                                        G_SIGNAL_RUN_LAST,
                                        G_STRUCT_OFFSET (PhotosBaseManagerClass,
                                                         item_removed),
                                        NULL, /*accumulator */
                                        NULL, /*accu_data */
                                        g_cclosure_marshal_VOID__OBJECT,
                                        G_TYPE_NONE,
                                        1,
                                        PHOTOS_TYPE_BASE_ITEM);

  g_type_class_add_private (class, sizeof (PhotosBaseManagerPrivate));
}


PhotosBaseManager *
photos_base_manager_new (void)
{
  return g_object_new (PHOTOS_TYPE_BASE_MANAGER, NULL);
}


void
photos_base_manager_add_item (PhotosBaseManager *self, PhotosBaseItem *item)
{
  const gchar *id = photos_base_item_get_id (item);

  g_object_ref (item);
  g_hash_table_insert (self->priv->items, (gpointer) id, item);
  g_signal_emit (self, signals[ITEM_ADDED], 0, item);
}


void
photos_base_manager_clear (PhotosBaseManager *self)
{
  PhotosBaseManagerPrivate *priv = self->priv;

  g_hash_table_remove_all (priv->items);
  g_clear_object (&priv->active_item);
}


PhotosBaseItem *
photos_base_manager_get_active_item (PhotosBaseManager *self)
{
  return self->priv->active_item;
}


PhotosBaseItem *
photos_base_manager_get_item_by_id (PhotosBaseManager *self, const gchar *id)
{
  return g_hash_table_lookup (self->priv->items, id);
}


GHashTable *
photos_base_manager_get_items (PhotosBaseManager *self)
{
  return self->priv->items;
}


guint
photos_base_manager_get_items_count (PhotosBaseManager *self)
{
  GList *keys;
  guint count;

  keys = g_hash_table_get_keys (self->priv->items);
  count = g_list_length (keys);
  g_list_free (keys);
  return count;
}


void
photos_base_manager_remove_item (PhotosBaseManager *self, PhotosBaseItem *item)
{
  const gchar *id = photos_base_item_get_id (item);
  photos_base_manager_remove_item_by_id (self, id);
}


void
photos_base_manager_remove_item_by_id (PhotosBaseManager *self, const gchar *id)
{
  PhotosBaseItem *item;

  item = photos_base_manager_get_item_by_id (self, id);
  if (item == NULL)
    return;

  g_signal_emit (self, signals[ITEM_REMOVED], 0, item);
  g_hash_table_remove (self->priv->items, id);
}


gboolean
photos_base_manager_set_active_item (PhotosBaseManager *self, PhotosBaseItem *item)
{
  PhotosBaseManagerPrivate *priv = self->priv;

  if (item == priv->active_item)
    return FALSE;

  if (priv->active_item != NULL)
    g_object_unref (priv->active_item);

  g_object_ref (item);
  priv->active_item = item;
  g_signal_emit (self, signals[ACTIVE_CHANGED], 0, item);
  return TRUE;
}


gboolean
photos_base_manager_set_active_item_by_id (PhotosBaseManager *self, const gchar *id)
{
  PhotosBaseItem *item;

  item = photos_base_manager_get_item_by_id (self, id);
  return photos_base_manager_set_active_item (self, item);
}
