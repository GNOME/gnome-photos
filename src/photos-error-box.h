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

#ifndef PHOTOS_ERROR_BOX_H
#define PHOTOS_ERROR_BOX_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define PHOTOS_TYPE_ERROR_BOX (photos_error_box_get_type ())

#define PHOTOS_ERROR_BOX(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
   PHOTOS_TYPE_ERROR_BOX, PhotosErrorBox))

#define PHOTOS_ERROR_BOX_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
   PHOTOS_TYPE_ERROR_BOX, PhotosErrorBoxClass))

#define PHOTOS_IS_ERROR_BOX(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
   PHOTOS_TYPE_ERROR_BOX))

#define PHOTOS_IS_ERROR_BOX_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
   PHOTOS_TYPE_ERROR_BOX))

#define PHOTOS_ERROR_BOX_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
   PHOTOS_TYPE_ERROR_BOX, PhotosErrorBoxClass))

typedef struct _PhotosErrorBox        PhotosErrorBox;
typedef struct _PhotosErrorBoxClass   PhotosErrorBoxClass;
typedef struct _PhotosErrorBoxPrivate PhotosErrorBoxPrivate;

struct _PhotosErrorBox
{
  GtkGrid parent_instance;
  PhotosErrorBoxPrivate *priv;
};

struct _PhotosErrorBoxClass
{
  GtkGridClass parent_class;
};

GType                  photos_error_box_get_type               (void) G_GNUC_CONST;

GtkWidget             *photos_error_box_new                    (void);

void                   photos_error_box_update                 (PhotosErrorBox *self,
                                                                const gchar *primary,
                                                                const gchar *secondary);

G_END_DECLS

#endif /* PHOTOS_ERROR_BOX_H */
