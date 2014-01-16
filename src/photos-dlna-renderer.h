/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2013 Intel Corporation. All rights reserved.
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

#ifndef PHOTOS_DLNA_RENDERER_H
#define PHOTOS_DLNA_RENDERER_H

#include <gio/gio.h>
#include <gtk/gtk.h>

#include "photos-base-item.h"

G_BEGIN_DECLS

#define PHOTOS_TYPE_DLNA_RENDERER (photos_dlna_renderer_get_type ())

#define PHOTOS_DLNA_RENDERER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
   PHOTOS_TYPE_DLNA_RENDERER, PhotosDlnaRenderer))

#define PHOTOS_DLNA_RENDERER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
   PHOTOS_TYPE_DLNA_RENDERER, PhotosDlnaRendererClass))

#define PHOTOS_IS_DLNA_RENDERER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
   PHOTOS_TYPE_DLNA_RENDERER))

#define PHOTOS_IS_DLNA_RENDERER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
   PHOTOS_TYPE_DLNA_RENDERER))

#define PHOTOS_DLNA_RENDERER_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
   PHOTOS_TYPE_DLNA_RENDERER, PhotosDlnaRendererClass))

typedef struct _PhotosDlnaRenderer        PhotosDlnaRenderer;
typedef struct _PhotosDlnaRendererClass   PhotosDlnaRendererClass;
typedef struct _PhotosDlnaRendererPrivate PhotosDlnaRendererPrivate;

struct _PhotosDlnaRenderer
{
  GObject parent_instance;
  PhotosDlnaRendererPrivate *priv;
};

struct _PhotosDlnaRendererClass
{
  GObjectClass parent_class;
};

GType                 photos_dlna_renderer_get_type           (void) G_GNUC_CONST;

void                  photos_dlna_renderer_new_for_bus        (GBusType             bus_type,
                                                               GDBusProxyFlags      flags,
                                                               const gchar         *name,
                                                               const gchar         *object_path,
                                                               GCancellable        *cancellable,
                                                               GAsyncReadyCallback  callback,
                                                               gpointer             user_data);

PhotosDlnaRenderer   *photos_dlna_renderer_new_for_bus_finish (GAsyncResult        *res,
                                                               GError             **error);

const gchar          *photos_dlna_renderer_get_object_path    (PhotosDlnaRenderer  *renderer);

void                  photos_dlna_renderer_share              (PhotosDlnaRenderer  *renderer,
                                                               PhotosBaseItem      *item,
                                                               GCancellable        *cancellable,
                                                               GAsyncReadyCallback  callback,
                                                               gpointer             user_data);

PhotosBaseItem       *photos_dlna_renderer_share_finish       (PhotosDlnaRenderer  *renderer,
                                                               GAsyncResult        *res,
                                                               GError             **error);

void                  photos_dlna_renderer_unshare            (PhotosDlnaRenderer  *self,
                                                               PhotosBaseItem      *item,
                                                               GCancellable        *cancellable,
                                                               GAsyncReadyCallback  callback,
                                                               gpointer             user_data);

void                  photos_dlna_renderer_unshare_finish     (PhotosDlnaRenderer  *self,
                                                               GAsyncResult        *res,
                                                               GError             **error);

void                  photos_dlna_renderer_unshare_all        (PhotosDlnaRenderer  *self,
                                                               GCancellable        *cancellable,
                                                               GAsyncReadyCallback  callback,
                                                               gpointer             user_data);

void                  photos_dlna_renderer_unshare_all_finish (PhotosDlnaRenderer  *self,
                                                               GAsyncResult        *res,
                                                               GError             **error);

const gchar          *photos_dlna_renderer_get_friendly_name  (PhotosDlnaRenderer  *self);

const gchar          *photos_dlna_renderer_get_udn            (PhotosDlnaRenderer  *self);

void                  photos_dlna_renderer_get_icon           (PhotosDlnaRenderer  *self,
                                                               const gchar         *requested_mimetype,
                                                               const gchar         *resolution,
                                                               GtkIconSize          size,
                                                               GCancellable        *cancellable,
                                                               GAsyncReadyCallback  callback,
                                                               gpointer             user_data);

GdkPixbuf *           photos_dlna_renderer_get_icon_finish    (PhotosDlnaRenderer  *self,
                                                               GAsyncResult        *res,
                                                               GError             **error);

guint                 photos_dlna_renderer_get_shared_count   (PhotosDlnaRenderer *self);

G_END_DECLS

#endif /* PHOTOS_DLNA_RENDERER_H */
