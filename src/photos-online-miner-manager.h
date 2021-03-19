/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2021 Red Hat, Inc.
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

#pragma once

#include <gio/gio.h>

G_BEGIN_DECLS

#define PHOTOS_TYPE_ONLINE_MINER_MANAGER (photos_online_miner_manager_get_type ())
G_DECLARE_FINAL_TYPE (PhotosOnlineMinerManager, photos_online_miner_manager, PHOTOS, ONLINE_MINER_MANAGER, GObject);

PhotosOnlineMinerManager *photos_online_miner_manager_dup_singleton                (GCancellable *cancellable,
                                                                                    GError **error);

GList                    *photos_online_miner_manager_get_running                  (PhotosOnlineMinerManager *self);

void                      photos_online_miner_manager_insert_shared_content_async  (PhotosOnlineMinerManager *self,
                                                                                    const gchar *provider_type,
                                                                                    const gchar *account_id,
                                                                                    const gchar *shared_id,
                                                                                    const gchar *source_urn,
                                                                                    GCancellable *cancellable,
                                                                                    GAsyncReadyCallback callback,
                                                                                    gpointer user_data);

gboolean                  photos_online_miner_manager_insert_shared_content_finish (PhotosOnlineMinerManager *self,
                                                                                    GAsyncResult *res,
                                                                                    GError **error);

G_END_DECLS
