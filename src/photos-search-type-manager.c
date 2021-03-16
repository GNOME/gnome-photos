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

#include <glib.h>
#include <glib/gi18n.h>

#include "photos-query.h"
#include "photos-search-type.h"
#include "photos-search-type-manager.h"


struct _PhotosSearchTypeManager
{
  PhotosBaseManager parent_instance;
};


G_DEFINE_TYPE (PhotosSearchTypeManager, photos_search_type_manager, PHOTOS_TYPE_BASE_MANAGER);


static PhotosSparqlTemplate *
photos_search_type_manager_get_sparql_template (PhotosBaseManager *mngr, gint flags)
{
  GObject *search_type;
  PhotosSparqlTemplate *sparql_template;

  if (flags & PHOTOS_QUERY_FLAGS_COLLECTIONS)
    search_type = photos_base_manager_get_object_by_id (mngr, PHOTOS_SEARCH_TYPE_STOCK_COLLECTIONS);
  else if (flags & PHOTOS_QUERY_FLAGS_FAVORITES)
    search_type = photos_base_manager_get_object_by_id (mngr, PHOTOS_SEARCH_TYPE_STOCK_FAVORITES);
  else if (flags & PHOTOS_QUERY_FLAGS_IMPORT || flags & PHOTOS_QUERY_FLAGS_OVERVIEW)
    search_type = photos_base_manager_get_object_by_id (mngr, PHOTOS_SEARCH_TYPE_STOCK_PHOTOS);
  else if (flags & PHOTOS_QUERY_FLAGS_SEARCH)
    search_type = photos_base_manager_get_active_object (mngr);
  else
    search_type = photos_base_manager_get_object_by_id (mngr, PHOTOS_SEARCH_TYPE_STOCK_ALL);

  sparql_template = photos_search_type_get_sparql_template (PHOTOS_SEARCH_TYPE (search_type));
  return sparql_template;
}


static void
photos_search_type_manager_init (PhotosSearchTypeManager *self)
{
  PhotosSearchType *search_type;

  {
    g_autoptr (PhotosSparqlTemplate) sparql_template = NULL;

    sparql_template = photos_sparql_template_new ("resource:///org/gnome/Photos/query/all.sparql.template");
    search_type = photos_search_type_new (PHOTOS_SEARCH_TYPE_STOCK_ALL, _("All"), sparql_template);
    photos_base_manager_add_object (PHOTOS_BASE_MANAGER (self), G_OBJECT (search_type));
    g_object_unref (search_type);
  }

  {
    g_autoptr (PhotosSparqlTemplate) sparql_template = NULL;

    sparql_template = photos_sparql_template_new ("resource:///org/gnome/Photos/query/collections.sparql.template");
    search_type = photos_search_type_new (PHOTOS_SEARCH_TYPE_STOCK_COLLECTIONS, _("Albums"), sparql_template);
    photos_base_manager_add_object (PHOTOS_BASE_MANAGER (self), G_OBJECT (search_type));
    g_object_unref (search_type);
  }

  {
    g_autoptr (PhotosSparqlTemplate) sparql_template = NULL;

    sparql_template = photos_sparql_template_new ("resource:///org/gnome/Photos/query/favorites.sparql.template");
    search_type = photos_search_type_new (PHOTOS_SEARCH_TYPE_STOCK_FAVORITES, _("Favorites"), sparql_template);
    photos_base_manager_add_object (PHOTOS_BASE_MANAGER (self), G_OBJECT (search_type));
    g_object_unref (search_type);
  }

  {
    g_autoptr (PhotosSparqlTemplate) sparql_template = NULL;

    sparql_template = photos_sparql_template_new ("resource:///org/gnome/Photos/query/photos.sparql.template");
    search_type = photos_search_type_new (PHOTOS_SEARCH_TYPE_STOCK_PHOTOS, _("Photos"), sparql_template);
    photos_base_manager_add_object (PHOTOS_BASE_MANAGER (self), G_OBJECT (search_type));
    g_object_unref (search_type);
  }

  photos_base_manager_set_active_object_by_id (PHOTOS_BASE_MANAGER (self), PHOTOS_SEARCH_TYPE_STOCK_ALL);
}


static void
photos_search_type_manager_class_init (PhotosSearchTypeManagerClass *class)
{
  PhotosBaseManagerClass *base_manager_class = PHOTOS_BASE_MANAGER_CLASS (class);
  base_manager_class->get_sparql_template = photos_search_type_manager_get_sparql_template;
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
