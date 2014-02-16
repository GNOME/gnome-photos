/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2012, 2013, 2014 Red Hat, Inc.
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
#include <tracker-sparql.h>

G_BEGIN_DECLS

#define PHOTOS_ERROR (photos_utils_error_quark ())
#define PHOTOS_FLASH_OFF (photos_utils_flash_off_quark ())
#define PHOTOS_FLASH_ON (photos_utils_flash_on_quark ())

#define PHOTOS_COLLECTION_SCREENSHOT \
  "http://www.semanticdesktop.org/ontologies/2007/03/22/nfo#image-category-screenshot"

GIcon           *photos_utils_create_collection_icon      (gint base_size, GList *pixbufs);

GdkPixbuf       *photos_utils_create_pixbuf_from_node     (GeglNode *node);

GIcon           *photos_utils_create_symbolic_icon        (const gchar *name, gint base_size);

gboolean         photos_utils_create_thumbnail            (GFile *file, GCancellable *cancellable, GError **error);

GIcon           *photos_utils_get_icon_from_cursor        (TrackerSparqlCursor *cursor);

const gchar     *photos_utils_dot_dir                     (void);

GQuark           photos_utils_error_quark                 (void);

gchar           *photos_utils_filename_strip_extension    (const gchar *filename_with_extension);

GQuark           photos_utils_flash_off_quark             (void);

GQuark           photos_utils_flash_on_quark              (void);

gint             photos_utils_get_icon_size               (void);

char*            photos_utils_get_pixbuf_common_suffix    (GdkPixbufFormat *format);

GdkPixbufFormat* photos_utils_get_pixbuf_format           (GFile *file);

GdkPixbufFormat* photos_utils_get_pixbuf_format_by_suffix (const char *suffix);

GSList*          photos_utils_get_pixbuf_savable_formats  (void);

GtkBorder       *photos_utils_get_thumbnail_frame_border  (void);

GList           *photos_utils_get_urns_from_paths         (GList *paths, GtkTreeModel *model);

GIcon           *photos_utils_icon_from_rdf_type          (const gchar *type);

gboolean         photos_utils_queue_thumbnail_job_for_file_finish (GAsyncResult *res);

void             photos_utils_set_edited_name             (const gchar *urn, const gchar *title);

void             photos_utils_set_favorite                (const gchar *urn, gboolean is_favorite);

G_END_DECLS

#endif /* PHOTOS_UTILS_H */
