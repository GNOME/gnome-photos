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

#ifndef PHOTOS_TRACKER_CONTROLLER_H
#define PHOTOS_TRACKER_CONTROLLER_H

#include <glib-object.h>

#include "photos-offset-controller.h"
#include "photos-query.h"

G_BEGIN_DECLS

#define PHOTOS_TYPE_TRACKER_CONTROLLER (photos_tracker_controller_get_type ())
G_DECLARE_DERIVABLE_TYPE (PhotosTrackerController, photos_tracker_controller, PHOTOS, TRACKER_CONTROLLER, GObject);

typedef struct _PhotosTrackerControllerPrivate PhotosTrackerControllerPrivate;

struct _PhotosTrackerControllerClass
{
  GObjectClass parent_class;

  GType base_item_type;

  /* virtual methods */
  PhotosOffsetController *(*get_offset_controller) (PhotosTrackerController *self);
  PhotosQuery *(*get_query) (PhotosTrackerController *self);

  /* signals */
  void (*query_error) (PhotosTrackerController *self, const gchar *primary, const gchar *secondary);
  void (*query_status_changed) (PhotosTrackerController *self, gboolean querying);
};

void                      photos_tracker_controller_set_frozen        (PhotosTrackerController *self,
                                                                       gboolean frozen);

gboolean                  photos_tracker_controller_get_query_status  (PhotosTrackerController *self);

void                      photos_tracker_controller_refresh_for_object (PhotosTrackerController *self);

void                      photos_tracker_controller_start             (PhotosTrackerController *self);

G_END_DECLS

#endif /* PHOTOS_TRACKER_CONTROLLER_H */
