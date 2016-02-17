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

#ifndef PHOTOS_SET_COLLECTION_JOB_H
#define PHOTOS_SET_COLLECTION_JOB_H

#include <glib-object.h>

G_BEGIN_DECLS

#define PHOTOS_TYPE_SET_COLLECTION_JOB (photos_set_collection_job_get_type ())

#define PHOTOS_SET_COLLECTION_JOB(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
   PHOTOS_TYPE_SET_COLLECTION_JOB, PhotosSetCollectionJob))

#define PHOTOS_IS_SET_COLLECTION_JOB(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
   PHOTOS_TYPE_SET_COLLECTION_JOB))

typedef void (*PhotosSetCollectionJobCallback) (gpointer);

typedef struct _PhotosSetCollectionJob      PhotosSetCollectionJob;
typedef struct _PhotosSetCollectionJobClass PhotosSetCollectionJobClass;

GType                     photos_set_collection_job_get_type    (void) G_GNUC_CONST;

PhotosSetCollectionJob   *photos_set_collection_job_new         (const gchar *collection_urn, gboolean setting);

void                      photos_set_collection_job_run         (PhotosSetCollectionJob *self,
                                                                 PhotosSetCollectionJobCallback callback,
                                                                 gpointer user_data);

G_END_DECLS

#endif /* PHOTOS_SET_COLLECTION_JOB_H */
