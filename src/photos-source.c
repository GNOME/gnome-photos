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

#include <gio/gio.h>

#include "photos-filterable.h"
#include "photos-query-builder.h"
#include "photos-source.h"


struct _PhotosSourcePrivate
{
  GIcon *icon;
  gboolean builtin;
  gchar *id;
  gchar *name;
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


static gchar *
photos_source_build_filter_resource (PhotosSource *self)
{
  PhotosSourcePrivate *priv = self->priv;
  gchar *filter;

  if (!priv->builtin)
    filter = g_strdup_printf ("(nie:dataSource (?urn) = \"%s\")", priv->id);
  else
    filter = g_strdup ("(false)");

  return filter;
}


static gchar *
photos_source_get_filter (PhotosFilterable *iface)
{
  PhotosSource *self = PHOTOS_SOURCE (iface);
  PhotosSourcePrivate *priv = self->priv;

  if (g_strcmp0 (priv->id, PHOTOS_SOURCE_STOCK_LOCAL) == 0)
    return photos_query_builder_filter_local ();

  if (g_strcmp0 (priv->id, PHOTOS_SOURCE_STOCK_ALL) == 0)
    return photos_query_builder_filter_local (); /* TODO: Add non local query */

  return photos_source_build_filter_resource (self);
}


static void
photos_source_dispose (GObject *object)
{
  PhotosSource *self = PHOTOS_SOURCE (object);
  PhotosSourcePrivate *priv = self->priv;

  g_clear_object (&priv->icon);

  G_OBJECT_CLASS (photos_source_parent_class)->dispose (object);
}


static void
photos_source_finalize (GObject *object)
{
  PhotosSource *self = PHOTOS_SOURCE (object);
  PhotosSourcePrivate *priv = self->priv;

  g_free (priv->id);
  g_free (priv->name);

  G_OBJECT_CLASS (photos_source_parent_class)->finalize (object);
}


static void
photos_source_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
  PhotosSource *self = PHOTOS_SOURCE (object);
  PhotosSourcePrivate *priv = self->priv;

  switch (prop_id)
    {
    case PROP_BUILTIN:
      priv->builtin = g_value_get_boolean (value);
      break;

    case PROP_ID:
      priv->id = g_value_dup_string (value);
      break;

    case PROP_NAME:
      priv->name = g_value_dup_string (value);
      break;

    case PROP_OBJECT:
      {
        GoaAccount *account;
        GoaObject *object;
        const gchar *id;
        const gchar *provider_icon;
        const gchar *provider_name;

        object = GOA_OBJECT (g_value_get_object (value));
        if (object == NULL)
          break;

        account = goa_object_peek_account (object);

        id = goa_account_get_id (account);
        priv->id = g_strdup_printf ("gp:goa-account:%s", id);

        provider_icon = goa_account_get_provider_icon (account);
        priv->icon = g_icon_new_for_string (provider_icon, NULL); /* TODO: use a GError */

        provider_name = goa_account_get_provider_name (account);
        priv->name = g_strdup (provider_name);
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
  PhotosSourcePrivate *priv;

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, PHOTOS_TYPE_SOURCE, PhotosSourcePrivate);
  priv = self->priv;
}


static void
photos_source_class_init (PhotosSourceClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->dispose = photos_source_dispose;
  object_class->finalize = photos_source_finalize;
  object_class->set_property = photos_source_set_property;

  g_object_class_install_property (object_class,
                                   PROP_BUILTIN,
                                   g_param_spec_boolean ("builtin",
                                                         "",
                                                         "",
                                                         FALSE,
                                                         G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE));

  g_object_class_install_property (object_class,
                                   PROP_ID,
                                   g_param_spec_string ("id",
                                                        "",
                                                        "",
                                                        "",
                                                        G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE));

  g_object_class_install_property (object_class,
                                   PROP_NAME,
                                   g_param_spec_string ("name",
                                                        "",
                                                        "",
                                                        "",
                                                        G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE));

  g_object_class_install_property (object_class,
                                   PROP_OBJECT,
                                   g_param_spec_object ("object",
                                                        "GoaObject instance",
                                                        "A GOA configured account from which the source was created",
                                                        GOA_TYPE_OBJECT,
                                                        G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE));

  g_type_class_add_private (class, sizeof (PhotosSourcePrivate));
}


static void
photos_filterable_interface_init (PhotosFilterableInterface *iface)
{
  iface->get_filter = photos_source_get_filter;
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
