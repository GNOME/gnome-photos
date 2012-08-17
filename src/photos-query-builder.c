/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2012 Red Hat, Inc.
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


#include "config.h"

#include <gio/gio.h>

#include "photos-offset-controller.h"
#include "photos-query-builder.h"
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
photos_query_builder_filter (void)
{
  PhotosBaseManager *src_mngr;
  PhotosBaseManager *srch_typ_mngr;
  gchar *sparql;
  gchar *src_mngr_filter;
  gchar *srch_typ_mngr_filter;

  src_mngr = photos_source_manager_new ();
  src_mngr_filter = photos_base_manager_get_filter (src_mngr);

  srch_typ_mngr = photos_search_type_manager_new ();
  srch_typ_mngr_filter = photos_base_manager_get_filter (srch_typ_mngr);

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
  return g_strdup ("OPTIONAL { ?urn nco:creator ?creater . } "
                   "OPTIONAL { ?urn nco:publisher ?publisher . }");
}


static gchar *
photos_query_builder_query (gboolean global, gint flags)
{
  gchar *filter;
  gchar *optional;
  gchar *sparql;
  gchar *tail_sparql = NULL;
  gchar *tmp;
  gchar *where_sparql;

  optional = photos_query_builder_optional ();
  where_sparql = g_strconcat ("WHERE { ?urn a rdfs:Resource ", optional, NULL);
  g_free (optional);

  if (!(flags & PHOTOS_QUERY_FLAGS_UNFILTERED))
    {
      if (global)
        {
          /* TODO: CollectionManager, etc.. */
        }

      filter = photos_query_builder_filter ();
      tmp = where_sparql;
      where_sparql = g_strconcat (where_sparql, filter, NULL);
      g_free (tmp);
      g_free (filter);
    }

  tmp = where_sparql;
  where_sparql = g_strconcat (where_sparql, " }", NULL);
  g_free (tmp);

  if (global)
    {
      PhotosOffsetController *offset_cntrlr;
      gint offset;
      gint step;

      offset_cntrlr = photos_offset_controller_new ();
      offset = photos_offset_controller_get_offset (offset_cntrlr);
      step = photos_offset_controller_get_step (offset_cntrlr);
      g_object_unref (offset_cntrlr);

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
                        "( EXISTS { ?urn nco:contributor ?contributor FILTER ( ?contributor != ?creator ) } ) ",
                        where_sparql,
                        tail_sparql,
                        NULL);
  g_free (where_sparql);
  g_free (tail_sparql);
  return sparql;
}


PhotosQuery *
photos_query_builder_count_query (void)
{
  gchar *filter;
  gchar *optional;
  gchar *sparql;

  filter = photos_query_builder_filter ();
  optional = photos_query_builder_optional ();
  sparql = g_strconcat ("SELECT DISTINCT COUNT(?urn) WHERE { ?urn a rdfs:Resource ", optional, filter, " }", NULL);

  g_free (optional);
  g_free (filter);

  return photos_query_new (sparql);
}


PhotosQuery *
photos_query_builder_global_query (void)
{
  gchar *sparql;

  sparql = photos_query_builder_query (TRUE, PHOTOS_QUERY_FLAGS_NONE);
  return photos_query_new (sparql);
}


PhotosQuery *
photos_query_builder_single_query (gint flags, const gchar *resource)
{
  GRegex *regex;
  gchar *replacement;
  gchar *sparql;
  gchar *tmp;

  tmp = photos_query_builder_query (TRUE, flags);

  regex = g_regex_new ("\\?urn", 0, 0, NULL);
  replacement = g_strconcat ("<", resource, ">", NULL);
  sparql = g_regex_replace (regex, tmp, -1, 0, replacement, 0, NULL);
  g_free (replacement);
  g_free (tmp);
  g_regex_unref (regex);

  return photos_query_new (sparql);
}


gchar *
photos_query_builder_filter_local (void)
{
  GFile *file;
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

  filter = g_strdup_printf ("((fn:starts-with (nie:url (?urn), \"%s\"))"
                            " || (fn:starts-with (nie:url (?urn), \"%s\"))"
                            " || (fn:starts-with (nie:url (?urn), \"%s\")))",
                            desktop_uri,
                            download_uri,
                            pictures_uri);
  g_free (desktop_uri);
  g_free (download_uri);
  g_free (pictures_uri);

  return filter;
}
