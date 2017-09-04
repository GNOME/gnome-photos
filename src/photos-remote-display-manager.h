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

#ifndef PHOTOS_REMOTE_DISPLAY_MANAGER_H
#define PHOTOS_REMOTE_DISPLAY_MANAGER_H

#include <glib-object.h>

#include "photos-base-item.h"
#include "photos-dlna-renderer.h"

G_BEGIN_DECLS

#define PHOTOS_TYPE_REMOTE_DISPLAY_MANAGER (photos_remote_display_manager_get_type ())
G_DECLARE_FINAL_TYPE (PhotosRemoteDisplayManager,
                      photos_remote_display_manager,
                      PHOTOS,
                      REMOTE_DISPLAY_MANAGER,
                      GObject);

PhotosRemoteDisplayManager *photos_remote_display_manager_dup_singleton   (void);

void                        photos_remote_display_manager_set_renderer    (PhotosRemoteDisplayManager *self,
                                                                           PhotosDlnaRenderer         *renderer);

PhotosDlnaRenderer         *photos_remote_display_manager_get_renderer    (PhotosRemoteDisplayManager *self);

void                        photos_remote_display_manager_render          (PhotosRemoteDisplayManager *self,
                                                                           PhotosBaseItem             *item);

void                        photos_remote_display_manager_stop            (PhotosRemoteDisplayManager *self);

gboolean                    photos_remote_display_manager_is_active       (PhotosRemoteDisplayManager *self);

G_END_DECLS

#endif /* PHOTOS_REMOTE_DISPLAY_MANAGER_H */
