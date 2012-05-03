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


struct _PhotosItemManagerPrivate
{
  GtkListStore *model;
};


G_DEFINE_TYPE (PhotosItemManager, photos_item_manager, PHOTOS_TYPE_BASE_MANAGER);


static gboolean
photos_item_manager_set_active_object (PhotosBaseManager *manager, GObject *object)
{
  gboolean ret_val;

  g_return_val_if_fail (PHOTOS_IS_BASE_ITEM (object), FALSE);

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

  g_clear_object (&self->priv->model);

  G_OBJECT_CLASS (photos_item_manager_parent_class)->dispose (object);
}


static void
photos_item_manager_init (PhotosItemManager *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, PHOTOS_TYPE_ITEM_MANAGER, PhotosItemManagerPrivate);
  self->priv->model = photos_item_model_new ();
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
  /* TODO: create local or other items */
  return NULL;
}


GtkListStore *
photos_item_manager_get_model (PhotosItemManager *self)
{
  return self->priv->model;
}
