/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2012, 2013, 2014 Red Hat, Inc.
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

#include <gio/gio.h>

#include "photos-collection-manager.h"
#include "photos-query-builder.h"
#include "photos-search-type.h"
#include "photos-source-manager.h"
#include "photos-search-type-manager.h"


static gchar *
photos_query_builder_convert_path_to_uri (const gchar *path)
{
  GFile *file;
  gchar *uri;

  if (path == NULL)
    return g_strdup ("");

  file = g_file_new_for_path (path);
  uri = g_file_get_uri (file);
  g_object_unref (file);

  return uri;
}


static gchar *
photos_query_builder_filter (gint flags)
{
  PhotosBaseManager *src_mngr;
  PhotosBaseManager *srch_typ_mngr;
  gchar *sparql;
  gchar *src_mngr_filter;
  gchar *srch_typ_mngr_filter;

  src_mngr = photos_source_manager_dup_singleton ();
  src_mngr_filter = photos_base_manager_get_filter (src_mngr, flags);

  srch_typ_mngr = photos_search_type_manager_dup_singleton ();
  srch_typ_mngr_filter = photos_base_manager_get_filter (srch_typ_mngr, flags);

  sparql = g_strdup_printf ("FILTER (%s && %s)", src_mngr_filter, srch_typ_mngr_filter);

  g_free (srch_typ_mngr_filter);
  g_object_unref (srch_typ_mngr);

  g_free (src_mngr_filter);
  g_object_unref (src_mngr);

  return sparql;
}


static gchar *
photos_query_builder_optional (void)
{
  return g_strdup ("OPTIONAL { ?urn nco:creator ?creator . } "
                   "OPTIONAL { ?urn nco:publisher ?publisher . }");
}


static gchar *
photos_query_builder_where (gboolean global, gint flags)
{
  PhotosBaseManager *col_mngr;
  PhotosBaseManager *srch_typ_mngr;
  gchar *col_mngr_where = NULL;
  gchar *filter = NULL;
  gchar *optional = NULL;
  gchar *sparql;
  gchar *srch_typ_mngr_where = NULL;

  col_mngr = photos_collection_manager_dup_singleton ();

  srch_typ_mngr = photos_search_type_manager_dup_singleton ();
  srch_typ_mngr_where = photos_base_manager_get_where (srch_typ_mngr, flags);

  optional = photos_query_builder_optional ();

  if (!(flags & PHOTOS_QUERY_FLAGS_UNFILTERED))
    {
      if (global)
        {
          /* TODO: SearchCategoryManager */

          col_mngr_where = photos_base_manager_get_where (col_mngr, flags);
        }

      filter = photos_query_builder_filter (flags);
    }

  sparql = g_strdup_printf ("WHERE { %s %s %s %s }",
                            srch_typ_mngr_where,
                            optional,
                            (col_mngr_where != NULL) ? col_mngr_where : "",
                            (filter != NULL) ? filter : "");

  g_free (col_mngr_where);
  g_free (filter);
  g_free (srch_typ_mngr_where);
  g_object_unref (col_mngr);
  g_object_unref (srch_typ_mngr);

  return sparql;
}


static gchar *
photos_query_builder_query (gboolean global, gint flags, PhotosOffsetController *offset_cntrlr)
{
  gchar *sparql;
  gchar *tail_sparql = NULL;
  gchar *where_sparql;

  where_sparql = photos_query_builder_where (global, flags);

  if (global)
    {
      gint offset;
      gint step;

      offset = photos_offset_controller_get_offset (offset_cntrlr);
      step = photos_offset_controller_get_step (offset_cntrlr);
      tail_sparql = g_strdup_printf ("ORDER BY DESC (?mtime) LIMIT %d OFFSET %d", step, offset);
    }

  sparql = g_strconcat ("SELECT DISTINCT ?urn "
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
                        "tracker:coalesce(nfo:fileCreated (?urn), nie:contentCreated (?urn))"
                        "nfo:width (?urn)"
                        "nfo:height (?urn)"
                        "nfo:equipment (?urn)"
                        "nmm:exposureTime (?urn)"
                        "nmm:fnumber (?urn)"
                        "nmm:focalLength (?urn)"
                        "nmm:isoSpeed (?urn)"
                        "nmm:flash (?urn)",
                        where_sparql,
                        tail_sparql,
                        NULL);
  g_free (where_sparql);
  g_free (tail_sparql);

  return sparql;
}


PhotosQuery *
photos_query_builder_create_collection_query (const gchar *name)
{
  GTimeVal tv;
  gchar *sparql;
  gchar *time;
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
  g_free (time);

  return photos_query_new (sparql);
}


PhotosQuery *
photos_query_builder_collection_icon_query (const gchar *resource)
{
  gchar *sparql;

  sparql = g_strdup_printf ("SELECT ?urn "
                            "tracker:coalesce(nfo:fileLastModified(?urn), nie:contentLastModified(?urn)) AS ?mtime "
                            "WHERE { ?urn nie:isPartOf <%s> } "
                            "ORDER BY DESC (?mtime) LIMIT 4",
                            resource);
  return photos_query_new (sparql);
}


PhotosQuery *
photos_query_builder_count_query (gint flags)
{
  gchar *sparql;
  gchar *where_sparql;

  where_sparql = photos_query_builder_where (TRUE, flags);
  sparql = g_strconcat ("SELECT DISTINCT COUNT(?urn) ", where_sparql, NULL);
  g_free (where_sparql);

  return photos_query_new (sparql);
}


PhotosQuery *
photos_query_builder_delete_resource_query (const gchar *resource)
{
  gchar *sparql;

  sparql = g_strdup_printf ("DELETE { <%s> a rdfs:Resource }", resource);
  return photos_query_new (sparql);
}


PhotosQuery *
photos_query_builder_equipment_query (GQuark equipment)
{
  const gchar *resource;
  gchar *sparql;

  resource = g_quark_to_string (equipment);
  sparql = g_strdup_printf ("SELECT nfo:manufacturer (<%s>) nfo:model (<%s>) WHERE {}", resource, resource);
  return photos_query_new (sparql);
}


PhotosQuery *
photos_query_builder_fetch_collections_query (const gchar *resource)
{
  gchar *sparql;

  sparql = g_strdup_printf ("SELECT ?urn WHERE { ?urn a nfo:DataContainer . <%s> nie:isPartOf ?urn }", resource);
  return photos_query_new (sparql);
}


PhotosQuery *
photos_query_builder_global_query (gint flags, PhotosOffsetController *offset_cntrlr)
{
  gchar *sparql;

  sparql = photos_query_builder_query (TRUE, flags, offset_cntrlr);
  return photos_query_new (sparql);
}


PhotosQuery *
photos_query_builder_set_collection_query (const gchar *item_urn, const gchar *collection_urn, gboolean setting)
{
  gchar *sparql;

  sparql = g_strdup_printf ("%s { <%s> nie:isPartOf <%s> }",
                            setting ? "INSERT" : "DELETE",
                            item_urn,
                            collection_urn);
  return photos_query_new (sparql);
}


PhotosQuery *
photos_query_builder_single_query (gint flags, const gchar *resource)
{
  GRegex *regex;
  gchar *replacement;
  gchar *sparql;
  gchar *tmp;

  tmp = photos_query_builder_query (FALSE, flags, NULL);

  regex = g_regex_new ("\\?urn", 0, 0, NULL);
  replacement = g_strconcat ("<", resource, ">", NULL);
  sparql = g_regex_replace (regex, tmp, -1, 0, replacement, 0, NULL);
  g_free (replacement);
  g_free (tmp);
  g_regex_unref (regex);

  return photos_query_new (sparql);
}


PhotosQuery *
photos_query_builder_update_mtime_query (const gchar *resource)
{
  GTimeVal tv;
  gchar *sparql;
  gchar *time;
  gint64 timestamp;

  timestamp = g_get_real_time () / G_USEC_PER_SEC;
  tv.tv_sec = timestamp;
  tv.tv_usec = 0;
  time = g_time_val_to_iso8601 (&tv);

  sparql = g_strdup_printf ("INSERT OR REPLACE { <%s> nie:contentLastModified \"%s\" }", resource, time);
  g_free (time);

  return photos_query_new (sparql);
}


gchar *
photos_query_builder_filter_local (void)
{
  gchar *desktop_uri;
  gchar *download_uri;
  gchar *filter;
  const gchar *path;
  gchar *pictures_uri;

  path = g_get_user_special_dir (G_USER_DIRECTORY_DESKTOP);
  desktop_uri = photos_query_builder_convert_path_to_uri (path);

  path = g_get_user_special_dir (G_USER_DIRECTORY_DOWNLOAD);
  download_uri = photos_query_builder_convert_path_to_uri (path);

  path = g_get_user_special_dir (G_USER_DIRECTORY_PICTURES);
  pictures_uri = photos_query_builder_convert_path_to_uri (path);

  filter = g_strdup_printf ("(fn:contains (nie:url (?urn), \"%s\")"
                            " || fn:contains (nie:url (?urn), \"%s\")"
                            " || fn:contains (nie:url (?urn), \"%s\")"
                            " || fn:starts-with (nao:identifier (?urn), \"%s\")"
                            " || (?urn = nfo:image-category-screenshot))",
                            desktop_uri,
                            download_uri,
                            pictures_uri,
                            PHOTOS_QUERY_LOCAL_COLLECTIONS_IDENTIFIER);
  g_free (desktop_uri);
  g_free (download_uri);
  g_free (pictures_uri);

  return filter;
}
