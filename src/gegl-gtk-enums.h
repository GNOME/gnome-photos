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
 * Copyright (C) 2011 Jon Nordby <jononor@gmail.com>
 */

#ifndef __GEGL_GTK_ENUMS_H__
#define __GEGL_GTK_ENUMS_H__

G_BEGIN_DECLS

/**
 * GeglGtkViewAutoscale:
 * @GEGL_GTK_VIEW_AUTOSCALE_DISABLED: Do not autoscale
 * @GEGL_GTK_VIEW_AUTOSCALE_WIDGET: Automatically scale the widget size
 * @GEGL_GTK_VIEW_AUTOSCALE_CONTENT: Automatically scale content to fit widget
 *
 * Specifies the autoscaling behavior of #GeglGtkView.
 **/
typedef enum {
    GEGL_GTK_VIEW_AUTOSCALE_DISABLED = 0,
    GEGL_GTK_VIEW_AUTOSCALE_WIDGET,
    GEGL_GTK_VIEW_AUTOSCALE_CONTENT
} GeglGtkViewAutoscale;

GType gegl_gtk_view_autoscale_get_type(void) G_GNUC_CONST;
#define GEGL_GTK_TYPE_VIEW_AUTOSCALE (gegl_gtk_view_autoscale_get_type())

G_END_DECLS

#endif /* __GEGL_GTK_ENUMS_H__ */
