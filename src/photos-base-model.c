/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2015 Alessandro Bono
 * Copyright © 2014 – 2017 Red Hat, Inc.
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


struct _PhotosBaseModel
{
  GObject parent_instance;
  GMenu *model;
  PhotosBaseManager *mngr;
};

enum
{
  PROP_0,
  PROP_MANAGER
};


G_DEFINE_TYPE (PhotosBaseModel, photos_base_model, G_TYPE_OBJECT);


static void
photos_base_model_action_state_changed (PhotosBaseModel *self, const gchar *action_name, GVariant *value)
{
  const gchar *id;

  id = g_variant_get_string (value, NULL);
  photos_base_manager_set_active_object_by_id (self->mngr, id);
}


static void
photos_base_model_active_changed (PhotosBaseModel *self, GObject *active_object)
{
  GApplication *app;
  GVariant *state;
  const gchar *action_id;
  const gchar *id;

  app = g_application_get_default ();
  action_id = photos_base_manager_get_action_id (self->mngr);
  id = photos_filterable_get_id (PHOTOS_FILTERABLE (active_object));
  state = g_variant_new ("s", id);
  g_action_group_change_action_state (G_ACTION_GROUP (app), action_id, state);
}


static void
photos_base_model_refresh (PhotosBaseModel *self)
{
  g_autoptr (GMenu) section = NULL;
  const gchar *action_id;
  const gchar *title;
  guint i;
  guint n_items;

  g_menu_remove_all (self->model);

  title = photos_base_manager_get_title (self->mngr);
  action_id = photos_base_manager_get_action_id (self->mngr);

  section = g_menu_new ();
  g_menu_append_section (self->model, title, G_MENU_MODEL (section));

  n_items = g_list_model_get_n_items (G_LIST_MODEL (self->mngr));
  for (i = 0; i < n_items; i++)
    {
      g_autoptr (GMenuItem) menu_item = NULL;
      g_autoptr (GObject) object = NULL;
      const gchar *id;
      g_autofree gchar *name = NULL;

      object = g_list_model_get_object (G_LIST_MODEL (self->mngr), i);
      id = photos_filterable_get_id (PHOTOS_FILTERABLE (object));
      g_object_get (object, "name", &name, NULL);

      menu_item = g_menu_item_new (name, NULL);
      g_menu_item_set_action_and_target (menu_item, action_id, "s", id);
      g_menu_append_item (section, menu_item);
    }
}


static void
photos_base_model_constructed (GObject *object)
{
  PhotosBaseModel *self = PHOTOS_BASE_MODEL (object);
  GApplication *app;
  const gchar *action_id;
  g_autofree gchar *detailed_signal = NULL;

  G_OBJECT_CLASS (photos_base_model_parent_class)->constructed (object);

  self->model = g_menu_new ();

  g_signal_connect_object (self->mngr,
                           "object-added",
                           G_CALLBACK (photos_base_model_refresh),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (self->mngr,
                           "object-removed",
                           G_CALLBACK (photos_base_model_refresh),
                           self,
                           G_CONNECT_SWAPPED);

  app = g_application_get_default ();
  action_id = photos_base_manager_get_action_id (self->mngr);
  detailed_signal = g_strconcat ("action-state-changed::", action_id, NULL);
  g_signal_connect_object (app,
                           detailed_signal,
                           G_CALLBACK (photos_base_model_action_state_changed),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->mngr,
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

  g_clear_object (&self->model);
  g_clear_object (&self->mngr);

  G_OBJECT_CLASS (photos_base_model_parent_class)->dispose (object);
}


static void
photos_base_model_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
  PhotosBaseModel *self = PHOTOS_BASE_MODEL (object);

  switch (prop_id)
    {
    case PROP_MANAGER:
      self->mngr = PHOTOS_BASE_MANAGER (g_value_dup_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
photos_base_model_init (PhotosBaseModel *self)
{
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
  return self->model;
}
