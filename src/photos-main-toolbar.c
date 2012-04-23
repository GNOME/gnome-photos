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
#include <gtk/gtk.h>

#include "gd-main-toolbar.h"
#include "photos-main-toolbar.h"
#include "photos-mode-controller.h"


struct _PhotosMainToolbarPrivate
{
  ClutterActor *actor;
  GtkWidget *widget;
  PhotosModeController *controller;
  gulong window_mode_id;
};


G_DEFINE_TYPE (PhotosMainToolbar, photos_main_toolbar, G_TYPE_OBJECT);


static void
photos_main_toolbar_destroy (GtkWidget *widget, gpointer user_data)
{
  PhotosMainToolbar *self = PHOTOS_MAIN_TOOLBAR (user_data);
  PhotosMainToolbarPrivate *priv = self->priv;

  if (priv->window_mode_id != 0)
    {
      g_signal_handler_disconnect (priv->controller, priv->window_mode_id);
      priv->window_mode_id = 0;
    }
}


static void
photos_main_toolbar_go_back_request (GdMainToolbar *toolbar, gpointer user_data)
{
  PhotosMainToolbar *self = PHOTOS_MAIN_TOOLBAR (user_data);
  PhotosMainToolbarPrivate *priv = self->priv;
  PhotosWindowMode mode;

  mode = photos_mode_controller_get_window_mode (priv->controller);
  if (mode == PHOTOS_WINDOW_MODE_PREVIEW)
    photos_mode_controller_set_window_mode (priv->controller, PHOTOS_WINDOW_MODE_OVERVIEW);
}


static void
photos_main_toolbar_populate_for_overview (PhotosMainToolbar *self)
{
}


static void
photos_main_toolbar_populate_for_preview (PhotosMainToolbar *self)
{
}


static void
photos_main_toolbar_selection_mode_request (GdMainToolbar *toolbar, gboolean request_mode, gpointer user_data)
{
}


static void
photos_main_toolbar_window_mode_changed (PhotosModeController *controller,
                                         PhotosWindowMode mode,
                                         PhotosWindowMode old_mode,
                                         gpointer user_data)
{
  PhotosMainToolbar *self = PHOTOS_MAIN_TOOLBAR (user_data);

  if (mode == PHOTOS_WINDOW_MODE_OVERVIEW)
    photos_main_toolbar_populate_for_overview (self);
  else if (mode == PHOTOS_WINDOW_MODE_PREVIEW)
    photos_main_toolbar_populate_for_preview (self);
}


static void
photos_main_toolbar_dispose (GObject *object)
{
  PhotosMainToolbar *self = PHOTOS_MAIN_TOOLBAR (object);
  PhotosMainToolbarPrivate *priv = self->priv;

  if (priv->controller != NULL)
    {
      g_object_unref (priv->controller);
      priv->controller = NULL;
    }

  G_OBJECT_CLASS (photos_main_toolbar_parent_class)->dispose (object);
}


static void
photos_main_toolbar_init (PhotosMainToolbar *self)
{
  PhotosMainToolbarPrivate *priv;
  GtkStyleContext *context;

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, PHOTOS_TYPE_MAIN_TOOLBAR, PhotosMainToolbarPrivate);
  priv = self->priv;

  priv->widget = gd_main_toolbar_new ();
  gtk_toolbar_set_icon_size (GTK_TOOLBAR (priv->widget), GTK_ICON_SIZE_MENU);
  context = gtk_widget_get_style_context (priv->widget);
  gtk_style_context_add_class (context, GTK_STYLE_CLASS_MENUBAR);
  gtk_widget_show (priv->widget);

  priv->actor = gtk_clutter_actor_new_with_contents (priv->widget);

  priv->controller = photos_mode_controller_new ();

  priv->window_mode_id = g_signal_connect (priv->controller,
                                           "window-mode-changed",
                                           G_CALLBACK (photos_main_toolbar_window_mode_changed),
                                           self);

  photos_main_toolbar_window_mode_changed (priv->controller,
                                           photos_mode_controller_get_window_mode (priv->controller),
                                           PHOTOS_WINDOW_MODE_NONE, /* unused */
                                           self);

  g_signal_connect (priv->widget, "destroy", G_CALLBACK (photos_main_toolbar_destroy), self);
  g_signal_connect (priv->widget,
                    "selection-mode-request",
                    G_CALLBACK (photos_main_toolbar_selection_mode_request),
                    self);
  g_signal_connect (priv->widget, "go-back-request", G_CALLBACK (photos_main_toolbar_go_back_request), self);
}


static void
photos_main_toolbar_class_init (PhotosMainToolbarClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->dispose = photos_main_toolbar_dispose;

  g_type_class_add_private (class, sizeof (PhotosMainToolbarPrivate));
}


PhotosMainToolbar *
photos_main_toolbar_new (void)
{
  return g_object_new (PHOTOS_TYPE_MAIN_TOOLBAR, NULL);
}
