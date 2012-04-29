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

#ifndef PHOTOS_VIEW_H
#define PHOTOS_VIEW_H

#include "gd-main-view.h"

G_BEGIN_DECLS

#define PHOTOS_TYPE_VIEW (photos_view_get_type ())

#define PHOTOS_VIEW(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
   PHOTOS_TYPE_VIEW, PhotosView))

#define PHOTOS_VIEW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
   PHOTOS_TYPE_VIEW, PhotosViewClass))

#define PHOTOS_IS_VIEW(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
   PHOTOS_TYPE_VIEW))

#define PHOTOS_IS_VIEW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
   PHOTOS_TYPE_VIEW))

#define PHOTOS_VIEW_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
   PHOTOS_TYPE_VIEW, PhotosViewClass))

typedef struct _PhotosView        PhotosView;
typedef struct _PhotosViewClass   PhotosViewClass;
typedef struct _PhotosViewPrivate PhotosViewPrivate;

struct _PhotosView
{
  GdMainView parent_instance;
  PhotosViewPrivate *priv;
};

struct _PhotosViewClass
{
  GdMainViewClass parent_class;
};

GType                  photos_view_get_type               (void) G_GNUC_CONST;

GtkWidget             *photos_view_new                    (void);

G_END_DECLS

#endif /* PHOTOS_VIEW_H */
