/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2012 – 2016 Red Hat, Inc.
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

#ifndef PHOTOS_BASE_MANAGER_H
#define PHOTOS_BASE_MANAGER_H

#include <glib-object.h>

G_BEGIN_DECLS

#define PHOTOS_TYPE_BASE_MANAGER (photos_base_manager_get_type ())
G_DECLARE_DERIVABLE_TYPE (PhotosBaseManager, photos_base_manager, PHOTOS, BASE_MANAGER, GObject);

typedef struct _PhotosBaseManagerPrivate PhotosBaseManagerPrivate;

struct _PhotosBaseManagerClass
{
  GObjectClass parent_class;

  /* virtual methods */
  void           (*add_object)             (PhotosBaseManager *self, GObject *object);
  GObject       *(*get_active_object)      (PhotosBaseManager *self);
  gchar         *(*get_filter)             (PhotosBaseManager *self, gint flags);
  GObject       *(*get_next_object)        (PhotosBaseManager *self, GObject *object);
  GObject       *(*get_object_by_id)       (PhotosBaseManager *self, const gchar *id);
  GObject       *(*get_previous_object)    (PhotosBaseManager *self, GObject *object);
  gchar         *(*get_where)              (PhotosBaseManager *self, gint flags);
  void           (*remove_object_by_id)    (PhotosBaseManager *self, const gchar *id);
  gboolean       (*set_active_object)      (PhotosBaseManager *self, GObject *object);

  /* signals */
  void           (*active_changed)         (PhotosBaseManager *self, GObject *object);
  void           (*clear)                  (PhotosBaseManager *self);
  void           (*object_added)           (PhotosBaseManager *self, GObject *object);
  void           (*object_removed)         (PhotosBaseManager *self, GObject *object);
};

PhotosBaseManager  *photos_base_manager_new                      (GCompareDataFunc sort_func, gpointer sort_data);

void                photos_base_manager_add_object               (PhotosBaseManager *self, GObject *object);

void                photos_base_manager_clear                    (PhotosBaseManager *self);

const gchar        *photos_base_manager_get_action_id            (PhotosBaseManager *self);

GObject            *photos_base_manager_get_active_object        (PhotosBaseManager *self);

gchar              *photos_base_manager_get_all_filter           (PhotosBaseManager *self);

gchar              *photos_base_manager_get_filter               (PhotosBaseManager *self, gint flags);

GObject            *photos_base_manager_get_next_object          (PhotosBaseManager *self, GObject *object);

GObject            *photos_base_manager_get_object_by_id         (PhotosBaseManager *self, const gchar *id);

guint               photos_base_manager_get_objects_count        (PhotosBaseManager *self);

GObject            *photos_base_manager_get_previous_object      (PhotosBaseManager *self, GObject *object);

const gchar        *photos_base_manager_get_title                (PhotosBaseManager *self);

gchar              *photos_base_manager_get_where                (PhotosBaseManager *self, gint flags);

void                photos_base_manager_process_new_objects      (PhotosBaseManager *self, GHashTable *new_objects);

void                photos_base_manager_remove_object            (PhotosBaseManager *self, GObject *object);

void                photos_base_manager_remove_object_by_id      (PhotosBaseManager *self, const gchar *id);

gboolean            photos_base_manager_set_active_object        (PhotosBaseManager *self, GObject *object);

gboolean            photos_base_manager_set_active_object_by_id  (PhotosBaseManager *self, const gchar *id);

G_END_DECLS

#endif /* PHOTOS_BASE_MANAGER_H */
