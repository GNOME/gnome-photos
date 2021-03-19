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

#define PHOTOS_TYPE_ONLINE_MINER_PROCESS (photos_online_miner_process_get_type ())
G_DECLARE_FINAL_TYPE (PhotosOnlineMinerProcess, photos_online_miner_process, PHOTOS, ONLINE_MINER_PROCESS, GObject);

PhotosOnlineMinerProcess *photos_online_miner_process_new                          (const gchar *address,
                                                                                    const gchar *provider_type);

GDBusConnection          *photos_online_miner_process_get_connection               (PhotosOnlineMinerProcess *self);

const gchar              *photos_online_miner_process_get_provider_name            (PhotosOnlineMinerProcess *self);

const gchar              *photos_online_miner_process_get_provider_type            (PhotosOnlineMinerProcess *self);

gboolean                  photos_online_miner_process_matches_credentials          (PhotosOnlineMinerProcess *self,
                                                                                    GCredentials *credentials);

void                      photos_online_miner_process_insert_shared_content_async  (PhotosOnlineMinerProcess *self,
                                                                                    const gchar *account_id,
                                                                                    const gchar *shared_id,
                                                                                    const gchar *source_urn,
                                                                                    GCancellable *cancellable,
                                                                                    GAsyncReadyCallback callback,
                                                                                    gpointer user_data);

gboolean                  photos_online_miner_process_insert_shared_content_finish (PhotosOnlineMinerProcess *self,
                                                                                    GAsyncResult *res,
                                                                                    GError **error);

void                      photos_online_miner_process_refresh_db_async             (PhotosOnlineMinerProcess *self,
                                                                                    GCancellable *cancellable,
                                                                                    GAsyncReadyCallback callback,
                                                                                    gpointer user_data);

gboolean                  photos_online_miner_process_refresh_db_finish            (PhotosOnlineMinerProcess *self,
                                                                                    GAsyncResult *res,
                                                                                    GError **error);

void                      photos_online_miner_process_set_connection               (PhotosOnlineMinerProcess *self,
                                                                                    GDBusConnection *connection);

void                      photos_online_miner_process_set_provider_name            (PhotosOnlineMinerProcess *self,
                                                                                    const gchar *provider_name);

G_END_DECLS
