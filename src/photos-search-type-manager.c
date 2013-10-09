/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2012, 2013 Red Hat, Inc.
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

#include <glib.h>
#include <glib/gi18n.h>

#include "photos-filterable.h"
#include "photos-query.h"
#include "photos-search-type.h"
#include "photos-search-type-manager.h"


G_DEFINE_TYPE (PhotosSearchTypeManager, photos_search_type_manager, PHOTOS_TYPE_BASE_MANAGER);


static GObject *
photos_search_type_manager_constructor (GType                  type,
                                        guint                  n_construct_params,
                                        GObjectConstructParam *construct_params)
{
  static GObject *self = NULL;

  if (self == NULL)
    {
      self = G_OBJECT_CLASS (photos_search_type_manager_parent_class)->constructor (type,
                                                                                    n_construct_params,
                                                                                    construct_params);
      g_object_add_weak_pointer (self, (gpointer) &self);
      return self;
    }

  return g_object_ref (self);
}


static void
photos_search_type_manager_init (PhotosSearchTypeManager *self)
{
  PhotosSearchType *search_type;
  const gchar *item_filter;
  gchar *col_filter;
  gchar *fav_filter;

  item_filter = "fn:contains (rdf:type (?urn), 'nmm#Photo')";
  col_filter = g_strdup_printf ("(fn:starts-with (nao:identifier (?urn), '%s')"
                                " || (?urn = nfo:image-category-screenshot))",
                                PHOTOS_QUERY_COLLECTIONS_IDENTIFIER);
  fav_filter = g_strdup_printf ("(%s || %s)", col_filter, item_filter);

  search_type = photos_search_type_new_full (PHOTOS_SEARCH_TYPE_STOCK_COLLECTIONS,
                                             _("Albums"),
                                             "?urn a nfo:DataContainer",
                                             col_filter);
  photos_base_manager_add_object (PHOTOS_BASE_MANAGER (self), G_OBJECT (search_type));
  g_object_unref (search_type);

  search_type = photos_search_type_new_full (PHOTOS_SEARCH_TYPE_STOCK_FAVORITES,
                                             _("Favorites"),
                                             "?urn nao:hasTag nao:predefined-tag-favorite",
                                             fav_filter);
  photos_base_manager_add_object (PHOTOS_BASE_MANAGER (self), G_OBJECT (search_type));
  g_object_unref (search_type);

  search_type = photos_search_type_new_full (PHOTOS_SEARCH_TYPE_STOCK_PHOTOS,
                                             _("Photos"),
                                             "?urn a nmm:Photo",
                                             "(true)");
  photos_base_manager_add_object (PHOTOS_BASE_MANAGER (self), G_OBJECT (search_type));
  g_object_unref (search_type);

  photos_base_manager_set_active_object_by_id (PHOTOS_BASE_MANAGER (self), PHOTOS_SEARCH_TYPE_STOCK_PHOTOS);

  g_free (col_filter);
  g_free (fav_filter);
}


static void
photos_search_type_manager_class_init (PhotosSearchTypeManagerClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->constructor = photos_search_type_manager_constructor;
}


PhotosBaseManager *
photos_search_type_manager_dup_singleton (void)
{
  return g_object_new (PHOTOS_TYPE_SEARCH_TYPE_MANAGER, NULL);
}


gchar *
photos_search_type_manager_get_where (PhotosSearchTypeManager *self)
{
  GObject *search_type;

  search_type = photos_base_manager_get_active_object (PHOTOS_BASE_MANAGER (self));
  return photos_filterable_get_where (PHOTOS_FILTERABLE (search_type));
}
