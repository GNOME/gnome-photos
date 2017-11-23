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

#ifndef PHOTOS_CREATE_COLLECTION_JOB_H
#define PHOTOS_CREATE_COLLECTION_JOB_H

#include <glib-object.h>

G_BEGIN_DECLS

#define PHOTOS_TYPE_CREATE_COLLECTION_JOB (photos_create_collection_job_get_type ())
G_DECLARE_FINAL_TYPE (PhotosCreateCollectionJob,
                      photos_create_collection_job,
                      PHOTOS,
                      CREATE_COLLECTION_JOB,
                      GObject);

PhotosCreateCollectionJob  *photos_create_collection_job_new         (const gchar *name);

void                        photos_create_collection_job_run         (PhotosCreateCollectionJob *self,
                                                                      GCancellable *cancellable,
                                                                      GAsyncReadyCallback callback,
                                                                      gpointer user_data);

gchar                      *photos_create_collection_job_finish      (PhotosCreateCollectionJob *self,
                                                                      GAsyncResult *res,
                                                                      GError **error);

G_END_DECLS

#endif /* PHOTOS_CREATE_COLLECTION_JOB_H */
