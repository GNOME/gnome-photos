/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2013 Red Hat, Inc.
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

#define PHOTOS_UPDATE_MTIME_JOB(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
   PHOTOS_TYPE_UPDATE_MTIME_JOB, PhotosUpdateMtimeJob))

#define PHOTOS_UPDATE_MTIME_JOB_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
   PHOTOS_TYPE_UPDATE_MTIME_JOB, PhotosUpdateMtimeJobClass))

#define PHOTOS_IS_UPDATE_MTIME_JOB(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
   PHOTOS_TYPE_UPDATE_MTIME_JOB))

#define PHOTOS_IS_UPDATE_MTIME_JOB_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
   PHOTOS_TYPE_UPDATE_MTIME_JOB))

#define PHOTOS_UPDATE_MTIME_JOB_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
   PHOTOS_TYPE_UPDATE_MTIME_JOB, PhotosUpdateMtimeJobClass))

typedef void (*PhotosUpdateMtimeJobCallback) (gpointer);

typedef struct _PhotosUpdateMtimeJob        PhotosUpdateMtimeJob;
typedef struct _PhotosUpdateMtimeJobClass   PhotosUpdateMtimeJobClass;
typedef struct _PhotosUpdateMtimeJobPrivate PhotosUpdateMtimeJobPrivate;

struct _PhotosUpdateMtimeJob
{
  GObject parent_instance;
  PhotosUpdateMtimeJobPrivate *priv;
};

struct _PhotosUpdateMtimeJobClass
{
  GObjectClass parent_class;
};

GType                   photos_update_mtime_job_get_type             (void) G_GNUC_CONST;

PhotosUpdateMtimeJob   *photos_update_mtime_job_new                  (const gchar *urn);

void                    photos_update_mtime_job_run                  (PhotosUpdateMtimeJob *self,
                                                                      PhotosUpdateMtimeJobCallback callback,
                                                                      gpointer user_data);

G_END_DECLS

#endif /* PHOTOS_UPDATE_MTIME_JOB_H */
