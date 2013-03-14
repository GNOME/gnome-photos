/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2012, 2013 Red Hat, Inc.
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

#ifndef PHOTOS_EMPTY_RESULTS_BOX_H
#define PHOTOS_EMPTY_RESULTS_BOX_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define PHOTOS_TYPE_EMPTY_RESULTS_BOX (photos_empty_results_box_get_type ())

#define PHOTOS_EMPTY_RESULTS_BOX(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
   PHOTOS_TYPE_EMPTY_RESULTS_BOX, PhotosEmptyResultsBox))

#define PHOTOS_EMPTY_RESULTS_BOX_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
   PHOTOS_TYPE_EMPTY_RESULTS_BOX, PhotosEmptyResultsBoxClass))

#define PHOTOS_IS_EMPTY_RESULTS_BOX(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
   PHOTOS_TYPE_EMPTY_RESULTS_BOX))

#define PHOTOS_IS_EMPTY_RESULTS_BOX_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
   PHOTOS_TYPE_EMPTY_RESULTS_BOX))

#define PHOTOS_EMPTY_RESULTS_BOX_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
   PHOTOS_TYPE_EMPTY_RESULTS_BOX, PhotosEmptyResultsBoxClass))

typedef struct _PhotosEmptyResultsBox        PhotosEmptyResultsBox;
typedef struct _PhotosEmptyResultsBoxClass   PhotosEmptyResultsBoxClass;
typedef struct _PhotosEmptyResultsBoxPrivate PhotosEmptyResultsBoxPrivate;

struct _PhotosEmptyResultsBox
{
  GtkGrid parent_instance;
  PhotosEmptyResultsBoxPrivate *priv;
};

struct _PhotosEmptyResultsBoxClass
{
  GtkGridClass parent_class;
};

GType               photos_empty_results_box_get_type           (void) G_GNUC_CONST;

GtkWidget          *photos_empty_results_box_new                (void);

G_END_DECLS

#endif /* PHOTOS_EMPTY_RESULTS_BOX_H */
