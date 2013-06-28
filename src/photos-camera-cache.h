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

#ifndef PHOTOS_CAMERA_CACHE_H
#define PHOTOS_CAMERA_CACHE_H

#include <gio/gio.h>
#include <glib.h>

G_BEGIN_DECLS

#define PHOTOS_TYPE_CAMERA_CACHE (photos_camera_cache_get_type ())

#define PHOTOS_CAMERA_CACHE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
   PHOTOS_TYPE_CAMERA_CACHE, PhotosCameraCache))

#define PHOTOS_CAMERA_CACHE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
   PHOTOS_TYPE_CAMERA_CACHE, PhotosCameraCacheClass))

#define PHOTOS_IS_CAMERA_CACHE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
   PHOTOS_TYPE_CAMERA_CACHE))

#define PHOTOS_IS_CAMERA_CACHE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
   PHOTOS_TYPE_CAMERA_CACHE))

#define PHOTOS_CAMERA_CACHE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
   PHOTOS_TYPE_CAMERA_CACHE, PhotosCameraCacheClass))

typedef struct _PhotosCameraCache        PhotosCameraCache;
typedef struct _PhotosCameraCacheClass   PhotosCameraCacheClass;
typedef struct _PhotosCameraCachePrivate PhotosCameraCachePrivate;

struct _PhotosCameraCache
{
  GObject parent_instance;
  PhotosCameraCachePrivate *priv;
};

struct _PhotosCameraCacheClass
{
  GObjectClass parent_class;
};

GType                  photos_camera_cache_get_type               (void) G_GNUC_CONST;

PhotosCameraCache     *photos_camera_cache_dup_singleton          (void);

void                   photos_camera_cache_get_camera_async       (PhotosCameraCache *self,
                                                                   GQuark id,
                                                                   GCancellable *cancellable,
                                                                   GAsyncReadyCallback callback,
                                                                   gpointer user_data);

gchar                 *photos_camera_cache_get_camera_finish      (PhotosCameraCache *self,
                                                                   GAsyncResult *res,
                                                                   GError **error);

G_END_DECLS

#endif /* PHOTOS_CAMERA_CACHE_H */
