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
 *   + Eye of GNOME
 *   + Shotwell
 *   + Totem
 */

#ifndef PHOTOS_UTILS_H
#define PHOTOS_UTILS_H

#include <cairo.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gegl.h>
#include <gio/gio.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <tracker-sparql.h>

#include "photos-base-item.h"
#include "photos-base-manager.h"

G_BEGIN_DECLS

#define PHOTOS_ERROR (photos_utils_error_quark ())
#define PHOTOS_FLASH_OFF (photos_utils_flash_off_quark ())
#define PHOTOS_FLASH_ON (photos_utils_flash_on_quark ())
#define PHOTOS_ORIENTATION_BOTTOM (photos_utils_orientation_bottom_quark ())
#define PHOTOS_ORIENTATION_LEFT (photos_utils_orientation_left_quark ())
#define PHOTOS_ORIENTATION_RIGHT (photos_utils_orientation_right_quark ())
#define PHOTOS_ORIENTATION_TOP (photos_utils_orientation_top_quark ())

#define PHOTOS_BASE_ITEM_EXTENSION_POINT_NAME "photos-base-item"
#define PHOTOS_SHARE_POINT_EXTENSION_POINT_NAME "photos-share-point"
#define PHOTOS_TOOL_EXTENSION_POINT_NAME "photos-tool"
#define PHOTOS_TRACKER_CONTROLLER_EXTENSION_POINT_NAME "photos-tracker-controller"

#define PHOTOS_COLLECTION_SCREENSHOT \
  "http://www.semanticdesktop.org/ontologies/2007/03/22/nfo#image-category-screenshot"
#define PHOTOS_EXPORT_SUBPATH "Exports"

gboolean         photos_utils_app_info_launch_uri         (GAppInfo *appinfo,
                                                           const gchar *uri,
                                                           GAppLaunchContext *launch_context,
                                                           GError **error);

void             photos_utils_border_pixbuf               (GdkPixbuf *pixbuf);

void             photos_utils_buffer_zoom_async           (GeglBuffer *buffer,
                                                           gdouble zoom,
                                                           GCancellable *cancellable,
                                                           GAsyncReadyCallback callback,
                                                           gpointer user_data);

GeglBuffer      *photos_utils_buffer_zoom_finish          (GeglBuffer *buffer, GAsyncResult *res, GError **error);

GdkPixbuf       *photos_utils_center_pixbuf               (GdkPixbuf *pixbuf, gint size);

gchar           *photos_utils_convert_path_to_uri         (const gchar *path);

GeglBuffer      *photos_utils_create_buffer_from_node     (GeglNode *node);

GIcon           *photos_utils_create_collection_icon      (gint base_size, GList *pixbufs);

GeglNode        *photos_utils_create_orientation_node     (GeglNode *parent, GQuark orientation);

GdkPixbuf       *photos_utils_create_pixbuf_from_node     (GeglNode *node);

GdkPixbuf       *photos_utils_create_placeholder_icon_for_scale (const gchar *name, gint size, gint scale);

GIcon           *photos_utils_create_symbolic_icon_for_scale (const gchar *name, gint base_size, gint scale);

gboolean         photos_utils_create_thumbnail            (GFile *file,
                                                           const gchar *mime_type,
                                                           gint64 mtime,
                                                           GCancellable *cancellable,
                                                           GError **error);

GIcon           *photos_utils_get_icon_from_cursor        (TrackerSparqlCursor *cursor);

GdkPixbuf       *photos_utils_downscale_pixbuf_for_scale  (GdkPixbuf *pixbuf, gint size, gint scale);

void             photos_utils_draw_rectangle_handles      (cairo_t *cr,
                                                           gdouble x,
                                                           gdouble y,
                                                           gdouble width,
                                                           gdouble height,
                                                           gdouble offset,
                                                           gdouble radius);

void             photos_utils_draw_rectangle_thirds       (cairo_t *cr,
                                                           gdouble x,
                                                           gdouble y,
                                                           gdouble width,
                                                           gdouble height);

void             photos_utils_ensure_builtins             (void);

void             photos_utils_ensure_extension_points     (void);

gboolean         photos_utils_equal_double                (gdouble a, gdouble b);

GQuark           photos_utils_error_quark                 (void);

gdouble          photos_utils_eval_radial_line            (gdouble crop_center_x,
                                                           gdouble crop_center_y,
                                                           gdouble corner_x,
                                                           gdouble corner_y,
                                                           gdouble event_x);

void             photos_utils_file_create_async           (GFile *file,
                                                           GFileCreateFlags flags,
                                                           gint io_priority,
                                                           GCancellable *cancellable,
                                                           GAsyncReadyCallback callback,
                                                           gpointer user_data);

GFileOutputStream *photos_utils_file_create_finish        (GFile *file,
                                                           GAsyncResult *res,
                                                           GFile **out_unique_file,
                                                           GError **error);

gchar           *photos_utils_filename_strip_extension    (const gchar *filename_with_extension);

GQuark           photos_utils_flash_off_quark             (void);

GQuark           photos_utils_flash_on_quark              (void);

gchar           *photos_utils_get_extension_from_mime_type (const gchar *mime_type);


gint             photos_utils_get_icon_size               (void);

gint             photos_utils_get_icon_size_unscaled      (void);

char*            photos_utils_get_pixbuf_common_suffix    (GdkPixbufFormat *format);

const gchar     *photos_utils_get_provider_name           (PhotosBaseManager *src_mngr, PhotosBaseItem *item);

GtkBorder       *photos_utils_get_thumbnail_frame_border  (void);

GList           *photos_utils_get_urns_from_paths         (GList *paths, GtkTreeModel *model);

GIcon           *photos_utils_icon_from_rdf_type          (const gchar *type);

void             photos_utils_list_box_header_func        (GtkListBoxRow *row,
                                                           GtkListBoxRow *before,
                                                           gpointer user_data);

gboolean         photos_utils_make_directory_with_parents (GFile *file, GCancellable *cancellable, GError **error);

GQuark           photos_utils_orientation_bottom_quark    (void);

GQuark           photos_utils_orientation_left_quark      (void);

GQuark           photos_utils_orientation_right_quark     (void);

GQuark           photos_utils_orientation_top_quark       (void);

void             photos_utils_remove_children_from_node   (GeglNode *node);

void             photos_utils_set_edited_name             (const gchar *urn, const gchar *title);

void             photos_utils_set_favorite                (const gchar *urn, gboolean is_favorite);

gboolean         photos_utils_set_string                  (gchar **string_ptr, const gchar *new_string);

gboolean         photos_utils_take_string                 (gchar **string_ptr, gchar *new_string);

G_END_DECLS

#endif /* PHOTOS_UTILS_H */
