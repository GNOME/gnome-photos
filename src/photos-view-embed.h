/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2012 Red Hat, Inc.
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

#ifndef PHOTOS_VIEW_EMBED_H
#define PHOTOS_VIEW_EMBED_H

#include <clutter/clutter.h>

G_BEGIN_DECLS

#define PHOTOS_TYPE_VIEW_EMBED (photos_view_embed_get_type ())

#define PHOTOS_VIEW_EMBED(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
   PHOTOS_TYPE_VIEW_EMBED, PhotosViewEmbed))

#define PHOTOS_VIEW_EMBED_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
   PHOTOS_TYPE_VIEW_EMBED, PhotosViewEmbedClass))

#define PHOTOS_IS_VIEW_EMBED(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
   PHOTOS_TYPE_VIEW_EMBED))

#define PHOTOS_IS_VIEW_EMBED_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
   PHOTOS_TYPE_VIEW_EMBED))

#define PHOTOS_VIEW_EMBED_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
   PHOTOS_TYPE_VIEW_EMBED, PhotosViewEmbedClass))

typedef struct _PhotosViewEmbed        PhotosViewEmbed;
typedef struct _PhotosViewEmbedClass   PhotosViewEmbedClass;
typedef struct _PhotosViewEmbedPrivate PhotosViewEmbedPrivate;

struct _PhotosViewEmbed
{
  ClutterBox parent_instance;
  PhotosViewEmbedPrivate *priv;
};

struct _PhotosViewEmbedClass
{
  ClutterBoxClass parent_class;
};

GType                  photos_view_embed_get_type               (void) G_GNUC_CONST;

ClutterActor          *photos_view_embed_new                    (ClutterBinLayout *layout);

G_END_DECLS

#endif /* PHOTOS_VIEW_EMBED_H */
