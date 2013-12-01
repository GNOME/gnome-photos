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

#ifndef PHOTOS_SEARCH_CONTROLLER_H
#define PHOTOS_SEARCH_CONTROLLER_H

#include <glib-object.h>

G_BEGIN_DECLS

#define PHOTOS_TYPE_SEARCH_CONTROLLER (photos_search_controller_get_type ())

#define PHOTOS_SEARCH_CONTROLLER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
   PHOTOS_TYPE_SEARCH_CONTROLLER, PhotosSearchController))

#define PHOTOS_SEARCH_CONTROLLER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
   PHOTOS_TYPE_SEARCH_CONTROLLER, PhotosSearchControllerClass))

#define PHOTOS_IS_SEARCH_CONTROLLER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
   PHOTOS_TYPE_SEARCH_CONTROLLER))

#define PHOTOS_IS_SEARCH_CONTROLLER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
   PHOTOS_TYPE_SEARCH_CONTROLLER))

#define PHOTOS_SEARCH_CONTROLLER_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
   PHOTOS_TYPE_SEARCH_CONTROLLER, PhotosSearchControllerClass))

typedef struct _PhotosSearchController        PhotosSearchController;
typedef struct _PhotosSearchControllerClass   PhotosSearchControllerClass;
typedef struct _PhotosSearchControllerPrivate PhotosSearchControllerPrivate;

struct _PhotosSearchController
{
  GObject parent_instance;
  PhotosSearchControllerPrivate *priv;
};

struct _PhotosSearchControllerClass
{
  GObjectClass parent_class;

  /* signals */
  void (*search_string_changed) (PhotosSearchController *self, const gchar *str);
};

GType                      photos_search_controller_get_type       (void) G_GNUC_CONST;

PhotosSearchController    *photos_search_controller_new            (void);

const gchar               *photos_search_controller_get_string     (PhotosSearchController *self);

gchar                    **photos_search_controller_get_terms      (PhotosSearchController *self);

void                       photos_search_controller_set_string     (PhotosSearchController *self, const gchar *str);

G_END_DECLS

#endif /* PHOTOS_SEARCH_CONTROLLER_H */
