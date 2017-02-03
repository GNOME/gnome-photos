/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2012 – 2017 Red Hat, Inc.
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

#include <gio/gio.h>

#include "egg-counter.h"
#include "photos-filterable.h"
#include "photos-query-builder.h"
#include "photos-source.h"


struct _PhotosSource
{
  GObject parent_instance;
  GIcon *icon;
  GoaObject *object;
  gboolean builtin;
  gchar *id;
  gchar *name;
};

struct _PhotosSourceClass
{
  GObjectClass parent_class;
};

enum
{
  PROP_0,
  PROP_BUILTIN,
  PROP_ID,
  PROP_NAME,
  PROP_OBJECT
};

static void photos_filterable_interface_init (PhotosFilterableInterface *iface);


G_DEFINE_TYPE_WITH_CODE (PhotosSource, photos_source, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (PHOTOS_TYPE_FILTERABLE,
                                                photos_filterable_interface_init));
EGG_DEFINE_COUNTER (instances, "PhotosSource", "Instances", "Number of PhotosSource instances")


static gchar *
photos_source_build_filter_resource (PhotosSource *self)
{
  gchar *filter;

  if (!self->builtin)
    filter = g_strdup_printf ("(nie:dataSource (?urn) = '%s')", self->id);
  else
    filter = g_strdup ("(false)");

  return filter;
}


static gboolean
photos_source_get_builtin (PhotosFilterable *iface)
{
  PhotosSource *self = PHOTOS_SOURCE (iface);
  return self->builtin;
}


static gchar *
photos_source_get_filter (PhotosFilterable *iface)
{
  PhotosSource *self = PHOTOS_SOURCE (iface);

  g_assert_cmpstr (self->id, !=, PHOTOS_SOURCE_STOCK_ALL);

  if (g_strcmp0 (self->id, PHOTOS_SOURCE_STOCK_LOCAL) == 0)
    return photos_query_builder_filter_local ();

  return photos_source_build_filter_resource (self);
}


static const gchar *
photos_source_get_id (PhotosFilterable *filterable)
{
  PhotosSource *self = PHOTOS_SOURCE (filterable);
  return self->id;
}


static void
photos_source_dispose (GObject *object)
{
  PhotosSource *self = PHOTOS_SOURCE (object);

  g_clear_object (&self->icon);
  g_clear_object (&self->object);

  G_OBJECT_CLASS (photos_source_parent_class)->dispose (object);
}


static void
photos_source_finalize (GObject *object)
{
  PhotosSource *self = PHOTOS_SOURCE (object);

  g_free (self->id);
  g_free (self->name);

  G_OBJECT_CLASS (photos_source_parent_class)->finalize (object);

  EGG_COUNTER_DEC (instances);
}


static void
photos_source_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
  PhotosSource *self = PHOTOS_SOURCE (object);

  switch (prop_id)
    {
    case PROP_BUILTIN:
      g_value_set_boolean (value, self->builtin);
      break;

    case PROP_ID:
      g_value_set_string (value, self->id);
      break;

    case PROP_NAME:
      g_value_set_string (value, self->name);
      break;

    case PROP_OBJECT:
      g_value_set_object (value, (gpointer) self->object);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
photos_source_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
  PhotosSource *self = PHOTOS_SOURCE (object);

  switch (prop_id)
    {
    case PROP_BUILTIN:
      self->builtin = g_value_get_boolean (value);
      break;

    case PROP_ID:
      self->id = g_value_dup_string (value);
      break;

    case PROP_NAME:
      self->name = g_value_dup_string (value);
      break;

    case PROP_OBJECT:
      {
        GoaAccount *account;
        const gchar *provider_icon;
        const gchar *provider_name;

        self->object = GOA_OBJECT (g_value_dup_object (value));
        if (self->object == NULL)
          break;

        account = goa_object_peek_account (self->object);
        self->id = g_strdup_printf ("gd:goa-account:%s", goa_account_get_id (account));

        provider_icon = goa_account_get_provider_icon (account);
        self->icon = g_icon_new_for_string (provider_icon, NULL); /* TODO: use a GError */

        provider_name = goa_account_get_provider_name (account);
        self->name = g_strdup (provider_name);
        break;
      }

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
photos_source_init (PhotosSource *self)
{
  EGG_COUNTER_INC (instances);
}


static void
photos_source_class_init (PhotosSourceClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->dispose = photos_source_dispose;
  object_class->finalize = photos_source_finalize;
  object_class->get_property = photos_source_get_property;
  object_class->set_property = photos_source_set_property;

  g_object_class_install_property (object_class,
                                   PROP_BUILTIN,
                                   g_param_spec_boolean ("builtin",
                                                         "",
                                                         "",
                                                         FALSE,
                                                         G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_ID,
                                   g_param_spec_string ("id",
                                                        "",
                                                        "",
                                                        NULL,
                                                        G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_NAME,
                                   g_param_spec_string ("name",
                                                        "",
                                                        "",
                                                        NULL,
                                                        G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_OBJECT,
                                   g_param_spec_object ("object",
                                                        "GoaObject instance",
                                                        "A GOA configured account from which the source was created",
                                                        GOA_TYPE_OBJECT,
                                                        G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE));
}


static void
photos_filterable_interface_init (PhotosFilterableInterface *iface)
{
  iface->get_builtin = photos_source_get_builtin;
  iface->get_filter = photos_source_get_filter;
  iface->get_id = photos_source_get_id;
}


PhotosSource *
photos_source_new (const gchar *id, const gchar *name, gboolean builtin)
{
  return g_object_new (PHOTOS_TYPE_SOURCE, "id", id, "name", name, "builtin", builtin, NULL);
}


PhotosSource *
photos_source_new_from_goa_object (GoaObject *object)
{
  g_return_val_if_fail (GOA_IS_OBJECT (object), NULL);
  return g_object_new (PHOTOS_TYPE_SOURCE, "object", object, NULL);
}


const gchar *
photos_source_get_name (PhotosSource *self)
{
  return self->name;
}


GoaObject *
photos_source_get_goa_object (PhotosSource *self)
{
  return self->object;
}


GIcon *
photos_source_get_icon (PhotosSource *self)
{
  return self->icon;
}
