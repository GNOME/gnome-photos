/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2013, 2014, 2015 Red Hat, Inc.
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

#ifndef PHOTOS_PREVIEW_NAV_BUTTONS_H
#define PHOTOS_PREVIEW_NAV_BUTTONS_H

#include <glib-object.h>
#include <gtk/gtk.h>

#include "photos-preview-view.h"

G_BEGIN_DECLS

#define PHOTOS_TYPE_PREVIEW_NAV_BUTTONS (photos_preview_nav_buttons_get_type ())

#define PHOTOS_PREVIEW_NAV_BUTTONS(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
   PHOTOS_TYPE_PREVIEW_NAV_BUTTONS, PhotosPreviewNavButtons))

#define PHOTOS_PREVIEW_NAV_BUTTONS_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
   PHOTOS_TYPE_PREVIEW_VIEW, PhotosPreviewNavButtonsClass))

#define PHOTOS_IS_PREVIEW_VIEW(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
   PHOTOS_TYPE_PREVIEW_VIEW))

#define PHOTOS_IS_PREVIEW_NAV_BUTTONS_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
   PHOTOS_TYPE_PREVIEW_VIEW))

#define PHOTOS_PREVIEW_NAV_BUTTONS_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
   PHOTOS_TYPE_PREVIEW_VIEW, PhotosPreviewNavButtonsClass))

typedef struct _PhotosPreviewNavButtons        PhotosPreviewNavButtons;
typedef struct _PhotosPreviewNavButtonsClass   PhotosPreviewNavButtonsClass;
typedef struct _PhotosPreviewNavButtonsPrivate PhotosPreviewNavButtonsPrivate;

struct _PhotosPreviewNavButtons
{
  GObject parent_instance;
  PhotosPreviewNavButtonsPrivate *priv;
};

struct _PhotosPreviewNavButtonsClass
{
  GObjectClass parent_class;
};

GType                       photos_preview_nav_buttons_get_type           (void) G_GNUC_CONST;

PhotosPreviewNavButtons    *photos_preview_nav_buttons_new                (PhotosPreviewView *preview_view,
                                                                           GtkOverlay *overlay);

void                        photos_preview_nav_buttons_hide               (PhotosPreviewNavButtons *self);

void                        photos_preview_nav_buttons_set_model          (PhotosPreviewNavButtons *self,
                                                                           GtkTreeModel *child_model,
                                                                           GtkTreePath *current_child_path);

void                        photos_preview_nav_buttons_show               (PhotosPreviewNavButtons *self);

G_END_DECLS

#endif /* PHOTOS_PREVIEW_NAV_BUTTONS_H */
