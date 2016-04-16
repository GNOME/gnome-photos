/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2015 Alessandro Bono
 * Copyright © 2014 – 2016 Red Hat, Inc.
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
#include "photos-filterable.h"


struct _PhotosBaseModelPrivate
{
  GMenu *model;
  PhotosBaseManager *mngr;
};

enum
{
  PROP_0,
  PROP_MANAGER
};


G_DEFINE_TYPE_WITH_PRIVATE (PhotosBaseModel, photos_base_model, G_TYPE_OBJECT);


static void
photos_base_model_action_state_changed (PhotosBaseModel *self, const gchar *action_name, GVariant *value)
{
  const gchar *id;

  id = g_variant_get_string (value, NULL);
  photos_base_manager_set_active_object_by_id (self->priv->mngr, id);
}


static void
photos_base_model_active_changed (PhotosBaseModel *self, GObject *active_object)
{
  PhotosBaseModelPrivate *priv = self->priv;
  GApplication *app;
  GVariant *state;
  const gchar *action_id;
  const gchar *id;

  app = g_application_get_default ();
  action_id = photos_base_manager_get_action_id (priv->mngr);
  id = photos_filterable_get_id (PHOTOS_FILTERABLE (active_object));
  state = g_variant_new ("s", id);
  g_action_group_change_action_state (G_ACTION_GROUP (app), action_id, state);
}


static void
photos_base_model_refresh (PhotosBaseModel *self)
{
  PhotosBaseModelPrivate *priv = self->priv;
  GHashTable *objects;
  GHashTableIter hash_iter;
  GMenu *section;
  GObject *object;
  const gchar *action_id;
  const gchar *id;
  const gchar *title;

  g_menu_remove_all (priv->model);

  title = photos_base_manager_get_title (priv->mngr);
  action_id = photos_base_manager_get_action_id (priv->mngr);

  section = g_menu_new ();
  g_menu_append_section (priv->model, title, G_MENU_MODEL (section));

  objects = photos_base_manager_get_objects (priv->mngr);

  g_hash_table_iter_init (&hash_iter, objects);
  while (g_hash_table_iter_next (&hash_iter, (gpointer *) &id, (gpointer *) &object))
    {
      GMenuItem *menu_item;
      GVariant *target_value;
      gchar *name;

      g_object_get (object, "name", &name, NULL);

      menu_item = g_menu_item_new (name, NULL);
      target_value = g_variant_new ("s", id);
      g_menu_item_set_action_and_target_value (menu_item, action_id, target_value);
      g_menu_append_item (section, menu_item);

      g_free (name);
      g_object_unref (menu_item);
    }

  g_object_unref (section);
}


static void
photos_base_model_constructed (GObject *object)
{
  PhotosBaseModel *self = PHOTOS_BASE_MODEL (object);
  PhotosBaseModelPrivate *priv = self->priv;
  GApplication *app;
  const gchar *action_id;
  gchar *detailed_signal;

  G_OBJECT_CLASS (photos_base_model_parent_class)->constructed (object);

  priv->model = g_menu_new ();

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

  app = g_application_get_default ();
  action_id = photos_base_manager_get_action_id (priv->mngr);
  detailed_signal = g_strconcat ("action-state-changed::", action_id, NULL);
  g_signal_connect_object (app,
                           detailed_signal,
                           G_CALLBACK (photos_base_model_action_state_changed),
                           self,
                           G_CONNECT_SWAPPED);
  g_free (detailed_signal);

  g_signal_connect_object (priv->mngr,
                           "active-changed",
                           G_CALLBACK (photos_base_model_active_changed),
                           self,
                           G_CONNECT_SWAPPED);

  photos_base_model_refresh (self);
}


static void
photos_base_model_dispose (GObject *object)
{
  PhotosBaseModel *self = PHOTOS_BASE_MODEL (object);
  PhotosBaseModelPrivate *priv = self->priv;

  g_clear_object (&priv->model);
  g_clear_object (&priv->mngr);

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
  self->priv = photos_base_model_get_instance_private (self);
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


PhotosBaseModel *
photos_base_model_new (PhotosBaseManager *mngr)
{
  return g_object_new (PHOTOS_TYPE_BASE_MODEL, "manager", mngr, NULL);
}


GMenu *
photos_base_model_get_model (PhotosBaseModel *self)
{
  return self->priv->model;
}
