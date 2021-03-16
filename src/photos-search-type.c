/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2012 – 2021 Red Hat, Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
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
  PhotosSparqlTemplate *sparql_template;
  gchar *id;
  gchar *name;
};

enum
{
  PROP_0,
  PROP_ID,
  PROP_NAME,
  PROP_SPARQL_TEMPLATE,
};

static void photos_search_type_filterable_iface_init (PhotosFilterableInterface *iface);


G_DEFINE_TYPE_WITH_CODE (PhotosSearchType, photos_search_type, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (PHOTOS_TYPE_FILTERABLE,
                                                photos_search_type_filterable_iface_init));


static const gchar *
photos_search_type_get_id (PhotosFilterable *filterable)
{
  PhotosSearchType *self = PHOTOS_SEARCH_TYPE (filterable);
  return self->id;
}


static gboolean
photos_search_type_is_search_criterion (PhotosFilterable *iface)
{
  return TRUE;
}


PhotosSparqlTemplate *
photos_search_type_get_sparql_template (PhotosSearchType *self)
{
  return self->sparql_template;
}


static void
photos_search_type_dispose (GObject *object)
{
  PhotosSearchType *self = PHOTOS_SEARCH_TYPE (object);

  g_clear_object (&self->sparql_template);

  G_OBJECT_CLASS (photos_search_type_parent_class)->dispose (object);
}


static void
photos_search_type_finalize (GObject *object)
{
  PhotosSearchType *self = PHOTOS_SEARCH_TYPE (object);

  g_free (self->id);
  g_free (self->name);

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

    case PROP_SPARQL_TEMPLATE:
      g_value_set_object (value, self->sparql_template);
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
    case PROP_ID:
      self->id = g_value_dup_string (value);
      break;

    case PROP_NAME:
      self->name = g_value_dup_string (value);
      break;

    case PROP_SPARQL_TEMPLATE:
      self->sparql_template = PHOTOS_SPARQL_TEMPLATE (g_value_dup_object (value));
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

  object_class->dispose = photos_search_type_dispose;
  object_class->finalize = photos_search_type_finalize;
  object_class->get_property = photos_search_type_get_property;
  object_class->set_property = photos_search_type_set_property;

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
                                   PROP_SPARQL_TEMPLATE,
                                   g_param_spec_object ("sparql-template",
                                                        "",
                                                        "",
                                                        PHOTOS_TYPE_SPARQL_TEMPLATE,
                                                        G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE));
}


static void
photos_search_type_filterable_iface_init (PhotosFilterableInterface *iface)
{
  iface->get_id = photos_search_type_get_id;
  iface->is_search_criterion = photos_search_type_is_search_criterion;
}


PhotosSearchType *
photos_search_type_new (const gchar *id, const gchar *name, PhotosSparqlTemplate *sparql_template)

{
  return g_object_new (PHOTOS_TYPE_SEARCH_TYPE,
                       "id", id,
                       "name", name,
                       "sparql-template", sparql_template,
                       NULL);
}
