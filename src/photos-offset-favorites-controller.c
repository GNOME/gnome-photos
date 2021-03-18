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

#include <gio/gio.h>

#include "photos-query-builder.h"
#include "photos-offset-favorites-controller.h"
#include "photos-search-context.h"


struct _PhotosOffsetFavoritesController
{
  PhotosOffsetController parent_instance;
};


G_DEFINE_TYPE (PhotosOffsetFavoritesController, photos_offset_favorites_controller, PHOTOS_TYPE_OFFSET_CONTROLLER);


static PhotosQuery *
photos_offset_favorites_controller_get_query (PhotosOffsetController *offset_cntrlr)
{
  GApplication *app;
  PhotosSearchContextState *state;

  app = g_application_get_default ();
  state = photos_search_context_get_state (PHOTOS_SEARCH_CONTEXT (app));

  return photos_query_builder_count_query (state, PHOTOS_QUERY_FLAGS_FAVORITES);
}


static GObject *
photos_offset_favorites_controller_constructor (GType type,
                                                guint n_construct_params,
                                                GObjectConstructParam *construct_params)
{
  static GObject *self = NULL;

  if (self == NULL)
    {
      self = G_OBJECT_CLASS (photos_offset_favorites_controller_parent_class)->constructor (type,
                                                                                            n_construct_params,
                                                                                            construct_params);
      g_object_add_weak_pointer (self, (gpointer) &self);
      return self;
    }

  return g_object_ref (self);
}


static void
photos_offset_favorites_controller_init (PhotosOffsetFavoritesController *self)
{
}


static void
photos_offset_favorites_controller_class_init (PhotosOffsetFavoritesControllerClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  PhotosOffsetControllerClass *offset_controller_class = PHOTOS_OFFSET_CONTROLLER_CLASS (class);

  object_class->constructor = photos_offset_favorites_controller_constructor;
  offset_controller_class->get_query = photos_offset_favorites_controller_get_query;
}


PhotosOffsetController *
photos_offset_favorites_controller_dup_singleton (void)
{
  return g_object_new (PHOTOS_TYPE_OFFSET_FAVORITES_CONTROLLER, NULL);
}
