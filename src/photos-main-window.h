/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2012 Red Hat, Inc.
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

#ifndef PHOTOS_MAIN_WINDOW_H
#define PHOTOS_MAIN_WINDOW_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define PHOTOS_TYPE_MAIN_WINDOW (photos_main_window_get_type ())

#define PHOTOS_MAIN_WINDOW(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
   PHOTOS_TYPE_MAIN_WINDOW, PhotosMainWindow))

#define PHOTOS_MAIN_WINDOW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
   PHOTOS_TYPE_MAIN_WINDOW, PhotosMainWindowClass))

#define PHOTOS_IS_MAIN_WINDOW(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
   PHOTOS_TYPE_MAIN_WINDOW))

#define PHOTOS_IS_MAIN_WINDOW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
   PHOTOS_TYPE_MAIN_WINDOW))

#define PHOTOS_MAIN_WINDOW_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
   PHOTOS_TYPE_MAIN_WINDOW, PhotosMainWindowClass))

typedef struct _PhotosMainWindow        PhotosMainWindow;
typedef struct _PhotosMainWindowClass   PhotosMainWindowClass;
typedef struct _PhotosMainWindowPrivate PhotosMainWindowPrivate;

struct _PhotosMainWindow
{
  GtkApplicationWindow parent_instance;
  PhotosMainWindowPrivate *priv;
};

struct _PhotosMainWindowClass
{
  GtkApplicationWindowClass parent_class;
};

GType                  photos_main_window_get_type               (void) G_GNUC_CONST;

GtkWidget             *photos_main_window_new                    (GtkApplication *application);

void                   photos_main_window_show_about             (PhotosMainWindow *self);

G_END_DECLS

#endif /* PHOTOS_MAIN_WINDOW_H */
