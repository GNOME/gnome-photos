/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2013 – 2016 Red Hat, Inc.
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

#include "photos-item-manager.h"
#include "photos-offset-collections-controller.h"
#include "photos-query-builder.h"
#include "photos-search-context.h"
#include "photos-tracker-collections-controller.h"
#include "photos-utils.h"


struct _PhotosTrackerCollectionsController
{
  PhotosTrackerController parent_instance;
  PhotosBaseManager *item_mngr;
  PhotosModeController *mode_cntrlr;
  PhotosOffsetController *offset_cntrlr;
};

struct _PhotosTrackerCollectionsControllerClass
{
  PhotosTrackerControllerClass parent_class;
};


G_DEFINE_TYPE_WITH_CODE (PhotosTrackerCollectionsController,
                         photos_tracker_collections_controller,
                         PHOTOS_TYPE_TRACKER_CONTROLLER,
                         photos_utils_ensure_extension_points ();
                         g_io_extension_point_implement (PHOTOS_TRACKER_CONTROLLER_EXTENSION_POINT_NAME,
                                                         g_define_type_id,
                                                         "collections",
                                                         0));


static void
photos_tracker_collections_controller_col_active_changed (PhotosTrackerCollectionsController *self)
{
  PhotosWindowMode mode;

  g_return_if_fail (self->mode_cntrlr != NULL);

  mode = photos_mode_controller_get_window_mode (self->mode_cntrlr);
  if (mode != PHOTOS_WINDOW_MODE_COLLECTIONS)
    return;

  photos_tracker_controller_refresh_for_object (PHOTOS_TRACKER_CONTROLLER (self));
}


static PhotosOffsetController *
photos_tracker_collections_controller_get_offset_controller (PhotosTrackerController *trk_cntrlr)
{
  PhotosTrackerCollectionsController *self = PHOTOS_TRACKER_COLLECTIONS_CONTROLLER (trk_cntrlr);
  return g_object_ref (self->offset_cntrlr);
}


static PhotosQuery *
photos_tracker_collections_controller_get_query (PhotosTrackerController *trk_cntrlr)
{
  PhotosTrackerCollectionsController *self = PHOTOS_TRACKER_COLLECTIONS_CONTROLLER (trk_cntrlr);
  GApplication *app;
  PhotosBaseItem *collection;
  PhotosSearchContextState *state;
  gint flags;

  g_return_val_if_fail (self->item_mngr != NULL, NULL);

  collection = photos_item_manager_get_active_collection (PHOTOS_ITEM_MANAGER (self->item_mngr));
  if (collection != NULL)
    flags = PHOTOS_QUERY_FLAGS_NONE;
  else
    flags = PHOTOS_QUERY_FLAGS_COLLECTIONS;

  app = g_application_get_default ();
  state = photos_search_context_get_state (PHOTOS_SEARCH_CONTEXT (app));

  return photos_query_builder_global_query (state, flags, self->offset_cntrlr);
}


static GObject *
photos_tracker_collections_controller_constructor (GType type,
                                                   guint n_construct_params,
                                                   GObjectConstructParam *construct_params)
{
  static GObject *self = NULL;

  if (self == NULL)
    {
      self = G_OBJECT_CLASS (photos_tracker_collections_controller_parent_class)->constructor (type,
                                                                                               n_construct_params,
                                                                                               construct_params);
      g_object_add_weak_pointer (self, (gpointer) &self);
      return self;
    }

  return g_object_ref (self);
}


static void
photos_tracker_collections_controller_dispose (GObject *object)
{
  PhotosTrackerCollectionsController *self = PHOTOS_TRACKER_COLLECTIONS_CONTROLLER (object);

  g_clear_object (&self->offset_cntrlr);

  G_OBJECT_CLASS (photos_tracker_collections_controller_parent_class)->dispose (object);
}


static void
photos_tracker_collections_controller_finalize (GObject *object)
{
  PhotosTrackerCollectionsController *self = PHOTOS_TRACKER_COLLECTIONS_CONTROLLER (object);

  if (self->item_mngr != NULL)
    g_object_remove_weak_pointer (G_OBJECT (self->item_mngr), (gpointer *) &self->item_mngr);

  if (self->mode_cntrlr != NULL)
    g_object_remove_weak_pointer (G_OBJECT (self->mode_cntrlr), (gpointer *) &self->mode_cntrlr);

  G_OBJECT_CLASS (photos_tracker_collections_controller_parent_class)->finalize (object);
}


static void
photos_tracker_collections_controller_init (PhotosTrackerCollectionsController *self)
{
  GApplication *app;
  PhotosSearchContextState *state;

  app = g_application_get_default ();
  state = photos_search_context_get_state (PHOTOS_SEARCH_CONTEXT (app));

  self->item_mngr = state->item_mngr;
  g_object_add_weak_pointer (G_OBJECT (self->item_mngr), (gpointer *) &self->item_mngr);
  g_signal_connect_swapped (self->item_mngr,
                            "active-collection-changed",
                            G_CALLBACK (photos_tracker_collections_controller_col_active_changed),
                            self);

  self->mode_cntrlr = state->mode_cntrlr;
  g_object_add_weak_pointer (G_OBJECT (self->mode_cntrlr), (gpointer *) &self->mode_cntrlr);

  self->offset_cntrlr = photos_offset_collections_controller_dup_singleton ();
}


static void
photos_tracker_collections_controller_class_init (PhotosTrackerCollectionsControllerClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  PhotosTrackerControllerClass *tracker_controller_class = PHOTOS_TRACKER_CONTROLLER_CLASS (class);

  object_class->constructor = photos_tracker_collections_controller_constructor;
  object_class->dispose = photos_tracker_collections_controller_dispose;
  object_class->finalize = photos_tracker_collections_controller_finalize;
  tracker_controller_class->get_offset_controller = photos_tracker_collections_controller_get_offset_controller;
  tracker_controller_class->get_query = photos_tracker_collections_controller_get_query;
}


PhotosTrackerController *
photos_tracker_collections_controller_dup_singleton (void)
{
  return g_object_new (PHOTOS_TYPE_TRACKER_COLLECTIONS_CONTROLLER, "mode", PHOTOS_WINDOW_MODE_COLLECTIONS, NULL);
}
