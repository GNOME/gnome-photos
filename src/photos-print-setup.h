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

#include <gegl.h>
#include <gtk/gtk.h>

#ifndef PHOTOS_PRINT_SETUP_H
#define PHOTOS_PRINT_SETUP_H

G_BEGIN_DECLS

#define PHOTOS_TYPE_PRINT_SETUP (photos_print_setup_get_type ())
G_DECLARE_FINAL_TYPE (PhotosPrintSetup, photos_print_setup, PHOTOS, PRINT_SETUP, GtkGrid);

GtkWidget        *photos_print_setup_new         (GeglNode *node, GtkPageSetup *page_setup);

void              photos_print_setup_get_options (PhotosPrintSetup *self,
						  gdouble *left,
						  gdouble *top,
						  gdouble *scale,
						  GtkUnit *unit);

void              photos_print_setup_update      (PhotosPrintSetup *self, GtkPageSetup *page_setup);

G_END_DECLS

#endif /* PHOTOS_PRINT_SETUP_H */
