/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2013 Intel Corporation. All rights reserved.
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

#ifndef PHOTOS_DLNA_RENDERERS_MANAGER_H
#define PHOTOS_DLNA_RENDERERS_MANAGER_H

#include <glib-object.h>

G_BEGIN_DECLS

#define PHOTOS_TYPE_DLNA_RENDERERS_MANAGER (photos_dlna_renderers_manager_get_type ())

#define PHOTOS_DLNA_RENDERERS_MANAGER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
   PHOTOS_TYPE_DLNA_RENDERERS_MANAGER, PhotosDlnaRenderersManager))

#define PHOTOS_DLNA_RENDERERS_MANAGER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
   PHOTOS_TYPE_DLNA_RENDERERS_MANAGER, PhotosDlnaRenderersManagerClass))

#define PHOTOS_IS_DLNA_RENDERERS_MANAGER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
   PHOTOS_TYPE_DLNA_RENDERERS_MANAGER))

#define PHOTOS_IS_DLNA_RENDERERS_MANAGER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
   PHOTOS_TYPE_DLNA_RENDERERS_MANAGER))

#define PHOTOS_DLNA_RENDERERS_MANAGER_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
   PHOTOS_TYPE_DLNA_RENDERERS_MANAGER, PhotosDlnaRenderersManagerClass))

typedef struct _PhotosDlnaRenderersManager        PhotosDlnaRenderersManager;
typedef struct _PhotosDlnaRenderersManagerClass   PhotosDlnaRenderersManagerClass;
typedef struct _PhotosDlnaRenderersManagerPrivate PhotosDlnaRenderersManagerPrivate;

struct _PhotosDlnaRenderersManager
{
  GObject parent_instance;
  PhotosDlnaRenderersManagerPrivate *priv;
};

struct _PhotosDlnaRenderersManagerClass
{
  GObjectClass parent_class;
};

GType                       photos_dlna_renderers_manager_get_type      (void) G_GNUC_CONST;

PhotosDlnaRenderersManager *photos_dlna_renderers_manager_dup_singleton (void);

GList                      *photos_dlna_renderers_manager_dup_renderers (PhotosDlnaRenderersManager *self);

gboolean                    photos_dlna_renderers_manager_is_available  (void);

G_END_DECLS

#endif /* PHOTOS_DLNA_RENDERERS_MANAGER_H */
