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

/* Based on code from:
 *   + Documents
 */

#ifndef PHOTOS_OFFSET_CONTROLLER_H
#define PHOTOS_OFFSET_CONTROLLER_H

#include <glib-object.h>

#include "photos-query.h"

G_BEGIN_DECLS

#define PHOTOS_TYPE_OFFSET_CONTROLLER (photos_offset_controller_get_type ())

#define PHOTOS_OFFSET_CONTROLLER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
   PHOTOS_TYPE_OFFSET_CONTROLLER, PhotosOffsetController))

#define PHOTOS_OFFSET_CONTROLLER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
   PHOTOS_TYPE_OFFSET_CONTROLLER, PhotosOffsetControllerClass))

#define PHOTOS_IS_OFFSET_CONTROLLER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
   PHOTOS_TYPE_OFFSET_CONTROLLER))

#define PHOTOS_IS_OFFSET_CONTROLLER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
   PHOTOS_TYPE_OFFSET_CONTROLLER))

#define PHOTOS_OFFSET_CONTROLLER_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
   PHOTOS_TYPE_OFFSET_CONTROLLER, PhotosOffsetControllerClass))

typedef struct _PhotosOffsetController        PhotosOffsetController;
typedef struct _PhotosOffsetControllerClass   PhotosOffsetControllerClass;
typedef struct _PhotosOffsetControllerPrivate PhotosOffsetControllerPrivate;

struct _PhotosOffsetController
{
  GObject parent_instance;
  PhotosOffsetControllerPrivate *priv;
};

struct _PhotosOffsetControllerClass
{
  GObjectClass parent_class;

  /* virtual methods */
  PhotosQuery *(*get_query) (void);

  /* signals */
  void (*count_changed)      (PhotosOffsetController *self, gint count);
  void (*offset_changed)     (PhotosOffsetController *self, gint offset);
};

GType                       photos_offset_controller_get_type           (void) G_GNUC_CONST;

gint                        photos_offset_controller_get_count          (PhotosOffsetController *self);

gint                        photos_offset_controller_get_offset         (PhotosOffsetController *self);

gint                        photos_offset_controller_get_remaining      (PhotosOffsetController *self);

gint                        photos_offset_controller_get_step           (PhotosOffsetController *self);

void                        photos_offset_controller_increase_offset    (PhotosOffsetController *self);

void                        photos_offset_controller_reset_count        (PhotosOffsetController *self);

void                        photos_offset_controller_reset_offset       (PhotosOffsetController *self);

G_END_DECLS

#endif /* PHOTOS_OFFSET_CONTROLLER_H */
