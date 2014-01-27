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

#include "photos-filterable.h"
#include "photos-search-match.h"


struct _PhotosSearchMatchPrivate
{
  gchar *filter;
  gchar *id;
  gchar *name;
  gchar *term;
};

enum
{
  PROP_0,
  PROP_FILTER,
  PROP_ID,
  PROP_NAME,
};

static void photos_filterable_interface_init (PhotosFilterableInterface *iface);


G_DEFINE_TYPE_WITH_CODE (PhotosSearchMatch, photos_search_match, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (PhotosSearchMatch)
                         G_IMPLEMENT_INTERFACE (PHOTOS_TYPE_FILTERABLE,
                                                photos_filterable_interface_init));


static gchar *
photos_search_match_get_filter (PhotosFilterable *iface)
{
  PhotosSearchMatch *self = PHOTOS_SEARCH_MATCH (iface);
  PhotosSearchMatchPrivate *priv = self->priv;

  return g_strdup_printf (priv->filter, priv->term);
}


static void
photos_search_match_finalize (GObject *object)
{
  PhotosSearchMatch *self = PHOTOS_SEARCH_MATCH (object);
  PhotosSearchMatchPrivate *priv = self->priv;

  g_free (priv->filter);
  g_free (priv->id);
  g_free (priv->name);
  g_free (priv->term);

  G_OBJECT_CLASS (photos_search_match_parent_class)->finalize (object);
}


static void
photos_search_match_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
  PhotosSearchMatch *self = PHOTOS_SEARCH_MATCH (object);
  PhotosSearchMatchPrivate *priv = self->priv;

  switch (prop_id)
    {
    case PROP_ID:
      g_value_set_string (value, priv->id);
      break;

    case PROP_NAME:
      g_value_set_string (value, priv->name);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
photos_search_match_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
  PhotosSearchMatch *self = PHOTOS_SEARCH_MATCH (object);
  PhotosSearchMatchPrivate *priv = self->priv;

  switch (prop_id)
    {
    case PROP_FILTER:
      priv->filter = g_value_dup_string (value);
      break;

    case PROP_ID:
      priv->id = g_value_dup_string (value);
      break;

    case PROP_NAME:
      priv->name = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
photos_search_match_init (PhotosSearchMatch *self)
{
  PhotosSearchMatchPrivate *priv;

  self->priv = photos_search_match_get_instance_private (self);
  priv = self->priv;

  priv->term = g_strdup ("");
}


static void
photos_search_match_class_init (PhotosSearchMatchClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->finalize = photos_search_match_finalize;
  object_class->get_property = photos_search_match_get_property;
  object_class->set_property = photos_search_match_set_property;

  g_object_class_install_property (object_class,
                                   PROP_FILTER,
                                   g_param_spec_string ("filter",
                                                        "",
                                                        "",
                                                        "(true)",
                                                        G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE));

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
}


static void
photos_filterable_interface_init (PhotosFilterableInterface *iface)
{
  iface->get_filter = photos_search_match_get_filter;
}


PhotosSearchMatch *
photos_search_match_new (const gchar *id, const gchar *name, const gchar *filter)
{
  return g_object_new (PHOTOS_TYPE_SEARCH_MATCH, "id", id, "name", name, "filter", filter, NULL);
}


void
photos_search_match_set_filter_term (PhotosSearchMatch *self, const gchar *term)
{
  PhotosSearchMatchPrivate *priv = self->priv;

  if (g_strcmp0 (priv->term, term) == 0)
    return;

  g_free (priv->term);
  priv->term = g_strdup (term);
}
