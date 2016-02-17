/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2014 – 2016 Red Hat, Inc.
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

#ifndef PHOTOS_SEARCH_PROVIDER_H
#define PHOTOS_SEARCH_PROVIDER_H

#include <gio/gio.h>

G_BEGIN_DECLS

#define PHOTOS_TYPE_SEARCH_PROVIDER (photos_search_provider_get_type ())

#define PHOTOS_SEARCH_PROVIDER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
   PHOTOS_TYPE_SEARCH_PROVIDER, PhotosSearchProvider))

#define PHOTOS_IS_SEARCH_PROVIDER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
   PHOTOS_TYPE_SEARCH_PROVIDER))

#define PHOTOS_SEARCH_PROVIDER_PATH_SUFFIX "/SearchProvider"

typedef struct _PhotosSearchProvider      PhotosSearchProvider;
typedef struct _PhotosSearchProviderClass PhotosSearchProviderClass;

GType                        photos_search_provider_get_type          (void) G_GNUC_CONST;

PhotosSearchProvider        *photos_search_provider_new               (void);

gboolean                     photos_search_provider_dbus_export       (PhotosSearchProvider *self,
                                                                       GDBusConnection *connection,
                                                                       const gchar *object_path,
                                                                       GError **error);

void                         photos_search_provider_dbus_unexport     (PhotosSearchProvider *self,
                                                                       GDBusConnection *connection,
                                                                       const gchar *object_path);

G_END_DECLS

#endif /* PHOTOS_SEARCH_PROVIDER_H */
