/*  Copyright 2016 Timm Bäder
 *
 * GTK+ is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * GLib is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with GTK+; see the file COPYING.  If not,
 * see <http://www.gnu.org/licenses/>.
 */

#include "gtkabstractimage.h"


G_DEFINE_ABSTRACT_TYPE (GtkAbstractImage, gtk_abstract_image, G_TYPE_OBJECT)

/* GtkAbstractImage {{{ */

enum
{
  CHANGED,
  LAST_SIGNAL
};

static guint image_signals[LAST_SIGNAL] = { 0 };

static void
gtk_abstract_image_init (GtkAbstractImage *image)
{}

static void
gtk_abstract_image_class_init (GtkAbstractImageClass *klass)
{
  image_signals[CHANGED] = g_signal_new (g_intern_static_string ("changed"),
                                         G_OBJECT_CLASS_TYPE (G_OBJECT_CLASS (klass)),
                                         G_SIGNAL_RUN_FIRST,
                                         G_STRUCT_OFFSET (GtkAbstractImageClass, changed),
                                         NULL, NULL,
                                         NULL,
                                         G_TYPE_NONE, 0);

  klass->changed = NULL;
}

int
gtk_abstract_image_get_width (GtkAbstractImage *image)
{
  g_return_val_if_fail (GTK_IS_ABSTRACT_IMAGE (image), 0);

  return GTK_ABSTRACT_IMAGE_GET_CLASS (image)->get_width (image);
}

int
gtk_abstract_image_get_height (GtkAbstractImage *image)
{
  g_return_val_if_fail (GTK_IS_ABSTRACT_IMAGE (image), 0);

  return GTK_ABSTRACT_IMAGE_GET_CLASS (image)->get_height (image);
}

void
gtk_abstract_image_draw (GtkAbstractImage *image, cairo_t *ct)
{
  g_return_if_fail (GTK_IS_ABSTRACT_IMAGE (image));

  GTK_ABSTRACT_IMAGE_GET_CLASS (image)->draw (image, ct);
}

int
gtk_abstract_image_get_scale_factor (GtkAbstractImage *image)
{
  g_return_val_if_fail (GTK_IS_ABSTRACT_IMAGE (image), 1);

  return GTK_ABSTRACT_IMAGE_GET_CLASS (image)->get_scale_factor (image);
}

/* }}} */

/* GtkPlayable {{{ */
G_DEFINE_TYPE (GtkPlayable, gtk_playable, GTK_TYPE_ABSTRACT_IMAGE)

void
gtk_playable_start (GtkPlayable *p)
{
  g_return_if_fail (GTK_IS_PLAYABLE (p));

  GTK_PLAYABLE_GET_CLASS (p)->start (p);
}

void
gtk_playable_stop (GtkPlayable *p)
{
  g_return_if_fail (GTK_IS_PLAYABLE (p));

  GTK_PLAYABLE_GET_CLASS (p)->stop (p);
}

static void
gtk_playable_init (GtkPlayable *p)
{

}

static void
gtk_playable_class_init (GtkPlayableClass *p_class)
{

}
/* }}} */

/* GtkPixbufAnimationImage {{{ */
G_DEFINE_TYPE (GtkPixbufAnimationImage, gtk_pixbuf_animation_image, GTK_TYPE_PLAYABLE)

GtkPixbufAnimationImage *
gtk_pixbuf_animation_image_new (GdkPixbufAnimation *animation, int scale_factor)
{
  GtkPixbufAnimationImage *image = g_object_new (GTK_TYPE_PIXBUF_ANIMATION_IMAGE, NULL);
  g_assert (animation);

  image->scale_factor = scale_factor;
  image->animation = animation;
  image->iter = gdk_pixbuf_animation_get_iter (animation, NULL);
  /* TODO: Use the delay for the CURRENT iter... */
  image->delay_ms = gdk_pixbuf_animation_iter_get_delay_time (image->iter);
  image->frame = gdk_cairo_surface_create_from_pixbuf (gdk_pixbuf_animation_iter_get_pixbuf (image->iter),
                                                       scale_factor, NULL);

  return image;
}

static int
gtk_pixbuf_animation_image_get_width (GtkAbstractImage *image)
{
  return gdk_pixbuf_animation_get_width (GTK_PIXBUF_ANIMATION_IMAGE (image)->animation);
}

static int
gtk_pixbuf_animation_image_get_height (GtkAbstractImage *image)
{
  return gdk_pixbuf_animation_get_height (GTK_PIXBUF_ANIMATION_IMAGE (image)->animation);
}

static int
gtk_pixbuf_animation_image_get_scale_factor (GtkAbstractImage *image)
{
  return GTK_PIXBUF_ANIMATION_IMAGE (image)->scale_factor;
}

static gboolean
gtk_pixbuf_animation_image_advance (gpointer user_data)
{
  GtkPixbufAnimationImage *image = user_data;

  gdk_pixbuf_animation_iter_advance (image->iter, NULL);
  image->frame = gdk_cairo_surface_create_from_pixbuf (gdk_pixbuf_animation_iter_get_pixbuf (image->iter),
                                                       image->scale_factor, NULL);

  g_signal_emit (image, image_signals[CHANGED], 0);

  return G_SOURCE_CONTINUE;
}

static void
gtk_pixbuf_animation_image_draw (GtkAbstractImage *_image, cairo_t *ct)
{
  GtkPixbufAnimationImage *image = GTK_PIXBUF_ANIMATION_IMAGE (_image);

  cairo_set_source_surface (ct, image->frame, 0, 0);
}

static void
gtk_pixbuf_animation_image_start (GtkPlayable *p)
{
  GtkPixbufAnimationImage *image = GTK_PIXBUF_ANIMATION_IMAGE (p);

  image->timeout_id = g_timeout_add (image->delay_ms, gtk_pixbuf_animation_image_advance, image);
}

static void
gtk_pixbuf_animation_image_stop (GtkPlayable *p)
{
  GtkPixbufAnimationImage *image = GTK_PIXBUF_ANIMATION_IMAGE (p);

  g_source_remove (image->timeout_id);
  image->timeout_id = 0;
}

static void
gtk_pixbuf_animation_image_init (GtkPixbufAnimationImage *image)
{
  image->timeout_id = 0;
}

static void
gtk_pixbuf_animation_image_class_init (GtkPixbufAnimationImageClass *klass)
{
  GtkAbstractImageClass *image_class = GTK_ABSTRACT_IMAGE_CLASS (klass);
  GtkPlayableClass *p_class = GTK_PLAYABLE_CLASS (klass);

  image_class->get_width = gtk_pixbuf_animation_image_get_width;
  image_class->get_height = gtk_pixbuf_animation_image_get_height;
  image_class->get_scale_factor = gtk_pixbuf_animation_image_get_scale_factor;
  image_class->draw = gtk_pixbuf_animation_image_draw;

  p_class->start = gtk_pixbuf_animation_image_start;
  p_class->stop = gtk_pixbuf_animation_image_stop;
}
/* }}} */

/* GtkSurfaceImage {{{ */

G_DEFINE_TYPE (GtkSurfaceImage, gtk_surface_image, GTK_TYPE_ABSTRACT_IMAGE)

static int
gtk_surface_image_get_width (GtkAbstractImage *image)
{
  return cairo_image_surface_get_width (GTK_SURFACE_IMAGE (image)->surface);
}

static int
gtk_surface_image_get_height (GtkAbstractImage *image)
{
  return cairo_image_surface_get_height (GTK_SURFACE_IMAGE (image)->surface);
}

static int
gtk_surface_image_get_scale_factor (GtkAbstractImage *image)
{
  double sx, sy;
  cairo_surface_t *surface = GTK_SURFACE_IMAGE (image)->surface;

  cairo_surface_get_device_scale (surface, &sx, &sy);

  return (int)sx;
}

static void
gtk_surface_image_draw (GtkAbstractImage *image, cairo_t *ct)
{
  cairo_set_source_surface (ct, GTK_SURFACE_IMAGE (image)->surface, 0, 0);
}

GtkSurfaceImage *
gtk_surface_image_new (cairo_surface_t *surface)
{
  GtkSurfaceImage *image = g_object_new (GTK_TYPE_SURFACE_IMAGE, NULL);
  image->surface = surface;

  return image;
}

GtkSurfaceImage *
gtk_surface_image_new_from_pixbuf (const GdkPixbuf *pixbuf, int scale_factor)
{
  GtkSurfaceImage *image = g_object_new (GTK_TYPE_SURFACE_IMAGE, NULL);
  image->surface = gdk_cairo_surface_create_from_pixbuf (pixbuf, scale_factor, NULL);

  return image;
}

static void
gtk_surface_image_init (GtkSurfaceImage *image)
{

}

static void
gtk_surface_image_class_init (GtkSurfaceImageClass *klass)
{
  GtkAbstractImageClass *image_class = GTK_ABSTRACT_IMAGE_CLASS (klass);

  image_class->get_width = gtk_surface_image_get_width;
  image_class->get_height = gtk_surface_image_get_height;
  image_class->get_scale_factor = gtk_surface_image_get_scale_factor;
  image_class->draw = gtk_surface_image_draw;
}

/* }}} */
