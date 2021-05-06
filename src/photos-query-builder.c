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

#include <string.h>

#include <tracker-sparql.h>

#include "photos-application.h"
#include "photos-base-manager.h"
#include "photos-query-builder.h"
#include "photos-search-type.h"
#include "photos-source-manager.h"
#include "photos-search-match-manager.h"
#include "photos-search-type-manager.h"


static const gchar *BLOCKED_MIME_TYPES_FILTER = "(nie:mimeType(?urn) != 'image/gif' "
                                                "&& nie:mimeType(?urn) != 'image/x-eps')";

static const gchar *COLLECTIONS_FILTER
  = "(fn:starts-with (nao:identifier (?urn), '" PHOTOS_QUERY_COLLECTIONS_IDENTIFIER "')"
    " || (?urn = nfo:image-category-screenshot))";


static gchar *
photos_query_builder_query (PhotosSearchContextState *state,
                            const gchar *values,
                            gint flags,
                            PhotosOffsetController *offset_cntrlr)
{
  GApplication *app;
  PhotosSparqlTemplate *sparql_template;
  const gchar *miner_files_name;

  /* These are the final list of properties used to create or update
   * an item. It is a superset of the properties that come from the
   * canonical source of an item. eg., an item's canonical source
   * might be the D-Bus endpoint, but its nao:predefined-tag-favorite
   * will always be in the private database.
   */
  const gchar *projection
    = "?urn "
      "?file "
      "?filename "
      "?mimetype "
      "?title "
      "?author "
      "?mtime "
      "?identifier "
      "?type "
      "?datasource "
      "( EXISTS { ?urn nao:hasTag nao:predefined-tag-favorite } ) "
      "?has_contributor "
      "?ctime "
      "?width "
      "?height "
      "?equipment "
      "?orientation "
      "?exposure_time "
      "?fnumber "
      "?focal_length "
      "?isospeed "
      "?flash "
      "?location ";

  /* These properties come from the canonical source of an item. eg.,
   * for local items the canonical source is the D-Bus endpoint, and
   * for user-created collections it's the private database.
   */
  const gchar *projection_database
    = "?urn "
      "?file "
      "nfo:fileName (?file) AS ?filename "
      "nie:mimeType (?urn) AS ?mimetype "
      "nie:title (?urn) AS ?title "
      "tracker:coalesce (nco:fullname (?creator), nco:fullname (?publisher), '') AS ?author "
      "tracker:coalesce (nfo:fileLastModified (?file), nie:contentLastModified (?urn)) AS ?mtime "
      "nao:identifier (?urn) AS ?identifier "
      "rdf:type (?urn) AS ?type "
      "nie:dataSource(?urn) AS ?datasource "
      "( EXISTS { ?urn nco:contributor ?contributor FILTER ( ?contributor != ?creator ) } ) AS ?has_contributor "
      "tracker:coalesce(nie:contentCreated (?urn), nfo:fileCreated (?file)) AS ?ctime "
      "nfo:width (?urn) AS ?width "
      "nfo:height (?urn) AS ?height "
      "nfo:equipment (?urn) AS ?equipment "
      "nfo:orientation (?urn) AS ?orientation "
      "nmm:exposureTime (?urn) AS ?exposure_time "
      "nmm:fnumber (?urn) AS ?fnumber "
      "nmm:focalLength (?urn) AS ?focal_length "
      "nmm:isoSpeed (?urn) AS ?isospeed "
      "nmm:flash (?urn) AS ?flash "
      "slo:location (?urn) AS ?location ";

  /* This merely forwards the properties coming from the canonical
   * source of an item until they can be aggregated into the final
   * list that's used to create or update an item.
   */
  const gchar *projection_forwarded
    = "?urn "
      "?file "
      "?filename "
      "?mimetype "
      "?title "
      "?author "
      "?mtime "
      "?identifier "
      "?type "
      "?datasource "
      "?has_contributor "
      "?ctime "
      "?width "
      "?height "
      "?equipment "
      "?orientation "
      "?exposure_time "
      "?fnumber "
      "?focal_length "
      "?isospeed "
      "?flash "
      "?location ";

  g_autofree gchar *item_mngr_where = NULL;
  g_autofree gchar *offset_limit = NULL;
  g_autofree gchar *src_mngr_filter = NULL;
  g_autofree gchar *srch_mtch_mngr_filter = NULL;
  gchar *sparql;

  app = g_application_get_default ();
  miner_files_name = photos_application_get_miner_files_name (PHOTOS_APPLICATION (app));

  sparql_template = photos_base_manager_get_sparql_template (state->srch_typ_mngr, flags);

  if (!(flags & PHOTOS_QUERY_FLAGS_UNFILTERED))
    {
      if (values == NULL)
        {
          /* TODO: SearchCategoryManager */

          item_mngr_where = photos_base_manager_get_where (state->item_mngr, flags);
        }

      src_mngr_filter = photos_base_manager_get_filter (state->src_mngr, flags);
      srch_mtch_mngr_filter = photos_base_manager_get_filter (state->srch_mtch_mngr, flags);
    }

  if (values == NULL && (flags & PHOTOS_QUERY_FLAGS_UNLIMITED) == 0)
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

  sparql
    = photos_sparql_template_get_sparql (sparql_template,
                                         "blocked_mime_types_filter", BLOCKED_MIME_TYPES_FILTER,
                                         "collections_filter", COLLECTIONS_FILTER,
                                         "item_where", item_mngr_where == NULL ? "" : item_mngr_where,
                                         "miner_files_name", miner_files_name,
                                         "order", "ORDER BY DESC (?ctime) DESC (?mtime)",
                                         "offset_limit", offset_limit ? offset_limit : "",
                                         "projection", projection,
                                         "projection_dbus", projection_database,
                                         "projection_forwarded", projection_forwarded,
                                         "projection_private", projection_database,
                                         "search_match_filter", srch_mtch_mngr_filter == NULL
                                                                ? "(true)"
                                                                : srch_mtch_mngr_filter,
                                         "source_filter", src_mngr_filter == NULL ? "(true)" : src_mngr_filter,
                                         "values", values == NULL ? "" : values,
                                         NULL);

  return sparql;
}


PhotosQuery *
photos_query_builder_create_collection_query (PhotosSearchContextState *state,
                                              const gchar *name,
                                              const gchar *identifier_tag)
{
  g_autoptr (GDateTime) now = NULL;
  PhotosQuery *query;
  g_autoptr (TrackerResource) collection = NULL;
  g_autofree gchar *identifier = NULL;
  g_autofree gchar *sparql = NULL;
  g_autofree gchar *time = NULL;

  identifier = g_strdup_printf ("%s%s",
                                PHOTOS_QUERY_LOCAL_COLLECTIONS_IDENTIFIER,
                                identifier_tag == NULL ? name : identifier_tag);

  now = g_date_time_new_now_utc ();
  time = g_date_time_format_iso8601 (now);

  collection = tracker_resource_new ("_:res");
  tracker_resource_add_uri (collection, "rdf:type", "nfo:DataContainer");
  tracker_resource_add_uri (collection, "rdf:type", "nie:DataObject");
  tracker_resource_set_string (collection, "nie:contentLastModified", time);
  tracker_resource_set_string (collection, "nie:title", name);
  tracker_resource_set_string (collection, "nao:identifier", identifier);

  sparql = tracker_resource_print_sparql_update (collection, NULL, "tracker:Pictures");
  query = photos_query_new (state, sparql);

  return query;
}


PhotosQuery *
photos_query_builder_collection_icon_query (PhotosSearchContextState *state, const gchar *resource)
{
  GApplication *app;
  PhotosQuery *query;
  const gchar *miner_files_name;
  g_autofree gchar *sparql = NULL;

  app = g_application_get_default ();
  miner_files_name = photos_application_get_miner_files_name (PHOTOS_APPLICATION (app));

  sparql
    = g_strdup_printf ("SELECT ?urn "
                       "tracker:coalesce(nfo:fileLastModified(?file), nie:contentLastModified(?urn)) AS ?mtime "
                       "WHERE {"
                       "  SERVICE <dbus:%s> {"
                       "    GRAPH tracker:Pictures {"
                       "      SELECT ?urn WHERE { ?urn a nmm:Photo ; nie:isStoredAs ?file . }"
                       "    }"
                       "  }"
                       "  ?urn nie:isLogicalPartOf <%s> . "
                       "}"
                       "ORDER BY DESC (?mtime) LIMIT 4",
                       miner_files_name,
                       resource);

  query = photos_query_new (state, sparql);
  return query;
}


PhotosQuery *
photos_query_builder_count_query (PhotosSearchContextState *state, gint flags)
{
  GApplication *app;
  PhotosQuery *query;
  PhotosSparqlTemplate *sparql_template;
  const gchar *miner_files_name;
  g_autofree gchar *item_mngr_where = NULL;
  g_autofree gchar *src_mngr_filter = NULL;
  g_autofree gchar *srch_mtch_mngr_filter = NULL;
  g_autofree gchar *sparql = NULL;

  app = g_application_get_default ();
  miner_files_name = photos_application_get_miner_files_name (PHOTOS_APPLICATION (app));

  sparql_template = photos_base_manager_get_sparql_template (state->srch_typ_mngr, flags);

  if ((flags & PHOTOS_QUERY_FLAGS_UNFILTERED) == 0)
    {
      item_mngr_where = photos_base_manager_get_where (state->item_mngr, flags);
      src_mngr_filter = photos_base_manager_get_filter (state->src_mngr, flags);
      srch_mtch_mngr_filter = photos_base_manager_get_filter (state->srch_mtch_mngr, flags);
    }

  sparql
    = photos_sparql_template_get_sparql (sparql_template,
                                         "blocked_mime_types_filter", BLOCKED_MIME_TYPES_FILTER,
                                         "collections_filter", COLLECTIONS_FILTER,
                                         "item_where", item_mngr_where == NULL ? "" : item_mngr_where,
                                         "miner_files_name", miner_files_name,
                                         "order", "",
                                         "offset_limit", "",
                                         "projection", "COUNT(?urn)",
                                         "projection_dbus", "?urn",
                                         "projection_forwarded", "?urn",
                                         "projection_private", "?urn",
                                         "search_match_filter", srch_mtch_mngr_filter == NULL
                                                                ? "(true)"
                                                                : srch_mtch_mngr_filter,
                                         "source_filter", src_mngr_filter == NULL ? "(true)" : src_mngr_filter,
                                         "values", "",
                                         NULL);

  query = photos_query_new (state, sparql);

  return query;
}


PhotosQuery *
photos_query_builder_delete_resource_query (PhotosSearchContextState *state, const gchar *resource)
{
  PhotosQuery *query;
  g_autofree gchar *sparql = NULL;

  sparql = g_strdup_printf ("DELETE DATA {"
                            "  GRAPH tracker:Pictures {"
                            "    <%s> a rdfs:Resource"
                            "  }"
                            "}",
                            resource);

  query = photos_query_new (state, sparql);
  return query;
}


PhotosQuery *
photos_query_builder_equipment_query (PhotosSearchContextState *state, GQuark equipment)
{
  GApplication *app;
  PhotosQuery *query;
  const gchar *miner_files_name;
  const gchar *resource;
  g_autofree gchar *sparql = NULL;

  app = g_application_get_default ();
  miner_files_name = photos_application_get_miner_files_name (PHOTOS_APPLICATION (app));
  resource = g_quark_to_string (equipment);

  sparql = g_strdup_printf ("SELECT ?manufacturer ?model WHERE {"
                            "  SERVICE <dbus:%s> {"
                            "    GRAPH tracker:Pictures {"
                            "      SELECT nfo:manufacturer (<%s>) AS ?manufacturer nfo:model (<%s>) AS ?model"
                            "      WHERE {}"
                            "    }"
                            "  }"
                            "}",
                            miner_files_name,
                            resource,
                            resource);

  query = photos_query_new (state, sparql);
  return query;
}


PhotosQuery *
photos_query_builder_fetch_collections_for_urn_query (PhotosSearchContextState *state, const gchar *resource)
{
  GApplication *app;
  PhotosQuery *query;
  const gchar *miner_files_name;
  g_autofree gchar *sparql = NULL;

  app = g_application_get_default ();
  miner_files_name = photos_application_get_miner_files_name (PHOTOS_APPLICATION (app));

  sparql = g_strdup_printf ("SELECT ?urn FROM tracker:Pictures WHERE {"
                            "  {"
                            "    GRAPH tracker:Pictures {"
                            "      SELECT ?urn WHERE { ?urn a nfo:DataContainer . <%s> nie:isLogicalPartOf ?urn }"
                            "    }"
                            "  }"
                            "  UNION"
                            "  {"
                            "    SERVICE <dbus:%s> {"
                            "      GRAPH tracker:Pictures {"
                            "        SELECT ?urn WHERE { ?urn a nfo:DataContainer . <%s> nie:isLogicalPartOf ?urn }"
                            "      }"
                            "    }"
                            "  }"
                            "}",
                            resource,
                            miner_files_name,
                            resource);

  query = photos_query_new (state, sparql);
  return query;
}


PhotosQuery *
photos_query_builder_fetch_collections_local (PhotosSearchContextState *state)
{
  PhotosQuery *query;
  g_autofree gchar *sparql = NULL;

  sparql = photos_query_builder_query (state,
                                       NULL,
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

  sparql = photos_query_builder_query (state, NULL, flags, offset_cntrlr);
  query = photos_query_new (state, sparql);

  return query;
}


PhotosQuery *
photos_query_builder_location_query (PhotosSearchContextState *state, const gchar *location_urn)
{
  GApplication *app;
  PhotosQuery *query;
  const gchar *miner_files_name;
  g_autofree gchar *sparql = NULL;

  app = g_application_get_default ();
  miner_files_name = photos_application_get_miner_files_name (PHOTOS_APPLICATION (app));

  sparql = g_strdup_printf ("SELECT ?latitude ?longitude WHERE {"
                            "  SERVICE <dbus:%s> {"
                            "    GRAPH tracker:Pictures {"
                            "      SELECT slo:latitude (<%s>) AS ?latitude slo:longitude (<%s>) AS ?longitude"
                            "      WHERE {}"
                            "    }"
                            "  }"
                            "}",
                            miner_files_name,
                            location_urn,
                            location_urn);

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

  if (setting)
    {
      sparql = g_strdup_printf ("INSERT DATA { "
                                "  GRAPH tracker:Pictures {"
                                "    <%s> a nmm:Photo ; nie:isLogicalPartOf <%s> ."
                                "  }"
                                "}", item_urn, collection_urn);
    }
  else
    {
      sparql = g_strdup_printf ("DELETE DATA { "
                                "  GRAPH tracker:Pictures {"
                                "    <%s> nie:isLogicalPartOf <%s> "
                                "  }"
                                "}", item_urn, collection_urn);
    }

  query = photos_query_new (state, sparql);
  return query;
}


PhotosQuery *
photos_query_builder_single_query (PhotosSearchContextState *state, gint flags, const gchar *resource)
{
  PhotosQuery *query;
  g_autofree gchar *sparql = NULL;
  g_autofree gchar *values = NULL;

  values = g_strdup_printf ("VALUES ?urn { <%s> }", resource);
  sparql = photos_query_builder_query (state, values, flags, NULL);
  query = photos_query_new (state, sparql);
  return query;
}


PhotosQuery *
photos_query_builder_update_mtime_query (PhotosSearchContextState *state, const gchar *resource)
{
  g_autoptr (GDateTime) now = NULL;
  PhotosQuery *query;
  g_autofree gchar *sparql = NULL;
  g_autofree gchar *time = NULL;

  now = g_date_time_new_now_utc ();
  time = g_date_time_format_iso8601 (now);

  sparql = g_strdup_printf ("WITH tracker:Pictures "
                            "DELETE { <%s> nie:contentLastModified ?time } "
                            "INSERT { "
                            "  <%s> a nmm:Photo ; nie:contentLastModified '%s' . "
                            "}"
                            "WHERE { <%s> nie:contentLastModified ?time }",
                            resource, resource, time, resource);

  query = photos_query_new (state, sparql);
  return query;
}
