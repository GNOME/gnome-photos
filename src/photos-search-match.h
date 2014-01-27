/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2014 Red Hat, Inc.
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

#ifndef PHOTOS_SEARCH_MATCH_H
#define PHOTOS_SEARCH_MATCH_H

#include <glib-object.h>

G_BEGIN_DECLS

#define PHOTOS_TYPE_SEARCH_MATCH (photos_search_match_get_type ())

#define PHOTOS_SEARCH_MATCH(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
   PHOTOS_TYPE_SEARCH_MATCH, PhotosSearchMatch))

#define PHOTOS_SEARCH_MATCH_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
   PHOTOS_TYPE_SEARCH_MATCH, PhotosSearchMatchClass))

#define PHOTOS_IS_SEARCH_MATCH(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
   PHOTOS_TYPE_SEARCH_MATCH))

#define PHOTOS_IS_SEARCH_MATCH_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
   PHOTOS_TYPE_SEARCH_MATCH))

#define PHOTOS_SEARCH_MATCH_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
   PHOTOS_TYPE_SEARCH_MATCH, PhotosSearchMatchClass))

#define PHOTOS_SEARCH_MATCH_STOCK_ALL "all"
#define PHOTOS_SEARCH_MATCH_STOCK_AUTHOR "author"
#define PHOTOS_SEARCH_MATCH_STOCK_TITLE "title"

typedef struct _PhotosSearchMatch        PhotosSearchMatch;
typedef struct _PhotosSearchMatchClass   PhotosSearchMatchClass;
typedef struct _PhotosSearchMatchPrivate PhotosSearchMatchPrivate;

struct _PhotosSearchMatch
{
  GObject parent_instance;
  PhotosSearchMatchPrivate *priv;
};

struct _PhotosSearchMatchClass
{
  GObjectClass parent_class;
};

GType                 photos_search_match_get_type           (void) G_GNUC_CONST;

PhotosSearchMatch    *photos_search_match_new                (const gchar *id,
                                                              const gchar *name,
                                                              const gchar *filter);

void                  photos_search_match_set_filter_term    (PhotosSearchMatch *self, const gchar *term);


G_END_DECLS

#endif /* PHOTOS_SEARCH_MATCH_H */
