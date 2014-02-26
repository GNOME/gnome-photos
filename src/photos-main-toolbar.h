/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2012, 2013, 2014 Red Hat, Inc.
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

#ifndef PHOTOS_MAIN_TOOLBAR_H
#define PHOTOS_MAIN_TOOLBAR_H

#include <gtk/gtk.h>

#include "photos-searchbar.h"

G_BEGIN_DECLS

#define PHOTOS_TYPE_MAIN_TOOLBAR (photos_main_toolbar_get_type ())

#define PHOTOS_MAIN_TOOLBAR(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
   PHOTOS_TYPE_MAIN_TOOLBAR, PhotosMainToolbar))

#define PHOTOS_MAIN_TOOLBAR_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
   PHOTOS_TYPE_MAIN_TOOLBAR, PhotosMainToolbarClass))

#define PHOTOS_IS_MAIN_TOOLBAR(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
   PHOTOS_TYPE_MAIN_TOOLBAR))

#define PHOTOS_IS_MAIN_TOOLBAR_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
   PHOTOS_TYPE_MAIN_TOOLBAR))

#define PHOTOS_MAIN_TOOLBAR_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
   PHOTOS_TYPE_MAIN_TOOLBAR, PhotosMainToolbarClass))

typedef struct _PhotosMainToolbar        PhotosMainToolbar;
typedef struct _PhotosMainToolbarClass   PhotosMainToolbarClass;
typedef struct _PhotosMainToolbarPrivate PhotosMainToolbarPrivate;

struct _PhotosMainToolbar
{
  GtkBox parent_instance;
  PhotosMainToolbarPrivate *priv;
};

struct _PhotosMainToolbarClass
{
  GtkBoxClass parent_class;
};

GType                  photos_main_toolbar_get_type               (void) G_GNUC_CONST;

GtkWidget             *photos_main_toolbar_new                    (GtkOverlay *overlay);

PhotosSearchbar *      photos_main_toolbar_get_searchbar          (PhotosMainToolbar *self);

gboolean               photos_main_toolbar_handle_event           (PhotosMainToolbar *self, GdkEventKey *event);

gboolean               photos_main_toolbar_is_focus               (PhotosMainToolbar *self);

void                   photos_main_toolbar_set_stack              (PhotosMainToolbar *self, GtkStack *stack);

G_END_DECLS

#endif /* PHOTOS_MAIN_TOOLBAR_H */
