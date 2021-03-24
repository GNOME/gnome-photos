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
#include <goa/goa.h>
#include <tracker-sparql.h>

G_BEGIN_DECLS

#define PHOTOS_TYPE_ONLINE_MINER (photos_online_miner_get_type ())
G_DECLARE_DERIVABLE_TYPE (PhotosOnlineMiner, photos_online_miner, PHOTOS, ONLINE_MINER, GApplication);

struct _PhotosOnlineMinerClass
{
  GApplicationClass parent_class;

  const gchar *identifier;
  const gchar *provider_type;
  guint version;

  /* virtual methods */
  void      (*refresh_account_async)   (PhotosOnlineMiner *self,
                                        GHashTable *identifier_to_urn_old,
                                        GoaObject *object,
                                        const gchar *datasource,
                                        GCancellable *cancellable,
                                        GAsyncReadyCallback callback,
                                        gpointer user_data);
  gboolean  (*refresh_account_finish)  (PhotosOnlineMiner *self, GAsyncResult *res, GError **error);
};

TrackerSparqlConnection    *photos_online_miner_get_connection    (PhotosOnlineMiner *self);

G_END_DECLS
