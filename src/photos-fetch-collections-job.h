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

#ifndef PHOTOS_FETCH_COLLECTIONS_JOB_H
#define PHOTOS_FETCH_COLLECTIONS_JOB_H

#include <glib-object.h>

G_BEGIN_DECLS

#define PHOTOS_TYPE_FETCH_COLLECTIONS_JOB (photos_fetch_collections_job_get_type ())
G_DECLARE_FINAL_TYPE (PhotosFetchCollectionsJob,
                      photos_fetch_collections_job,
                      PHOTOS,
                      FETCH_COLLECTIONS_JOB,
                      GObject);

PhotosFetchCollectionsJob *photos_fetch_collections_job_new         (const gchar *urn);

void                       photos_fetch_collections_job_run         (PhotosFetchCollectionsJob *self,
                                                                     GCancellable *cancellable,
                                                                     GAsyncReadyCallback callback,
                                                                     gpointer user_data);

GList                     *photos_fetch_collections_job_finish      (PhotosFetchCollectionsJob *self,
                                                                     GAsyncResult *res,
                                                                     GError **error);

const gchar               *photos_fetch_collections_job_get_urn     (PhotosFetchCollectionsJob *self);

G_END_DECLS

#endif /* PHOTOS_FETCH_COLLECTIONS_JOB_H */
