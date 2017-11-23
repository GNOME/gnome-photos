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

#ifndef PHOTOS_TRACKER_QUEUE_H
#define PHOTOS_TRACKER_QUEUE_H

#include <gio/gio.h>

#include "photos-query.h"

G_BEGIN_DECLS

#define PHOTOS_TYPE_TRACKER_QUEUE (photos_tracker_queue_get_type ())
G_DECLARE_FINAL_TYPE (PhotosTrackerQueue, photos_tracker_queue, PHOTOS, TRACKER_QUEUE, GObject);

PhotosTrackerQueue    *photos_tracker_queue_dup_singleton          (GCancellable *cancellable, GError **error);

void                   photos_tracker_queue_select                 (PhotosTrackerQueue *self,
                                                                    PhotosQuery *query,
                                                                    GCancellable *cancellable,
                                                                    GAsyncReadyCallback callback,
                                                                    gpointer user_data,
                                                                    GDestroyNotify destroy_data);

void                   photos_tracker_queue_update                 (PhotosTrackerQueue *self,
                                                                    PhotosQuery *query,
                                                                    GCancellable *cancellable,
                                                                    GAsyncReadyCallback callback,
                                                                    gpointer user_data,
                                                                    GDestroyNotify destroy_data);

void                   photos_tracker_queue_update_blank           (PhotosTrackerQueue *self,
                                                                    PhotosQuery *query,
                                                                    GCancellable *cancellable,
                                                                    GAsyncReadyCallback callback,
                                                                    gpointer user_data,
                                                                    GDestroyNotify destroy_data);

G_END_DECLS

#endif /* PHOTOS_TRACKER_QUEUE_H */
