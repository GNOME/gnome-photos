/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2014, 2015 Red Hat, Inc.
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

#include "photos-base-manager.h"
#include "photos-item-manager.h"
#include "photos-offset-search-controller.h"
#include "photos-query-builder.h"
#include "photos-search-context.h"
#include "photos-search-controller.h"
#include "photos-tracker-search-controller.h"
#include "photos-utils.h"


struct _PhotosTrackerSearchControllerPrivate
{
  PhotosBaseManager *item_mngr;
  PhotosBaseManager *src_mngr;
  PhotosBaseManager *srch_mtch_mngr;
  PhotosBaseManager *srch_typ_mngr;
  PhotosModeController *mode_cntrlr;
  PhotosOffsetController *offset_cntrlr;
  PhotosSearchController *srch_cntrlr;
};


G_DEFINE_TYPE_WITH_CODE (PhotosTrackerSearchController,
                         photos_tracker_search_controller,
                         PHOTOS_TYPE_TRACKER_CONTROLLER,
                         G_ADD_PRIVATE (PhotosTrackerSearchController)
                         photos_utils_ensure_extension_points ();
                         g_io_extension_point_implement (PHOTOS_TRACKER_CONTROLLER_EXTENSION_POINT_NAME,
                                                         g_define_type_id,
                                                         "search",
                                                         0));


static void
photos_tracker_search_controller_col_active_changed (PhotosTrackerSearchController *self)
{
  PhotosWindowMode mode;

  mode = photos_mode_controller_get_window_mode (self->priv->mode_cntrlr);
  if (mode != PHOTOS_WINDOW_MODE_SEARCH)
    return;

  photos_tracker_controller_refresh_for_object (PHOTOS_TRACKER_CONTROLLER (self));
}


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
  GApplication *app;
  PhotosSearchContextState *state;

  app = g_application_get_default ();
  state = photos_search_context_get_state (PHOTOS_SEARCH_CONTEXT (app));

  return photos_query_builder_global_query (state, PHOTOS_QUERY_FLAGS_SEARCH, self->priv->offset_cntrlr);
}


static void
photos_tracker_search_controller_search_match_active_changed (PhotosTrackerSearchController *self)
{
  const gchar *str;

  str = photos_search_controller_get_string (self->priv->srch_cntrlr);
  if (str == NULL || str[0] == '\0')
    return;

  photos_tracker_controller_refresh_for_object (PHOTOS_TRACKER_CONTROLLER (self));
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
  PhotosTrackerSearchControllerPrivate *priv = self->priv;

  g_clear_object (&priv->item_mngr);
  g_clear_object (&priv->src_mngr);
  g_clear_object (&priv->srch_mtch_mngr);
  g_clear_object (&priv->srch_typ_mngr);
  g_clear_object (&priv->mode_cntrlr);
  g_clear_object (&priv->offset_cntrlr);
  g_clear_object (&priv->srch_cntrlr);

  G_OBJECT_CLASS (photos_tracker_search_controller_parent_class)->dispose (object);
}


static void
photos_tracker_search_controller_init (PhotosTrackerSearchController *self)
{
  PhotosTrackerSearchControllerPrivate *priv;
  GApplication *app;
  PhotosSearchContextState *state;

  self->priv = photos_tracker_search_controller_get_instance_private (self);
  priv = self->priv;

  app = g_application_get_default ();
  state = photos_search_context_get_state (PHOTOS_SEARCH_CONTEXT (app));

  priv->item_mngr = g_object_ref (state->item_mngr);
  g_signal_connect_swapped (priv->item_mngr,
                            "active-collection-changed",
                            G_CALLBACK (photos_tracker_search_controller_col_active_changed),
                            self);

  priv->src_mngr = g_object_ref (state->src_mngr);
  g_signal_connect_swapped (priv->src_mngr,
                            "active-changed",
                            G_CALLBACK (photos_tracker_controller_refresh_for_object),
                            self);

  priv->srch_mtch_mngr = g_object_ref (state->srch_mtch_mngr);
  g_signal_connect_swapped (priv->srch_mtch_mngr,
                            "active-changed",
                            G_CALLBACK (photos_tracker_search_controller_search_match_active_changed),
                            self);

  priv->srch_typ_mngr = g_object_ref (state->srch_typ_mngr);
  g_signal_connect_swapped (priv->srch_typ_mngr,
                            "active-changed",
                            G_CALLBACK (photos_tracker_controller_refresh_for_object),
                            self);

  priv->mode_cntrlr = g_object_ref (state->mode_cntrlr);
  priv->offset_cntrlr = photos_offset_search_controller_dup_singleton ();

  priv->srch_cntrlr = g_object_ref (state->srch_cntrlr);
  g_signal_connect_swapped (priv->srch_cntrlr,
                            "search-string-changed",
                            G_CALLBACK (photos_tracker_controller_refresh_for_object),
                            self);
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
