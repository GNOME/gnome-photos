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

#include <glib.h>
#include <glib/gi18n.h>

#include "photos-filterable.h"
#include "photos-query.h"
#include "photos-search-type.h"
#include "photos-search-type-manager.h"


struct _PhotosSearchTypeManager
{
  PhotosBaseManager parent_instance;
};


G_DEFINE_TYPE (PhotosSearchTypeManager, photos_search_type_manager, PHOTOS_TYPE_BASE_MANAGER);


static const gchar *BLACKLISTED_MIME_TYPES[] =
{
  "image/gif",
  "image/x-eps"
};


static gchar *
photos_search_type_manager_get_filter (PhotosBaseManager *mngr, gint flags)
{
  GObject *search_type;
  gchar *filter;

  if (flags & PHOTOS_QUERY_FLAGS_COLLECTIONS)
    search_type = photos_base_manager_get_object_by_id (mngr, PHOTOS_SEARCH_TYPE_STOCK_COLLECTIONS);
  else if (flags & PHOTOS_QUERY_FLAGS_FAVORITES)
    search_type = photos_base_manager_get_object_by_id (mngr, PHOTOS_SEARCH_TYPE_STOCK_FAVORITES);
  else if (flags & PHOTOS_QUERY_FLAGS_OVERVIEW)
    search_type = photos_base_manager_get_object_by_id (mngr, PHOTOS_SEARCH_TYPE_STOCK_PHOTOS);
  else if (flags & PHOTOS_QUERY_FLAGS_SEARCH)
    search_type = photos_base_manager_get_active_object (mngr);
  else
    search_type = photos_base_manager_get_object_by_id (mngr, PHOTOS_SEARCH_TYPE_STOCK_ALL);

  filter = photos_filterable_get_filter (PHOTOS_FILTERABLE (search_type));
  return filter;
}


static gchar *
photos_search_type_manager_get_where (PhotosBaseManager *mngr, gint flags)
{
  GObject *search_type;

  if (flags & PHOTOS_QUERY_FLAGS_COLLECTIONS)
    search_type = photos_base_manager_get_object_by_id (mngr, PHOTOS_SEARCH_TYPE_STOCK_COLLECTIONS);
  else if (flags & PHOTOS_QUERY_FLAGS_FAVORITES)
    search_type = photos_base_manager_get_object_by_id (mngr, PHOTOS_SEARCH_TYPE_STOCK_FAVORITES);
  else if (flags & PHOTOS_QUERY_FLAGS_OVERVIEW)
    search_type = photos_base_manager_get_object_by_id (mngr, PHOTOS_SEARCH_TYPE_STOCK_PHOTOS);
  else if (flags & PHOTOS_QUERY_FLAGS_SEARCH)
    search_type = photos_base_manager_get_active_object (mngr);
  else
    search_type = photos_base_manager_get_object_by_id (mngr, PHOTOS_SEARCH_TYPE_STOCK_ALL);

  return photos_filterable_get_where (PHOTOS_FILTERABLE (search_type));
}


static void
photos_search_type_manager_init (PhotosSearchTypeManager *self)
{
  PhotosSearchType *search_type;
  gchar *item_filter;
  gchar *all_filter;
  gchar *blacklisted_mime_types_filter;
  gchar *col_filter;
  gchar **strv;
  guint i;
  guint n_elements;

  n_elements = G_N_ELEMENTS (BLACKLISTED_MIME_TYPES);
  strv = (gchar **) g_malloc0_n (n_elements + 1, sizeof (gchar *));
  for (i = 0; i < n_elements; i++)
    strv[i] = g_strdup_printf ("nie:mimeType(?urn) != '%s'", BLACKLISTED_MIME_TYPES[i]);

  blacklisted_mime_types_filter = g_strjoinv (" && ", strv);

  item_filter = g_strdup_printf ("(fn:contains (?type, 'nmm#Photo') && %s)", blacklisted_mime_types_filter);
  col_filter = g_strdup_printf ("(fn:contains (?type, 'nfo#DataContainer')"
                                " && ?count > 0"
                                " && (fn:starts-with (nao:identifier (?urn), '%s')"
                                "     || (?urn = nfo:image-category-screenshot)))",
                                PHOTOS_QUERY_COLLECTIONS_IDENTIFIER);
  all_filter = g_strdup_printf ("(%s || %s)", col_filter, item_filter);

  search_type = photos_search_type_new_full (PHOTOS_SEARCH_TYPE_STOCK_ALL,
                                             _("All"),
                                             "?urn a rdfs:Resource. "
                                             "OPTIONAL {?item a nmm:Photo; nie:isPartOf ?urn}",
                                             all_filter);
  photos_base_manager_add_object (PHOTOS_BASE_MANAGER (self), G_OBJECT (search_type));
  g_object_unref (search_type);

  search_type = photos_search_type_new_full (PHOTOS_SEARCH_TYPE_STOCK_COLLECTIONS,
                                             _("Albums"),
                                             "?urn a nfo:DataContainer. "
                                             "?item a nmm:Photo; nie:isPartOf ?urn.",
                                             col_filter);
  photos_base_manager_add_object (PHOTOS_BASE_MANAGER (self), G_OBJECT (search_type));
  g_object_unref (search_type);

  search_type = photos_search_type_new_full (PHOTOS_SEARCH_TYPE_STOCK_FAVORITES,
                                             _("Favorites"),
                                             "?urn a nmm:Photo; nao:hasTag nao:predefined-tag-favorite. ",
                                             blacklisted_mime_types_filter);
  photos_base_manager_add_object (PHOTOS_BASE_MANAGER (self), G_OBJECT (search_type));
  g_object_unref (search_type);

  search_type = photos_search_type_new_full (PHOTOS_SEARCH_TYPE_STOCK_PHOTOS,
                                             _("Photos"),
                                             "?urn a nmm:Photo",
                                             blacklisted_mime_types_filter);
  photos_base_manager_add_object (PHOTOS_BASE_MANAGER (self), G_OBJECT (search_type));
  g_object_unref (search_type);

  photos_base_manager_set_active_object_by_id (PHOTOS_BASE_MANAGER (self), PHOTOS_SEARCH_TYPE_STOCK_PHOTOS);

  g_free (item_filter);
  g_free (all_filter);
  g_free (blacklisted_mime_types_filter);
  g_free (col_filter);
  g_strfreev (strv);
}


static void
photos_search_type_manager_class_init (PhotosSearchTypeManagerClass *class)
{
  PhotosBaseManagerClass *base_manager_class = PHOTOS_BASE_MANAGER_CLASS (class);

  base_manager_class->get_filter = photos_search_type_manager_get_filter;
  base_manager_class->get_where = photos_search_type_manager_get_where;
}


PhotosBaseManager *
photos_search_type_manager_new (void)
{
  return g_object_new (PHOTOS_TYPE_SEARCH_TYPE_MANAGER,
                       "action-id", "search-type",
  /* Translators: "Type" refers to a search filter. eg., All, Albums,
   * Favorites and Photos.
   */
                       "title", C_("Search Filter", "Type"),
                       NULL);
}
