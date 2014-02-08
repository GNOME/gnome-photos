/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2014 Red Hat, Inc.
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

#ifndef PHOTOS_FETCH_IDS_JOB_H
#define PHOTOS_FETCH_IDS_JOB_H

#include <gio/gio.h>

#include "photos-search-context.h"

G_BEGIN_DECLS

#define PHOTOS_TYPE_FETCH_IDS_JOB (photos_fetch_ids_job_get_type ())

#define PHOTOS_FETCH_IDS_JOB(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
   PHOTOS_TYPE_FETCH_IDS_JOB, PhotosFetchIdsJob))

#define PHOTOS_FETCH_IDS_JOB_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
   PHOTOS_TYPE_FETCH_IDS_JOB, PhotosFetchIdsJobClass))

#define PHOTOS_IS_FETCH_IDS_JOB(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
   PHOTOS_TYPE_FETCH_IDS_JOB))

#define PHOTOS_IS_FETCH_IDS_JOB_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
   PHOTOS_TYPE_FETCH_IDS_JOB))

#define PHOTOS_FETCH_IDS_JOB_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
   PHOTOS_TYPE_FETCH_IDS_JOB, PhotosFetchIdsJobClass))

typedef void (*PhotosFetchIdsJobCallback) (const gchar *const *, gpointer);

typedef struct _PhotosFetchIdsJob        PhotosFetchIdsJob;
typedef struct _PhotosFetchIdsJobClass   PhotosFetchIdsJobClass;
typedef struct _PhotosFetchIdsJobPrivate PhotosFetchIdsJobPrivate;

struct _PhotosFetchIdsJob
{
  GObject parent_instance;
  PhotosFetchIdsJobPrivate *priv;
};

struct _PhotosFetchIdsJobClass
{
  GObjectClass parent_class;
};

GType                photos_fetch_ids_job_get_type          (void) G_GNUC_CONST;

PhotosFetchIdsJob   *photos_fetch_ids_job_new               (const gchar *const *terms);

void                 photos_fetch_ids_job_run               (PhotosFetchIdsJob *self,
                                                             PhotosSearchContextState *state,
                                                             GCancellable *cancellable,
                                                             PhotosFetchIdsJobCallback callback,
                                                             gpointer user_data);

G_END_DECLS

#endif /* PHOTOS_FETCH_IDS_JOB_H */
