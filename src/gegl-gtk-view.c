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

#include "config.h"

#include <glib-object.h>
#include <gtk/gtk.h>
#include <gegl.h>

#ifdef HAVE_CAIRO_GOBJECT
#include <cairo-gobject.h>
#endif

#include "gegl-gtk-view.h"
#include "gegl-gtk-view-helper.h"
#include "gegl-gtk-marshal.h"

/**
 * SECTION:gegl-gtk-view
 * @short_description: Widget for displaying a #GeglNode
 * @stability: Unstable
 * @include: gegl-gtk.h
 *
 * The view widget displays the output of a node in a GEGL graph.
 * It will tracks changes in the node, and will therefore automatically
 * show the correct content when the GEGL graph is changed.
 *
 * For setting which #GeglNode to display, use gegl_gtk_view_set_node(),
 * or use the gegl_gtk_view_new_for_node() convenience constructor.
 *
 * Transformations:
 *
 * The widget can show a transformed view of the GeglNode. Scaling and
 * transformations are supported, as well as autoscaling.
 * For manual control over the transformation see
 * methods gegl_gtk_view_set_scale(), gegl_gtk_view_set_x() and
 * gegl_gtk_view_set_y(), or use the corresponding properties.
 * For changing the autoscaling behavior, see
 * gegl_gtk_view_set_autoscale_policy()
 * For getting the effective affine transformation applied, use
 * gegl_gtk_view_get_transformation()
 *
 * Examples:
 *
 * In the GEGL-GTK example directories, you can find code examples for
 * how to use #GeglGtkView in files with names starting with gegl-gtk-view
 **/

/*
 * This class is responsible for providing the public interface
 * consumers expect of the view widget, and for rendering onto the widget.
 * Tracking changes in the GeglNode, dealing with model<->view transformations
 * et.c. is delegated to the internal/private class ViewHelper.
 *
 * This separation of concerns keeps the classes small and "stupid", and
 * allows to test a lot of functionality without having to instantiate
 * a widget and rely on the presence and behaviour of a windowing system.
 */

/*
 * TODO: Emit a transformation-changed signal whenever the tranformation changes
 */

G_DEFINE_TYPE(GeglGtkView, gegl_gtk_view, GTK_TYPE_DRAWING_AREA)


enum {
    PROP_0,
    PROP_NODE,
    PROP_X,
    PROP_Y,
    PROP_SCALE,
    PROP_BLOCK,
    PROP_AUTOSCALE_POLICY
};

#ifdef HAVE_CAIRO_GOBJECT
enum {
    SIGNAL_DRAW_BACKGROUND,
    SIGNAL_DRAW_OVERLAY,
    N_SIGNALS
};

static guint gegl_view_signals[N_SIGNALS];
#endif

static ViewHelper *
get_private(GeglGtkView *self)
{
    return VIEW_HELPER(self->priv);
}

#define GET_PRIVATE(self) (get_private(self))


static void      finalize(GObject        *gobject);
static void      set_property(GObject        *gobject,
                              guint           prop_id,
                              const GValue   *value,
                              GParamSpec     *pspec);
static void      get_property(GObject        *gobject,
                              guint           prop_id,
                              GValue         *value,
                              GParamSpec     *pspec);

#ifdef HAVE_GTK2
static gboolean  expose_event(GtkWidget      *widget,
                              GdkEventExpose *event);
#endif
#ifdef HAVE_GTK3
static gboolean  draw(GtkWidget *widget,
                      cairo_t *cr);
#endif


static void
trigger_redraw(ViewHelper *priv, GeglRectangle *rect, GeglGtkView *view);
static void
size_allocate(GtkWidget *widget, GdkRectangle *allocation, gpointer user_data);

static void
view_size_changed(ViewHelper *priv, GeglRectangle *rect, GeglGtkView *view);

static void
gegl_gtk_view_class_init(GeglGtkViewClass *klass)
{
    GObjectClass   *gobject_class = G_OBJECT_CLASS(klass);
    GtkWidgetClass *widget_class  = GTK_WIDGET_CLASS(klass);

    gobject_class->finalize     = finalize;
    gobject_class->set_property = set_property;
    gobject_class->get_property = get_property;

#ifdef HAVE_GTK2
    widget_class->expose_event        = expose_event;
#endif

#ifdef HAVE_GTK3
    widget_class->draw                = draw;
#endif

    g_object_class_install_property(gobject_class, PROP_X,
                                    g_param_spec_float("x",
                                            "X",
                                            "X origin",
                                            -G_MAXFLOAT, G_MAXFLOAT, 0.0,
                                            G_PARAM_READABLE));
    g_object_class_install_property(gobject_class, PROP_Y,
                                    g_param_spec_float("y",
                                            "Y",
                                            "Y origin",
                                            -G_MAXFLOAT, G_MAXFLOAT, 0.0,
                                            G_PARAM_READABLE));
    g_object_class_install_property(gobject_class, PROP_SCALE,
                                    g_param_spec_double("scale",
                                            "Scale",
                                            "Zoom factor",
                                            0.0, 100.0, 1.00,
                                            G_PARAM_CONSTRUCT |
                                            G_PARAM_READWRITE));
    g_object_class_install_property(gobject_class, PROP_NODE,
                                    g_param_spec_object("node",
                                            "Node",
                                            "The node to render",
                                            G_TYPE_OBJECT,
                                            G_PARAM_CONSTRUCT |
                                            G_PARAM_READWRITE));
    g_object_class_install_property(gobject_class, PROP_BLOCK,
                                    g_param_spec_boolean("block",
                                            "Blocking render",
                                            "Make sure all data requested to blit is generated.",
                                            FALSE,
                                            G_PARAM_READWRITE));
    g_object_class_install_property(gobject_class, PROP_AUTOSCALE_POLICY,
                                    g_param_spec_enum("autoscale-policy",
                                            "Autoscale policy", "The autoscaling behavior used",
                                            GEGL_GTK_TYPE_VIEW_AUTOSCALE,
                                            GEGL_GTK_VIEW_AUTOSCALE_CONTENT,
                                            G_PARAM_READWRITE |
                                            G_PARAM_CONSTRUCT));


/* XXX: maybe we should just allow a second GeglNode to be specified for background? */

/**
 * GeglGtkView::draw-background:
 * @widget: the #GeglGtkView widget that emitted the signal
 * @cr: the #CairoContext to render to
 * @rect: the area that was updated, view coordinates
 *
 * Emitted during painting, before the node contents has been rendered.
 * Allows consumers to draw a custom background for the widget.
 *
 * Note:
 * Manipulating the view widget in the signal handler is not supported.
 * This signal is only available if GEGL-GTK was build with Cairo GObject support.
 **/

/**
* GeglGtkView::draw-overlay:
* @widget: the #GeglGtkView widget that emitted the signal
* @cr: the #CairoContext to render to
* @rect: the area that was updated, in view coordinates
*
* Emitted during painting, before the node contents has been rendered.
*
* Allows consumers to draw an overlay for the widget, for instance
* for simple user interface elements.
*
* Note:
* Manipulating the view widget in the signal handler is not supported.
* This signal is only available if GEGL-GTK was build with Cairo GObject support.
**/
#ifdef HAVE_CAIRO_GOBJECT
    gegl_view_signals[SIGNAL_DRAW_BACKGROUND] =
        g_signal_new("draw-background",
                     G_TYPE_FROM_CLASS(klass),
                     0,
                     0,
                     NULL,
                     NULL,
                     gegl_gtk_marshal_VOID__BOXED_BOXED,
                     G_TYPE_NONE, 2, CAIRO_GOBJECT_TYPE_CONTEXT, GDK_TYPE_RECTANGLE);

    gegl_view_signals[SIGNAL_DRAW_OVERLAY] =
        g_signal_new("draw-overlay",
                     G_TYPE_FROM_CLASS(klass),
                     0,
                     0,
                     NULL,
                     NULL,
                     gegl_gtk_marshal_VOID__BOXED_BOXED,
                     G_TYPE_NONE, 2, CAIRO_GOBJECT_TYPE_CONTEXT, GDK_TYPE_RECTANGLE);
#endif

}

static void
gegl_gtk_view_init(GeglGtkView *self)
{
    self->priv = (GeglGtkViewPrivate *)view_helper_new();

    g_signal_connect(self->priv, "redraw-needed", G_CALLBACK(trigger_redraw), (gpointer)self);
    g_signal_connect(self->priv, "size-changed", G_CALLBACK(view_size_changed), (gpointer)self);

    g_signal_connect(self, "size-allocate", G_CALLBACK(size_allocate), NULL);
}

static void
finalize(GObject *gobject)
{
    GeglGtkView *self = GEGL_GTK_VIEW(gobject);

    g_object_unref(G_OBJECT(self->priv));

    G_OBJECT_CLASS(gegl_gtk_view_parent_class)->finalize(gobject);
}

static void
set_property(GObject      *gobject,
             guint         property_id,
             const GValue *value,
             GParamSpec   *pspec)
{
    GeglGtkView *self = GEGL_GTK_VIEW(gobject);
    ViewHelper *priv = GET_PRIVATE(self);

    switch (property_id) {
    case PROP_NODE:
        gegl_gtk_view_set_node(self, GEGL_NODE(g_value_get_object(value)));
        break;
    case PROP_BLOCK:
        priv->block = g_value_get_boolean(value);
        break;
    case PROP_SCALE:
        gegl_gtk_view_set_scale(self, g_value_get_double(value));
        break;
    case PROP_AUTOSCALE_POLICY:
        gegl_gtk_view_set_autoscale_policy(self, g_value_get_enum(value));
        break;
    default:

        G_OBJECT_WARN_INVALID_PROPERTY_ID(gobject, property_id, pspec);
        break;
    }
}

static void
get_property(GObject      *gobject,
             guint         property_id,
             GValue       *value,
             GParamSpec   *pspec)
{
    GeglGtkView *self = GEGL_GTK_VIEW(gobject);
    ViewHelper *priv = GET_PRIVATE(self);

    switch (property_id) {
    case PROP_NODE:
        g_value_set_object(value, gegl_gtk_view_get_node(self));
        break;
    case PROP_X:
        g_value_set_float(value, gegl_gtk_view_get_x(self));
        break;
    case PROP_BLOCK:
        g_value_set_boolean(value, priv->block);
        break;
    case PROP_Y:
        g_value_set_float(value, gegl_gtk_view_get_y(self));
        break;
    case PROP_SCALE:
        g_value_set_double(value, gegl_gtk_view_get_scale(self));
        break;
    case PROP_AUTOSCALE_POLICY:
        g_value_set_enum(value, gegl_gtk_view_get_autoscale_policy(self));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(gobject, property_id, pspec);
        break;
    }
}

/* Trigger a redraw */
static void
trigger_redraw(ViewHelper *priv,
               GeglRectangle *rect,
               GeglGtkView *view)
{
    if (rect->width < 0 || rect->height < 0)
        gtk_widget_queue_draw(GTK_WIDGET(view));
    else
        gtk_widget_queue_draw_area(GTK_WIDGET(view),
                                   rect->x, rect->y, rect->width, rect->height);
}

/* Bounding box of the node view changed */
static void
view_size_changed(ViewHelper *priv, GeglRectangle *rect, GeglGtkView *view)
{
    /* Resize the widget to fit the entire view bounding box
     * TODO: implement a policy for this
     * consumers should be able to have the view not autoscale at all
     * or to have it autoscale the content to fit the size of widget */
    gtk_widget_set_size_request(GTK_WIDGET(view), rect->width, rect->height);
}

static void
size_allocate(GtkWidget *widget, GdkRectangle *allocation, gpointer user_data)
{
    GeglGtkView *self = GEGL_GTK_VIEW(widget);
    gint scale_factor;

    scale_factor = gtk_widget_get_scale_factor (widget);
    view_helper_set_allocation(GET_PRIVATE(self), allocation, scale_factor);
}

static void
draw_implementation(GeglGtkView *self, cairo_t *cr, GdkRectangle *rect)
{
    ViewHelper *priv = GET_PRIVATE(self);

#ifdef HAVE_CAIRO_GOBJECT
    /* Draw background */
    cairo_save(cr);
    g_signal_emit(G_OBJECT(self), gegl_view_signals[SIGNAL_DRAW_BACKGROUND],
                  0, cr, rect, NULL);
    cairo_restore(cr);
#endif

    /* Draw the gegl node */
    cairo_save(cr);
    view_helper_draw(priv, cr, rect);
    cairo_restore(cr);

#ifdef HAVE_CAIRO_GOBJECT
    /* Draw overlay */
    cairo_save(cr);
    g_signal_emit(G_OBJECT(self), gegl_view_signals[SIGNAL_DRAW_OVERLAY],
                  0, cr, rect, NULL);
    cairo_restore(cr);
#endif
}

#ifdef HAVE_GTK3
static gboolean
draw(GtkWidget *widget, cairo_t *cr)
{
    GeglGtkView *self = GEGL_GTK_VIEW(widget);
    ViewHelper *priv = GET_PRIVATE(self);
    GdkRectangle rect;

    if (!priv->node)
        return FALSE;

    gdk_cairo_get_clip_rectangle(cr, &rect);

    draw_implementation(self, cr, &rect);

    return FALSE;
}
#endif

#ifdef HAVE_GTK2
static gboolean
expose_event(GtkWidget      *widget,
             GdkEventExpose *event)
{
    GeglGtkView *self = GEGL_GTK_VIEW(widget);
    ViewHelper *priv = GET_PRIVATE(self);
    cairo_t      *cr;
    GdkRectangle rect;

    if (!priv->node)
        return FALSE;

    cr = gdk_cairo_create(widget->window);
    gdk_cairo_region(cr, event->region);
    cairo_clip(cr);
    gdk_region_get_clipbox(event->region, &rect);

    draw_implementation(self, cr, &rect);

    cairo_destroy(cr);

    return FALSE;
}
#endif


/**
 * gegl_gtk_view_new:
 *
 * Create a new #GeglGtkView
 *
 * Returns: New #GeglGtkView
 **/
GeglGtkView *
gegl_gtk_view_new(void)
{
    return GEGL_GTK_VIEW(g_object_new(GEGL_GTK_TYPE_VIEW, NULL));
}

/**
 * gegl_gtk_view_new_for_node:
 * @node: The #GeglNode to display
 *
 * Create a new #GeglGtkView for a given #GeglNode
 *
 * Returns: New #GeglGtkView displaying @node
 **/
GeglGtkView *
gegl_gtk_view_new_for_node(GeglNode *node)
{
    GeglGtkView *view = gegl_gtk_view_new();
    gegl_gtk_view_set_node(view, node);
    return view;
}

/**
 * gegl_gtk_view_set_node:
 * @self: A #GeglGtkView
 * @node: (transfer full)(allow-none): a #GeglNode instance or %NULL
 *
 * Change the #GeglNode to display
 **/
void
gegl_gtk_view_set_node(GeglGtkView *self, GeglNode *node)
{
    view_helper_set_node(GET_PRIVATE(self), node);
}

/**
 * gegl_gtk_view_get_node:
 * @self: A #GeglGtkView
 * Returns: (transfer none): The #GeglNode this widget displays
 *
 * Get the displayed #GeglNode
 **/
GeglNode *
gegl_gtk_view_get_node(GeglGtkView *self)
{
    return view_helper_get_node(GET_PRIVATE(self));
}

/**
 * gegl_gtk_view_set_scale:
 * @self: A #GeglGtkView
 * @scale:
 *
 * Setter for the :scale property
 **/
void
gegl_gtk_view_set_scale(GeglGtkView *self, float scale)
{
    view_helper_set_scale(GET_PRIVATE(self), scale);
}

/**
 * gegl_gtk_view_get_scale:
 * @self: A #GeglGtkView
 *
 * Getter for the :scale property
 *
 * Returns:
 **/
float
gegl_gtk_view_get_scale(GeglGtkView *self)
{
    return view_helper_get_scale(GET_PRIVATE(self));
}

/**
 * gegl_gtk_view_get_x:
 * @self: A #GeglGtkView
 *
 * Getter for the :x property
 *
 * Returns:
 **/
float
gegl_gtk_view_get_x(GeglGtkView *self)
{
    return view_helper_get_x(GET_PRIVATE(self));
}

/**
 * gegl_gtk_view_get_y:
 * @self: A #GeglGtkView
 *
 * Getter for the :y property
 *
 * Returns:
 **/
float
gegl_gtk_view_get_y(GeglGtkView *self)
{
    return view_helper_get_y(GET_PRIVATE(self));
}

/**
 * gegl_gtk_view_get_transformation:
 * @self: A #GeglGtkView
 * @matrix: (out caller-allocates): Pointer to location for transformation matrix
 *
 * Get the model->view transformation
 *
 * The transformation matrix describes the transformation between the
 * model (the output of the GeglNode) and the view (the display in the widget).
 * To transform coordinates use gegl_matrix3_transform_point().
 * To get a matrix representing the view->model space transformation, use gegl_matrix3_invert()
 **/
void gegl_gtk_view_get_transformation(GeglGtkView *self, GeglMatrix3 *matrix)
{
    view_helper_get_transformation(GET_PRIVATE(self), matrix);
}

/**
 * gegl_gtk_view_set_autoscale_policy:
 * @self: A #GeglGtkView
 * @autoscale: #GeglGtkViewAutoscale policy to use
 *
 * Set the autoscaling policy
 **/
void
gegl_gtk_view_set_autoscale_policy(GeglGtkView *self, GeglGtkViewAutoscale autoscale)
{
    view_helper_set_autoscale_policy(GET_PRIVATE(self), autoscale);
}

/**
 * gegl_gtk_view_get_autoscale_policy:
 * @self: A #GeglGtkView
 *
 * Get the autoscaling policy
 *
 * Returns: Current #GeglGtkViewAutoscale policy in use
 **/
GeglGtkViewAutoscale
gegl_gtk_view_get_autoscale_policy(GeglGtkView *self)
{
    return view_helper_get_autoscale_policy(GET_PRIVATE(self));
}
