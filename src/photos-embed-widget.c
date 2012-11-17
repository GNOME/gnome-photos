/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2012 Red Hat, Inc.
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
 */


#include "config.h"

#include <gtk/gtk.h>

#include "photos-embed-widget.h"


G_DEFINE_TYPE (PhotosEmbedWidget, photos_embed_widget, GTK_CLUTTER_TYPE_EMBED);


static gboolean
photos_embed_widget_key_press_event (GtkWidget *widget, GdkEventKey *event)
{
  return FALSE;
}


static gboolean
photos_embed_widget_key_release_event (GtkWidget *widget, GdkEventKey *event)
{
  return FALSE;
}


static void
photos_embed_widget_init (PhotosEmbedWidget *self)
{
}


static void
photos_embed_widget_class_init (PhotosEmbedWidgetClass *class)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  /* We overide all keyboard handling of GtkClutterEmbed, as it interfers
   * with the key event propagation and thus focus navigation in GTK+.
   * We also make the embed itself non-focusable, as we want to treat it
   * like a container of GTK+ widget rather than an edge widget which gets
   * keyboard events.
   *
   * This means we will never get any Clutter key events, but that is
   * fine, as all our keyboard input is into GtkClutterActors, and Clutter
   * is just used as a nice way of animating and rendering GTK+ widgets
   * and some non-active graphical things.
   */
  widget_class->key_press_event = photos_embed_widget_key_press_event;
  widget_class->key_release_event = photos_embed_widget_key_release_event;
}


GtkWidget *
photos_embed_widget_new (void)
{
  return g_object_new (PHOTOS_TYPE_EMBED_WIDGET, "use-layout-size", TRUE, "can-focus", FALSE, NULL);
}
