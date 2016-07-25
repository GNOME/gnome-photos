/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2012 – 2016 Red Hat, Inc.
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
  gchar *action_id;
  gchar *title;
};

enum
{
  PROP_0,
  PROP_ACTION_ID,
  PROP_TITLE
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


G_DEFINE_TYPE_WITH_PRIVATE (PhotosBaseManager, photos_base_manager, G_TYPE_OBJECT);


static void
photos_base_manager_default_add_object (PhotosBaseManager *self, GObject *object)
{
  PhotosBaseManagerPrivate *priv;
  GObject *old_object;
  const gchar *id;

  priv = photos_base_manager_get_instance_private (self);

  id = photos_filterable_get_id (PHOTOS_FILTERABLE (object));
  old_object = photos_base_manager_get_object_by_id (self, id);
  if (old_object != NULL)
    return;

  g_hash_table_insert (priv->objects, g_strdup (id), g_object_ref (object));
  g_signal_emit (self, signals[OBJECT_ADDED], 0, object);
}


static gchar *
photos_base_manager_default_get_filter (PhotosBaseManager *self, gint flags)
{
  return g_strdup ("(true)");
}


static GObject *
photos_base_manager_default_get_object_by_id (PhotosBaseManager *self, const gchar *id)
{
  PhotosBaseManagerPrivate *priv;
  GObject *ret_val = NULL;

  priv = photos_base_manager_get_instance_private (self);

  if (id == NULL)
    goto out;

  ret_val = g_hash_table_lookup (priv->objects, id);

 out:
  return ret_val;
}


static gchar *
photos_base_manager_default_get_where (PhotosBaseManager *self, gint flags)
{
  return g_strdup ("");
}


static void
photos_base_manager_default_remove_object_by_id (PhotosBaseManager *self, const gchar *id)
{
  PhotosBaseManagerPrivate *priv;
  GObject *object;

  priv = photos_base_manager_get_instance_private (self);

  object = photos_base_manager_get_object_by_id (self, id);
  if (object == NULL)
    return;

  g_object_ref (object);
  g_hash_table_remove (priv->objects, id);
  g_signal_emit (self, signals[OBJECT_REMOVED], 0, object);
  g_object_unref (object);
}


static gboolean
photos_base_manager_default_set_active_object (PhotosBaseManager *self, GObject *object)
{
  PhotosBaseManagerPrivate *priv;

  priv = photos_base_manager_get_instance_private (self);

  if (!g_set_object (&priv->active_object, object))
    return FALSE;

  g_signal_emit (self, signals[ACTIVE_CHANGED], 0, object);
  return TRUE;
}


static void
photos_base_manager_dispose (GObject *object)
{
  PhotosBaseManager *self = PHOTOS_BASE_MANAGER (object);
  PhotosBaseManagerPrivate *priv;

  priv = photos_base_manager_get_instance_private (self);

  if (priv->objects != NULL)
    {
      g_hash_table_unref (priv->objects);
      priv->objects = NULL;
    }

  g_clear_object (&priv->active_object);

  G_OBJECT_CLASS (photos_base_manager_parent_class)->dispose (object);
}


static void
photos_base_manager_finalize (GObject *object)
{
  PhotosBaseManager *self = PHOTOS_BASE_MANAGER (object);
  PhotosBaseManagerPrivate *priv;

  priv = photos_base_manager_get_instance_private (self);

  g_free (priv->action_id);
  g_free (priv->title);

  G_OBJECT_CLASS (photos_base_manager_parent_class)->finalize (object);
}


static void
photos_base_manager_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
  PhotosBaseManager *self = PHOTOS_BASE_MANAGER (object);
  PhotosBaseManagerPrivate *priv;

  priv = photos_base_manager_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_ACTION_ID:
      priv->action_id = g_value_dup_string (value);
      break;

    case PROP_TITLE:
      priv->title = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
photos_base_manager_init (PhotosBaseManager *self)
{
  PhotosBaseManagerPrivate *priv;

  priv = photos_base_manager_get_instance_private (self);
  priv->objects = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
}


static void
photos_base_manager_class_init (PhotosBaseManagerClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->dispose = photos_base_manager_dispose;
  object_class->finalize = photos_base_manager_finalize;
  object_class->set_property = photos_base_manager_set_property;
  class->add_object = photos_base_manager_default_add_object;
  class->get_filter = photos_base_manager_default_get_filter;
  class->get_object_by_id = photos_base_manager_default_get_object_by_id;
  class->get_where = photos_base_manager_default_get_where;
  class->remove_object_by_id = photos_base_manager_default_remove_object_by_id;
  class->set_active_object = photos_base_manager_default_set_active_object;

  g_object_class_install_property (object_class,
                                   PROP_ACTION_ID,
                                   g_param_spec_string ("action_id",
                                                        "Action ID",
                                                        "GAction ID for search options",
                                                        NULL,
                                                        G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE));

  g_object_class_install_property (object_class,
                                   PROP_TITLE,
                                   g_param_spec_string ("title",
                                                        "Title",
                                                        "The name of this manager",
                                                        NULL,
                                                        G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE));

  signals[ACTIVE_CHANGED] = g_signal_new ("active-changed",
                                          G_TYPE_FROM_CLASS (class),
                                          G_SIGNAL_RUN_LAST,
                                          G_STRUCT_OFFSET (PhotosBaseManagerClass, active_changed),
                                          NULL, /*accumulator */
                                          NULL, /*accu_data */
                                          g_cclosure_marshal_VOID__OBJECT,
                                          G_TYPE_NONE,
                                          1,
                                          G_TYPE_OBJECT);

  signals[CLEAR] = g_signal_new ("clear",
                                 G_TYPE_FROM_CLASS (class),
                                 G_SIGNAL_RUN_LAST,
                                 G_STRUCT_OFFSET (PhotosBaseManagerClass, clear),
                                 NULL, /*accumulator */
                                 NULL, /*accu_data */
                                 g_cclosure_marshal_VOID__VOID,
                                 G_TYPE_NONE,
                                 0);

  signals[OBJECT_ADDED] = g_signal_new ("object-added",
                                        G_TYPE_FROM_CLASS (class),
                                        G_SIGNAL_RUN_LAST,
                                        G_STRUCT_OFFSET (PhotosBaseManagerClass, object_added),
                                        NULL, /*accumulator */
                                        NULL, /* accu_data */
                                        g_cclosure_marshal_VOID__OBJECT,
                                        G_TYPE_NONE,
                                        1,
                                        G_TYPE_OBJECT);

  signals[OBJECT_REMOVED] = g_signal_new ("object-removed",
                                          G_TYPE_FROM_CLASS (class),
                                          G_SIGNAL_RUN_LAST,
                                          G_STRUCT_OFFSET (PhotosBaseManagerClass, object_removed),
                                          NULL, /*accumulator */
                                          NULL, /*accu_data */
                                          g_cclosure_marshal_VOID__OBJECT,
                                          G_TYPE_NONE,
                                          1,
                                          G_TYPE_OBJECT);
}


void
photos_base_manager_add_object (PhotosBaseManager *self, GObject *object)
{
  PHOTOS_BASE_MANAGER_GET_CLASS (self)->add_object (self, object);
}


void
photos_base_manager_clear (PhotosBaseManager *self)
{
  PhotosBaseManagerPrivate *priv;

  priv = photos_base_manager_get_instance_private (self);

  g_hash_table_remove_all (priv->objects);
  g_clear_object (&priv->active_object);
  g_signal_emit (self, signals[CLEAR], 0);
}


const gchar *
photos_base_manager_get_action_id (PhotosBaseManager *self)
{
  PhotosBaseManagerPrivate *priv;

  priv = photos_base_manager_get_instance_private (self);
  return priv->action_id;
}


GObject *
photos_base_manager_get_active_object (PhotosBaseManager *self)
{
  PhotosBaseManagerPrivate *priv;

  priv = photos_base_manager_get_instance_private (self);
  return priv->active_object;
}


gchar *
photos_base_manager_get_all_filter (PhotosBaseManager *self)
{
  PhotosBaseManagerPrivate *priv;
  GList *l;
  GList *values;
  const gchar *blank = "(true)";
  gchar *filter;
  gchar **strv;
  gchar *tmp;
  guint i;
  guint length;

  priv = photos_base_manager_get_instance_private (self);

  values = g_hash_table_get_values (priv->objects);
  length = g_list_length (values);
  strv = (gchar **) g_malloc0_n (length + 1, sizeof (gchar *));

  for (i = 0, l = values; l != NULL; l = l->next)
    {
      PhotosFilterable *filterable = PHOTOS_FILTERABLE (l->data);
      const gchar *id;

      id = photos_filterable_get_id (filterable);
      if (g_strcmp0 (id, "all") != 0)
        {
          gchar *str;

          str = photos_filterable_get_filter (filterable);
          if (g_strcmp0 (str, blank) == 0)
            g_free (str);
          else
            {
              strv[i] = str;
              i++;
            }
        }
    }

  length = g_strv_length (strv);
  if (length == 0)
    strv[0] = g_strdup (blank);

  filter = g_strjoinv (" || ", strv);
  g_strfreev (strv);

  tmp = filter;
  filter = g_strconcat ("(", filter, ")", NULL);
  g_free (tmp);

  g_list_free (values);
  return filter;
}


gchar *
photos_base_manager_get_filter (PhotosBaseManager *self, gint flags)
{
  return PHOTOS_BASE_MANAGER_GET_CLASS (self)->get_filter (self, flags);
}


GObject *
photos_base_manager_get_object_by_id (PhotosBaseManager *self, const gchar *id)
{
  return PHOTOS_BASE_MANAGER_GET_CLASS (self)->get_object_by_id (self, id);
}


GHashTable *
photos_base_manager_get_objects (PhotosBaseManager *self)
{
  PhotosBaseManagerPrivate *priv;

  priv = photos_base_manager_get_instance_private (self);
  return priv->objects;
}


guint
photos_base_manager_get_objects_count (PhotosBaseManager *self)
{
  PhotosBaseManagerPrivate *priv;
  guint count;

  priv = photos_base_manager_get_instance_private (self);
  count = g_hash_table_size (priv->objects);
  return count;
}


const gchar *
photos_base_manager_get_title (PhotosBaseManager *self)
{
  PhotosBaseManagerPrivate *priv;

  priv = photos_base_manager_get_instance_private (self);
  return priv->title;
}


gchar *
photos_base_manager_get_where (PhotosBaseManager *self, gint flags)
{
  return PHOTOS_BASE_MANAGER_GET_CLASS (self)->get_where (self, flags);
}


void
photos_base_manager_process_new_objects (PhotosBaseManager *self, GHashTable *new_objects)
{
  GHashTable *old_objects;
  GHashTableIter iter;
  GObject *object;
  const gchar *id;

  old_objects = photos_base_manager_get_objects (self);

  g_hash_table_iter_init (&iter, old_objects);
  while (g_hash_table_iter_next (&iter, (gpointer *) &id, (gpointer *) &object))
    {
      gboolean builtin;

      /* If old objects are not found in the newer hash table, remove
       * them.
       */
      builtin = photos_filterable_get_builtin (PHOTOS_FILTERABLE (object));
      if (g_hash_table_lookup (new_objects, id) == NULL && !builtin)
        {
          g_object_ref (object);
          g_hash_table_iter_remove (&iter);
          g_signal_emit (self, signals[OBJECT_REMOVED], 0, object);
          g_object_unref (object);
        }
    }

  g_hash_table_iter_init (&iter, new_objects);
  while (g_hash_table_iter_next (&iter, (gpointer *) &id, (gpointer *) &object))
    {
      /* If new items are not found in the older hash table, add
       * them.
       */
      if (g_hash_table_lookup (old_objects, id) == NULL)
        photos_base_manager_add_object (self, object);
    }

  /* TODO: merge existing item properties with new values. */
}


void
photos_base_manager_remove_object (PhotosBaseManager *self, GObject *object)
{
  const gchar *id;

  g_return_if_fail (PHOTOS_IS_BASE_MANAGER (self));
  g_return_if_fail (PHOTOS_IS_FILTERABLE (object));

  id = photos_filterable_get_id (PHOTOS_FILTERABLE (object));
  photos_base_manager_remove_object_by_id (self, id);
}


void
photos_base_manager_remove_object_by_id (PhotosBaseManager *self, const gchar *id)
{
  g_return_if_fail (PHOTOS_IS_BASE_MANAGER (self));
  g_return_if_fail (id != NULL && id[0] != '\0');

  PHOTOS_BASE_MANAGER_GET_CLASS (self)->remove_object_by_id (self, id);
}


gboolean
photos_base_manager_set_active_object (PhotosBaseManager *self, GObject *object)
{
  g_return_val_if_fail (PHOTOS_IS_BASE_MANAGER (self), FALSE);
  g_return_val_if_fail (PHOTOS_IS_FILTERABLE (object), FALSE);

  return PHOTOS_BASE_MANAGER_GET_CLASS (self)->set_active_object (self, object);
}


gboolean
photos_base_manager_set_active_object_by_id (PhotosBaseManager *self, const gchar *id)
{
  GObject *object;

  g_return_val_if_fail (PHOTOS_IS_BASE_MANAGER (self), FALSE);
  g_return_val_if_fail (id != NULL && id[0] != '\0', FALSE);

  object = photos_base_manager_get_object_by_id (self, id);
  return photos_base_manager_set_active_object (self, object);
}
