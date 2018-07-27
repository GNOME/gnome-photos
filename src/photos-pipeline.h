/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2015 – 2017 Red Hat, Inc.
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

#ifndef PHOTOS_PIPELINE_H
#define PHOTOS_PIPELINE_H

#include <stdarg.h>

#include <gegl.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define PHOTOS_TYPE_PIPELINE (photos_pipeline_get_type ())
G_DECLARE_FINAL_TYPE (PhotosPipeline, photos_pipeline, PHOTOS, PIPELINE, GObject);

void                   photos_pipeline_new_async         (GeglNode *parent,
                                                          const gchar *const *uris,
                                                          GCancellable *cancellable,
                                                          GAsyncReadyCallback callback,
                                                          gpointer user_data);

PhotosPipeline        *photos_pipeline_new_finish        (GAsyncResult *res, GError **error);

void                   photos_pipeline_add_valist        (PhotosPipeline *self,
                                                          const gchar *operation,
                                                          const gchar *first_property_name,
                                                          va_list ap);

gboolean               photos_pipeline_get               (PhotosPipeline *self,
                                                          const gchar *operation,
                                                          const gchar *first_property_name,
                                                          ...) G_GNUC_NULL_TERMINATED G_GNUC_WARN_UNUSED_RESULT;

gboolean               photos_pipeline_get_valist        (PhotosPipeline *self,
                                                          const gchar *operation,
                                                          const gchar *first_property_name,
                                                          va_list ap) G_GNUC_WARN_UNUSED_RESULT;

GeglNode              *photos_pipeline_get_graph         (PhotosPipeline *self);

GeglNode              *photos_pipeline_get_output        (PhotosPipeline *self);

gboolean               photos_pipeline_is_edited         (PhotosPipeline *self);

GeglProcessor         *photos_pipeline_new_processor     (PhotosPipeline *self);

void                   photos_pipeline_save_async        (PhotosPipeline *self,
                                                          GCancellable *cancellable,
                                                          GAsyncReadyCallback callback,
                                                          gpointer user_data);

gboolean               photos_pipeline_save_finish       (PhotosPipeline *self, GAsyncResult *res, GError **error);

gboolean               photos_pipeline_remove            (PhotosPipeline *self, const gchar *operation);

void                   photos_pipeline_revert            (PhotosPipeline *self);

void                   photos_pipeline_revert_to_original(PhotosPipeline *self);

void                   photos_pipeline_set_parent        (PhotosPipeline *self, GeglNode *parent);

void                   photos_pipeline_snapshot          (PhotosPipeline *self);

G_END_DECLS

#endif /* PHOTOS_PIPELINE_H */
