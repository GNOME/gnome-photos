/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2016 Red Hat, Inc.
 * Copyright © 2016 Umang Jain
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

#ifndef PHOTOS_SHARE_POINT_H
#define PHOTOS_SHARE_POINT_H

#include <gio/gio.h>

#include "photos-base-item.h"

G_BEGIN_DECLS

#define PHOTOS_TYPE_SHARE_POINT (photos_share_point_get_type ())
G_DECLARE_DERIVABLE_TYPE (PhotosSharePoint, photos_share_point, PHOTOS, SHARE_POINT, GObject)

typedef struct _PhotosSharePointPrivate PhotosSharePointPrivate;

struct _PhotosSharePointClass
{
  GObjectClass parent_class;

  /* virtual methods */
  GIcon          *(*get_icon)       (PhotosSharePoint *self);
  const gchar    *(*get_name)       (PhotosSharePoint *self);
  gchar          *(*parse_error)    (PhotosSharePoint *self, GError *error);
  void            (*share_async)    (PhotosSharePoint *self,
                                     PhotosBaseItem *item,
                                     GCancellable *cancellable,
                                     GAsyncReadyCallback callback,
                                     gpointer user_data);
  gboolean        (*share_finish)   (PhotosSharePoint *self, GAsyncResult *res, GError **error);
};

GIcon                  *photos_share_point_get_icon               (PhotosSharePoint *self);

const gchar            *photos_share_point_get_name               (PhotosSharePoint *self);

gchar                  *photos_share_point_parse_error            (PhotosSharePoint *self, GError *error);

void                    photos_share_point_share_async            (PhotosSharePoint *self,
                                                                   PhotosBaseItem *item,
                                                                   GCancellable *cancellable,
                                                                   GAsyncReadyCallback callback,
                                                                   gpointer user_data);

gboolean                photos_share_point_share_finish           (PhotosSharePoint *self,
                                                                   GAsyncResult *res,
                                                                   GError **error);

G_END_DECLS

#endif /* PHOTOS_SHARE_POINT_H */
