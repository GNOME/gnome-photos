/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2014 – 2017 Red Hat, Inc.
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

#ifndef PHOTOS_PRINT_NOTIFICATION_H
#define PHOTOS_PRINT_NOTIFICATION_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define PHOTOS_TYPE_PRINT_NOTIFICATION (photos_print_notification_get_type ())
G_DECLARE_FINAL_TYPE (PhotosPrintNotification, photos_print_notification, PHOTOS, PRINT_NOTIFICATION, GtkGrid);

void                photos_print_notification_new                (GtkPrintOperation *print_op);

G_END_DECLS

#endif /* PHOTOS_PRINT_NOTIFICATION_H */
