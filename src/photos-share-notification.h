/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2017 – 2021 Red Hat, Inc.
 * Copyright © 2016 Umang Jain
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


#ifndef PHOTOS_SHARE_NOTIFICATION_H
#define PHOTOS_SHARE_NOTIFICATION_H

#include <gtk/gtk.h>

#include "photos-base-item.h"
#include "photos-share-point.h"

G_BEGIN_DECLS

#define PHOTOS_TYPE_SHARE_NOTIFICATION (photos_share_notification_get_type ())
G_DECLARE_FINAL_TYPE (PhotosShareNotification, photos_share_notification, PHOTOS, SHARE_NOTIFICATION, GtkGrid);

void                photos_share_notification_new             (PhotosSharePoint *share_point,
                                                               PhotosBaseItem *item,
                                                               const gchar *uri);

void                photos_share_notification_new_with_error  (PhotosSharePoint *share_point, GError *error);

G_END_DECLS

#endif /* PHOTOS_SHARE_NOTIFICATION_H */
