/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2014 – 2017 Red Hat, Inc.
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

#ifndef PHOTOS_OVERVIEW_SEARCHBAR_H
#define PHOTOS_OVERVIEW_SEARCHBAR_H

#include "photos-searchbar.h"
#include "photos-search-popover.h"

G_BEGIN_DECLS

#define PHOTOS_TYPE_OVERVIEW_SEARCHBAR (photos_overview_searchbar_get_type ())

#define PHOTOS_OVERVIEW_SEARCHBAR(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
   PHOTOS_TYPE_OVERVIEW_SEARCHBAR, PhotosOverviewSearchbar))

#define PHOTOS_IS_OVERVIEW_SEARCHBAR(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
   PHOTOS_TYPE_OVERVIEW_SEARCHBAR))

typedef struct _PhotosOverviewSearchbar      PhotosOverviewSearchbar;
typedef struct _PhotosOverviewSearchbarClass PhotosOverviewSearchbarClass;

GType                photos_overview_searchbar_get_type                      (void) G_GNUC_CONST;

GtkWidget           *photos_overview_searchbar_new                           (void);

G_END_DECLS

#endif /* PHOTOS_OVERVIEW_SEARCHBAR_H */
