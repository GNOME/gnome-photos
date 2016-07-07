/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2014 – 2016 Red Hat, Inc.
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

#ifndef PHOTOS_CREATE_COLLECTION_ICON_JOB_H
#define PHOTOS_CREATE_COLLECTION_ICON_JOB_H

#include <gio/gio.h>

G_BEGIN_DECLS

#define PHOTOS_TYPE_CREATE_COLLECTION_ICON_JOB (photos_create_collection_icon_job_get_type ())
G_DECLARE_FINAL_TYPE (PhotosCreateCollectionIconJob,
                      photos_create_collection_icon_job,
                      PHOTOS,
                      CREATE_COLLECTION_ICON_JOB,
                      GObject)

typedef void (*PhotosCreateCollectionIconJobCallback) (GIcon *, gpointer);

PhotosCreateCollectionIconJob *photos_create_collection_icon_job_new       (const gchar *urn);

GIcon                         *photos_create_collection_icon_job_finish    (PhotosCreateCollectionIconJob *self,
                                                                            GAsyncResult *res,
                                                                            GError **error);

void                           photos_create_collection_icon_job_run (PhotosCreateCollectionIconJob *self,
                                                                      GCancellable *cancellable,
                                                                      GAsyncReadyCallback callback,
                                                                      gpointer user_data);

G_END_DECLS

#endif /* PHOTOS_CREATE_COLLECTION_ICON_JOB_H */
