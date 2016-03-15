/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2013 – 2016 Red Hat, Inc.
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

#ifndef PHOTOS_COLLECTION_ICON_WATCHER_H
#define PHOTOS_COLLECTION_ICON_WATCHER_H

#include <gio/gio.h>

#include "photos-base-item.h"

G_BEGIN_DECLS

#define PHOTOS_TYPE_COLLECTION_ICON_WATCHER (photos_collection_icon_watcher_get_type ())

#define PHOTOS_COLLECTION_ICON_WATCHER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
   PHOTOS_TYPE_COLLECTION_ICON_WATCHER, PhotosCollectionIconWatcher))

#define PHOTOS_IS_COLLECTION_ICON_WATCHER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
   PHOTOS_TYPE_COLLECTION_ICON_WATCHER))

typedef void (*PhotosCollectionIconWatcherCallback) (TrackerSparqlCursor *, gpointer);

typedef struct _PhotosCollectionIconWatcher      PhotosCollectionIconWatcher;
typedef struct _PhotosCollectionIconWatcherClass PhotosCollectionIconWatcherClass;

GType                         photos_collection_icon_watcher_get_type    (void) G_GNUC_CONST;

PhotosCollectionIconWatcher  *photos_collection_icon_watcher_new         (PhotosBaseItem *collection);

void                          photos_collection_icon_watcher_refresh     (PhotosCollectionIconWatcher *self);

G_END_DECLS

#endif /* PHOTOS_COLLECTION_ICON_WATCHER_H */
