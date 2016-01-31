/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2016 Red Hat, Inc.
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

#ifndef PHOTOS_IMAGE_CONTAINER_H
#define PHOTOS_IMAGE_CONTAINER_H

#include <gegl.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define PHOTOS_TYPE_IMAGE_CONTAINER (photos_image_container_get_type ())

#define PHOTOS_IMAGE_CONTAINER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
   PHOTOS_TYPE_IMAGE_CONTAINER, PhotosImageContainer))

#define PHOTOS_IMAGE_CONTAINER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
   PHOTOS_TYPE_IMAGE_CONTAINER, PhotosImageContainerClass))

#define PHOTOS_IS_IMAGE_CONTAINER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
   PHOTOS_TYPE_IMAGE_CONTAINER))

#define PHOTOS_IS_IMAGE_CONTAINER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
   PHOTOS_TYPE_IMAGE_CONTAINER))

#define PHOTOS_IMAGE_CONTAINER_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
   PHOTOS_TYPE_IMAGE_CONTAINER, PhotosImageContainerClass))

typedef struct _PhotosImageContainer      PhotosImageContainer;
typedef struct _PhotosImageContainerClass PhotosImageContainerClass;

GType               photos_image_container_get_type           (void) G_GNUC_CONST;

GtkWidget          *photos_image_container_new                (void);

GeglNode           *photos_image_container_get_node           (PhotosImageContainer *self);

GtkWidget          *photos_image_container_get_view           (PhotosImageContainer *self);

void                photos_image_container_set_node           (PhotosImageContainer *self, GeglNode *node);

G_END_DECLS

#endif /* PHOTOS_IMAGE_CONTAINER_H */
