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

#ifndef __GEGL_GTK_VIEW_H__
#define __GEGL_GTK_VIEW_H__

#include <gtk/gtk.h>
#include <gegl.h>

#include "gegl-gtk-enums.h"

G_BEGIN_DECLS

#define GEGL_GTK_TYPE_VIEW            (gegl_gtk_view_get_type ())
#define GEGL_GTK_VIEW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GEGL_GTK_TYPE_VIEW, GeglGtkView))
#define GEGL_GTK_VIEW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GEGL_GTK_TYPE_VIEW, GeglGtkViewClass))
#define GEGL_GTK_IS_VIEW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GEGL_GTK_TYPE_VIEW))
#define GEGL_GTK_IS_VIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GEGL_GTK_TYPE_VIEW))
#define GEGL_GTK_VIEW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GEGL_GTK_TYPE_VIEW, GeglGtkViewClass))

typedef struct _GeglGtkView        GeglGtkView;
typedef struct _GeglGtkViewClass   GeglGtkViewClass;

typedef struct _ViewHelper GeglGtkViewPrivate;

struct _GeglGtkView {
    /*< private >*/
    GtkDrawingArea parent_instance;
    GeglGtkViewPrivate *priv; /* Can't use the GType private mechanism for GObjects */
};

struct _GeglGtkViewClass {
    /*< private >*/
    GtkDrawingAreaClass parent_class;
};

GType           gegl_gtk_view_get_type(void) G_GNUC_CONST;


GeglGtkView *gegl_gtk_view_new(void);
GeglGtkView *gegl_gtk_view_new_for_node(GeglNode *node);
GeglGtkView *gegl_gtk_view_new_for_buffer(GeglBuffer *buffer);

void gegl_gtk_view_set_node(GeglGtkView *self, GeglNode *node);
GeglNode *gegl_gtk_view_get_node(GeglGtkView *self);

void gegl_gtk_view_set_scale(GeglGtkView *self, float scale);
float gegl_gtk_view_get_scale(GeglGtkView *self);

void gegl_gtk_view_set_x(GeglGtkView *self, float x);
float gegl_gtk_view_get_x(GeglGtkView *self);

void gegl_gtk_view_set_y(GeglGtkView *self, float y);
float gegl_gtk_view_get_y(GeglGtkView *self);

void gegl_gtk_view_get_transformation(GeglGtkView *self, GeglMatrix3 *matrix);

void gegl_gtk_view_set_autoscale_policy(GeglGtkView *self, GeglGtkViewAutoscale autoscale);
GeglGtkViewAutoscale gegl_gtk_view_get_autoscale_policy(GeglGtkView *self);

G_END_DECLS

#endif /* __GEGL_GTK_VIEW_H__ */
