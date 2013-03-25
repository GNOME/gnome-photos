/* This file is part of GEGL-GTK
 *
 * GEGL-GTK is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * GEGL-GTK is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with GEGL-GTK; if not, see <http://www.gnu.org/licenses/>.
 *
 * Copyright (C) 2003, 2004, 2006 Øyvind Kolås <pippin@gimp.org>
 * Copyright (C) 2011 Jon Nordby <jononor@gmail.com>
 */

#ifndef __VIEW_HELPER_H__
#define __VIEW_HELPER_H__

#include <glib-object.h>
#include <gegl.h>
#include <gtk/gtk.h>

#include <gegl-gtk-enums.h>

G_BEGIN_DECLS

#define VIEW_HELPER_TYPE            (view_helper_get_type ())
#define VIEW_HELPER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), VIEW_HELPER_TYPE, ViewHelper))
#define VIEW_HELPER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  VIEW_HELPER_TYPE, ViewHelperClass))
#define IS_VIEW_HELPER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), VIEW_HELPER_TYPE))
#define IS_VIEW_HELPER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  VIEW_HELPER_TYPE))
#define VIEW_HELPER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  VIEW_HELPER_TYPE, ViewHelperClass))

typedef struct _ViewHelper        ViewHelper;
typedef struct _ViewHelperClass   ViewHelperClass;

struct _ViewHelper {
    GObject parent_instance;

    GeglNode      *node;
    gfloat         x;
    gfloat         y;
    gdouble        scale;
    gboolean       block;    /* blocking render */
    GeglGtkViewAutoscale autoscale_policy;

    guint          monitor_id;
    GeglProcessor *processor;
    GQueue        *processing_queue; /* Queue of rectangles that needs to be processed */
    GeglRectangle *currently_processed_rect;

    GdkRectangle   widget_allocation; /* The allocated size of the widget */
};

struct _ViewHelperClass {
    GObjectClass parent_class;
};


GType view_helper_get_type(void) G_GNUC_CONST;

ViewHelper *view_helper_new(void);

void view_helper_draw(ViewHelper *self, cairo_t *cr, GdkRectangle *rect);
void view_helper_set_allocation(ViewHelper *self, GdkRectangle *allocation);

void view_helper_set_node(ViewHelper *self, GeglNode *node);
GeglNode *view_helper_get_node(ViewHelper *self);

void view_helper_set_scale(ViewHelper *self, float scale);
float view_helper_get_scale(ViewHelper *self);

void view_helper_set_x(ViewHelper *self, float x);
float view_helper_get_x(ViewHelper *self);

void view_helper_set_y(ViewHelper *self, float y);
float view_helper_get_y(ViewHelper *self);

void view_helper_get_transformation(ViewHelper *self, GeglMatrix3 *matrix);

void view_helper_set_autoscale_policy(ViewHelper *self, GeglGtkViewAutoscale autoscale);
GeglGtkViewAutoscale view_helper_get_autoscale_policy(ViewHelper *self);

G_END_DECLS

#endif /* __VIEW_HELPER_H__ */
