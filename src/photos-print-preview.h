/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2006 – 2007 The Free Software Foundation
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
 *   + Eye of GNOME
 */

#ifndef PHOTOS_PRINT_PREVIEW_H
#define PHOTOS_PRINT_PREVIEW_H

G_BEGIN_DECLS

#define PHOTOS_TYPE_PRINT_PREVIEW (photos_print_preview_get_type ())
G_DECLARE_FINAL_TYPE (PhotosPrintPreview, photos_print_preview, PHOTOS, PRINT_PREVIEW, GtkAspectFrame);

GtkWidget   *photos_print_preview_new                   (void);

GtkWidget   *photos_print_preview_new_with_pixbuf       (GdkPixbuf *pixbuf);

void         photos_print_preview_set_page_margins      (PhotosPrintPreview *self,
                                                         gfloat l_margin,
                                                         gfloat r_margin,
                                                         gfloat t_margin,
                                                         gfloat b_margin);

void         photos_print_preview_set_from_page_setup   (PhotosPrintPreview *self,
                                                         GtkPageSetup *setup);

void         photos_print_preview_get_image_position    (PhotosPrintPreview *self,
                                                         gdouble *x,
                                                         gdouble *y);

void         photos_print_preview_set_image_position    (PhotosPrintPreview *self,
                                                         gdouble x,
                                                         gdouble y);

gboolean     photos_print_preview_point_in_image_area   (PhotosPrintPreview *self,
                                                         guint x,
                                                         guint y);

void         photos_print_preview_set_scale             (PhotosPrintPreview *self,
                                                         gfloat scale);

gfloat       photos_print_preview_get_scale             (PhotosPrintPreview *self);

G_END_DECLS

#endif /* PHOTOS_PRINT_PREVIEW_H */
