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

#ifndef PHOTOS_TRACKER_QUEUE_H
#define PHOTOS_TRACKER_QUEUE_H

#include <gio/gio.h>

G_BEGIN_DECLS

#define PHOTOS_TYPE_TRACKER_QUEUE (photos_tracker_queue_get_type ())

#define PHOTOS_TRACKER_QUEUE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
   PHOTOS_TYPE_TRACKER_QUEUE, PhotosTrackerQueue))

#define PHOTOS_TRACKER_QUEUE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
   PHOTOS_TYPE_TRACKER_QUEUE, PhotosTrackerQueueClass))

#define PHOTOS_IS_TRACKER_QUEUE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
   PHOTOS_TYPE_TRACKER_QUEUE))

#define PHOTOS_IS_TRACKER_QUEUE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
   PHOTOS_TYPE_TRACKER_QUEUE))

#define PHOTOS_TRACKER_QUEUE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
   PHOTOS_TYPE_TRACKER_QUEUE, PhotosTrackerQueueClass))

typedef struct _PhotosTrackerQueue        PhotosTrackerQueue;
typedef struct _PhotosTrackerQueueClass   PhotosTrackerQueueClass;
typedef struct _PhotosTrackerQueuePrivate PhotosTrackerQueuePrivate;

struct _PhotosTrackerQueue
{
  GObject parent_instance;
  PhotosTrackerQueuePrivate *priv;
};

struct _PhotosTrackerQueueClass
{
  GObjectClass parent_class;
};

GType                  photos_tracker_queue_get_type               (void) G_GNUC_CONST;

PhotosTrackerQueue    *photos_tracker_queue_new                    (void);

void                   photos_tracker_queue_select                 (PhotosTrackerQueue *self,
                                                                    const gchar *sparql,
                                                                    GCancellable *cancellable,
                                                                    GAsyncReadyCallback callback,
                                                                    gpointer user_data,
                                                                    GDestroyNotify destroy_data);

void                   photos_tracker_queue_update                 (PhotosTrackerQueue *self,
                                                                    const gchar *sparql,
                                                                    GCancellable *cancellable,
                                                                    GAsyncReadyCallback callback,
                                                                    gpointer user_data,
                                                                    GDestroyNotify destroy_data);

G_END_DECLS

#endif /* PHOTOS_TRACKER_QUEUE_H */
