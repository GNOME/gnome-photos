/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2013 – 2017 Red Hat, Inc.
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

#include "photos-item-manager.h"
#include "photos-preview-view.h"

G_BEGIN_DECLS

#define PHOTOS_TYPE_PREVIEW_NAV_BUTTONS (photos_preview_nav_buttons_get_type ())
G_DECLARE_FINAL_TYPE (PhotosPreviewNavButtons, photos_preview_nav_buttons, PHOTOS, PREVIEW_NAV_BUTTONS, GObject);

PhotosPreviewNavButtons    *photos_preview_nav_buttons_new                (PhotosPreviewView *preview_view,
                                                                           GtkOverlay *overlay);

void                        photos_preview_nav_buttons_hide               (PhotosPreviewNavButtons *self);

void                        photos_preview_nav_buttons_set_auto_hide      (PhotosPreviewNavButtons *self,
                                                                           gboolean auto_hide);

void                        photos_preview_nav_buttons_set_mode           (PhotosPreviewNavButtons *self,
                                                                           PhotosWindowMode old_mode);

void                        photos_preview_nav_buttons_set_show_navigation (PhotosPreviewNavButtons *self,
                                                                            gboolean show_navigation);

void                        photos_preview_nav_buttons_show               (PhotosPreviewNavButtons *self);

G_END_DECLS

#endif /* PHOTOS_PREVIEW_NAV_BUTTONS_H */
