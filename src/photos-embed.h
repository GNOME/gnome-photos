/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2012, 2013, 2014 Red Hat, Inc.
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

#ifndef PHOTOS_EMBED_H
#define PHOTOS_EMBED_H

#include <gtk/gtk.h>

#include "photos-main-toolbar.h"
#include "photos-preview-view.h"

G_BEGIN_DECLS

#define PHOTOS_TYPE_EMBED (photos_embed_get_type ())

#define PHOTOS_EMBED(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
   PHOTOS_TYPE_EMBED, PhotosEmbed))

#define PHOTOS_EMBED_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
   PHOTOS_TYPE_EMBED, PhotosEmbedClass))

#define PHOTOS_IS_EMBED(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
   PHOTOS_TYPE_EMBED))

#define PHOTOS_IS_EMBED_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
   PHOTOS_TYPE_EMBED))

#define PHOTOS_EMBED_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
   PHOTOS_TYPE_EMBED, PhotosEmbedClass))

typedef struct _PhotosEmbed        PhotosEmbed;
typedef struct _PhotosEmbedClass   PhotosEmbedClass;
typedef struct _PhotosEmbedPrivate PhotosEmbedPrivate;

struct _PhotosEmbed
{
  GtkBox parent_instance;
  PhotosEmbedPrivate *priv;
};

struct _PhotosEmbedClass
{
  GtkBoxClass parent_class;
};

GType                  photos_embed_get_type               (void) G_GNUC_CONST;

GtkWidget             *photos_embed_new                    (void);

PhotosMainToolbar     *photos_embed_get_main_toolbar       (PhotosEmbed *self);

PhotosPreviewView     *photos_embed_get_preview            (PhotosEmbed *self);

G_END_DECLS

#endif /* PHOTOS_EMBED_H */
