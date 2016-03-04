/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2016 Rafael Fonseca
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

#ifndef PHOTOS_DONE_NOTIFICATION_H
#define PHOTOS_DONE_NOTIFICATION_H

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define PHOTOS_TYPE_DONE_NOTIFICATION (photos_done_notification_get_type ())

#define PHOTOS_DONE_NOTIFICATION(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
   PHOTOS_TYPE_DONE_NOTIFICATION, PhotosDoneNotification))

#define PHOTOS_IS_DONE_NOTIFICATION(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
   PHOTOS_TYPE_DONE_NOTIFICATION))

typedef struct _PhotosDoneNotification      PhotosDoneNotification;
typedef struct _PhotosDoneNotificationClass PhotosDoneNotificationClass;

GType             photos_done_notification_get_type (void) G_GNUC_CONST;

void              photos_done_notification_new      (PhotosBaseItem *item);

G_END_DECLS

#endif /* PHOTOS_DONE_NOTIFICATION_H */
