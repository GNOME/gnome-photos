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

#ifndef PHOTOS_UPDATE_MTIME_JOB_H
#define PHOTOS_UPDATE_MTIME_JOB_H

#include <glib-object.h>

G_BEGIN_DECLS

#define PHOTOS_TYPE_UPDATE_MTIME_JOB (photos_update_mtime_job_get_type ())
G_DECLARE_FINAL_TYPE (PhotosUpdateMtimeJob, photos_update_mtime_job, PHOTOS, UPDATE_MTIME_JOB, GObject);

PhotosUpdateMtimeJob   *photos_update_mtime_job_new                  (const gchar *urn);

void                    photos_update_mtime_job_run                  (PhotosUpdateMtimeJob *self,
                                                                      GCancellable *cancellable,
                                                                      GAsyncReadyCallback callback,
                                                                      gpointer user_data);

gboolean                photos_update_mtime_job_finish               (PhotosUpdateMtimeJob *self,
                                                                      GAsyncResult *res,
                                                                      GError **error);


G_END_DECLS

#endif /* PHOTOS_UPDATE_MTIME_JOB_H */
