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

#ifndef PHOTOS_OFFSET_CONTROLLER_H
#define PHOTOS_OFFSET_CONTROLLER_H

#include <glib-object.h>

#include "photos-query.h"

G_BEGIN_DECLS

#define PHOTOS_TYPE_OFFSET_CONTROLLER (photos_offset_controller_get_type ())
G_DECLARE_DERIVABLE_TYPE (PhotosOffsetController, photos_offset_controller, PHOTOS, OFFSET_CONTROLLER, GObject);

typedef struct _PhotosOffsetControllerPrivate PhotosOffsetControllerPrivate;

struct _PhotosOffsetControllerClass
{
  GObjectClass parent_class;

  /* virtual methods */
  PhotosQuery *(*get_query) (PhotosOffsetController *self);

  /* signals */
  void (*count_changed)      (PhotosOffsetController *self, gint count);
  void (*offset_changed)     (PhotosOffsetController *self, gint offset);
};

PhotosOffsetController *    photos_offset_controller_new                (void);

gint                        photos_offset_controller_get_count          (PhotosOffsetController *self);

gint                        photos_offset_controller_get_offset         (PhotosOffsetController *self);

gint                        photos_offset_controller_get_remaining      (PhotosOffsetController *self);

gint                        photos_offset_controller_get_step           (PhotosOffsetController *self);

void                        photos_offset_controller_increase_offset    (PhotosOffsetController *self);

void                        photos_offset_controller_reset_count        (PhotosOffsetController *self);

void                        photos_offset_controller_reset_offset       (PhotosOffsetController *self);

G_END_DECLS

#endif /* PHOTOS_OFFSET_CONTROLLER_H */
