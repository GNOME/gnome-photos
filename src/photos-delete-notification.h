/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2014 Pranav Kant
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

#ifndef PHOTOS_DELETE_NOTIFICATION_H
#define PHOTOS_DELETE_NOTIFICATION_H

#include <glib.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define PHOTOS_TYPE_DELETE_NOTIFICATION (photos_delete_notification_get_type ())

#define PHOTOS_DELETE_NOTIFICATION(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
   PHOTOS_TYPE_DELETE_NOTIFICATION, PhotosDeleteNotification))

#define PHOTOS_DELETE_NOTIFICATION_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
   PHOTOS_TYPE_DELETE_NOTIFICATION, PhotosDeleteNotificationClass))

#define PHOTOS_IS_DELETE_NOTIFICATION(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
   PHOTOS_TYPE_DELETE_NOTIFICATION))

#define PHOTOS_IS_DELETE_NOTIFICATION_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
   PHOTOS_TYPE_DELETE_NOTIFICATION))

#define PHOTOS_DELETE_NOTIFICATION_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
   PHOTOS_TYPE_DELETE_NOTIFICATION, PhotosDeleteNotificationClass))

typedef struct _PhotosDeleteNotification        PhotosDeleteNotification;
typedef struct _PhotosDeleteNotificationClass   PhotosDeleteNotificationClass;
typedef struct _PhotosDeleteNotificationPrivate PhotosDeleteNotificationPrivate;

struct _PhotosDeleteNotification
{
  GtkGrid parent_instance;
  PhotosDeleteNotificationPrivate *priv;
};

struct _PhotosDeleteNotificationClass
{
  GtkGridClass parent_class;
};

GType               photos_delete_notification_get_type           (void) G_GNUC_CONST;

void                photos_delete_notification_new                (GList *items);

G_END_DECLS

#endif /* PHOTOS_DELETE_NOTIFICATION_H */
