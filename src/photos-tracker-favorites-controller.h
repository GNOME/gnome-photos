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

#ifndef PHOTOS_TRACKER_FAVORITES_CONTROLLER_H
#define PHOTOS_TRACKER_FAVORITES_CONTROLLER_H

#include "photos-tracker-controller.h"

G_BEGIN_DECLS

#define PHOTOS_TYPE_TRACKER_FAVORITES_CONTROLLER (photos_tracker_favorites_controller_get_type ())

#define PHOTOS_TRACKER_FAVORITES_CONTROLLER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
   PHOTOS_TYPE_TRACKER_FAVORITES_CONTROLLER, PhotosTrackerFavoritesController))

#define PHOTOS_IS_TRACKER_FAVORITES_CONTROLLER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
   PHOTOS_TYPE_TRACKER_FAVORITES_CONTROLLER))

typedef struct _PhotosTrackerFavoritesController      PhotosTrackerFavoritesController;
typedef struct _PhotosTrackerFavoritesControllerClass PhotosTrackerFavoritesControllerClass;

GType                     photos_tracker_favorites_controller_get_type          (void) G_GNUC_CONST;

PhotosTrackerController  *photos_tracker_favorites_controller_dup_singleton     (void);

G_END_DECLS

#endif /* PHOTOS_TRACKER_FAVORITES_CONTROLLER_H */
