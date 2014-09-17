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

#ifndef PHOTOS_TRACKER_CHANGE_EVENT_H
#define PHOTOS_TRACKER_CHANGE_EVENT_H

#include <glib.h>

G_BEGIN_DECLS

typedef enum
{
  PHOTOS_TRACKER_CHANGE_EVENT_CHANGED,
  PHOTOS_TRACKER_CHANGE_EVENT_CREATED,
  PHOTOS_TRACKER_CHANGE_EVENT_DELETED
} PhotosTrackerChangeEventType;

typedef struct _PhotosTrackerChangeEvent PhotosTrackerChangeEvent;

PhotosTrackerChangeEvent  *photos_tracker_change_event_new        (gint32 urn_id,
                                                                   gint32 predicate_id,
                                                                   gboolean is_delete);

PhotosTrackerChangeEvent  *photos_tracker_change_event_copy       (PhotosTrackerChangeEvent *event);

void                       photos_tracker_change_event_free       (PhotosTrackerChangeEvent *self);

PhotosTrackerChangeEventType photos_tracker_change_event_get_type (PhotosTrackerChangeEvent *self);

gint32                     photos_tracker_change_event_get_predicate_id (PhotosTrackerChangeEvent *self);

const gchar               *photos_tracker_change_event_get_urn    (PhotosTrackerChangeEvent *self);

gint32                     photos_tracker_change_event_get_urn_id (PhotosTrackerChangeEvent *self);

void                       photos_tracker_change_event_merge      (PhotosTrackerChangeEvent *self,
                                                                   PhotosTrackerChangeEvent *event);

void                       photos_tracker_change_event_set_resolved_values (PhotosTrackerChangeEvent *self,
                                                                            const gchar *urn,
                                                                            const gchar *predicate);

G_END_DECLS

#endif /* PHOTOS_TRACKER_CHANGE_EVENT_H */
