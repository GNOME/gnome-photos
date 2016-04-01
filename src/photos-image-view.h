/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2015 – 2016 Red Hat, Inc.
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

#ifndef PHOTOS_IMAGE_VIEW_H
#define PHOTOS_IMAGE_VIEW_H

#include <gegl.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define PHOTOS_TYPE_IMAGE_VIEW (photos_image_view_get_type ())

#define PHOTOS_IMAGE_VIEW(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
   PHOTOS_TYPE_IMAGE_VIEW, PhotosImageView))

#define PHOTOS_IMAGE_VIEW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
   PHOTOS_TYPE_IMAGE_VIEW, PhotosImageViewClass))

#define PHOTOS_IS_IMAGE_VIEW(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
   PHOTOS_TYPE_IMAGE_VIEW))

#define PHOTOS_IS_IMAGE_VIEW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
   PHOTOS_TYPE_IMAGE_VIEW))

#define PHOTOS_IMAGE_VIEW_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
   PHOTOS_TYPE_IMAGE_VIEW, PhotosImageViewClass))

typedef struct _PhotosImageView      PhotosImageView;
typedef struct _PhotosImageViewClass PhotosImageViewClass;

GType               photos_image_view_get_type           (void) G_GNUC_CONST;

GtkWidget          *photos_image_view_new                (void);

GtkWidget          *photos_image_view_new_from_node      (GeglNode *node);

GeglNode           *photos_image_view_get_node           (PhotosImageView *self);

double              photos_image_view_get_x              (PhotosImageView *self);

double              photos_image_view_get_y              (PhotosImageView *self);

double              photos_image_view_get_zoom           (PhotosImageView *self);

void                photos_image_view_set_node           (PhotosImageView *self, GeglNode *node);

G_END_DECLS

#endif /* PHOTOS_IMAGE_VIEW_H */
