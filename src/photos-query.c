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

#include "photos-base-manager.h"
#include "photos-query.h"
#include "photos-utils.h"


struct _PhotosQuery
{
  GObject parent_instance;
  PhotosSearchContextState *state;
  PhotosSource *source;
  gchar *sparql;
  gchar *tag;
};

enum
{
  PROP_0,
  PROP_SPARQL,
  PROP_STATE
};


G_DEFINE_TYPE (PhotosQuery, photos_query, G_TYPE_OBJECT);


const gchar *PHOTOS_QUERY_COLLECTIONS_IDENTIFIER = "photos:collection:";
const gchar *PHOTOS_QUERY_LOCAL_COLLECTIONS_IDENTIFIER = "photos:collection:local:";


static void
photos_query_constructed (GObject *object)
{
  PhotosQuery *self = PHOTOS_QUERY (object);

  G_OBJECT_CLASS (photos_query_parent_class)->constructed (object);

  if (self->state != NULL)
    {
      PhotosSource *source;

      source = PHOTOS_SOURCE (photos_base_manager_get_active_object (self->state->src_mngr));
      g_set_object (&self->source, source);
    }

  self->state = NULL; /* We will not need it any more */
}


static void
photos_query_dispose (GObject *object)
{
  PhotosQuery *self = PHOTOS_QUERY (object);

  g_clear_object (&self->source);

  G_OBJECT_CLASS (photos_query_parent_class)->dispose (object);
}


static void
photos_query_finalize (GObject *object)
{
  PhotosQuery *self = PHOTOS_QUERY (object);

  g_free (self->sparql);
  g_free (self->tag);

  G_OBJECT_CLASS (photos_query_parent_class)->finalize (object);
}


static void
photos_query_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
  PhotosQuery *self = PHOTOS_QUERY (object);

  switch (prop_id)
    {
    case PROP_SPARQL:
      g_value_set_string (value, self->sparql);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
photos_query_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
  PhotosQuery *self = PHOTOS_QUERY (object);

  switch (prop_id)
    {
    case PROP_SPARQL:
      self->sparql = g_value_dup_string (value);
      break;

    case PROP_STATE:
      self->state = (PhotosSearchContextState *) g_value_get_pointer (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
photos_query_init (PhotosQuery *self)
{
}


static void
photos_query_class_init (PhotosQueryClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->constructed = photos_query_constructed;
  object_class->dispose = photos_query_dispose;
  object_class->finalize = photos_query_finalize;
  object_class->get_property = photos_query_get_property;
  object_class->set_property = photos_query_set_property;

  g_object_class_install_property (object_class,
                                   PROP_SPARQL,
                                   g_param_spec_string ("sparql",
                                                        "SPARQL",
                                                        "A SPARQL query that's meant to be sent to Tracker",
                                                        NULL,
                                                        G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_STATE,
                                   g_param_spec_pointer ("state",
                                                         "State",
                                                         "The PhotosSearchContextState for this query",
                                                         G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE));
}


PhotosQuery *
photos_query_new (PhotosSearchContextState *state, const gchar *sparql)
{
  g_return_val_if_fail (sparql != NULL && sparql[0] != '\0', NULL);
  return g_object_new (PHOTOS_TYPE_QUERY, "state", state, "sparql", sparql, NULL);
}


const gchar *
photos_query_get_sparql (PhotosQuery *self)
{
  g_return_val_if_fail (PHOTOS_IS_QUERY (self), NULL);
  return self->sparql;
}


PhotosSource *
photos_query_get_source (PhotosQuery *self)
{
  g_return_val_if_fail (PHOTOS_IS_QUERY (self), NULL);
  return self->source;
}


const gchar *
photos_query_get_tag (PhotosQuery *self)
{
  g_return_val_if_fail (PHOTOS_IS_QUERY (self), NULL);
  return self->tag;
}


void
photos_query_set_tag (PhotosQuery *self, const gchar *tag)
{
  g_return_if_fail (PHOTOS_IS_QUERY (self));
  g_return_if_fail (tag != NULL && tag[0] != '\0');

  photos_utils_set_string (&self->tag, tag);
}
