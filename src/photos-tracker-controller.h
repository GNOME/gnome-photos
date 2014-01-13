/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2012, 2014 Red Hat, Inc.
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

#ifndef PHOTOS_TRACKER_CONTROLLER_H
#define PHOTOS_TRACKER_CONTROLLER_H

#include <glib-object.h>

#include "photos-offset-controller.h"
#include "photos-query.h"

G_BEGIN_DECLS

#define PHOTOS_TYPE_TRACKER_CONTROLLER (photos_tracker_controller_get_type ())

#define PHOTOS_TRACKER_CONTROLLER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
   PHOTOS_TYPE_TRACKER_CONTROLLER, PhotosTrackerController))

#define PHOTOS_TRACKER_CONTROLLER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
   PHOTOS_TYPE_TRACKER_CONTROLLER, PhotosTrackerControllerClass))

#define PHOTOS_IS_TRACKER_CONTROLLER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
   PHOTOS_TYPE_TRACKER_CONTROLLER))

#define PHOTOS_IS_TRACKER_CONTROLLER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
   PHOTOS_TYPE_TRACKER_CONTROLLER))

#define PHOTOS_TRACKER_CONTROLLER_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
   PHOTOS_TYPE_TRACKER_CONTROLLER, PhotosTrackerControllerClass))

typedef struct _PhotosTrackerController        PhotosTrackerController;
typedef struct _PhotosTrackerControllerClass   PhotosTrackerControllerClass;
typedef struct _PhotosTrackerControllerPrivate PhotosTrackerControllerPrivate;

struct _PhotosTrackerController
{
  GObject parent_instance;
  PhotosTrackerControllerPrivate *priv;
};

struct _PhotosTrackerControllerClass
{
  GObjectClass parent_class;

  /* virtual methods */
  PhotosOffsetController *(*get_offset_controller) (PhotosTrackerController *self);
  PhotosQuery *(*get_query) (PhotosTrackerController *self);

  /* signals */
  void (*query_error) (PhotosTrackerController *self, const gchar *primary, const gchar *secondary);
  void (*query_status_changed) (PhotosTrackerController *self, gboolean querying);
};

GType                     photos_tracker_controller_get_type          (void) G_GNUC_CONST;

gboolean                  photos_tracker_controller_get_query_status  (PhotosTrackerController *self);

void                      photos_tracker_controller_start             (PhotosTrackerController *self);

G_END_DECLS

#endif /* PHOTOS_TRACKER_CONTROLLER_H */
