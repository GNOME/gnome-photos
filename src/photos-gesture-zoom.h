/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2017 – 2021 Red Hat, Inc.
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

#ifndef PHOTOS_GESTURE_ZOOM_H
#define PHOTOS_GESTURE_ZOOM_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define PHOTOS_TYPE_GESTURE_ZOOM (photos_gesture_zoom_get_type ())
G_DECLARE_FINAL_TYPE (PhotosGestureZoom,
                      photos_gesture_zoom,
                      PHOTOS,
                      GESTURE_ZOOM,
                      GObject);

typedef enum
{
  PHOTOS_GESTURE_ZOOM_DIRECTION_NONE,
  PHOTOS_GESTURE_ZOOM_DIRECTION_DECREASING,
  PHOTOS_GESTURE_ZOOM_DIRECTION_INCREASING
} PhotosGestureZoomDirection;

PhotosGestureZoom    *photos_gesture_zoom_new                           (GtkGesture *gesture);

GtkGesture           *photos_gesture_zoom_get_gesture                   (PhotosGestureZoom *self);

G_END_DECLS

#endif /* PHOTOS_GESTURE_ZOOM_H */
