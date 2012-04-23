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


#include "config.h"

#include <clutter-gtk/clutter-gtk.h>
#include <glib.h>
#include <glib/gi18n.h>

#include "photos-main-window.h"


struct _PhotosMainWindowPrivate
{
  GtkWidget *clutter_embed;
};


G_DEFINE_TYPE (PhotosMainWindow, photos_main_window, GTK_TYPE_APPLICATION_WINDOW)


static void
photos_main_window_init (PhotosMainWindow *self)
{
  PhotosMainWindowPrivate *priv;

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                            PHOTOS_TYPE_MAIN_WINDOW,
                                            PhotosMainWindowPrivate);
  priv = self->priv;

  priv->clutter_embed = gtk_clutter_embed_new ();
  gtk_container_add (GTK_CONTAINER (self), priv->clutter_embed);
  gtk_widget_show (priv->clutter_embed);
}


static void
photos_main_window_class_init (PhotosMainWindowClass *class)
{
  g_type_class_add_private (class, sizeof (PhotosMainWindowPrivate));
}


GtkWidget *
photos_main_window_new (GtkApplication *application)
{
  g_return_val_if_fail (GTK_IS_APPLICATION (application), NULL);

  g_object_new (PHOTOS_TYPE_MAIN_WINDOW,
                "application", application,
                "hide-titlebar-when-maximized", TRUE,
                "title", _(PACKAGE_NAME),
                "window-position", GTK_WIN_POS_CENTER,
                NULL);
}
