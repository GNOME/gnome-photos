/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2012 – 2017 Red Hat, Inc.
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

#ifndef PHOTOS_ITEM_MANAGER_H
#define PHOTOS_ITEM_MANAGER_H

#include <gegl.h>
#include <gtk/gtk.h>
#include <tracker-sparql.h>

#include "photos-base-item.h"
#include "photos-base-manager.h"

G_BEGIN_DECLS

#define PHOTOS_TYPE_ITEM_MANAGER (photos_item_manager_get_type ())
G_DECLARE_FINAL_TYPE (PhotosItemManager, photos_item_manager, PHOTOS, ITEM_MANAGER, PhotosBaseManager);

#define PHOTOS_TYPE_MODE_CONTROLLER (photos_item_manager_get_type ())

#define PHOTOS_MODE_CONTROLLER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
   PHOTOS_TYPE_MODE_CONTROLLER, PhotosModeController))

#define PHOTOS_IS_MODE_CONTROLLER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
   PHOTOS_TYPE_MODE_CONTROLLER))

typedef enum
{
  PHOTOS_LOAD_STATE_NONE,
  PHOTOS_LOAD_STATE_ERROR,
  PHOTOS_LOAD_STATE_FINISHED,
  PHOTOS_LOAD_STATE_STARTED
} PhotosLoadState;

typedef enum
{
  PHOTOS_WINDOW_MODE_NONE,
  PHOTOS_WINDOW_MODE_COLLECTION_VIEW,
  PHOTOS_WINDOW_MODE_COLLECTIONS,
  PHOTOS_WINDOW_MODE_EDIT,
  PHOTOS_WINDOW_MODE_FAVORITES,
  PHOTOS_WINDOW_MODE_OVERVIEW,
  PHOTOS_WINDOW_MODE_PREVIEW,
  PHOTOS_WINDOW_MODE_SEARCH
} PhotosWindowMode;

typedef struct _PhotosItemManager      PhotosModeController;
typedef struct _PhotosItemManagerClass PhotosModeControllerClass;

PhotosBaseManager        *photos_item_manager_new                          (void);

void                      photos_item_manager_add_item                     (PhotosItemManager *self,
                                                                            TrackerSparqlCursor *cursor,
                                                                            gboolean force);

void                      photos_item_manager_add_item_for_mode            (PhotosItemManager *self,
                                                                            PhotosWindowMode mode,
                                                                            TrackerSparqlCursor *cursor);

void                      photos_item_manager_clear                        (PhotosItemManager *self,
                                                                            PhotosWindowMode mode);

PhotosBaseItem           *photos_item_manager_create_item                  (PhotosItemManager *self,
                                                                            TrackerSparqlCursor *cursor);

PhotosBaseItem           *photos_item_manager_get_active_collection        (PhotosItemManager *self);

GHashTable               *photos_item_manager_get_collections              (PhotosItemManager *self);

PhotosBaseManager        *photos_item_manager_get_for_mode                 (PhotosItemManager *self,
                                                                            PhotosWindowMode mode);

PhotosLoadState           photos_item_manager_get_load_state               (PhotosItemManager *self);

void                      photos_item_manager_hide_item                    (PhotosItemManager *self,
                                                                            PhotosBaseItem *item);

void                      photos_item_manager_set_constraints_for_mode     (PhotosItemManager *self,
                                                                            gboolean constrain,
                                                                            PhotosWindowMode mode);

void                      photos_item_manager_wait_for_file_async          (PhotosItemManager *self,
                                                                            GFile *file,
                                                                            GCancellable *cancellable,
                                                                            GAsyncReadyCallback callback,
                                                                            gpointer user_data);

gchar                    *photos_item_manager_wait_for_file_finish         (PhotosItemManager *self,
                                                                            GAsyncResult *res,
                                                                            GError **error);

void                      photos_item_manager_wait_for_changes_async       (PhotosItemManager *self,
                                                                            PhotosBaseItem *item,
                                                                            GCancellable *cancellable,
                                                                            GAsyncReadyCallback callback,
                                                                            gpointer user_data);

gchar                    *photos_item_manager_wait_for_changes_finish      (PhotosItemManager *self,
                                                                            GAsyncResult *res,
                                                                            GError **error);

void                      photos_item_manager_unhide_item                  (PhotosItemManager *self,
                                                                            PhotosBaseItem *item);

gboolean                  photos_mode_controller_get_can_fullscreen        (PhotosModeController *self);

gboolean                  photos_mode_controller_get_fullscreen            (PhotosModeController *self);

PhotosWindowMode          photos_mode_controller_get_window_mode           (PhotosModeController *self);

void                      photos_mode_controller_go_back                   (PhotosModeController *self);

void                      photos_mode_controller_toggle_fullscreen         (PhotosModeController *self);

void                      photos_mode_controller_set_fullscreen            (PhotosModeController *self,
                                                                            gboolean              fullscreen);

void                      photos_mode_controller_set_window_mode           (PhotosModeController *self,
                                                                            PhotosWindowMode      mode);

G_END_DECLS

#endif /* PHOTOS_ITEM_MANAGER_H */
