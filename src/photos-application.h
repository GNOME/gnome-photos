/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2012 – 2017 Red Hat, Inc.
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

#ifndef PHOTOS_APPLICATION_H
#define PHOTOS_APPLICATION_H

#include <gtk/gtk.h>

#include "photos-gom-miner.h"

G_BEGIN_DECLS

#define PHOTOS_TYPE_APPLICATION (photos_application_get_type ())
G_DECLARE_FINAL_TYPE (PhotosApplication, photos_application, PHOTOS, APPLICATION, GtkApplication);

GApplication          *photos_application_new                    (void);

GomMiner              *photos_application_get_miner              (PhotosApplication *self,
                                                                  const gchar *provider_type);

GList                 *photos_application_get_miners_running     (PhotosApplication *self);

gint                   photos_application_get_scale_factor       (PhotosApplication *self);

void                   photos_application_hold                   (PhotosApplication *self);

void                   photos_application_release                (PhotosApplication *self);

G_END_DECLS

#endif /* PHOTOS_APPLICATION_H */
