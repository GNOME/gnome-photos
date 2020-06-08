/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2012 – 2019 Red Hat, Inc.
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

#include <string.h>

#include "photos-base-manager.h"
#include "photos-query.h"
#include "photos-query-builder.h"
#include "photos-search-type.h"
#include "photos-source-manager.h"
#include "photos-search-match-manager.h"
#include "photos-search-type-manager.h"

#define PHOTOS_QUERY_COLLECTIONS_IDENTIFIER "photos:collection:"
#define PHOTOS_QUERY_LOCAL_COLLECTIONS_IDENTIFIER "photos:collection:local:"

const gchar *collections_default_filter = \
  "(fn:starts-with (nao:identifier (?urn), '" PHOTOS_QUERY_COLLECTIONS_IDENTIFIER "')"
  "   || (?urn = nfo:image-category-screenshot))";


/* This includes mimetype blocklist */
const gchar *photos_default_filter = \
  "(nie:mimeType(?urn) != 'image/gif' && nie:mimeType(?urn) != 'image/x-eps')";


static gchar *
photos_query_builder_query (PhotosSearchContextState *state,
                            gboolean global,
                            gint flags,
                            PhotosOffsetController *offset_cntrlr)
{
  PhotosSparqlTemplate *template;
  const gchar *projection = NULL;
  g_autofree gchar *item_pattern = NULL;
  g_autofree gchar *search_filter = NULL;
  g_autofree gchar *source_filter = NULL;
  const gchar *order = NULL;
  g_autofree gchar *offset_limit = NULL;
  gchar *sparql;

  template = photos_base_manager_get_sparql_template (state->srch_typ_mngr, flags);

  projection = "?urn "
               "nie:url (?urn) "
               "nfo:fileName (?urn) "
               "nie:mimeType (?urn) "
               "nie:title (?urn) "
               "tracker:coalesce (nco:fullname (?creator), nco:fullname (?publisher), '') "
               "tracker:coalesce (nfo:fileLastModified (?urn), nie:contentLastModified (?urn)) AS ?mtime "
               "nao:identifier (?urn) "
               "rdf:type (?urn) "
               "nie:dataSource(?urn) "
               "( EXISTS { ?urn nao:hasTag nao:predefined-tag-favorite } ) "
               "( EXISTS { ?urn nco:contributor ?contributor FILTER ( ?contributor != ?creator ) } ) "
               "tracker:coalesce(nfo:fileCreated (?urn), nie:contentCreated (?urn)) "
               "nfo:width (?urn) "
               "nfo:height (?urn) "
               "nfo:equipment (?urn) "
               "nfo:orientation (?urn) "
               "nmm:exposureTime (?urn) "
               "nmm:fnumber (?urn) "
               "nmm:focalLength (?urn) "
               "nmm:isoSpeed (?urn) "
               "nmm:flash (?urn) "
               "slo:location (?urn) ";

  item_pattern = photos_base_manager_get_where (state->item_mngr, flags);

  if (!(flags & PHOTOS_QUERY_FLAGS_UNFILTERED))
    {
      source_filter = photos_base_manager_get_filter (state->src_mngr, flags);
      search_filter = photos_base_manager_get_filter (state->srch_mtch_mngr, flags);
    }

  order = "ORDER BY DESC (?mtime)";

  if (global && (flags & PHOTOS_QUERY_FLAGS_UNLIMITED) == 0)
    {
      gint offset = 0;
      gint step = 60;

      if (offset_cntrlr != NULL)
        {
          offset = photos_offset_controller_get_offset (offset_cntrlr);
          step = photos_offset_controller_get_step (offset_cntrlr);
        }

      offset_limit = g_strdup_printf ("LIMIT %d OFFSET %d", step, offset);
    }

  sparql = photos_sparql_template_get_sparql (template,
                                              "projection", projection,
                                              "collections_default_filter", collections_default_filter,
                                              "item_pattern", item_pattern,
                                              "photos_default_filter", photos_default_filter,
                                              "source_filter", source_filter ? source_filter : "",
                                              "search_filter", search_filter ? search_filter : "",
                                              "order", order,
                                              "offset_limit", offset_limit ? offset_limit : "",
                                              NULL);

  return sparql;
}


PhotosQuery *
photos_query_builder_create_collection_query (PhotosSearchContextState *state,
                                              const gchar *name,
                                              const gchar *identifier_tag)
{
  PhotosQuery *query;
  g_autofree gchar *identifier = NULL;
  g_autofree gchar *sparql = NULL;
  g_autoptr (GDateTime) datetime = NULL;
  g_autofree gchar *time = NULL;

  identifier = g_strdup_printf ("%s%s",
                                PHOTOS_QUERY_LOCAL_COLLECTIONS_IDENTIFIER,
                                identifier_tag == NULL ? name : identifier_tag);

  datetime = g_date_time_new_now_utc ();
  time = g_date_time_format_iso8601 (datetime);

  sparql = g_strdup_printf ("INSERT { _:res a nfo:DataContainer ; a nie:DataObject ; "
                            "nie:contentLastModified '%s' ; "
                            "nie:title '%s' ; "
                            "nao:identifier '%s' }",
                            time,
                            name,
                            identifier);

  query = photos_query_new (state, sparql);

  return query;
}


PhotosQuery *
photos_query_builder_collection_icon_query (PhotosSearchContextState *state, const gchar *resource)
{
  PhotosQuery *query;
  g_autofree gchar *sparql = NULL;

  sparql = g_strdup_printf ("SELECT ?urn "
                            "tracker:coalesce(nfo:fileLastModified(?urn), nie:contentLastModified(?urn)) AS ?mtime "
                            "WHERE { ?urn nie:isPartOf <%s> } "
                            "ORDER BY DESC (?mtime) LIMIT 4",
                            resource);

  query = photos_query_new (state, sparql);

  return query;
}


PhotosQuery *
photos_query_builder_count_query (PhotosSearchContextState *state, gint flags)
{
  PhotosSparqlTemplate *template;
  const gchar *projection = NULL;
  g_autofree gchar *item_pattern = NULL;
  g_autofree gchar *search_filter = NULL;
  g_autofree gchar *source_filter = NULL;
  g_autofree gchar *sparql = NULL;
  PhotosQuery *query;

  template = photos_base_manager_get_sparql_template (state->srch_typ_mngr, flags);

  projection = "COUNT(?urn) ";

  item_pattern = photos_base_manager_get_where (state->item_mngr, flags);

  if (! (flags & PHOTOS_QUERY_FLAGS_UNFILTERED))
    {
      source_filter = photos_base_manager_get_filter (state->src_mngr, flags);
      search_filter = photos_base_manager_get_filter (state->srch_mtch_mngr, flags);
    }

  sparql = photos_sparql_template_get_sparql (template,
                                              "projection", projection,
                                              "collections_default_filter", collections_default_filter,
                                              "item_pattern", item_pattern,
                                              "photos_default_filter", photos_default_filter,
                                              "source_filter", source_filter ? source_filter : "",
                                              "search_filter", search_filter ? search_filter : "",
                                              "order", "",
                                              "offset_limit", "",
                                              NULL);

  query = photos_query_new (state, sparql);

  return query;
}


PhotosQuery *
photos_query_builder_delete_resource_query (PhotosSearchContextState *state, const gchar *resource)
{
  PhotosQuery *query;
  g_autofree gchar *sparql = NULL;

  sparql = g_strdup_printf ("DELETE { <%s> a rdfs:Resource }", resource);
  query = photos_query_new (state, sparql);

  return query;
}


PhotosQuery *
photos_query_builder_equipment_query (PhotosSearchContextState *state, GQuark equipment)
{
  PhotosQuery *query;
  const gchar *resource;
  g_autofree gchar *sparql = NULL;

  resource = g_quark_to_string (equipment);
  sparql = g_strdup_printf ("SELECT nfo:manufacturer (<%s>) nfo:model (<%s>) WHERE {}", resource, resource);
  query = photos_query_new (state, sparql);

  return query;
}


PhotosQuery *
photos_query_builder_fetch_collections_for_urn_query (PhotosSearchContextState *state, const gchar *resource)
{
  PhotosQuery *query;
  g_autofree gchar *sparql = NULL;

  sparql = g_strdup_printf ("SELECT ?urn WHERE { ?urn a nfo:DataContainer . <%s> nie:isPartOf ?urn }", resource);
  query = photos_query_new (state, sparql);

  return query;
}


PhotosQuery *
photos_query_builder_fetch_collections_local (PhotosSearchContextState *state)
{
  PhotosQuery *query;
  g_autofree gchar *sparql = NULL;

  sparql = photos_query_builder_query (state,
                                       TRUE,
                                       PHOTOS_QUERY_FLAGS_COLLECTIONS
                                       | PHOTOS_QUERY_FLAGS_LOCAL
                                       | PHOTOS_QUERY_FLAGS_UNLIMITED,
                                       NULL);

  query = photos_query_new (NULL, sparql);

  return query;
}


PhotosQuery *
photos_query_builder_global_query (PhotosSearchContextState *state,
                                   gint flags,
                                   PhotosOffsetController *offset_cntrlr)
{
  PhotosQuery *query;
  g_autofree gchar *sparql = NULL;

  sparql = photos_query_builder_query (state, TRUE, flags, offset_cntrlr);
  query = photos_query_new (state, sparql);

  return query;
}


PhotosQuery *
photos_query_builder_location_query (PhotosSearchContextState *state, const gchar *location_urn)
{
  PhotosQuery *query;
  g_autofree gchar *sparql = NULL;

  sparql = g_strdup_printf ("SELECT slo:latitude (<%s>) slo:longitude (<%s>) WHERE {}", location_urn, location_urn);
  query = photos_query_new (state, sparql);

  return query;
}


PhotosQuery *
photos_query_builder_set_collection_query (PhotosSearchContextState *state,
                                           const gchar *item_urn,
                                           const gchar *collection_urn,
                                           gboolean setting)
{
  PhotosQuery *query;
  g_autofree gchar *sparql = NULL;

  sparql = g_strdup_printf ("%s { <%s> nie:isPartOf <%s> }",
                            setting ? "INSERT" : "DELETE",
                            item_urn,
                            collection_urn);
  query = photos_query_new (state, sparql);

  return query;
}


PhotosQuery *
photos_query_builder_single_query (PhotosSearchContextState *state, gint flags, const gchar *resource)
{
  g_autoptr (GRegex) regex = NULL;
  PhotosQuery *query;
  g_autofree gchar *replacement = NULL;
  g_autofree gchar *sparql = NULL;
  g_autofree gchar *tmp = NULL;

  tmp = photos_query_builder_query (state, FALSE, flags, NULL);

  regex = g_regex_new ("\\?urn", 0, 0, NULL);
  replacement = g_strconcat ("<", resource, ">", NULL);
  sparql = g_regex_replace (regex, tmp, -1, 0, replacement, 0, NULL);
  query = photos_query_new (state, sparql);

  return query;
}


PhotosQuery *
photos_query_builder_update_mtime_query (PhotosSearchContextState *state, const gchar *resource)
{
  PhotosQuery *query;
  g_autofree gchar *sparql = NULL;
  g_autoptr (GDateTime) datetime = NULL;
  g_autofree gchar *time = NULL;

  datetime = g_date_time_new_now_utc ();
  time = g_date_time_format_iso8601 (datetime);

  sparql = g_strdup_printf ("INSERT OR REPLACE { <%s> nie:contentLastModified '%s' }", resource, time);
  query = photos_query_new (state, sparql);

  return query;
}
