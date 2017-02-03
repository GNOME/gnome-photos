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

#include "photos-filterable.h"
#include "photos-search-type.h"


struct _PhotosSearchType
{
  GObject parent_instance;
  gchar *filter;
  gchar *id;
  gchar *name;
  gchar *where;
};

struct _PhotosSearchTypeClass
{
  GObjectClass parent_class;
};

enum
{
  PROP_0,
  PROP_FILTER,
  PROP_ID,
  PROP_NAME,
  PROP_WHERE,
};

static void photos_search_type_filterable_iface_init (PhotosFilterableInterface *iface);


G_DEFINE_TYPE_WITH_CODE (PhotosSearchType, photos_search_type, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (PHOTOS_TYPE_FILTERABLE,
                                                photos_search_type_filterable_iface_init));


static gchar *
photos_search_type_get_filter (PhotosFilterable *iface)
{
  PhotosSearchType *self = PHOTOS_SEARCH_TYPE (iface);
  return g_strdup (self->filter);
}


static const gchar *
photos_search_type_get_id (PhotosFilterable *filterable)
{
  PhotosSearchType *self = PHOTOS_SEARCH_TYPE (filterable);
  return self->id;
}


static gchar *
photos_search_type_get_where (PhotosFilterable *iface)
{
  PhotosSearchType *self = PHOTOS_SEARCH_TYPE (iface);
  return g_strdup (self->where);
}


static void
photos_search_type_finalize (GObject *object)
{
  PhotosSearchType *self = PHOTOS_SEARCH_TYPE (object);

  g_free (self->filter);
  g_free (self->id);
  g_free (self->name);
  g_free (self->where);

  G_OBJECT_CLASS (photos_search_type_parent_class)->finalize (object);
}


static void
photos_search_type_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
  PhotosSearchType *self = PHOTOS_SEARCH_TYPE (object);

  switch (prop_id)
    {
    case PROP_ID:
      g_value_set_string (value, self->id);
      break;

    case PROP_NAME:
      g_value_set_string (value, self->name);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
photos_search_type_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
  PhotosSearchType *self = PHOTOS_SEARCH_TYPE (object);

  switch (prop_id)
    {
    case PROP_FILTER:
      self->filter = g_value_dup_string (value);
      break;

    case PROP_ID:
      self->id = g_value_dup_string (value);
      break;

    case PROP_NAME:
      self->name = g_value_dup_string (value);
      break;

    case PROP_WHERE:
      self->where = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
photos_search_type_init (PhotosSearchType *self)
{
}


static void
photos_search_type_class_init (PhotosSearchTypeClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->finalize = photos_search_type_finalize;
  object_class->get_property = photos_search_type_get_property;
  object_class->set_property = photos_search_type_set_property;

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

  g_object_class_install_property (object_class,
                                   PROP_WHERE,
                                   g_param_spec_string ("where",
                                                        "",
                                                        "",
                                                        "",
                                                        G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE));
}


static void
photos_search_type_filterable_iface_init (PhotosFilterableInterface *iface)
{
  iface->get_filter = photos_search_type_get_filter;
  iface->get_id = photos_search_type_get_id;
  iface->get_where = photos_search_type_get_where;
}


PhotosSearchType *
photos_search_type_new (const gchar *id, const gchar *name)
{
  return g_object_new (PHOTOS_TYPE_SEARCH_TYPE, "id", id, "name", name, NULL);
}


PhotosSearchType *
photos_search_type_new_full (const gchar *id, const gchar *name, const gchar *where, const gchar *filter)
{
  return g_object_new (PHOTOS_TYPE_SEARCH_TYPE,
                       "id", id,
                       "name", name,
                       "filter", filter,
                       "where", where,
                       NULL);
}
