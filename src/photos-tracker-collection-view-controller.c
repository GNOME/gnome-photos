/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2017 Red Hat, Inc.
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

#include "photos-item-manager.h"
#include "photos-offset-collection-view-controller.h"
#include "photos-query-builder.h"
#include "photos-search-context.h"
#include "photos-tracker-collection-view-controller.h"
#include "photos-utils.h"


struct _PhotosTrackerCollectionViewController
{
  PhotosTrackerController parent_instance;
  PhotosBaseManager *item_mngr;
  PhotosOffsetController *offset_cntrlr;
};


G_DEFINE_TYPE_WITH_CODE (PhotosTrackerCollectionViewController,
                         photos_tracker_collection_view_controller,
                         PHOTOS_TYPE_TRACKER_CONTROLLER,
                         photos_utils_ensure_extension_points ();
                         g_io_extension_point_implement (PHOTOS_TRACKER_CONTROLLER_EXTENSION_POINT_NAME,
                                                         g_define_type_id,
                                                         "collection-view",
                                                         0));


static void
photos_tracker_collection_view_controller_active_collection_changed (PhotosTrackerCollectionViewController *self,
                                                                     PhotosBaseItem *active_collection)
{
  PhotosBaseManager *item_mngr_chld;
  gboolean frozen;
  guint n_items;

  g_return_if_fail (PHOTOS_IS_TRACKER_COLLECTION_VIEW_CONTROLLER (self));
  g_return_if_fail (active_collection == NULL || PHOTOS_IS_BASE_ITEM (active_collection));
  g_return_if_fail (self->item_mngr != NULL);

  item_mngr_chld = photos_item_manager_get_for_mode (PHOTOS_ITEM_MANAGER (self->item_mngr),
                                                     PHOTOS_WINDOW_MODE_COLLECTION_VIEW);
  n_items = g_list_model_get_n_items (G_LIST_MODEL (item_mngr_chld));
  g_return_if_fail ((PHOTOS_BASE_ITEM (active_collection) && n_items == 0)
                    || (active_collection == NULL && n_items > 0));

  frozen = active_collection == NULL;
  photos_tracker_controller_set_frozen (PHOTOS_TRACKER_CONTROLLER (self), frozen);

  if (active_collection == NULL)
    photos_item_manager_clear (PHOTOS_ITEM_MANAGER (self->item_mngr), PHOTOS_WINDOW_MODE_COLLECTION_VIEW);
  else
    photos_tracker_controller_refresh_for_object (PHOTOS_TRACKER_CONTROLLER (self));
}


static PhotosOffsetController *
photos_tracker_collection_view_controller_get_offset_controller (PhotosTrackerController *trk_cntrlr)
{
  PhotosTrackerCollectionViewController *self = PHOTOS_TRACKER_COLLECTION_VIEW_CONTROLLER (trk_cntrlr);
  return g_object_ref (self->offset_cntrlr);
}


static PhotosQuery *
photos_tracker_collection_view_controller_get_query (PhotosTrackerController *trk_cntrlr)
{
  PhotosTrackerCollectionViewController *self = PHOTOS_TRACKER_COLLECTION_VIEW_CONTROLLER (trk_cntrlr);
  GApplication *app;
  PhotosBaseItem *collection;
  PhotosSearchContextState *state;

  g_return_val_if_fail (self->item_mngr != NULL, NULL);

  collection = photos_item_manager_get_active_collection (PHOTOS_ITEM_MANAGER (self->item_mngr));
  g_return_val_if_fail (PHOTOS_IS_BASE_ITEM (collection), NULL);

  app = g_application_get_default ();
  state = photos_search_context_get_state (PHOTOS_SEARCH_CONTEXT (app));

  return photos_query_builder_global_query (state, PHOTOS_QUERY_FLAGS_NONE, self->offset_cntrlr);
}


static GObject *
photos_tracker_collection_view_controller_constructor (GType type,
                                                       guint n_construct_params,
                                                       GObjectConstructParam *construct_params)
{
  static GObject *self = NULL;

  if (self == NULL)
    {
      self = G_OBJECT_CLASS (photos_tracker_collection_view_controller_parent_class)
        ->constructor (type, n_construct_params, construct_params);
      g_object_add_weak_pointer (self, (gpointer) &self);
      return self;
    }

  return g_object_ref (self);
}


static void
photos_tracker_collection_view_controller_dispose (GObject *object)
{
  PhotosTrackerCollectionViewController *self = PHOTOS_TRACKER_COLLECTION_VIEW_CONTROLLER (object);

  g_clear_object (&self->offset_cntrlr);

  G_OBJECT_CLASS (photos_tracker_collection_view_controller_parent_class)->dispose (object);
}


static void
photos_tracker_collection_view_controller_finalize (GObject *object)
{
  PhotosTrackerCollectionViewController *self = PHOTOS_TRACKER_COLLECTION_VIEW_CONTROLLER (object);

  if (self->item_mngr != NULL)
    g_object_remove_weak_pointer (G_OBJECT (self->item_mngr), (gpointer *) &self->item_mngr);

  G_OBJECT_CLASS (photos_tracker_collection_view_controller_parent_class)->finalize (object);
}


static void
photos_tracker_collection_view_controller_init (PhotosTrackerCollectionViewController *self)
{
  GApplication *app;
  PhotosSearchContextState *state;

  app = g_application_get_default ();
  state = photos_search_context_get_state (PHOTOS_SEARCH_CONTEXT (app));

  self->item_mngr = state->item_mngr;
  g_object_add_weak_pointer (G_OBJECT (self->item_mngr), (gpointer *) &self->item_mngr);
  g_signal_connect_swapped (self->item_mngr,
                            "active-collection-changed",
                            G_CALLBACK (photos_tracker_collection_view_controller_active_collection_changed),
                            self);

  self->offset_cntrlr = photos_offset_collection_view_controller_dup_singleton ();

  photos_tracker_controller_set_frozen (PHOTOS_TRACKER_CONTROLLER (self), TRUE);
}


static void
photos_tracker_collection_view_controller_class_init (PhotosTrackerCollectionViewControllerClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  PhotosTrackerControllerClass *tracker_controller_class = PHOTOS_TRACKER_CONTROLLER_CLASS (class);

  object_class->constructor = photos_tracker_collection_view_controller_constructor;
  object_class->dispose = photos_tracker_collection_view_controller_dispose;
  object_class->finalize = photos_tracker_collection_view_controller_finalize;
  tracker_controller_class->get_offset_controller = photos_tracker_collection_view_controller_get_offset_controller;
  tracker_controller_class->get_query = photos_tracker_collection_view_controller_get_query;
}


PhotosTrackerController *
photos_tracker_collection_view_controller_dup_singleton (void)
{
  return g_object_new (PHOTOS_TYPE_TRACKER_COLLECTION_VIEW_CONTROLLER,
                       "mode", PHOTOS_WINDOW_MODE_COLLECTION_VIEW,
                       NULL);
}
