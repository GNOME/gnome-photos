/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2012 – 2016 Red Hat, Inc.
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

#ifndef PHOTOS_SEARCH_TYPE_H
#define PHOTOS_SEARCH_TYPE_H

#include <glib-object.h>

G_BEGIN_DECLS

#define PHOTOS_TYPE_SEARCH_TYPE (photos_search_type_get_type ())

#define PHOTOS_SEARCH_TYPE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
   PHOTOS_TYPE_SEARCH_TYPE, PhotosSearchType))

#define PHOTOS_SEARCH_TYPE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
   PHOTOS_TYPE_SEARCH_TYPE, PhotosSearchTypeClass))

#define PHOTOS_IS_SEARCH_TYPE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
   PHOTOS_TYPE_SEARCH_TYPE))

#define PHOTOS_IS_SEARCH_TYPE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
   PHOTOS_TYPE_SEARCH_TYPE))

#define PHOTOS_SEARCH_TYPE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
   PHOTOS_TYPE_SEARCH_TYPE, PhotosSearchTypeClass))

#define PHOTOS_SEARCH_TYPE_STOCK_ALL "all"
#define PHOTOS_SEARCH_TYPE_STOCK_COLLECTIONS "collections"
#define PHOTOS_SEARCH_TYPE_STOCK_FAVORITES "favorites"
#define PHOTOS_SEARCH_TYPE_STOCK_PHOTOS "photos"

typedef struct _PhotosSearchType        PhotosSearchType;
typedef struct _PhotosSearchTypeClass   PhotosSearchTypeClass;
typedef struct _PhotosSearchTypePrivate PhotosSearchTypePrivate;

struct _PhotosSearchType
{
  GObject parent_instance;
  PhotosSearchTypePrivate *priv;
};

struct _PhotosSearchTypeClass
{
  GObjectClass parent_class;
};

GType                photos_search_type_get_type           (void) G_GNUC_CONST;

PhotosSearchType    *photos_search_type_new                (const gchar *id, const gchar *name);

PhotosSearchType    *photos_search_type_new_full           (const gchar *id,
                                                            const gchar *name,
                                                            const gchar *where,
                                                            const gchar *filter);

G_END_DECLS

#endif /* PHOTOS_SEARCH_TYPE_H */
