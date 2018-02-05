/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2012 – 2017 Red Hat, Inc.
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
#include "photos-query-builder.h"
#include "photos-search-type.h"
#include "photos-source-manager.h"
#include "photos-search-match-manager.h"
#include "photos-search-type-manager.h"


static gchar *
photos_query_builder_filter (PhotosSearchContextState *state, gint flags)
{
  gchar *sparql;
  g_autofree gchar *src_mngr_filter = NULL;
  g_autofree gchar *srch_mtch_mngr_filter = NULL;
  g_autofree gchar *srch_typ_mngr_filter = NULL;

  src_mngr_filter = photos_base_manager_get_filter (state->src_mngr, flags);
  srch_mtch_mngr_filter = photos_base_manager_get_filter (state->srch_mtch_mngr, flags);
  srch_typ_mngr_filter = photos_base_manager_get_filter (state->srch_typ_mngr, flags);

  sparql = g_strdup_printf ("FILTER (%s && %s && %s)",
                            src_mngr_filter,
                            srch_mtch_mngr_filter,
                            srch_typ_mngr_filter);

  return sparql;
}


static gchar *
photos_query_builder_optional (void)
{
  return g_strdup ("OPTIONAL { ?urn nco:creator ?creator . } "
                   "OPTIONAL { ?urn nco:publisher ?publisher . }");
}


static gchar *
photos_query_builder_inner_where (PhotosSearchContextState *state, gboolean global, gint flags)
{
  g_autofree gchar *item_mngr_where = NULL;
  gchar *sparql;
  g_autofree gchar *srch_typ_mngr_where = NULL;

  srch_typ_mngr_where = photos_base_manager_get_where (state->srch_typ_mngr, flags);

  if (!(flags & PHOTOS_QUERY_FLAGS_UNFILTERED))
    {
      if (global)
        {
          /* TODO: SearchCategoryManager */

          item_mngr_where = photos_base_manager_get_where (state->item_mngr, flags);
        }
    }

  sparql = g_strdup_printf ("WHERE { %s %s }",
                            srch_typ_mngr_where,
                            (item_mngr_where != NULL) ? item_mngr_where : "");

  return sparql;
}


static gchar *
photos_query_builder_where (PhotosSearchContextState *state, gboolean global, gint flags)
{
  const gchar *count_items = "COUNT (?item) AS ?count";
  gboolean item_defined;
  g_autofree gchar *filter = NULL;
  g_autofree gchar *optional = NULL;
  gchar *sparql;
  g_autofree gchar *where_sparql = NULL;

  where_sparql = photos_query_builder_inner_where (state, global, flags);
  item_defined = strstr (where_sparql, "?item") != NULL;

  optional = photos_query_builder_optional ();

  if (!(flags & PHOTOS_QUERY_FLAGS_UNFILTERED))
    filter = photos_query_builder_filter (state, flags);

  sparql = g_strdup_printf ("WHERE {{"
                            "    SELECT ?urn rdf:type (?urn) AS ?type %s %s GROUP BY (?urn)"
                            "  }"
                            "  %s %s"
                            "}",
                            item_defined ? count_items : "",
                            where_sparql,
                            optional,
                            (filter != NULL) ? filter : "");

  return sparql;
}


static gchar *
photos_query_builder_query (PhotosSearchContextState *state,
                            gboolean global,
                            gint flags,
                            PhotosOffsetController *offset_cntrlr)
{
  gchar *sparql;
  g_autofree gchar *tail_sparql = NULL;
  g_autofree gchar *where_sparql = NULL;

  where_sparql = photos_query_builder_where (state, global, flags);

  if (global && (flags & PHOTOS_QUERY_FLAGS_UNLIMITED) == 0)
    {
      gint offset = 0;
      gint step = 60;

      if (offset_cntrlr != NULL)
        {
          offset = photos_offset_controller_get_offset (offset_cntrlr);
          step = photos_offset_controller_get_step (offset_cntrlr);
        }

      tail_sparql = g_strdup_printf ("ORDER BY DESC (?mtime) LIMIT %d OFFSET %d", step, offset);
    }

  sparql = g_strconcat ("SELECT ?urn "
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
                        "slo:location (?urn) ",
                        where_sparql,
                        tail_sparql,
                        NULL);

  return sparql;
}


PhotosQuery *
photos_query_builder_create_collection_query (PhotosSearchContextState *state, const gchar *name)
{
  GTimeVal tv;
  PhotosQuery *query;
  g_autofree gchar *sparql = NULL;
  g_autofree gchar *time = NULL;
  gint64 timestamp;

  timestamp = g_get_real_time () / G_USEC_PER_SEC;
  tv.tv_sec = timestamp;
  tv.tv_usec = 0;
  time = g_time_val_to_iso8601 (&tv);

  sparql = g_strdup_printf ("INSERT { _:res a nfo:DataContainer ; a nie:DataObject ; "
                            "nie:contentLastModified '%s' ; "
                            "nie:title '%s' ; "
                            "nao:identifier '%s%s' }",
                            time,
                            name,
                            PHOTOS_QUERY_LOCAL_COLLECTIONS_IDENTIFIER,
                            name);

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
  PhotosQuery *query;
  g_autofree gchar *sparql = NULL;
  g_autofree gchar *where_sparql = NULL;

  where_sparql = photos_query_builder_where (state, TRUE, flags);
  sparql = g_strconcat ("SELECT DISTINCT COUNT(?urn) ", where_sparql, NULL);
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
  GTimeVal tv;
  PhotosQuery *query;
  g_autofree gchar *sparql = NULL;
  g_autofree gchar *time = NULL;
  gint64 timestamp;

  timestamp = g_get_real_time () / G_USEC_PER_SEC;
  tv.tv_sec = timestamp;
  tv.tv_usec = 0;
  time = g_time_val_to_iso8601 (&tv);

  sparql = g_strdup_printf ("INSERT OR REPLACE { <%s> nie:contentLastModified '%s' }", resource, time);
  query = photos_query_new (state, sparql);

  return query;
}
