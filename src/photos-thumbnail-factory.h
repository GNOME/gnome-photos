/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2017 Red Hat, Inc.
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

#ifndef PHOTOS_THUMBNAIL_FACTORY_H
#define PHOTOS_THUMBNAIL_FACTORY_H

#include <gio/gio.h>

G_BEGIN_DECLS

#define PHOTOS_TYPE_THUMBNAIL_FACTORY (photos_thumbnail_factory_get_type ())
G_DECLARE_FINAL_TYPE (PhotosThumbnailFactory, photos_thumbnail_factory, PHOTOS, THUMBNAIL_FACTORY, GObject);

PhotosThumbnailFactory  *photos_thumbnail_factory_dup_singleton          (GCancellable *cancellable,
                                                                          GError **error);

gboolean                 photos_thumbnail_factory_generate_thumbnail     (PhotosThumbnailFactory *self,
                                                                          GFile *file,
                                                                          const gchar *mime_type,
                                                                          GQuark orientation,
                                                                          gint64 original_height,
                                                                          gint64 original_width,
                                                                          const gchar *const *pipeline_uris,
                                                                          const gchar *thumbnail_path,
                                                                          GCancellable *cancellable,
                                                                          GError **error);

G_END_DECLS

#endif /* PHOTOS_THUMBNAIL_FACTORY_H */
