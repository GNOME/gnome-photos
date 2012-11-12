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

/* Based on code from:
 *   + Documents
 */


#include "config.h"

#include <glib.h>

#include "photos-base-manager.h"
#include "photos-filterable.h"


struct _PhotosBaseManagerPrivate
{
  GHashTable *objects;
  GObject *active_object;
};

enum
{
  ACTIVE_CHANGED,
  CLEAR,
  OBJECT_ADDED,
  OBJECT_REMOVED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };


G_DEFINE_TYPE (PhotosBaseManager, photos_base_manager, G_TYPE_OBJECT);


static gboolean
photos_base_manager_default_set_active_object (PhotosBaseManager *self, GObject *object)
{
  PhotosBaseManagerPrivate *priv = self->priv;

  if (object == priv->active_object)
    return FALSE;

  if (priv->active_object != NULL)
    g_object_unref (priv->active_object);

  if (object != NULL)
    g_object_ref (object);

  priv->active_object = object;
  g_signal_emit (self, signals[ACTIVE_CHANGED], 0, object);
  return TRUE;
}


static gchar *
photos_base_manager_get_all_filter (PhotosBaseManager *self)
{
  GList *l;
  GList *values;
  gchar *filter;
  gchar **strv;
  gchar *tmp;
  guint i;
  guint length;

  values = g_hash_table_get_values (self->priv->objects);
  length = g_list_length (values);
  strv = (gchar **) g_malloc0_n (length + 1, sizeof (gchar *));

  for (i = 0, l = values; l != NULL; l = l->next)
    {
      gchar *id;

      g_object_get (l->data, "id", &id, NULL);
      if (g_strcmp0 (id, "all") != 0)
        {
          strv[i] = photos_filterable_get_filter (PHOTOS_FILTERABLE (l->data));
          i++;
        }
      g_free (id);
    }

  filter = g_strjoinv (" || ", strv);
  g_strfreev (strv);

  tmp = filter;
  filter = g_strconcat ("(", filter, ")", NULL);
  g_free (tmp);

  g_list_free (values);
  return filter;
}


static void
photos_base_manager_dispose (GObject *object)
{
  PhotosBaseManager *self = PHOTOS_BASE_MANAGER (object);
  PhotosBaseManagerPrivate *priv = self->priv;

  if (priv->objects != NULL)
    {
      g_hash_table_unref (priv->objects);
      priv->objects = NULL;
    }

  g_clear_object (&priv->active_object);

  G_OBJECT_CLASS (photos_base_manager_parent_class)->dispose (object);
}


static void
photos_base_manager_init (PhotosBaseManager *self)
{
  PhotosBaseManagerPrivate *priv;

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, PHOTOS_TYPE_BASE_MANAGER, PhotosBaseManagerPrivate);
  priv = self->priv;

  priv->objects = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
}


static void
photos_base_manager_class_init (PhotosBaseManagerClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->dispose = photos_base_manager_dispose;
  class->set_active_object = photos_base_manager_default_set_active_object;

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
                                          G_TYPE_OBJECT);

  signals[CLEAR] = g_signal_new ("clear",
                                 G_TYPE_FROM_CLASS (class),
                                 G_SIGNAL_RUN_LAST,
                                 G_STRUCT_OFFSET (PhotosBaseManagerClass,
                                                  clear),
                                 NULL, /*accumulator */
                                 NULL, /*accu_data */
                                 g_cclosure_marshal_VOID__VOID,
                                 G_TYPE_NONE,
                                 0);

  signals[OBJECT_ADDED] = g_signal_new ("object-added",
                                        G_TYPE_FROM_CLASS (class),
                                        G_SIGNAL_RUN_LAST,
                                        G_STRUCT_OFFSET (PhotosBaseManagerClass,
                                                         object_added),
                                        NULL, /*accumulator */
                                        NULL, /* accu_data */
                                        g_cclosure_marshal_VOID__OBJECT,
                                        G_TYPE_NONE,
                                        1,
                                        G_TYPE_OBJECT);

  signals[OBJECT_REMOVED] = g_signal_new ("object-removed",
                                          G_TYPE_FROM_CLASS (class),
                                          G_SIGNAL_RUN_LAST,
                                          G_STRUCT_OFFSET (PhotosBaseManagerClass,
                                                           object_removed),
                                          NULL, /*accumulator */
                                          NULL, /*accu_data */
                                          g_cclosure_marshal_VOID__OBJECT,
                                          G_TYPE_NONE,
                                          1,
                                          G_TYPE_OBJECT);

  g_type_class_add_private (class, sizeof (PhotosBaseManagerPrivate));
}


void
photos_base_manager_add_object (PhotosBaseManager *self, GObject *object)
{
  gchar *id;

  g_object_get (object, "id", &id, NULL);

  g_hash_table_insert (self->priv->objects, (gpointer) id, g_object_ref (object));
  g_signal_emit (self, signals[OBJECT_ADDED], 0, object);
}


void
photos_base_manager_clear (PhotosBaseManager *self)
{
  PhotosBaseManagerPrivate *priv = self->priv;

  g_hash_table_remove_all (priv->objects);
  g_clear_object (&priv->active_object);
  g_signal_emit (self, signals[CLEAR], 0);
}


GObject *
photos_base_manager_get_active_object (PhotosBaseManager *self)
{
  return self->priv->active_object;
}


gchar *
photos_base_manager_get_filter (PhotosBaseManager *self)
{
  PhotosBaseManagerPrivate *priv = self->priv;
  const gchar *blank = "(true)";
  gchar *filter;
  gchar *id;

  if (priv->active_object == NULL)
    return g_strdup (blank);

  g_return_val_if_fail (PHOTOS_IS_FILTERABLE (priv->active_object), g_strdup (blank));

  g_object_get (priv->active_object, "id", &id, NULL);
  if (g_strcmp0 (id, "all") == 0)
    filter = photos_base_manager_get_all_filter (self);
  else
    filter = photos_filterable_get_filter (PHOTOS_FILTERABLE (priv->active_object));

  g_free (id);
  return filter;
}


GObject *
photos_base_manager_get_object_by_id (PhotosBaseManager *self, const gchar *id)
{
  return g_hash_table_lookup (self->priv->objects, id);
}


GHashTable *
photos_base_manager_get_objects (PhotosBaseManager *self)
{
  return self->priv->objects;
}


guint
photos_base_manager_get_objects_count (PhotosBaseManager *self)
{
  GList *keys;
  guint count;

  keys = g_hash_table_get_keys (self->priv->objects);
  count = g_list_length (keys);
  g_list_free (keys);
  return count;
}


void
photos_base_manager_remove_object (PhotosBaseManager *self, GObject *object)
{
  gchar *id;

  g_object_get (object, "id", &id, NULL);
  photos_base_manager_remove_object_by_id (self, id);
  g_free (id);
}


void
photos_base_manager_remove_object_by_id (PhotosBaseManager *self, const gchar *id)
{
  GObject *object;

  object = photos_base_manager_get_object_by_id (self, id);
  if (object == NULL)
    return;

  g_signal_emit (self, signals[OBJECT_REMOVED], 0, object);
  g_hash_table_remove (self->priv->objects, id);
}


gboolean
photos_base_manager_set_active_object (PhotosBaseManager *self, GObject *object)
{
  return PHOTOS_BASE_MANAGER_GET_CLASS (self)->set_active_object (self, object);
}


gboolean
photos_base_manager_set_active_object_by_id (PhotosBaseManager *self, const gchar *id)
{
  GObject *object;

  object = photos_base_manager_get_object_by_id (self, id);
  return photos_base_manager_set_active_object (self, object);
}
