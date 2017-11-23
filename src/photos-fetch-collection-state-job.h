/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2013 – 2017 Red Hat, Inc.
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

#ifndef PHOTOS_FETCH_COLLECTION_STATE_JOB_H
#define PHOTOS_FETCH_COLLECTION_STATE_JOB_H

#include <glib-object.h>

G_BEGIN_DECLS

#define PHOTOS_TYPE_FETCH_COLLECTION_STATE_JOB (photos_fetch_collection_state_job_get_type ())
G_DECLARE_FINAL_TYPE (PhotosFetchCollectionStateJob,
                      photos_fetch_collection_state_job,
                      PHOTOS,
                      FETCH_COLLECTION_STATE_JOB,
                      GObject);

typedef enum
{
  PHOTOS_COLLECTION_STATE_NORMAL          = 0,
  PHOTOS_COLLECTION_STATE_ACTIVE          = 1 << 0,
  PHOTOS_COLLECTION_STATE_INCONSISTENT    = 1 << 1,
  PHOTOS_COLLECTION_STATE_HIDDEN          = 1 << 2
} PhotosCollectionState;

typedef void (*PhotosFetchCollectionStateJobCallback) (GHashTable *, gpointer);

PhotosFetchCollectionStateJob *photos_fetch_collection_state_job_new      (void);

void                           photos_fetch_collection_state_job_run (PhotosFetchCollectionStateJob *self,
                                                                      PhotosFetchCollectionStateJobCallback callback,
                                                                      gpointer user_data);

G_END_DECLS

#endif /* PHOTOS_FETCH_COLLECTION_STATE_JOB_H */
