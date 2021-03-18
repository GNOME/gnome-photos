/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2014 Pranav Kant
 * Copyright © 2012 – 2021 Red Hat, Inc.
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

#ifndef PHOTOS_BASE_ITEM_H
#define PHOTOS_BASE_ITEM_H

#include <cairo.h>
#include <gegl.h>
#include <gio/gio.h>
#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define PHOTOS_TYPE_BASE_ITEM (photos_base_item_get_type ())
G_DECLARE_DERIVABLE_TYPE (PhotosBaseItem, photos_base_item, PHOTOS, BASE_ITEM, GObject);

typedef struct _PhotosBaseItemSize PhotosBaseItemSize;

struct _PhotosBaseItemSize
{
  gdouble zoom;
  gint height;
  gint width;
  gsize bytes;
};

typedef struct _PhotosBaseItemPrivate PhotosBaseItemPrivate;

struct _PhotosBaseItemClass
{
  GObjectClass parent_class;

  const gchar *miner_name;
  const gchar *miner_object_path;

  /* virtual methods */
  gchar      *(*create_filename_fallback)   (PhotosBaseItem *self);
  gchar      *(*create_name_fallback)       (PhotosBaseItem *self);
  GStrv       (*create_pipeline_paths)      (PhotosBaseItem *self);
  gboolean    (*create_thumbnail)           (PhotosBaseItem *self, GCancellable *cancellable, GError **error);
  gchar      *(*create_thumbnail_path)      (PhotosBaseItem *self);
  GFile      *(*download)                   (PhotosBaseItem *self, GCancellable *cancellable, GError **error);
  GtkWidget  *(*get_source_widget)          (PhotosBaseItem *self);
  gboolean    (*metadata_add_shared)        (PhotosBaseItem  *self,
                                             const gchar     *provider_type,
                                             const gchar     *account_identity,
                                             const gchar     *shared_id,
                                             GCancellable    *cancellable,
                                             GError         **error);
  void        (*open)                       (PhotosBaseItem *self, GtkWindow *parent, guint32 timestamp);
  void        (*refresh_icon)               (PhotosBaseItem *self);
  void        (*set_favorite)               (PhotosBaseItem *self, gboolean favorite);
  void        (*trash_async)                (PhotosBaseItem *self,
                                             GCancellable *cancellable,
                                             GAsyncReadyCallback callback,
                                             gpointer user_data);
  gboolean    (*trash_finish)               (PhotosBaseItem *self, GAsyncResult *res, GError **error);
  void        (*update_type_description)    (PhotosBaseItem *self);

  /* signals */
  void        (*info_updated)               (PhotosBaseItem *self);
};

gboolean            photos_base_item_can_edit                (PhotosBaseItem *self);

gboolean            photos_base_item_can_trash               (PhotosBaseItem *self);

GStrv               photos_base_item_create_pipeline_paths   (PhotosBaseItem *self) G_GNUC_WARN_UNUSED_RESULT;

cairo_surface_t    *photos_base_item_create_preview          (PhotosBaseItem *self,
                                                              gint size,
                                                              gint scale,
                                                              const gchar *operation,
                                                              const gchar *first_property_name,
                                                              ...) G_GNUC_NULL_TERMINATED G_GNUC_WARN_UNUSED_RESULT;

gchar              *photos_base_item_create_thumbnail_path   (PhotosBaseItem *self) G_GNUC_WARN_UNUSED_RESULT;

void                photos_base_item_destroy                 (PhotosBaseItem *self);

GFile              *photos_base_item_download                (PhotosBaseItem *self,
                                                              GCancellable *cancellable,
                                                              GError **error);

void                photos_base_item_download_async          (PhotosBaseItem *self,
                                                              GCancellable *cancellable,
                                                              GAsyncReadyCallback callback,
                                                              gpointer user_data);

GFile              *photos_base_item_download_finish         (PhotosBaseItem *self,
                                                              GAsyncResult *res,
                                                              GError **error);

const gchar        *photos_base_item_get_author              (PhotosBaseItem *self);

gboolean            photos_base_item_get_bbox_edited         (PhotosBaseItem *self, GeglRectangle *out_bbox);

gboolean            photos_base_item_get_bbox_source         (PhotosBaseItem *self, GeglRectangle *bbox);

gint64              photos_base_item_get_date_created        (PhotosBaseItem *self);

const gchar        *photos_base_item_get_default_app_name    (PhotosBaseItem *self);

GQuark              photos_base_item_get_equipment           (PhotosBaseItem *self);

gdouble             photos_base_item_get_exposure_time       (PhotosBaseItem *self);

GQuark              photos_base_item_get_flash               (PhotosBaseItem *self);

const gchar        *photos_base_item_get_filename            (PhotosBaseItem *self);

gdouble             photos_base_item_get_fnumber             (PhotosBaseItem *self);

gdouble             photos_base_item_get_focal_length        (PhotosBaseItem *self);

gint64              photos_base_item_get_height              (PhotosBaseItem *self);

const gchar        *photos_base_item_get_identifier          (PhotosBaseItem *self);

gdouble             photos_base_item_get_iso_speed           (PhotosBaseItem *self);

const gchar        *photos_base_item_get_location            (PhotosBaseItem *self);

const gchar        *photos_base_item_get_mime_type           (PhotosBaseItem *self);

gint64              photos_base_item_get_mtime               (PhotosBaseItem *self);

const gchar        *photos_base_item_get_name                (PhotosBaseItem *self);

const gchar        *photos_base_item_get_name_with_fallback  (PhotosBaseItem *self);

GQuark              photos_base_item_get_orientation         (PhotosBaseItem *self);

GdkPixbuf          *photos_base_item_get_original_icon       (PhotosBaseItem *self);

const gchar        *photos_base_item_get_resource_urn        (PhotosBaseItem *self);

GtkWidget          *photos_base_item_get_source_widget       (PhotosBaseItem *self);

cairo_surface_t    *photos_base_item_get_surface             (PhotosBaseItem *self);

const gchar        *photos_base_item_get_type_description    (PhotosBaseItem *self);

const gchar        *photos_base_item_get_uri                 (PhotosBaseItem *self);

gchar              *photos_base_item_get_where               (PhotosBaseItem *self);

gint64              photos_base_item_get_width               (PhotosBaseItem *self);

void                photos_base_item_guess_save_sizes_async  (PhotosBaseItem *self,
                                                              GCancellable *cancellable,
                                                              GAsyncReadyCallback callback,
                                                              gpointer user_data);

gboolean            photos_base_item_guess_save_sizes_finish (PhotosBaseItem *self,
                                                              GAsyncResult *res,
                                                              PhotosBaseItemSize *out_full_size,
                                                              PhotosBaseItemSize *out_reduced_size,
                                                              GError **error);

gboolean            photos_base_item_is_collection           (PhotosBaseItem *self);

gboolean            photos_base_item_is_favorite             (PhotosBaseItem *self);

void                photos_base_item_load_async              (PhotosBaseItem *self,
                                                              GCancellable *cancellable,
                                                              GAsyncReadyCallback callback,
                                                              gpointer user_data);

GeglNode           *photos_base_item_load_finish             (PhotosBaseItem *self,
                                                              GAsyncResult *res,
                                                              GError **error);

void                photos_base_item_mark_busy               (PhotosBaseItem *self);

void                photos_base_item_metadata_add_shared_async  (PhotosBaseItem *self,
                                                                 const gchar *provider_type,
                                                                 const gchar *account_identity,
                                                                 const gchar *shared_id,
                                                                 GCancellable *cancellable,
                                                                 GAsyncReadyCallback callback,
                                                                 gpointer user_data);

gboolean            photos_base_item_metadata_add_shared_finish (PhotosBaseItem *self,
                                                                 GAsyncResult *res,
                                                                 GError **error);

void                photos_base_item_open                    (PhotosBaseItem *self,
                                                              GtkWindow *parent,
                                                              guint32 timestamp);

void                photos_base_item_operation_add_async     (PhotosBaseItem *self,
                                                              GCancellable *cancellable,
                                                              GAsyncReadyCallback callback,
                                                              gpointer user_data,
                                                              const gchar *operation,
                                                              const gchar *first_property_name,
                                                              ...) G_GNUC_NULL_TERMINATED;

gboolean            photos_base_item_operation_add_finish    (PhotosBaseItem *self,
                                                              GAsyncResult *res,
                                                              GError **error);

gboolean            photos_base_item_operation_get           (PhotosBaseItem *self,
                                                              const gchar *operation,
                                                              const gchar *first_property_name,
                                                              ...) G_GNUC_NULL_TERMINATED G_GNUC_WARN_UNUSED_RESULT;

void                photos_base_item_operation_remove_async  (PhotosBaseItem *self,
                                                              const gchar *operation,
                                                              GCancellable *cancellable,
                                                              GAsyncReadyCallback callback,
                                                              gpointer user_data);

gboolean            photos_base_item_operation_remove_finish (PhotosBaseItem *self,
                                                              GAsyncResult *res,
                                                              GError **error);

void                photos_base_item_pipeline_is_edited_async          (PhotosBaseItem *self,
                                                                        GCancellable *cancellable,
                                                                        GAsyncReadyCallback callback,
                                                                        gpointer user_data);

gboolean            photos_base_item_pipeline_is_edited_finish         (PhotosBaseItem *self,
                                                                        GAsyncResult *res,
                                                                        GError **error);

void                photos_base_item_pipeline_revert_async             (PhotosBaseItem *self,
                                                                        GCancellable *cancellable,
                                                                        GAsyncReadyCallback callback,
                                                                        gpointer user_data);

gboolean            photos_base_item_pipeline_revert_finish            (PhotosBaseItem *self,
                                                                        GAsyncResult *res,
                                                                        GError **error);

void                photos_base_item_pipeline_revert_to_original_async (PhotosBaseItem *self,
                                                                        GCancellable *cancellable,
                                                                        GAsyncReadyCallback callback,
                                                                        gpointer user_data);

gboolean            photos_base_item_pipeline_revert_to_original_finish (PhotosBaseItem *self,
                                                                         GAsyncResult *res,
                                                                         GError **error);

void                photos_base_item_pipeline_save_async     (PhotosBaseItem *self,
                                                              GCancellable *cancellable,
                                                              GAsyncReadyCallback callback,
                                                              gpointer user_data);

gboolean            photos_base_item_pipeline_save_finish    (PhotosBaseItem *self,
                                                              GAsyncResult *res,
                                                              GError **error);

void                photos_base_item_pipeline_snapshot       (PhotosBaseItem *self);

void                photos_base_item_print                   (PhotosBaseItem *self, GtkWidget *toplevel);

GFileInfo          *photos_base_item_query_info              (PhotosBaseItem *self,
                                                              const gchar *attributes,
                                                              GFileQueryInfoFlags flags,
                                                              GCancellable *cancellable,
                                                              GError **error);

void                photos_base_item_query_info_async        (PhotosBaseItem *self,
                                                              const gchar *attributes,
                                                              GFileQueryInfoFlags flags,
                                                              gint io_priority,
                                                              GCancellable *cancellable,
                                                              GAsyncReadyCallback callback,
                                                              gpointer user_data);

GFileInfo          *photos_base_item_query_info_finish       (PhotosBaseItem *self,
                                                              GAsyncResult *res,
                                                              GError **error);

void                photos_base_item_refresh                 (PhotosBaseItem *self);

void                photos_base_item_save_to_dir_async       (PhotosBaseItem *self,
                                                              GFile *dir,
                                                              gdouble zoom,
                                                              GCancellable *cancellable,
                                                              GAsyncReadyCallback callback,
                                                              gpointer user_data);

GFile              *photos_base_item_save_to_dir_finish      (PhotosBaseItem *self,
                                                              GAsyncResult *res,
                                                              GError **error) G_GNUC_WARN_UNUSED_RESULT;

void                photos_base_item_save_to_file_async      (PhotosBaseItem *self,
                                                              GFile *file,
                                                              GFileCreateFlags flags,
                                                              gdouble zoom,
                                                              GCancellable *cancellable,
                                                              GAsyncReadyCallback callback,
                                                              gpointer user_data);

gboolean            photos_base_item_save_to_file_finish     (PhotosBaseItem *self,
                                                              GAsyncResult *res,
                                                              GError **error);

void                photos_base_item_save_to_stream_async    (PhotosBaseItem *self,
                                                              GOutputStream *stream,
                                                              gdouble zoom,
                                                              GCancellable *cancellable,
                                                              GAsyncReadyCallback callback,
                                                              gpointer user_data);

gboolean            photos_base_item_save_to_stream_finish   (PhotosBaseItem *self,
                                                              GAsyncResult *res,
                                                              GError **error);

void                photos_base_item_set_default_app         (PhotosBaseItem *self, GAppInfo *default_app);

void                photos_base_item_set_default_app_name    (PhotosBaseItem *self, const gchar *default_app_name);

void                photos_base_item_set_favorite            (PhotosBaseItem *self, gboolean favorite);

void                photos_base_item_trash_async             (PhotosBaseItem *self,
                                                              GCancellable *cancellable,
                                                              GAsyncReadyCallback callback,
                                                              gpointer user_data);

gboolean            photos_base_item_trash_finish            (PhotosBaseItem *self,
                                                              GAsyncResult *res,
                                                              GError **error);

void                photos_base_item_unmark_busy             (PhotosBaseItem *self);

G_END_DECLS

#endif /* PHOTOS_BASE_ITEM_H */
