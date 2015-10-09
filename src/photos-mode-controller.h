/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2012, 2013, 2014, 2015 Red Hat, Inc.
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

#ifndef PHOTOS_MODE_CONTROLLER_H
#define PHOTOS_MODE_CONTROLLER_H

#include <glib-object.h>

G_BEGIN_DECLS

#define PHOTOS_TYPE_MODE_CONTROLLER (photos_mode_controller_get_type ())

#define PHOTOS_MODE_CONTROLLER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
   PHOTOS_TYPE_MODE_CONTROLLER, PhotosModeController))

#define PHOTOS_IS_MODE_CONTROLLER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
   PHOTOS_TYPE_MODE_CONTROLLER))

typedef enum
{
  PHOTOS_WINDOW_MODE_NONE,
  PHOTOS_WINDOW_MODE_COLLECTIONS,
  PHOTOS_WINDOW_MODE_FAVORITES,
  PHOTOS_WINDOW_MODE_OVERVIEW,
  PHOTOS_WINDOW_MODE_PREVIEW,
  PHOTOS_WINDOW_MODE_SEARCH
} PhotosWindowMode;

typedef struct _PhotosModeController      PhotosModeController;
typedef struct _PhotosModeControllerClass PhotosModeControllerClass;

GType                  photos_mode_controller_get_type               (void) G_GNUC_CONST;

PhotosModeController  *photos_mode_controller_dup_singleton          (void);

gboolean               photos_mode_controller_get_can_fullscreen     (PhotosModeController *self);

gboolean               photos_mode_controller_get_fullscreen         (PhotosModeController *self);

PhotosWindowMode       photos_mode_controller_get_window_mode        (PhotosModeController *self);

void                   photos_mode_controller_go_back                (PhotosModeController *self);

void                   photos_mode_controller_toggle_fullscreen      (PhotosModeController *self);

void                   photos_mode_controller_set_fullscreen         (PhotosModeController *self,
                                                                      gboolean              fullscreen);

void                   photos_mode_controller_set_window_mode        (PhotosModeController *self,
                                                                      PhotosWindowMode      mode);

G_END_DECLS

#endif /* PHOTOS_MODE_CONTROLLER_H */
