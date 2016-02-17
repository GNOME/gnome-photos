/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2013 – 2016 Red Hat, Inc.
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

#ifndef PHOTOS_FETCH_COLLECTION_STATE_JOB_H
#define PHOTOS_FETCH_COLLECTION_STATE_JOB_H

#include <glib-object.h>

G_BEGIN_DECLS

#define PHOTOS_TYPE_FETCH_COLLECTION_STATE_JOB (photos_fetch_collection_state_job_get_type ())

#define PHOTOS_FETCH_COLLECTION_STATE_JOB(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
   PHOTOS_TYPE_FETCH_COLLECTION_STATE_JOB, PhotosFetchCollectionStateJob))

#define PHOTOS_IS_FETCH_COLLECTION_STATE_JOB(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
   PHOTOS_TYPE_FETCH_COLLECTION_STATE_JOB))

typedef enum
{
  PHOTOS_COLLECTION_STATE_NORMAL          = 0,
  PHOTOS_COLLECTION_STATE_ACTIVE          = 1 << 0,
  PHOTOS_COLLECTION_STATE_INCONSISTENT    = 1 << 1,
  PHOTOS_COLLECTION_STATE_HIDDEN          = 1 << 2
} PhotosCollectionState;

typedef void (*PhotosFetchCollectionStateJobCallback) (GHashTable *, gpointer);

typedef struct _PhotosFetchCollectionStateJob      PhotosFetchCollectionStateJob;
typedef struct _PhotosFetchCollectionStateJobClass PhotosFetchCollectionStateJobClass;

GType                          photos_fetch_collection_state_job_get_type (void) G_GNUC_CONST;

PhotosFetchCollectionStateJob *photos_fetch_collection_state_job_new      (void);

void                           photos_fetch_collection_state_job_run (PhotosFetchCollectionStateJob *self,
                                                                      PhotosFetchCollectionStateJobCallback callback,
                                                                      gpointer user_data);

G_END_DECLS

#endif /* PHOTOS_FETCH_COLLECTION_STATE_JOB_H */
