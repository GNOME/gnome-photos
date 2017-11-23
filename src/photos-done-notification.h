/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2016 Rafael Fonseca
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

#ifndef PHOTOS_DONE_NOTIFICATION_H
#define PHOTOS_DONE_NOTIFICATION_H

#include <gtk/gtk.h>

#include "photos-base-item.h"

G_BEGIN_DECLS

#define PHOTOS_TYPE_DONE_NOTIFICATION (photos_done_notification_get_type ())
G_DECLARE_FINAL_TYPE (PhotosDoneNotification, photos_done_notification, PHOTOS, DONE_NOTIFICATION, GtkGrid);

void              photos_done_notification_new      (PhotosBaseItem *item);

G_END_DECLS

#endif /* PHOTOS_DONE_NOTIFICATION_H */
