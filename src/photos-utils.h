/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2012, 2013 Red Hat, Inc.
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
 */

#ifndef PHOTOS_UTILS_H
#define PHOTOS_UTILS_H

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gegl.h>
#include <gio/gio.h>
#include <glib.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

GIcon           *photos_utils_create_collection_icon      (gint base_size, GList *pixbufs);

GdkPixbuf       *photos_utils_create_pixbuf_from_node     (GeglNode *node);

GIcon           *photos_utils_create_symbolic_icon        (const gchar *name, gint base_size);

const gchar     *photos_utils_dot_dir                     (void);

GdkPixbuf       *photos_utils_embed_image_in_frame        (GdkPixbuf *source_image,
                                                           const gchar *frame_image_path,
                                                           GtkBorder *slice_width,
                                                           GtkBorder *border_width);

gchar           *photos_utils_filename_strip_extension    (const gchar *filename_with_extension);

gint             photos_utils_get_icon_size               (void);

char*            photos_utils_get_pixbuf_common_suffix    (GdkPixbufFormat *format);

GdkPixbufFormat* photos_utils_get_pixbuf_format           (GFile *file);

GdkPixbufFormat* photos_utils_get_pixbuf_format_by_suffix (const char *suffix);

GSList*          photos_utils_get_pixbuf_savable_formats  (void);

GtkBorder       *photos_utils_get_thumbnail_frame_border  (void);

GList           *photos_utils_get_urns_from_paths         (GList *paths, GtkTreeModel *model);

void             photos_utils_queue_thumbnail_job_for_file_async (GFile *file,
                                                                  GAsyncReadyCallback callback,
                                                                  gpointer user_data);

gboolean         photos_utils_queue_thumbnail_job_for_file_finish (GAsyncResult *res);

void             photos_utils_set_edited_name             (const gchar *urn, const gchar *title);

void             photos_utils_set_favorite                (const gchar *urn, gboolean is_favorite);

G_END_DECLS

#endif /* PHOTOS_UTILS_H */
