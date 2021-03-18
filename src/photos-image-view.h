/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2015 – 2021 Red Hat, Inc.
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

#ifndef PHOTOS_IMAGE_VIEW_H
#define PHOTOS_IMAGE_VIEW_H

#include <gegl.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define PHOTOS_TYPE_IMAGE_VIEW (photos_image_view_get_type ())
G_DECLARE_FINAL_TYPE (PhotosImageView, photos_image_view, PHOTOS, IMAGE_VIEW, GtkDrawingArea);

GtkWidget          *photos_image_view_new                (void);

GtkWidget          *photos_image_view_new_from_node      (GeglNode *node);

gboolean            photos_image_view_get_best_fit       (PhotosImageView *self);

GeglNode           *photos_image_view_get_node           (PhotosImageView *self);

gdouble             photos_image_view_get_x              (PhotosImageView *self);

gdouble             photos_image_view_get_y              (PhotosImageView *self);

gdouble             photos_image_view_get_zoom           (PhotosImageView *self);

void                photos_image_view_set_best_fit       (PhotosImageView *self,
                                                          gboolean best_fit,
                                                          gboolean enable_animation);

void                photos_image_view_set_node           (PhotosImageView *self, GeglNode *node);

void                photos_image_view_set_zoom           (PhotosImageView *self,
                                                          gdouble zoom,
                                                          gboolean enable_animation);

G_END_DECLS

#endif /* PHOTOS_IMAGE_VIEW_H */
