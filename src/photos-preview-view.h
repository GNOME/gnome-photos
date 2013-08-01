/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2013 Red Hat, Inc.
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

#ifndef PHOTOS_PREVIEW_VIEW_H
#define PHOTOS_PREVIEW_VIEW_H

#include <gegl.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define PHOTOS_TYPE_PREVIEW_VIEW (photos_preview_view_get_type ())

#define PHOTOS_PREVIEW_VIEW(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
   PHOTOS_TYPE_PREVIEW_VIEW, PhotosPreviewView))

#define PHOTOS_PREVIEW_VIEW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
   PHOTOS_TYPE_PREVIEW_VIEW, PhotosPreviewViewClass))

#define PHOTOS_IS_PREVIEW_VIEW(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
   PHOTOS_TYPE_PREVIEW_VIEW))

#define PHOTOS_IS_PREVIEW_VIEW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
   PHOTOS_TYPE_PREVIEW_VIEW))

#define PHOTOS_PREVIEW_VIEW_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
   PHOTOS_TYPE_PREVIEW_VIEW, PhotosPreviewViewClass))

typedef struct _PhotosPreviewView        PhotosPreviewView;
typedef struct _PhotosPreviewViewClass   PhotosPreviewViewClass;
typedef struct _PhotosPreviewViewPrivate PhotosPreviewViewPrivate;

struct _PhotosPreviewView
{
  GtkScrolledWindow parent_instance;
  PhotosPreviewViewPrivate *priv;
};

struct _PhotosPreviewViewClass
{
  GtkScrolledWindowClass parent_class;
};

GType                  photos_preview_view_get_type               (void) G_GNUC_CONST;

GtkWidget             *photos_preview_view_new                    (GtkOverlay *overlay);

void                   photos_preview_view_set_model              (PhotosPreviewView *self,
                                                                   GtkTreeModel *model,
                                                                   GtkTreePath *current_path);

void                   photos_preview_view_set_node               (PhotosPreviewView *self, GeglNode *node);

G_END_DECLS

#endif /* PHOTOS_PREVIEW_VIEW_H */
