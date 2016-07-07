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

#ifndef PHOTOS_DELETE_ITEM_JOB_H
#define PHOTOS_DELETE_ITEM_JOB_H

#include <glib-object.h>

G_BEGIN_DECLS

#define PHOTOS_TYPE_DELETE_ITEM_JOB (photos_delete_item_job_get_type ())
G_DECLARE_FINAL_TYPE (PhotosDeleteItemJob, photos_delete_item_job, PHOTOS, DELETE_ITEM_JOB, GObject)

PhotosDeleteItemJob        *photos_delete_item_job_new         (const gchar *urn);

void                        photos_delete_item_job_run         (PhotosDeleteItemJob *self,
                                                                GCancellable *cancellable,
                                                                GAsyncReadyCallback callback,
                                                                gpointer user_data);

gboolean                    photos_delete_item_job_finish      (PhotosDeleteItemJob *self,
                                                                GAsyncResult *res,
                                                                GError **error);

G_END_DECLS

#endif /* PHOTOS_DELETE_ITEM_JOB_H */
