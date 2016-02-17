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

#ifndef PHOTOS_APPLICATION_H
#define PHOTOS_APPLICATION_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define PHOTOS_TYPE_APPLICATION (photos_application_get_type ())

#define PHOTOS_APPLICATION(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
   PHOTOS_TYPE_APPLICATION, PhotosApplication))

#define PHOTOS_APPLICATION_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
   PHOTOS_TYPE_APPLICATION, PhotosApplicationClass))

#define PHOTOS_IS_APPLICATION(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
   PHOTOS_TYPE_APPLICATION))

#define PHOTOS_IS_APPLICATION_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
   PHOTOS_TYPE_APPLICATION))

#define PHOTOS_APPLICATION_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
   PHOTOS_TYPE_APPLICATION, PhotosApplicationClass))

typedef struct _PhotosApplication        PhotosApplication;
typedef struct _PhotosApplicationClass   PhotosApplicationClass;
typedef struct _PhotosApplicationPrivate PhotosApplicationPrivate;

struct _PhotosApplication
{
  GtkApplication parent_instance;
  PhotosApplicationPrivate *priv;
};

struct _PhotosApplicationClass
{
  GtkApplicationClass parent_class;

  /* signals */
  void (*miners_changed) (PhotosApplication *self, GList *miners_running);
};

GType                  photos_application_get_type               (void) G_GNUC_CONST;

GtkApplication        *photos_application_new                    (void);

GList                 *photos_application_get_miners_running     (PhotosApplication *self);

gint                   photos_application_get_scale_factor       (PhotosApplication *self);

G_END_DECLS

#endif /* PHOTOS_APPLICATION_H */
