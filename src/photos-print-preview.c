/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2006, 2007, 2008 The Free Software Foundation
 * Copyright © 2013, 2015 Red Hat, Inc.
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
 *   + Eye of GNOME
 */

#include <gtk/gtk.h>
#include <cairo.h>
#include <gdk/gdkkeysyms.h>

#include "photos-print-preview.h"


struct _PhotosPrintPreviewPrivate {
	GtkWidget *area;
	GdkPixbuf *pixbuf;
	GdkPixbuf *pixbuf_scaled;

	/* The surface to set to the cairo context, created from the image */
	cairo_surface_t *surface;

        /* Flag whether we have to create surface */
	gboolean flag_create_surface;

	/* the alignment of the pixbuf in the page */
	gfloat pixbuf_x_align, pixbuf_y_align;

	/* real paper size, in inches */
	gfloat p_width, p_height;

	/* page margins, in inches */
	gfloat l_margin, r_margin, t_margin, b_margin;

	/* page margins, relatives to the widget size */
	gint l_rmargin, r_rmargin, t_rmargin, b_rmargin;

	/* pixbuf width, relative to the widget size */
	gint r_width, r_height;

	/* scale of the pixbuf, as defined by the user */
	gfloat i_scale;

	/* scale of the page, relative to the widget size */
	gfloat p_scale;

	/* whether we are currently grabbing the pixbuf */
	gboolean grabbed;

	/* the last cursor position */
	gdouble cursorx, cursory;

	/* if we reject to move the pixbuf,
	   store the delta here */
	gdouble r_dx, r_dy;
};


G_DEFINE_TYPE_WITH_PRIVATE (PhotosPrintPreview, photos_print_preview, GTK_TYPE_ASPECT_FRAME);


/* Signal IDs */
enum {
	SIGNAL_PIXBUF_MOVED,
	SIGNAL_PIXBUF_SCALED,
	SIGNAL_LAST
};
static gint preview_signals [SIGNAL_LAST];

enum {
	PROP_0,
	PROP_PIXBUF,
	PROP_PIXBUF_X_ALIGN,
	PROP_PIXBUF_Y_ALIGN,
	PROP_PIXBUF_SCALE,
	PROP_PAPER_WIDTH,
	PROP_PAPER_HEIGHT,
	PROP_PAGE_LEFT_MARGIN,
	PROP_PAGE_RIGHT_MARGIN,
	PROP_PAGE_TOP_MARGIN,
	PROP_PAGE_BOTTOM_MARGIN
};

static void photos_print_preview_draw (PhotosPrintPreview *preview, cairo_t *cr);
static void photos_print_preview_finalize (GObject *object);
static void update_relative_sizes (PhotosPrintPreview *preview);
static void create_surface (PhotosPrintPreview *preview);
static void create_image_scaled (PhotosPrintPreview *preview);
static gboolean create_surface_when_idle (PhotosPrintPreview *preview);

static void
photos_print_preview_get_property (GObject    *object,
				guint       prop_id,
				GValue     *value,
				GParamSpec *pspec)
{
	PhotosPrintPreviewPrivate *priv = PHOTOS_PRINT_PREVIEW (object)->priv;

	switch (prop_id) {
	case PROP_PIXBUF:
		g_value_set_object (value, priv->pixbuf);
		break;
	case PROP_PIXBUF_X_ALIGN:
		g_value_set_float (value, priv->pixbuf_x_align);
		break;
	case PROP_PIXBUF_Y_ALIGN:
		g_value_set_float (value, priv->pixbuf_y_align);
		break;
	case PROP_PIXBUF_SCALE:
		g_value_set_float (value, priv->i_scale);
		break;
	case PROP_PAPER_WIDTH:
		g_value_set_float (value, priv->p_width);
		break;
	case PROP_PAPER_HEIGHT:
		g_value_set_float (value, priv->p_height);
		break;
	case PROP_PAGE_LEFT_MARGIN:
		g_value_set_float (value, priv->l_margin);
		break;
	case PROP_PAGE_RIGHT_MARGIN:
		g_value_set_float (value, priv->r_margin);
		break;
	case PROP_PAGE_TOP_MARGIN:
		g_value_set_float (value, priv->t_margin);
		break;
	case PROP_PAGE_BOTTOM_MARGIN:
		g_value_set_float (value, priv->b_margin);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
photos_print_preview_set_property (GObject      *object,
				guint         prop_id,
				const GValue *value,
				GParamSpec   *pspec)
{
	PhotosPrintPreviewPrivate *priv = PHOTOS_PRINT_PREVIEW (object)->priv;
	gboolean paper_size_changed = FALSE;

	switch (prop_id) {
	case PROP_PIXBUF:
		if (priv->pixbuf) {
			g_object_unref (priv->pixbuf);
		}
		priv->pixbuf = GDK_PIXBUF (g_value_dup_object (value));

		if (priv->pixbuf_scaled) {
			g_object_unref (priv->pixbuf_scaled);
			priv->pixbuf_scaled = NULL;
		}

		priv->flag_create_surface = TRUE;
		break;
	case PROP_PIXBUF_X_ALIGN:
		priv->pixbuf_x_align = g_value_get_float (value);
		break;
	case PROP_PIXBUF_Y_ALIGN:
		priv->pixbuf_y_align = g_value_get_float (value);
		break;
	case PROP_PIXBUF_SCALE:
		priv->i_scale = g_value_get_float (value);
		priv->flag_create_surface = TRUE;
		break;
	case PROP_PAPER_WIDTH:
		priv->p_width = g_value_get_float (value);
		paper_size_changed = TRUE;
		break;
	case PROP_PAPER_HEIGHT:
		priv->p_height = g_value_get_float (value);
		paper_size_changed = TRUE;
		break;
	case PROP_PAGE_LEFT_MARGIN:
		priv->l_margin = g_value_get_float (value);
		break;
	case PROP_PAGE_RIGHT_MARGIN:
		priv->r_margin = g_value_get_float (value);
		break;
	case PROP_PAGE_TOP_MARGIN:
		priv->t_margin = g_value_get_float (value);
		break;
	case PROP_PAGE_BOTTOM_MARGIN:
		priv->b_margin = g_value_get_float (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}

	if (paper_size_changed) {
		g_object_set (object,
			      "ratio", priv->p_width/priv->p_height,
			      NULL);
	}

	update_relative_sizes (PHOTOS_PRINT_PREVIEW (object));
	gtk_widget_queue_draw (priv->area);
}

static void
photos_print_preview_class_init (PhotosPrintPreviewClass *klass)
{
	GObjectClass *gobject_class;

	gobject_class = (GObjectClass*) klass;

	gobject_class->get_property = photos_print_preview_get_property;
	gobject_class->set_property = photos_print_preview_set_property;
	gobject_class->finalize     = photos_print_preview_finalize;

/**
 * PhotosPrintPreview:image:
 *
 * The "image" property defines the image that is previewed
 * in the widget.
 */
	g_object_class_install_property (gobject_class,
					 PROP_PIXBUF,
					 g_param_spec_object ("pixbuf",
							      "GdkPixbuf object",
							      "",
							      GDK_TYPE_PIXBUF,
							      G_PARAM_READWRITE));

/**
 * PhotosPrintPreview:pixbuf-x-align:
 *
 * The "pixbuf-x-align" property defines the horizontal alignment
 * of the image in the widget.
 */
	g_object_class_install_property (gobject_class,
					 PROP_PIXBUF_X_ALIGN,
					 g_param_spec_float ("pixbuf-x-align",
							      "Horizontal alignment for the image",
							      "",
							      0,
							      1,
							      0.5,
							      G_PARAM_READWRITE));

/**
 * PhotosPrintPreview:pixbuf-y-align:
 *
 * The "pixbuf-y-align" property defines the horizontal alignment
 * of the image in the widget.
 */
	g_object_class_install_property (gobject_class,
					 PROP_PIXBUF_Y_ALIGN,
					 g_param_spec_float ("pixbuf-y-align",
							      "Vertical alignment for the image",
							      "",
							      0,
							      1,
							      0.5,
							      G_PARAM_READWRITE));

/**
 * PhotosPrintPreview:pixbuf-scale:
 *
 * The "pixbuf-scale" property defines the scaling of the image
 * that the user wants for the printing.
 */
	g_object_class_install_property (gobject_class,
					 PROP_PIXBUF_SCALE,
					 g_param_spec_float ("pixbuf-scale",
							     "The scale for the image",
							      "",
							      0,
							      1,
							      1,
							      G_PARAM_READWRITE));

/**
 * PhotosPrintPreview:paper-width:
 *
 * The width of the previewed paper, in inches.
 */
	g_object_class_install_property (gobject_class,
					 PROP_PAPER_WIDTH,
					 g_param_spec_float ("paper-width",
							     "Real paper width in inches",
							     "",
							     0,
							     100,
							     8.5,
							     G_PARAM_READWRITE));

/**
 * PhotosPrintPreview:paper-height:
 *
 * The height of the previewed paper, in inches.
 */
	g_object_class_install_property (gobject_class,
					 PROP_PAPER_HEIGHT,
					 g_param_spec_float ("paper-height",
							     "Real paper height in inches",
							      "",
							      0,
							      200,
							      11,
							      G_PARAM_READWRITE));

/**
 * PhotosPrintPreview:page-left-margin:
 *
 * The size of the page's left margin, in inches.
 */
	g_object_class_install_property (gobject_class,
					 PROP_PAGE_LEFT_MARGIN,
					 g_param_spec_float ("page-left-margin",
							     "Left margin of the page in inches",
							     "",
							     0,
							     100,
							     0.25,
							     G_PARAM_READWRITE));

/**
 * PhotosPrintPreview:page-right-margin:
 *
 * The size of the page's right margin, in inches.
 */
	g_object_class_install_property (gobject_class,
					 PROP_PAGE_RIGHT_MARGIN,
					 g_param_spec_float ("page-right-margin",
							     "Right margin of the page in inches",
							      "",
							      0,
							      200,
							      0.25,
							      G_PARAM_READWRITE));
/**
 * PhotosPrintPreview:page-top-margin:
 *
 * The size of the page's top margin, in inches.
 */
	g_object_class_install_property (gobject_class,
					 PROP_PAGE_TOP_MARGIN,
					 g_param_spec_float ("page-top-margin",
							     "Top margin of the page in inches",
							     "",
							      0,
							      100,
							      0.25,
							      G_PARAM_READWRITE));

/**
 * PhotosPrintPreview:page-bottom-margin:
 *
 * The size of the page's bottom margin, in inches.
 */
	g_object_class_install_property (gobject_class,
					 PROP_PAGE_BOTTOM_MARGIN,
					 g_param_spec_float ("page-bottom-margin",
							     "Bottom margin of the page in inches",
							      "",
							      0,
							      200,
							      0.56,
							      G_PARAM_READWRITE));

/**
 * PhotosPrintPreview::pixbuf-moved:
 * @preview: the object which received the signal
 *
 * The #PhotosPrintPreview::pixbuf-moved signal is emitted when the position
 * of the image is changed.
 */
	preview_signals [SIGNAL_PIXBUF_MOVED] =
		g_signal_new ("pixbuf_moved",
			      G_TYPE_FROM_CLASS (gobject_class),
			      G_SIGNAL_RUN_FIRST, 0, NULL, NULL,
			      g_cclosure_marshal_VOID__VOID, G_TYPE_NONE,
			      0, NULL);

/**
 * PhotosPrintPreview::pixbuf-scaled:
 * @preview: the object which received the signal
 *
 * The ::pixbuf-scaled signal is emmited when the scale of the image is changed.
 */
	preview_signals [SIGNAL_PIXBUF_SCALED] =
		g_signal_new ("pixbuf_scaled",
			      G_TYPE_FROM_CLASS (gobject_class),
			      G_SIGNAL_RUN_FIRST, 0, NULL, NULL,
			      g_cclosure_marshal_VOID__VOID, G_TYPE_NONE,
			      0, NULL);
}

static void
photos_print_preview_finalize (GObject *object)
{
	PhotosPrintPreviewPrivate *priv;

	priv = PHOTOS_PRINT_PREVIEW (object)->priv;

	if (priv->pixbuf) {
		g_object_unref (priv->pixbuf);
		priv->pixbuf = NULL;
	}

	if (priv->pixbuf_scaled) {
		g_object_unref (priv->pixbuf_scaled);
		priv->pixbuf_scaled = NULL;
	}

	if (priv->surface) {
		cairo_surface_destroy (priv->surface);
		priv->surface = NULL;
	}

	G_OBJECT_CLASS (photos_print_preview_parent_class)->finalize (object);
}

static void
photos_print_preview_init (PhotosPrintPreview *preview)
{
	PhotosPrintPreviewPrivate *priv;
	gfloat ratio;

	preview->priv = photos_print_preview_get_instance_private (preview);
	priv = preview->priv;

	priv->area = GTK_WIDGET (gtk_drawing_area_new ());

	gtk_container_add (GTK_CONTAINER (preview), priv->area);

	priv->p_width  =  8.5;
	priv->p_height = 11.0;

	ratio = priv->p_width/priv->p_height;

	gtk_aspect_frame_set (GTK_ASPECT_FRAME (preview),
			      0.5, 0.5, ratio, FALSE);

	priv->pixbuf = NULL;
	priv->pixbuf_scaled = NULL;
	priv->pixbuf_x_align = 0.5;
	priv->pixbuf_y_align = 0.5;
	priv->i_scale = 1;

	priv->surface = NULL;
	priv->flag_create_surface = TRUE;

	priv->p_scale = 0;

	priv->l_margin = 0.25;
	priv->r_margin = 0.25;
	priv->t_margin = 0.25;
	priv->b_margin = 0.56;

	priv->grabbed = FALSE;
	priv->cursorx = 0;
	priv->cursory = 0;
	priv->r_dx    = 0;
	priv->r_dy    = 0;
}

static gboolean button_press_event_cb   (GtkWidget *widget, GdkEventButton *bev, gpointer user_data);
static gboolean button_release_event_cb (GtkWidget *widget, GdkEventButton *bev, gpointer user_data);
static gboolean motion_notify_event_cb  (GtkWidget *widget, GdkEventMotion *mev, gpointer user_data);
static gboolean key_press_event_cb      (GtkWidget *widget, GdkEventKey *event, gpointer user_data);

static gboolean draw_cb (GtkDrawingArea *drawing_area, cairo_t *cr, gpointer  user_data);
static void size_allocate_cb (GtkWidget *widget, GtkAllocation *allocation, gpointer user_data);

/**
 * photos_print_preview_new_with_pixbuf:
 * @pixbuf: a #GdkPixbuf
 *
 * Creates a new #PhotosPrintPreview widget, and sets the #GdkPixbuf to preview
 * on it.
 *
 * Returns: A new #PhotosPrintPreview widget.
 **/
GtkWidget *
photos_print_preview_new_with_pixbuf (GdkPixbuf *pixbuf)
{
	PhotosPrintPreview *preview;

	g_return_val_if_fail (GDK_IS_PIXBUF (pixbuf), NULL);

	preview = PHOTOS_PRINT_PREVIEW (photos_print_preview_new ());

	preview->priv->pixbuf = g_object_ref (pixbuf);

	update_relative_sizes (preview);

	return GTK_WIDGET (preview);
}

/**
 * photos_print_preview_new:
 *
 * Creates a new #PhotosPrintPreview widget, setting it to the default values,
 * and leaving the page empty. You still need to set the #PhotosPrintPreview:image
 * property to make it useful.
 *
 * Returns: A new and empty #PhotosPrintPreview widget.
 **/
GtkWidget *
photos_print_preview_new (void)
{
	PhotosPrintPreview *preview;
	GtkWidget *area;

	preview = g_object_new (PHOTOS_TYPE_PRINT_PREVIEW, NULL);

	area = preview->priv->area;

	gtk_widget_set_events (area,
			       GDK_EXPOSURE_MASK            |
			       GDK_POINTER_MOTION_MASK      |
			       GDK_BUTTON_PRESS_MASK        |
			       GDK_BUTTON_RELEASE_MASK      |
			       GDK_SCROLL_MASK              |
			       GDK_KEY_PRESS_MASK);

	g_object_set (G_OBJECT (area),
		      "can-focus", TRUE,
		      NULL);

/* 	update_relative_sizes (preview); */

	g_signal_connect (G_OBJECT (area), "draw",
			  G_CALLBACK (draw_cb), preview);

	g_signal_connect (G_OBJECT (area), "motion-notify-event",
			  G_CALLBACK (motion_notify_event_cb), preview);

 	g_signal_connect (G_OBJECT (area), "button-press-event",
 			  G_CALLBACK (button_press_event_cb), preview);

	g_signal_connect (G_OBJECT (area), "button-release-event",
			  G_CALLBACK (button_release_event_cb), preview);

	g_signal_connect (G_OBJECT (area), "key-press-event",
			  G_CALLBACK (key_press_event_cb), preview);

	g_signal_connect (area, "size-allocate",
			  G_CALLBACK (size_allocate_cb), preview);

	return GTK_WIDGET (preview);
}

static gboolean
draw_cb (GtkDrawingArea *drawing_area,
		 cairo_t *cr,
		 gpointer  user_data)
{
	update_relative_sizes (PHOTOS_PRINT_PREVIEW (user_data));

	photos_print_preview_draw (PHOTOS_PRINT_PREVIEW (user_data), cr);

	if (cairo_status (cr) != CAIRO_STATUS_SUCCESS) {
		fprintf (stderr, "Cairo is unhappy: %s\n",
			 cairo_status_to_string (cairo_status (cr)));
	}

	return TRUE;
}


/**
 * get_current_image_coordinates:
 * @preview: an #PhotosPrintPreview
 * @x0: A pointer where to store the x coordinate.
 * @y0: A pointer where to store the y coordinate.
 *
 * This function returns the current image coordinates, according
 * with the properties of the given @preview widget.
 **/
static void
get_current_image_coordinates (PhotosPrintPreview *preview, gint *x0, gint *y0)
{
  PhotosPrintPreviewPrivate *priv;
  GtkAllocation allocation;

  priv = preview->priv;
  gtk_widget_get_allocation (GTK_WIDGET (priv->area), &allocation);

  *x0 = (gint) ((1 - priv->pixbuf_x_align) * priv->l_rmargin +  priv->pixbuf_x_align * (allocation.width - priv->r_rmargin - priv->r_width));
  *y0 = (gint) ((1 - priv->pixbuf_y_align) * priv->t_rmargin +  priv->pixbuf_y_align * (allocation.height - priv->b_rmargin - priv->r_height));
}


/**
 * press_inside_image_area:
 * @preview: an #PhotosPrintPreview
 * @x: the points x coordinate
 * @y: the points y coordinate
 *
 * Returns whether the given point is inside the image area.
 *
 * Returns: %TRUE if the given point is inside of the image area,
 * %FALSE otherwise.
 **/
static gboolean
press_inside_image_area (PhotosPrintPreview *preview, guint x, guint y)
{
  PhotosPrintPreviewPrivate *priv;
  gint x0;
  gint y0;
  gint xs = (gint) x;
  gint ys = (gint) y;

  priv = preview->priv;
  get_current_image_coordinates (preview, &x0, &y0);

  if (xs >= x0 &&  ys >= y0 && xs <= x0 + priv->r_width && ys <= y0 + priv->r_height)
    return TRUE;

  return FALSE;
}


gboolean
photos_print_preview_point_in_image_area (PhotosPrintPreview *preview, guint x, guint y)
{
  g_return_val_if_fail (PHOTOS_IS_PRINT_PREVIEW (preview), FALSE);

  return press_inside_image_area (preview, x, y);
}


static void
create_image_scaled (PhotosPrintPreview *preview)
{
  PhotosPrintPreviewPrivate *priv = preview->priv;

  if (priv->pixbuf_scaled == NULL)
    {
      gint i_height;
      gint i_width;
      GtkAllocation allocation;

      gtk_widget_get_allocation (priv->area, &allocation);
      i_width = gdk_pixbuf_get_width (priv->pixbuf);
      i_height = gdk_pixbuf_get_height (priv->pixbuf);

      if ((i_width > allocation.width) || (i_height > allocation.height))
        {
          gdouble scale;

          scale = MIN ((gdouble) allocation.width/i_width, (gdouble) allocation.height/i_height);
          priv->pixbuf_scaled = gdk_pixbuf_scale_simple (priv->pixbuf,
                                                         i_width * scale,
                                                         i_height * scale,
                                                         GDK_INTERP_TILES);
        }
      else
        {
          priv->pixbuf_scaled = priv->pixbuf;
          g_object_ref (priv->pixbuf_scaled);
        }
    }
}

static GdkPixbuf *
create_preview_buffer (PhotosPrintPreview *preview)
{
	GdkPixbuf *pixbuf;
	gint width, height;
	GdkInterpType type = GDK_INTERP_TILES;

	if (preview->priv->pixbuf == NULL) {
		return NULL;
	}

	create_image_scaled (preview);

	width  = gdk_pixbuf_get_width (preview->priv->pixbuf);
	height = gdk_pixbuf_get_height (preview->priv->pixbuf);

	width   *= preview->priv->i_scale * preview->priv->p_scale;
	height  *= preview->priv->i_scale * preview->priv->p_scale;

	if (width < 1 || height < 1)
		return NULL;

	/* to use GDK_INTERP_TILES for small pixbufs is expensive and unnecessary */
	if (width < 25 || height < 25)
		type = GDK_INTERP_NEAREST;

	if (preview->priv->pixbuf_scaled) {
		pixbuf = gdk_pixbuf_scale_simple (preview->priv->pixbuf_scaled,
						  width, height, type);
	} else {
		pixbuf = gdk_pixbuf_scale_simple (preview->priv->pixbuf,
						  width, height, type);
	}

	return pixbuf;
}


static void
create_surface (PhotosPrintPreview *preview)
{
  PhotosPrintPreviewPrivate *priv = preview->priv;
  GdkPixbuf *pixbuf;

  if (priv->surface != NULL)
    {
      cairo_surface_destroy (priv->surface);
      priv->surface = NULL;
    }

  pixbuf = create_preview_buffer (preview);
  if (pixbuf != NULL)
    {
      priv->surface = gdk_cairo_surface_create_from_pixbuf (pixbuf, 0,
                                                            gtk_widget_get_window (GTK_WIDGET (preview)));
      g_object_unref (pixbuf);
    }

  priv->flag_create_surface = FALSE;
}


static gboolean
create_surface_when_idle (PhotosPrintPreview *preview)
{
  create_surface (preview);

  return FALSE;
}


static gboolean
button_press_event_cb (GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
  PhotosPrintPreview *preview = PHOTOS_PRINT_PREVIEW (user_data);

  preview->priv->cursorx = event->x;
  preview->priv->cursory = event->y;

  switch (event->button)
    {
    case 1:
      preview->priv->grabbed = press_inside_image_area (preview, event->x, event->y);
      break;

    default:
      break;
    }

  if (preview->priv->grabbed)
    {
      gtk_widget_queue_draw (GTK_WIDGET (preview));
    }

  gtk_widget_grab_focus (preview->priv->area);

  return FALSE;
}


static gboolean
button_release_event_cb (GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
  PhotosPrintPreview *preview = PHOTOS_PRINT_PREVIEW (user_data);

  switch (event->button)
    {
    case 1:
      preview->priv->grabbed = FALSE;
      preview->priv->r_dx = 0;
      preview->priv->r_dy = 0;
      gtk_widget_queue_draw (GTK_WIDGET (preview));
      break;

    default:
      break;
    }

  return FALSE;
}


static gboolean
key_press_event_cb (GtkWidget *widget, GdkEventKey *event, gpointer user_data)
{
  gboolean stop_emission = FALSE;
  const gchar *property;
  gfloat align;
  gfloat delta;

  delta = 0;

  switch (event->keyval)
    {
    case GDK_KEY_Left:
      property = "pixbuf-x-align";
      delta = -0.01;
      break;

    case GDK_KEY_Right:
      property = "pixbuf-x-align";
      delta = 0.01;
      break;

    case GDK_KEY_Up:
      property = "pixbuf-y-align";
      delta = -0.01;
      break;

    case GDK_KEY_Down:
      property = "pixbuf-y-align";
      delta = 0.01;
      break;

    default:
      break;
    }

  if (delta != 0)
    {
      g_object_get (G_OBJECT (user_data), property, &align, NULL);
      align += delta;
      align = CLAMP (align, 0, 1);
      g_object_set (G_OBJECT (user_data), property, align, NULL);

      stop_emission = TRUE;
      g_signal_emit (G_OBJECT (user_data), preview_signals[SIGNAL_PIXBUF_MOVED], 0);
    }

  return stop_emission;
}


static gboolean
motion_notify_event_cb (GtkWidget *widget, GdkEventMotion *event, gpointer user_data)
{
  PhotosPrintPreviewPrivate *priv = PHOTOS_PRINT_PREVIEW (user_data)->priv;
  GtkAllocation allocation;
  gdouble dx, dy;

  if (priv->grabbed)
    {
      dx = event->x - priv->cursorx;
      dy = event->y - priv->cursory;

      gtk_widget_get_allocation (widget, &allocation);

      /* Make sure the image stays inside the margins */

      priv->pixbuf_x_align += (dx + priv->r_dx) / (allocation.width  - priv->r_width - priv->l_rmargin - priv->r_rmargin);
      if (priv->pixbuf_x_align < 0. || priv->pixbuf_x_align > 1.)
        {
          priv->pixbuf_x_align = CLAMP (priv->pixbuf_x_align, 0., 1.);
          priv->r_dx += dx;
        }
      else
        priv->r_dx = 0;

      priv->pixbuf_y_align += (dy + priv->r_dy) / (allocation.height - priv->r_height - priv->t_rmargin - priv->b_rmargin);
      if (priv->pixbuf_y_align < 0. || priv->pixbuf_y_align > 1.)
        {
          priv->pixbuf_y_align = CLAMP (priv->pixbuf_y_align, 0., 1.);
          priv->r_dy += dy;
        }
      else
        priv->r_dy = 0;

      /* we do this to correctly change the property values */
      g_object_set (PHOTOS_PRINT_PREVIEW (user_data),
                    "pixbuf-x-align", priv->pixbuf_x_align,
                    "pixbuf-y-align", priv->pixbuf_y_align,
                    NULL);

      priv->cursorx = event->x;
      priv->cursory = event->y;

      g_signal_emit (G_OBJECT (user_data), preview_signals[SIGNAL_PIXBUF_MOVED], 0);
    }
  else
    {
      if (press_inside_image_area (PHOTOS_PRINT_PREVIEW (user_data), event->x, event->y))
        {
          GdkCursor *cursor;
          cursor = gdk_cursor_new_for_display (gtk_widget_get_display (widget), GDK_FLEUR);
          gdk_window_set_cursor (gtk_widget_get_window (widget), cursor);
          g_object_unref (cursor);
        }
      else
        {
          gdk_window_set_cursor (gtk_widget_get_window (widget), NULL);
        }
    }

  return FALSE;
}


static void
size_allocate_cb (GtkWidget *widget, GtkAllocation *allocation, gpointer user_data)
{
  PhotosPrintPreview *preview;

  preview = PHOTOS_PRINT_PREVIEW (user_data);
  update_relative_sizes (preview);

  preview->priv->flag_create_surface = TRUE;

  if (preview->priv->pixbuf_scaled != NULL)
    {
      g_object_unref (preview->priv->pixbuf_scaled);
      preview->priv->pixbuf_scaled = NULL;
    }

  g_idle_add ((GSourceFunc) create_surface_when_idle, preview);
}


static void
photos_print_preview_draw (PhotosPrintPreview *preview, cairo_t *cr)
{
	PhotosPrintPreviewPrivate *priv;
	GtkWidget *area;
	GtkAllocation allocation;
	gint x0, y0;
	gboolean has_focus;

	priv = preview->priv;
	area = priv->area;

	has_focus = gtk_widget_has_focus (area);

	gtk_widget_get_allocation (area, &allocation);

	/* draw the page */
	cairo_set_source_rgb (cr, 1., 1., 1.);
 	cairo_rectangle (cr, 0, 0, allocation.width, allocation.height);
 	cairo_fill (cr);

	/* draw the page margins */
	cairo_set_source_rgb (cr, 0., 0., 0.);
	cairo_set_line_width (cr, 0.1);
	cairo_rectangle (cr,
			 priv->l_rmargin, priv->t_rmargin,
			 allocation.width - priv->l_rmargin - priv->r_rmargin,
			 allocation.height - priv->t_rmargin - priv->b_rmargin);
	cairo_stroke (cr);

	get_current_image_coordinates (preview, &x0, &y0);

	if (priv->flag_create_surface) {
		create_surface (preview);
	}

	if (priv->surface) {
		cairo_set_source_surface (cr, priv->surface, x0, y0);
		cairo_paint (cr);
	} else if (priv->pixbuf_scaled) {
		/* just in the remote case we don't have the surface */

		/* adjust (x0, y0) to the new scale */
		gdouble scale = priv->i_scale * priv->p_scale *
			gdk_pixbuf_get_width (priv->pixbuf) / gdk_pixbuf_get_width (priv->pixbuf_scaled);
		x0 /= scale;
		y0 /= scale;

		cairo_scale (cr, scale, scale);
		gdk_cairo_set_source_pixbuf (cr, priv->pixbuf_scaled, x0, y0);
		cairo_paint (cr);
	} else if (priv->pixbuf) {
		/* just in the remote case we don't have the surface */

		/* adjust (x0, y0) to the new scale */
		x0 /=  priv->i_scale * priv->p_scale;
		y0 /=  priv->i_scale * priv->p_scale;

		cairo_scale (cr, priv->i_scale*priv->p_scale, priv->i_scale*priv->p_scale);
		gdk_cairo_set_source_pixbuf (cr, priv->pixbuf, x0, y0);
		cairo_paint (cr);
	}

	if (has_focus) {
		GtkStyleContext *ctx;

		ctx = gtk_widget_get_style_context (area);
		gtk_render_focus (ctx, cr, x0, y0,
				  priv->r_width, priv->r_height);
	}
}

static void
update_relative_sizes (PhotosPrintPreview *preview)
{
	PhotosPrintPreviewPrivate *priv;
	GtkAllocation allocation;
	gint i_width, i_height;

	priv = preview->priv;

	if (priv->pixbuf != NULL) {
		i_width = gdk_pixbuf_get_width (priv->pixbuf);
		i_height = gdk_pixbuf_get_height (priv->pixbuf);
	} else {
		i_width = i_height = 0;
	}

	gtk_widget_get_allocation (priv->area, &allocation);

	priv->p_scale = (gfloat) allocation.width / (priv->p_width * 72.0);

	priv->r_width  = (gint) i_width  * priv->i_scale * priv->p_scale;
	priv->r_height = (gint) i_height * priv->i_scale * priv->p_scale;

	priv->l_rmargin = (gint) (72. * priv->l_margin * priv->p_scale);
	priv->r_rmargin = (gint) (72. * priv->r_margin * priv->p_scale);
	priv->t_rmargin = (gint) (72. * priv->t_margin * priv->p_scale);
	priv->b_rmargin = (gint) (72. * priv->b_margin * priv->p_scale);
}

/**
 * photos_print_preview_set_page_margins:
 * @preview: a #PhotosPrintPreview
 * @l_margin: Left margin.
 * @r_margin: Right margin.
 * @t_margin: Top margin.
 * @b_margin: Bottom margin.
 *
 * Manually set the margins, in inches.
 **/
void
photos_print_preview_set_page_margins (PhotosPrintPreview *preview,
				    gfloat l_margin,
				    gfloat r_margin,
				    gfloat t_margin,
				    gfloat b_margin)
{
	g_return_if_fail (PHOTOS_IS_PRINT_PREVIEW (preview));

	g_object_set (G_OBJECT(preview),
		      "page-left-margin",   l_margin,
		      "page-right-margin",  r_margin,
		      "page-top-margin",    t_margin,
		      "page-bottom-margin", r_margin,
		      NULL);
}

/**
 * photos_print_preview_set_from_page_setup:
 * @preview: a #PhotosPrintPreview
 * @setup: a #GtkPageSetup to set the properties from
 *
 * Sets up the page properties from a #GtkPageSetup. Useful when using the
 * widget with the GtkPrint API.
 **/
void
photos_print_preview_set_from_page_setup (PhotosPrintPreview *preview,
				       GtkPageSetup *setup)
{
	g_return_if_fail (PHOTOS_IS_PRINT_PREVIEW (preview));
	g_return_if_fail (GTK_IS_PAGE_SETUP (setup));

	g_object_set (G_OBJECT (preview),
		      "page-left-margin", gtk_page_setup_get_left_margin (setup, GTK_UNIT_INCH),
		      "page-right-margin", gtk_page_setup_get_right_margin (setup, GTK_UNIT_INCH),
		      "page-top-margin", gtk_page_setup_get_top_margin (setup, GTK_UNIT_INCH),
		      "page-bottom-margin", gtk_page_setup_get_bottom_margin (setup, GTK_UNIT_INCH),
		      "paper-width", gtk_page_setup_get_paper_width (setup, GTK_UNIT_INCH),
		      "paper-height", gtk_page_setup_get_paper_height (setup, GTK_UNIT_INCH),
		      NULL);

}

/**
 * photos_print_preview_get_image_position:
 * @preview: a #PhotosPrintPreview
 * @x: a pointer to a #gdouble, or %NULL to ignore it
 * @y: a pointer to a #gdouble, or %NULL to ignore it
 *
 * Gets current image position in inches, relative to the margins. A
 * (0, 0) position is the intersection between the left and top margins.
 **/
void
photos_print_preview_get_image_position (PhotosPrintPreview *preview,
				      gdouble *x,
				      gdouble *y)
{
	PhotosPrintPreviewPrivate *priv;
	gdouble width, height;

	g_return_if_fail (PHOTOS_IS_PRINT_PREVIEW (preview));

	priv = preview->priv;

	if (x != NULL) {
		width  = gdk_pixbuf_get_width (priv->pixbuf)  * priv->i_scale / 72.;
		*x = priv->pixbuf_x_align * (priv->p_width  - priv->l_margin - priv->r_margin - width);
	}
	if (y != NULL) {
		height = gdk_pixbuf_get_height (priv->pixbuf) * priv->i_scale / 72.;
		*y = priv->pixbuf_y_align * (priv->p_height - priv->t_margin - priv->b_margin - height);
	}
}

/**
 * photos_print_preview_set_image_position:
 * @preview: a #PhotosPrintPreview
 * @x: The X coordinate, in inches, or -1 to ignore it.
 * @y: The Y coordinate, in inches, or -1 to ignore it.
 *
 * Sets the image position. You can pass -1 to one of the coordinates if you
 * only want to set the other.
 **/
void
photos_print_preview_set_image_position (PhotosPrintPreview *preview,
				      gdouble x,
				      gdouble y)
{
	PhotosPrintPreviewPrivate *priv;
	gfloat x_align, y_align;
	gdouble width, height;

	g_return_if_fail (PHOTOS_IS_PRINT_PREVIEW (preview));

	priv = preview->priv;

	if (x != -1) {
		width  = gdk_pixbuf_get_width (priv->pixbuf) * priv->i_scale / 72.;
		x_align = CLAMP (x/(priv->p_width - priv->l_margin - priv->r_margin - width), 0, 1);
		g_object_set (preview, "pixbuf-x-align", x_align, NULL);
	}

	if (y != -1) {
		height  = gdk_pixbuf_get_height (priv->pixbuf) * priv->i_scale / 72.;
		y_align = CLAMP (y/(priv->p_height - priv->t_margin - priv->b_margin - height), 0, 1);
		g_object_set (preview, "pixbuf-y-align", y_align, NULL);
	}
}

/**
 * photos_print_preview_set_scale:
 * @preview: a #PhotosPrintPreview
 * @scale: a scale value, between 0 and 1.
 *
 * Sets the scale for the image.
 **/
void
photos_print_preview_set_scale (PhotosPrintPreview *preview, gfloat scale)
{
	g_return_if_fail (PHOTOS_IS_PRINT_PREVIEW (preview));

	g_object_set (preview,
		      "pixbuf-scale", scale,
		      NULL);

	g_signal_emit (G_OBJECT (preview),
		       preview_signals
		       [SIGNAL_PIXBUF_SCALED], 0);

}

/**
 * photos_print_preview_get_scale:
 * @preview: A #PhotosPrintPreview.
 *
 * Gets the scale for the image.
 *
 * Returns: The scale for the image.
 **/
gfloat
photos_print_preview_get_scale (PhotosPrintPreview *preview)
{
	gfloat scale;

	g_return_val_if_fail (PHOTOS_IS_PRINT_PREVIEW (preview), 0);

	g_object_get (preview,
		      "pixbuf-scale", &scale,
		      NULL);

	return scale;
}
