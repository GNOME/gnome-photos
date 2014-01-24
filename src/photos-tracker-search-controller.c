/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2014 Red Hat, Inc.
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

#include "photos-mode-controller.h"
#include "photos-offset-search-controller.h"
#include "photos-query-builder.h"
#include "photos-tracker-search-controller.h"


struct _PhotosTrackerSearchControllerPrivate
{
  PhotosOffsetController *offset_cntrlr;
};


G_DEFINE_TYPE_WITH_PRIVATE (PhotosTrackerSearchController,
                            photos_tracker_search_controller,
                            PHOTOS_TYPE_TRACKER_CONTROLLER);


static PhotosOffsetController *
photos_tracker_search_controller_get_offset_controller (PhotosTrackerController *trk_cntrlr)
{
  PhotosTrackerSearchController *self = PHOTOS_TRACKER_SEARCH_CONTROLLER (trk_cntrlr);
  return g_object_ref (self->priv->offset_cntrlr);
}


static PhotosQuery *
photos_tracker_search_controller_get_query (PhotosTrackerController *trk_cntrlr)
{
  PhotosTrackerSearchController *self = PHOTOS_TRACKER_SEARCH_CONTROLLER (trk_cntrlr);
  return photos_query_builder_global_query (PHOTOS_QUERY_FLAGS_SEARCH, self->priv->offset_cntrlr);
}


static GObject *
photos_tracker_search_controller_constructor (GType type,
                                              guint n_construct_params,
                                              GObjectConstructParam *construct_params)
{
  static GObject *self = NULL;

  if (self == NULL)
    {
      self = G_OBJECT_CLASS (photos_tracker_search_controller_parent_class)->constructor (type,
                                                                                          n_construct_params,
                                                                                          construct_params);
      g_object_add_weak_pointer (self, (gpointer) &self);
      return self;
    }

  return g_object_ref (self);
}


static void
photos_tracker_search_controller_dispose (GObject *object)
{
  PhotosTrackerSearchController *self = PHOTOS_TRACKER_SEARCH_CONTROLLER (object);

  g_clear_object (&self->priv->offset_cntrlr);

  G_OBJECT_CLASS (photos_tracker_search_controller_parent_class)->dispose (object);
}


static void
photos_tracker_search_controller_init (PhotosTrackerSearchController *self)
{
  PhotosTrackerSearchControllerPrivate *priv;

  self->priv = photos_tracker_search_controller_get_instance_private (self);
  priv = self->priv;

  priv->offset_cntrlr = photos_offset_search_controller_dup_singleton ();
}


static void
photos_tracker_search_controller_class_init (PhotosTrackerSearchControllerClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  PhotosTrackerControllerClass *tracker_controller_class = PHOTOS_TRACKER_CONTROLLER_CLASS (class);

  object_class->constructor = photos_tracker_search_controller_constructor;
  object_class->dispose = photos_tracker_search_controller_dispose;
  tracker_controller_class->get_offset_controller = photos_tracker_search_controller_get_offset_controller;
  tracker_controller_class->get_query = photos_tracker_search_controller_get_query;
}


PhotosTrackerController *
photos_tracker_search_controller_dup_singleton (void)
{
  return g_object_new (PHOTOS_TYPE_TRACKER_SEARCH_CONTROLLER, "mode", PHOTOS_WINDOW_MODE_SEARCH, NULL);
}
