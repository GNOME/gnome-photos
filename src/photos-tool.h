/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2015 – 2021 Red Hat, Inc.
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

#ifndef PHOTOS_TOOL_H
#define PHOTOS_TOOL_H

#include <cairo.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>

#include "photos-base-item.h"
#include "photos-image-view.h"

G_BEGIN_DECLS

#define PHOTOS_TYPE_TOOL (photos_tool_get_type ())
G_DECLARE_DERIVABLE_TYPE (PhotosTool, photos_tool, PHOTOS, TOOL, GObject);

struct _PhotosToolClass
{
  GObjectClass parent_class;

  const gchar *icon_name;
  const gchar *name;

  /* virtual methods */
  void          (*activate)                   (PhotosTool *self, PhotosBaseItem *item, PhotosImageView *view);
  void          (*deactivate)                 (PhotosTool *self);
  void          (*draw)                       (PhotosTool *self, cairo_t *cr, GdkRectangle *rect);
  GtkWidget    *(*get_widget)                 (PhotosTool *self);
  gboolean      (*left_click_event)           (PhotosTool *self, GdkEventButton *event);
  gboolean      (*left_unclick_event)         (PhotosTool *self, GdkEventButton *event);
  gboolean      (*motion_event)               (PhotosTool *self, GdkEventMotion *event);

  /* signals */
  void          (*activated)                  (PhotosTool *self);
  void          (*hide_requested)             (PhotosTool *self);
};

void                photos_tool_activate                (PhotosTool *self,
                                                         PhotosBaseItem *item,
                                                         PhotosImageView *view);

void                photos_tool_deactivate              (PhotosTool *self);

void                photos_tool_draw                    (PhotosTool *self, cairo_t *cr, GdkRectangle *rect);

const gchar        *photos_tool_get_icon_name           (PhotosTool *self);

const gchar        *photos_tool_get_name                (PhotosTool *self);

GtkWidget          *photos_tool_get_widget              (PhotosTool *self);

gboolean            photos_tool_left_click_event        (PhotosTool *self, GdkEventButton *event);

gboolean            photos_tool_left_unclick_event      (PhotosTool *self, GdkEventButton *event);

gboolean            photos_tool_motion_event            (PhotosTool *self, GdkEventMotion *event);

G_END_DECLS

#endif /* PHOTOS_TOOL_H */
