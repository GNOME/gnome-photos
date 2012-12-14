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

#ifndef PHOTOS_LOAD_MORE_BUTTON_H
#define PHOTOS_LOAD_MORE_BUTTON_H

#include <gtk/gtk.h>

#include "photos-mode-controller.h"

G_BEGIN_DECLS

#define PHOTOS_TYPE_LOAD_MORE_BUTTON (photos_load_more_button_get_type ())

#define PHOTOS_LOAD_MORE_BUTTON(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
   PHOTOS_TYPE_LOAD_MORE_BUTTON, PhotosLoadMoreButton))

#define PHOTOS_LOAD_MORE_BUTTON_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
   PHOTOS_TYPE_LOAD_MORE_BUTTON, PhotosLoadMoreButtonClass))

#define PHOTOS_IS_LOAD_MORE_BUTTON(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
   PHOTOS_TYPE_LOAD_MORE_BUTTON))

#define PHOTOS_IS_LOAD_MORE_BUTTON_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
   PHOTOS_TYPE_LOAD_MORE_BUTTON))

#define PHOTOS_LOAD_MORE_BUTTON_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
   PHOTOS_TYPE_LOAD_MORE_BUTTON, PhotosLoadMoreButtonClass))

typedef struct _PhotosLoadMoreButton        PhotosLoadMoreButton;
typedef struct _PhotosLoadMoreButtonClass   PhotosLoadMoreButtonClass;
typedef struct _PhotosLoadMoreButtonPrivate PhotosLoadMoreButtonPrivate;

struct _PhotosLoadMoreButton
{
  GtkButton parent_instance;
  PhotosLoadMoreButtonPrivate *priv;
};

struct _PhotosLoadMoreButtonClass
{
  GtkButtonClass parent_class;
};

GType                  photos_load_more_button_get_type               (void) G_GNUC_CONST;

GtkWidget             *photos_load_more_button_new                    (PhotosWindowMode mode);

void                   photos_load_more_button_set_block              (PhotosLoadMoreButton *self, gboolean block);

G_END_DECLS

#endif /* PHOTOS_LOAD_MORE_BUTTON_H */
