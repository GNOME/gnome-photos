/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2014 Red Hat, Inc.
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

#include "photos-base-model.h"


struct _PhotosBaseModelPrivate
{
  PhotosBaseManager *mngr;
};

enum
{
  PROP_0,
  PROP_MANAGER
};


G_DEFINE_TYPE_WITH_PRIVATE (PhotosBaseModel, photos_base_model, GTK_TYPE_LIST_STORE);


static void
photos_base_model_refresh (PhotosBaseModel *self)
{
  PhotosBaseModelPrivate *priv = self->priv;
  GHashTable *objects;
  GHashTableIter hash_iter;
  GObject *object;
  GtkTreeIter model_iter;
  const gchar *id;
  const gchar *title;

  gtk_list_store_clear (GTK_LIST_STORE (self));

  title = photos_base_manager_get_title (priv->mngr);
  if (title != NULL)
    {
      gtk_list_store_append (GTK_LIST_STORE (self), &model_iter);
      gtk_list_store_set (GTK_LIST_STORE (self),
                          &model_iter,
                          PHOTOS_BASE_MODEL_ID, "heading",
                          PHOTOS_BASE_MODEL_NAME, "",
                          PHOTOS_BASE_MODEL_HEADING_TEXT, title,
                          -1);
    }

  objects = photos_base_manager_get_objects (priv->mngr);

  g_hash_table_iter_init (&hash_iter, objects);
  while (g_hash_table_iter_next (&hash_iter, (gpointer *) &id, (gpointer *) &object))
    {
      gchar *name;

      gtk_list_store_append (GTK_LIST_STORE (self), &model_iter);

      g_object_get (object, "name", &name, NULL);
      gtk_list_store_set (GTK_LIST_STORE (self),
                          &model_iter,
                          PHOTOS_BASE_MODEL_ID, id,
                          PHOTOS_BASE_MODEL_NAME, name,
                          PHOTOS_BASE_MODEL_HEADING_TEXT, "",
                          -1);
      g_free (name);
    }
}


static void
photos_base_model_constructed (GObject *object)
{
  PhotosBaseModel *self = PHOTOS_BASE_MODEL (object);
  PhotosBaseModelPrivate *priv = self->priv;

  G_OBJECT_CLASS (photos_base_model_parent_class)->constructed (object);

  g_signal_connect_object (priv->mngr,
                           "object-added",
                           G_CALLBACK (photos_base_model_refresh),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (priv->mngr,
                           "object-removed",
                           G_CALLBACK (photos_base_model_refresh),
                           self,
                           G_CONNECT_SWAPPED);

  photos_base_model_refresh (self);
}


static void
photos_base_model_dispose (GObject *object)
{
  PhotosBaseModel *self = PHOTOS_BASE_MODEL (object);

  g_clear_object (&self->priv->mngr);

  G_OBJECT_CLASS (photos_base_model_parent_class)->dispose (object);
}


static void
photos_base_model_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
  PhotosBaseModel *self = PHOTOS_BASE_MODEL (object);

  switch (prop_id)
    {
    case PROP_MANAGER:
      self->priv->mngr = PHOTOS_BASE_MANAGER (g_value_dup_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
photos_base_model_init (PhotosBaseModel *self)
{
  GType columns[] = {G_TYPE_STRING,    /* ID */
                     G_TYPE_STRING,    /* NAME */
                     G_TYPE_STRING};   /* HEADING TEXT */

  self->priv = photos_base_model_get_instance_private (self);

  gtk_list_store_set_column_types (GTK_LIST_STORE (self), sizeof (columns) / sizeof (columns[0]), columns);
}


static void
photos_base_model_class_init (PhotosBaseModelClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->constructed = photos_base_model_constructed;
  object_class->dispose = photos_base_model_dispose;
  object_class->set_property = photos_base_model_set_property;

  g_object_class_install_property (object_class,
                                   PROP_MANAGER,
                                   g_param_spec_object ("manager",
                                                        "PhotosBaseManager object",
                                                        "The manager whose data is held by this model",
                                                        PHOTOS_TYPE_BASE_MANAGER,
                                                        G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE));
}


GtkListStore *
photos_base_model_new (PhotosBaseManager *mngr)
{
  return g_object_new (PHOTOS_TYPE_BASE_MODEL, "manager", mngr, NULL);
}
