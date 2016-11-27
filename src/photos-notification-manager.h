/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2012 – 2016 Red Hat, Inc.
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

#ifndef PHOTOS_NOTIFICATION_MANAGER_H
#define PHOTOS_NOTIFICATION_MANAGER_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define PHOTOS_TYPE_NOTIFICATION_MANAGER (photos_notification_manager_get_type ())
G_DECLARE_FINAL_TYPE (PhotosNotificationManager,
                      photos_notification_manager,
                      PHOTOS,
                      NOTIFICATION_MANAGER,
                      GtkRevealer);

GtkWidget          *photos_notification_manager_dup_singleton      (void);

void                photos_notification_manager_add_notification   (PhotosNotificationManager *self,
                                                                    GtkWidget *notification);

G_END_DECLS

#endif /* PHOTOS_NOTIFICATION_MANAGER_H */
