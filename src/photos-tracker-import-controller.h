/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2018 Red Hat, Inc.
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

/* Based on code from:
 *   + Documents
 */

#ifndef PHOTOS_TRACKER_IMPORT_CONTROLLER_H
#define PHOTOS_TRACKER_IMPORT_CONTROLLER_H

#include "photos-tracker-controller.h"

G_BEGIN_DECLS

#define PHOTOS_TYPE_TRACKER_IMPORT_CONTROLLER (photos_tracker_import_controller_get_type ())
G_DECLARE_FINAL_TYPE (PhotosTrackerImportController,
                      photos_tracker_import_controller,
                      PHOTOS,
                      TRACKER_IMPORT_CONTROLLER,
                      PhotosTrackerController);

PhotosTrackerController  *photos_tracker_import_controller_dup_singleton     (void);

G_END_DECLS

#endif /* PHOTOS_TRACKER_IMPORT_CONTROLLER_H */
