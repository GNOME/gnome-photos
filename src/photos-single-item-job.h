/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2012 – 2017 Red Hat, Inc.
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

#ifndef PHOTOS_SINGLE_ITEM_JOB_H
#define PHOTOS_SINGLE_ITEM_JOB_H

#include <gio/gio.h>
#include <tracker-sparql.h>

#include "photos-search-context.h"

G_BEGIN_DECLS

#define PHOTOS_TYPE_SINGLE_ITEM_JOB (photos_single_item_job_get_type ())
G_DECLARE_FINAL_TYPE (PhotosSingleItemJob, photos_single_item_job, PHOTOS, SINGLE_ITEM_JOB, GObject);

typedef void (*PhotosSingleItemJobCallback) (TrackerSparqlCursor *, gpointer);

PhotosSingleItemJob   *photos_single_item_job_new                  (const gchar *urn);

TrackerSparqlCursor   *photos_single_item_job_finish               (PhotosSingleItemJob *self,
                                                                    GAsyncResult *res,
                                                                    GError **error);

void                   photos_single_item_job_run                  (PhotosSingleItemJob *self,
                                                                    PhotosSearchContextState *state,
                                                                    gint flags,
                                                                    GCancellable *cancellable,
                                                                    GAsyncReadyCallback callback,
                                                                    gpointer user_data);

G_END_DECLS

#endif /* PHOTOS_SINGLE_ITEM_JOB_H */
