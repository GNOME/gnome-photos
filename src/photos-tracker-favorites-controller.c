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

#include "photos-query-builder.h"
#include "photos-tracker-favorites-controller.h"


G_DEFINE_TYPE (PhotosTrackerFavoritesController,
               photos_tracker_favorites_controller,
               PHOTOS_TYPE_TRACKER_CONTROLLER);


static PhotosQuery *
photos_tracker_favorites_controller_get_query (void)
{
  return photos_query_builder_global_favorites_query ();
}


static GObject *
photos_tracker_favorites_controller_constructor (GType type,
                                                guint n_construct_params,
                                                GObjectConstructParam *construct_params)
{
  static GObject *self = NULL;

  if (self == NULL)
    {
      self = G_OBJECT_CLASS (photos_tracker_favorites_controller_parent_class)->constructor (type,
                                                                                             n_construct_params,
                                                                                             construct_params);
      g_object_add_weak_pointer (self, (gpointer) &self);
      return self;
    }

  return g_object_ref (self);
}


static void
photos_tracker_favorites_controller_init (PhotosTrackerFavoritesController *self)
{
}


static void
photos_tracker_favorites_controller_class_init (PhotosTrackerFavoritesControllerClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  PhotosTrackerControllerClass *tracker_controller_class = PHOTOS_TRACKER_CONTROLLER_CLASS (class);

  object_class->constructor = photos_tracker_favorites_controller_constructor;
  tracker_controller_class->get_query = photos_tracker_favorites_controller_get_query;
}


PhotosTrackerController *
photos_tracker_favorites_controller_new (void)
{
  return g_object_new (PHOTOS_TYPE_TRACKER_FAVORITES_CONTROLLER, NULL);
}
