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

#ifndef PHOTOS_BASE_ITEM_H
#define PHOTOS_BASE_ITEM_H

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define PHOTOS_TYPE_BASE_ITEM (photos_base_item_get_type ())

#define PHOTOS_BASE_ITEM(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
   PHOTOS_TYPE_BASE_ITEM, PhotosBaseItem))

#define PHOTOS_BASE_ITEM_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
   PHOTOS_TYPE_BASE_ITEM, PhotosBaseItemClass))

#define PHOTOS_IS_BASE_ITEM(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
   PHOTOS_TYPE_BASE_ITEM))

#define PHOTOS_IS_BASE_ITEM_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
   PHOTOS_TYPE_BASE_ITEM))

#define PHOTOS_BASE_ITEM_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
   PHOTOS_TYPE_BASE_ITEM, PhotosBaseItemClass))

typedef struct _PhotosBaseItem        PhotosBaseItem;
typedef struct _PhotosBaseItemClass   PhotosBaseItemClass;
typedef struct _PhotosBaseItemPrivate PhotosBaseItemPrivate;

struct _PhotosBaseItem
{
  GObject parent_instance;
  PhotosBaseItemPrivate *priv;
};

struct _PhotosBaseItemClass
{
  GObjectClass parent_class;

  void (*update_type_description) (PhotosBaseItem *self);
};

GType               photos_base_item_get_type           (void) G_GNUC_CONST;

const gchar        *photos_base_item_get_author         (PhotosBaseItem *self);

GdkPixbuf          *photos_base_item_get_icon           (PhotosBaseItem *self);

const gchar        *photos_base_item_get_id             (PhotosBaseItem *self);

glong               photos_base_item_get_mtime          (PhotosBaseItem *self);

const gchar        *photos_base_item_get_name           (PhotosBaseItem *self);

const gchar        *photos_base_item_get_uri            (PhotosBaseItem *self);

G_END_DECLS

#endif /* PHOTOS_BASE_ITEM_H */
