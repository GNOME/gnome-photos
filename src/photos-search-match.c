/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2014 – 2017 Red Hat, Inc.
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
#include "photos-search-match.h"


struct _PhotosSearchMatch
{
  GObject parent_instance;
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

static void photos_search_match_filterable_iface_init (PhotosFilterableInterface *iface);


G_DEFINE_TYPE_WITH_CODE (PhotosSearchMatch, photos_search_match, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (PHOTOS_TYPE_FILTERABLE,
                                                photos_search_match_filterable_iface_init));


static gchar *
photos_search_match_get_filter (PhotosFilterable *iface)
{
  PhotosSearchMatch *self = PHOTOS_SEARCH_MATCH (iface);
  char *ret_val;

  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wformat-nonliteral"

  ret_val = g_strdup_printf (self->filter, self->term);

  #pragma GCC diagnostic pop

  return ret_val;
}


static const gchar *
photos_search_match_get_id (PhotosFilterable *filterable)
{
  PhotosSearchMatch *self = PHOTOS_SEARCH_MATCH (filterable);
  return self->id;
}


static void
photos_search_match_finalize (GObject *object)
{
  PhotosSearchMatch *self = PHOTOS_SEARCH_MATCH (object);

  g_free (self->filter);
  g_free (self->id);
  g_free (self->name);
  g_free (self->term);

  G_OBJECT_CLASS (photos_search_match_parent_class)->finalize (object);
}


static void
photos_search_match_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
  PhotosSearchMatch *self = PHOTOS_SEARCH_MATCH (object);

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
photos_search_match_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
  PhotosSearchMatch *self = PHOTOS_SEARCH_MATCH (object);

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

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
photos_search_match_init (PhotosSearchMatch *self)
{
  self->term = g_strdup ("");
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
photos_search_match_filterable_iface_init (PhotosFilterableInterface *iface)
{
  iface->get_filter = photos_search_match_get_filter;
  iface->get_id = photos_search_match_get_id;
}


PhotosSearchMatch *
photos_search_match_new (const gchar *id, const gchar *name, const gchar *filter)
{
  return g_object_new (PHOTOS_TYPE_SEARCH_MATCH, "id", id, "name", name, "filter", filter, NULL);
}


void
photos_search_match_set_filter_term (PhotosSearchMatch *self, const gchar *term)
{
  if (g_strcmp0 (self->term, term) == 0)
    return;

  g_free (self->term);
  self->term = g_strdup (term);
}
