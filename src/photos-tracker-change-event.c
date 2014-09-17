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


#include "config.h"

#include "photos-tracker-change-event.h"


struct _PhotosTrackerChangeEvent
{
  PhotosTrackerChangeEventType type;
  gchar *predicate;
  gchar *urn;
  gint32 predicate_id;
  gint32 urn_id;
};


static const gchar *RDF_TYPE = "http://www.w3.org/1999/02/22-rdf-syntax-ns#type";


void
photos_tracker_change_event_free (PhotosTrackerChangeEvent *self)
{
  g_free (self->predicate);
  g_free (self->urn);
  g_slice_free (PhotosTrackerChangeEvent, self);
}


PhotosTrackerChangeEvent *
photos_tracker_change_event_new (gint32 urn_id, gint32 predicate_id, gboolean is_delete)
{
  PhotosTrackerChangeEvent *self;

  self = g_slice_new0 (PhotosTrackerChangeEvent);
  self->urn_id = urn_id;
  self->predicate_id = predicate_id;

  if (is_delete)
    self->type = PHOTOS_TRACKER_CHANGE_EVENT_DELETED;
  else
    self->type = PHOTOS_TRACKER_CHANGE_EVENT_CREATED;

  return self;
}


PhotosTrackerChangeEvent *
photos_tracker_change_event_copy (PhotosTrackerChangeEvent *event)
{
  PhotosTrackerChangeEvent *self;

  self = g_slice_new0 (PhotosTrackerChangeEvent);
  self->type = event->type;
  self->predicate = g_strdup (event->predicate);
  self->urn = g_strdup (event->urn);
  self->predicate_id = event->predicate_id;
  self->urn_id = event->urn_id;

  return self;
}


PhotosTrackerChangeEventType
photos_tracker_change_event_get_type (PhotosTrackerChangeEvent *self)
{
  return self->type;
}


gint32
photos_tracker_change_event_get_predicate_id (PhotosTrackerChangeEvent *self)
{
  return self->predicate_id;
}


const gchar *
photos_tracker_change_event_get_urn (PhotosTrackerChangeEvent *self)
{
  return self->urn;
}


gint32
photos_tracker_change_event_get_urn_id (PhotosTrackerChangeEvent *self)
{
  return self->urn_id;
}


void
photos_tracker_change_event_merge (PhotosTrackerChangeEvent *self, PhotosTrackerChangeEvent *event)
{
  if (event->type == PHOTOS_TRACKER_CHANGE_EVENT_DELETED || event->type == PHOTOS_TRACKER_CHANGE_EVENT_CREATED)
    self->type = event->type;
}


void
photos_tracker_change_event_set_resolved_values (PhotosTrackerChangeEvent *self,
                                                 const gchar *urn,
                                                 const gchar *predicate)
{
  self->urn = g_strdup (urn);
  self->predicate = g_strdup (predicate);

  if (g_strcmp0 (predicate, RDF_TYPE) != 0)
    self->type = PHOTOS_TRACKER_CHANGE_EVENT_CHANGED;
}
