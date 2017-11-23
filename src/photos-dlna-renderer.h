/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2013 Intel Corporation. All rights reserved.
 * Copyright © 2016 – 2017 Red Hat, Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef PHOTOS_DLNA_RENDERER_H
#define PHOTOS_DLNA_RENDERER_H

#include <gio/gio.h>
#include <gtk/gtk.h>

#include "photos-base-item.h"

G_BEGIN_DECLS

#define PHOTOS_TYPE_DLNA_RENDERER (photos_dlna_renderer_get_type ())
G_DECLARE_FINAL_TYPE (PhotosDlnaRenderer, photos_dlna_renderer, PHOTOS, DLNA_RENDERER, GObject);

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
