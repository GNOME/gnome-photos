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

/* Based on code from:
 *   + Documents
 */

#ifndef PHOTOS_VIEW_CONTAINER_H
#define PHOTOS_VIEW_CONTAINER_H

#include <gtk/gtk.h>

#include "photos-mode-controller.h"

G_BEGIN_DECLS

#define PHOTOS_TYPE_VIEW_CONTAINER (photos_view_container_get_type ())

#define PHOTOS_VIEW_CONTAINER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
   PHOTOS_TYPE_VIEW_CONTAINER, PhotosViewContainer))

#define PHOTOS_VIEW_CONTAINER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
   PHOTOS_TYPE_VIEW_CONTAINER, PhotosViewContainerClass))

#define PHOTOS_IS_VIEW_CONTAINER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
   PHOTOS_TYPE_VIEW_CONTAINER))

#define PHOTOS_IS_VIEW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
   PHOTOS_TYPE_VIEW_CONTAINER))

#define PHOTOS_VIEW_CONTAINER_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
   PHOTOS_TYPE_VIEW_CONTAINER, PhotosViewContainerClass))

typedef struct _PhotosViewContainer        PhotosViewContainer;
typedef struct _PhotosViewContainerClass   PhotosViewContainerClass;
typedef struct _PhotosViewContainerPrivate PhotosViewContainerPrivate;

struct _PhotosViewContainer
{
  GtkGrid parent_instance;
  PhotosViewContainerPrivate *priv;
};

struct _PhotosViewContainerClass
{
  GtkGridClass parent_class;
};

GType                  photos_view_container_get_type               (void) G_GNUC_CONST;

GtkWidget             *photos_view_container_new                    (PhotosWindowMode mode);

GtkTreePath           *photos_view_container_get_current_path       (PhotosViewContainer *self);

GtkListStore          *photos_view_container_get_model              (PhotosViewContainer *self);

G_END_DECLS

#endif /* PHOTOS_VIEW_CONTAINER_H */
