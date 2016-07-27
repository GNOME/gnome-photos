/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2016 Red Hat, Inc.
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

#include "photos-filterable.h"
#include "photos-share-point-online.h"


struct _PhotosSharePointOnlinePrivate
{
  GIcon *icon;
  PhotosSource *source;
  gchar *id;
  gchar *name;
};

enum
{
  PROP_0,
  PROP_SOURCE,
};

static void photos_filterable_interface_init (PhotosFilterableInterface *iface);


G_DEFINE_ABSTRACT_TYPE_WITH_CODE (PhotosSharePointOnline, photos_share_point_online, PHOTOS_TYPE_SHARE_POINT,
                                  G_ADD_PRIVATE (PhotosSharePointOnline)
                                  G_IMPLEMENT_INTERFACE (PHOTOS_TYPE_FILTERABLE, photos_filterable_interface_init));


static GIcon *
photos_share_point_online_get_icon (PhotosSharePoint *share_point)
{
  PhotosSharePointOnline *self = PHOTOS_SHARE_POINT_ONLINE (share_point);
  PhotosSharePointOnlinePrivate *priv;

  priv = photos_share_point_online_get_instance_private (self);
  return priv->icon;
}


static const gchar *
photos_share_point_online_get_id (PhotosFilterable *filterable)
{
  PhotosSharePointOnline *self = PHOTOS_SHARE_POINT_ONLINE (filterable);
  PhotosSharePointOnlinePrivate *priv;

  priv = photos_share_point_online_get_instance_private (self);
  return priv->id;
}


static const gchar *
photos_share_point_online_get_name (PhotosSharePoint *share_point)
{
  PhotosSharePointOnline *self = PHOTOS_SHARE_POINT_ONLINE (share_point);
  PhotosSharePointOnlinePrivate *priv;

  priv = photos_share_point_online_get_instance_private (self);
  return priv->name;
}


static void
photos_share_point_online_dispose (GObject *object)
{
  PhotosSharePointOnline *self = PHOTOS_SHARE_POINT_ONLINE (object);
  PhotosSharePointOnlinePrivate *priv;

  priv = photos_share_point_online_get_instance_private (self);

  g_clear_object (&priv->icon);
  g_clear_object (&priv->source);

  G_OBJECT_CLASS (photos_share_point_online_parent_class)->dispose (object);
}


static void
photos_share_point_online_finalize (GObject *object)
{
  PhotosSharePointOnline *self = PHOTOS_SHARE_POINT_ONLINE (object);
  PhotosSharePointOnlinePrivate *priv;

  priv = photos_share_point_online_get_instance_private (self);

  g_free (priv->id);
  g_free (priv->name);

  G_OBJECT_CLASS (photos_share_point_online_parent_class)->finalize (object);
}


static void
photos_share_point_online_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
  PhotosSharePointOnline *self = PHOTOS_SHARE_POINT_ONLINE (object);
  PhotosSharePointOnlinePrivate *priv;

  priv = photos_share_point_online_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_SOURCE:
      g_value_set_object (value, priv->source);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
photos_share_point_online_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
  PhotosSharePointOnline *self = PHOTOS_SHARE_POINT_ONLINE (object);
  PhotosSharePointOnlinePrivate *priv;

  priv = photos_share_point_online_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_SOURCE:
      {
        GIcon *icon;
        const gchar *id;
        const gchar *name;

        priv->source = PHOTOS_SOURCE (g_value_dup_object (value));
        if (priv->source == NULL)
          break;

        id = photos_filterable_get_id (PHOTOS_FILTERABLE (priv->source));
        priv->id = g_strdup (id);

        icon = photos_source_get_icon (priv->source);
        priv->icon = g_object_ref (icon);

        name = photos_source_get_name (priv->source);
        priv->name = g_strdup (name);
        break;
      }

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
photos_share_point_online_init (PhotosSharePointOnline *self)
{
}


static void
photos_share_point_online_class_init (PhotosSharePointOnlineClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  PhotosSharePointClass *share_point_class = PHOTOS_SHARE_POINT_CLASS (class);

  object_class->dispose = photos_share_point_online_dispose;
  object_class->finalize = photos_share_point_online_finalize;
  object_class->get_property = photos_share_point_online_get_property;
  object_class->set_property = photos_share_point_online_set_property;
  share_point_class->get_icon = photos_share_point_online_get_icon;
  share_point_class->get_name = photos_share_point_online_get_name;

  g_object_class_install_property (object_class,
                                   PROP_SOURCE,
                                   g_param_spec_object ("source",
                                                        "PhotosSource instance",
                                                        "The online source corresponding to this share point",
                                                        PHOTOS_TYPE_SOURCE,
                                                        G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE));
}


static void
photos_filterable_interface_init (PhotosFilterableInterface *iface)
{
  iface->get_id = photos_share_point_online_get_id;
}


PhotosSource *
photos_share_point_online_get_source (PhotosSharePointOnline *self)
{
  PhotosSharePointOnlinePrivate *priv;

  priv = photos_share_point_online_get_instance_private (self);
  return priv->source;
}
