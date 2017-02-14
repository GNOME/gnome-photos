/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2013 – 2017 Red Hat, Inc.
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

#ifndef PHOTOS_GEGL_H
#define PHOTOS_GEGL_H

#include <babl/babl.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gegl.h>
#include <gio/gio.h>
#include <glib.h>

G_BEGIN_DECLS

void             photos_gegl_buffer_zoom_async            (GeglBuffer *buffer,
                                                           gdouble zoom,
                                                           GCancellable *cancellable,
                                                           GAsyncReadyCallback callback,
                                                           gpointer user_data);

GeglBuffer      *photos_gegl_buffer_zoom_finish           (GeglBuffer *buffer, GAsyncResult *res, GError **error);

GeglNode        *photos_gegl_create_orientation_node      (GeglNode *parent, GQuark orientation);

GdkPixbuf       *photos_gegl_create_pixbuf_from_node      (GeglNode *node);

GeglBuffer      *photos_gegl_dup_buffer_from_node         (GeglNode *node, const Babl *format);

void             photos_gegl_ensure_builtins              (void);

GeglBuffer      *photos_gegl_get_buffer_from_node         (GeglNode *node, const Babl *format);

void             photos_gegl_processor_process_async      (GeglProcessor *processor,
                                                           GCancellable *cancellable,
                                                           GAsyncReadyCallback callback,
                                                           gpointer user_data);

gboolean         photos_gegl_processor_process_finish     (GeglProcessor *processor,
                                                           GAsyncResult *res,
                                                           GError **error);

void             photos_gegl_remove_children_from_node    (GeglNode *node);

G_END_DECLS

#endif /* PHOTOS_GEGL_H */
