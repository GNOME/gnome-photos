/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2013 – 2016 Red Hat, Inc.
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
 *   + Clocks
 */

#ifndef PHOTOS_HEADER_BAR_H
#define PHOTOS_HEADER_BAR_H

#include <gtk/gtk.h>
#include <libgd/gd.h>

G_BEGIN_DECLS

#define PHOTOS_TYPE_HEADER_BAR (photos_header_bar_get_type ())

#define PHOTOS_HEADER_BAR(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
   PHOTOS_TYPE_HEADER_BAR, PhotosHeaderBar))

#define PHOTOS_HEADER_BAR_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
   PHOTOS_TYPE_HEADER_BAR, PhotosHeaderBarClass))

#define PHOTOS_IS_HEADER_BAR(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
   PHOTOS_TYPE_HEADER_BAR))

#define PHOTOS_IS_HEADER_BAR_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
   PHOTOS_TYPE_HEADER_BAR))

#define PHOTOS_HEADER_BAR_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
   PHOTOS_TYPE_HEADER_BAR, PhotosHeaderBarClass))

typedef enum
{
  PHOTOS_HEADER_BAR_MODE_NONE,
  PHOTOS_HEADER_BAR_MODE_NORMAL,
  PHOTOS_HEADER_BAR_MODE_SELECTION,
  PHOTOS_HEADER_BAR_MODE_STANDALONE
} PhotosHeaderBarMode;

typedef struct _PhotosHeaderBar        PhotosHeaderBar;
typedef struct _PhotosHeaderBarClass   PhotosHeaderBarClass;
typedef struct _PhotosHeaderBarPrivate PhotosHeaderBarPrivate;

struct _PhotosHeaderBar
{
  GtkHeaderBar parent_instance;
  PhotosHeaderBarPrivate *priv;
};

struct _PhotosHeaderBarClass
{
  GtkHeaderBarClass parent_class;
};

GType                  photos_header_bar_get_type               (void) G_GNUC_CONST;

GtkWidget             *photos_header_bar_new                    (void);

void                   photos_header_bar_clear                  (PhotosHeaderBar *self);

void                   photos_header_bar_set_mode               (PhotosHeaderBar *self, PhotosHeaderBarMode mode);

void                   photos_header_bar_set_selection_menu     (PhotosHeaderBar *self,
                                                                 GtkButton *selection_menu);

void                   photos_header_bar_set_stack              (PhotosHeaderBar *self, GtkStack *stack);

G_END_DECLS

#endif /* PHOTOS_HEADER_BAR_H */
