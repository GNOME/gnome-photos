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
 *   + Eye of GNOME
 *   + Shotwell
 */

#ifndef PHOTOS_UTILS_H
#define PHOTOS_UTILS_H

#include <cairo.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gio/gio.h>
#include <glib.h>
#include <gtk/gtk.h>

#include "photos-base-item.h"
#include "photos-base-manager.h"
#include "photos-item-manager.h"
#include "photos-offset-controller.h"
#include "photos-tracker-controller.h"

G_BEGIN_DECLS

#define PHOTOS_BASE_ITEM_EXTENSION_POINT_NAME "photos-base-item"
#define PHOTOS_SHARE_POINT_EXTENSION_POINT_NAME "photos-share-point"
#define PHOTOS_SHARE_POINT_ONLINE_EXTENSION_POINT_NAME "photos-share-point-online"
#define PHOTOS_TOOL_EXTENSION_POINT_NAME "photos-tool"
#define PHOTOS_TRACKER_CONTROLLER_EXTENSION_POINT_NAME "photos-tracker-controller"

#define PHOTOS_COLLECTION_SCREENSHOT \
  "http://www.semanticdesktop.org/ontologies/2007/03/22/nfo#image-category-screenshot"
#define PHOTOS_EXPORT_SUBPATH "Exports"

typedef enum
{
  PHOTOS_ZOOM_EVENT_NONE,
  PHOTOS_ZOOM_EVENT_KEYBOARD_ACCELERATOR,
  PHOTOS_ZOOM_EVENT_MOUSE_CLICK,
  PHOTOS_ZOOM_EVENT_SCROLL,
  PHOTOS_ZOOM_EVENT_TOUCH
} PhotosZoomEvent;

GdkPixbuf       *photos_utils_center_pixbuf               (GdkPixbuf *pixbuf, gint size);

gchar           *photos_utils_convert_path_to_uri         (const gchar *path);

GIcon           *photos_utils_create_collection_icon      (gint base_size, GList *pixbufs);

GdkPixbuf       *photos_utils_create_placeholder_icon_for_scale (const gchar *name, gint size, gint scale);

GIcon           *photos_utils_create_symbolic_icon_for_scale (const gchar *name, gint base_size, gint scale);

gboolean         photos_utils_create_thumbnail            (GFile *file,
                                                           const gchar *mime_type,
                                                           gint64 mtime,
                                                           GQuark orientation,
                                                           gint64 original_height,
                                                           gint64 original_width,
                                                           const gchar *pipeline_uri,
                                                           const gchar *thumbnail_path,
                                                           GCancellable *cancellable,
                                                           GError **error);

GVariant        *photos_utils_create_zoom_target_value    (gdouble delta, PhotosZoomEvent event);

GIcon           *photos_utils_get_icon_from_item          (PhotosBaseItem *item);

gdouble          photos_utils_get_zoom_delta              (GVariant *dictionary);

PhotosZoomEvent  photos_utils_get_zoom_event              (GVariant *dictionary);

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

gdouble          photos_utils_eval_radial_line            (gdouble crop_center_x,
                                                           gdouble crop_center_y,
                                                           gdouble corner_x,
                                                           gdouble corner_y,
                                                           gdouble event_x);

gboolean         photos_utils_file_copy_as_thumbnail      (GFile *source,
                                                           GFile *destination,
                                                           const gchar *original_uri,
                                                           gint64 original_height,
                                                           gint64 original_width,
                                                           GCancellable *cancellable,
                                                           GError **error);

GFileInfo       *photos_utils_file_query_info             (GFile *file,
                                                           const gchar *attributes,
                                                           GFileQueryInfoFlags flags,
                                                           GCancellable *cancellable,
                                                           GError **error);

void             photos_utils_file_query_info_async       (GFile *file,
                                                           const gchar *attributes,
                                                           GFileQueryInfoFlags flags,
                                                           gint io_priority,
                                                           GCancellable *cancellable,
                                                           GAsyncReadyCallback callback,
                                                           gpointer user_data);

GFileInfo       *photos_utils_file_query_info_finish      (GFile *file, GAsyncResult *res, GError **error);

void             photos_utils_get_controller              (PhotosWindowMode mode,
                                                           PhotosOffsetController **out_offset_cntrlr,
                                                           PhotosTrackerController **out_trk_cntrlr);

gchar           *photos_utils_get_extension_from_mime_type (const gchar *mime_type);


gint             photos_utils_get_icon_size               (void);

gint             photos_utils_get_icon_size_unscaled      (void);

char*            photos_utils_get_pixbuf_common_suffix    (GdkPixbufFormat *format);

const gchar     *photos_utils_get_provider_name           (PhotosBaseManager *src_mngr, PhotosBaseItem *item);

gboolean         photos_utils_get_selection_mode          (void);

gchar           *photos_utils_get_thumbnail_path_for_file (GFile *file);

gchar           *photos_utils_get_thumbnail_path_for_uri  (const gchar *uri);

GList           *photos_utils_get_urns_from_items         (GList *items);

const gchar     *photos_utils_get_version                 (void);

void             photos_utils_list_box_header_func        (GtkListBoxRow *row,
                                                           GtkListBoxRow *before,
                                                           gpointer user_data);

GAppLaunchContext *photos_utils_new_app_launch_context_from_widget (GtkWidget *widget);

void             photos_utils_object_list_free_full       (GList *objects);

gchar           *photos_utils_print_zoom_action_detailed_name (const gchar *action_name,
                                                               gdouble delta,
                                                               PhotosZoomEvent event);

gboolean         photos_utils_scrolled_window_can_scroll  (GtkScrolledWindow *scrolled_window);

void             photos_utils_scrolled_window_scroll      (GtkScrolledWindow *scrolled_window,
                                                           gdouble delta_x,
                                                           gdouble delta_y);

void             photos_utils_set_edited_name             (const gchar *urn, const gchar *title);

void             photos_utils_set_favorite                (const gchar *urn, gboolean is_favorite);

gboolean         photos_utils_set_string                  (gchar **string_ptr, const gchar *new_string);

gboolean         photos_utils_take_string                 (gchar **string_ptr, gchar *new_string);

G_END_DECLS

#endif /* PHOTOS_UTILS_H */
