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
#include "photos-mode-controller.h"


struct _PhotosMainWindowPrivate
{
  GtkWidget *clutter_embed;
  PhotosModeController *controller;
  guint configure_id;
};


G_DEFINE_TYPE (PhotosMainWindow, photos_main_window, GTK_TYPE_APPLICATION_WINDOW)


enum
{
  CONFIGURE_ID_TIMEOUT = 100 /* ms */
};


static void
photos_main_window_save_geometry (PhotosMainWindow *self)
{
}


static gboolean
photos_main_window_configure_id_timeout (gpointer user_data)
{
  PhotosMainWindow *self = PHOTOS_MAIN_WINDOW (user_data);

  photos_main_window_save_geometry (self);
  return FALSE;
}


static gboolean
photos_main_window_configure_event (GtkWidget *widget, GdkEventConfigure *event)
{
  PhotosMainWindow *self = PHOTOS_MAIN_WINDOW (widget);
  PhotosMainWindowPrivate *priv = self->priv;
  gboolean ret_val;

  ret_val = GTK_WIDGET_CLASS (photos_main_window_parent_class)->configure_event (widget, event);

  if (photos_mode_controller_get_fullscreen (priv->controller))
    return ret_val;

  if (priv->configure_id != 0)
    {
      g_source_remove (priv->configure_id);
      priv->configure_id = 0;
    }

  priv->configure_id = g_timeout_add (CONFIGURE_ID_TIMEOUT, photos_main_window_configure_id_timeout, self);
  return ret_val;
}


static gboolean
photos_main_window_delete_event (GtkWidget *widget, GdkEventAny *event)
{
  PhotosMainWindow *self = PHOTOS_MAIN_WINDOW (widget);
  PhotosMainWindowPrivate *priv = self->priv;

  if (priv->configure_id != 0)
    {
      g_source_remove (priv->configure_id);
      priv->configure_id = 0;
    }

  photos_main_window_save_geometry (self);
  return FALSE;
}


static void
photos_main_window_fullscreen_changed (PhotosModeController *controller, gboolean fullscreen, gpointer user_data)
{
  PhotosMainWindow *self = PHOTOS_MAIN_WINDOW (user_data);
  PhotosMainWindowPrivate *priv = self->priv;

  if (fullscreen)
    gtk_window_fullscreen (GTK_WINDOW (self));
  else
    gtk_window_unfullscreen (GTK_WINDOW (self));
}


static gboolean
photos_main_window_handle_key_overview (PhotosMainWindow *self, GdkEventKey *event)
{
  return FALSE;
}


static gboolean
photos_main_window_handle_key_preview (PhotosMainWindow *self, GdkEventKey *event)
{
  PhotosMainWindowPrivate *priv = self->priv;
  gboolean fullscreen;

  if (event->keyval == GDK_KEY_f)
    {
      photos_mode_controller_toggle_fullscreen (priv->controller);
      return TRUE;
    }

  fullscreen = photos_mode_controller_get_fullscreen (priv->controller);
  if (event->keyval == GDK_KEY_Escape && fullscreen)
    {
      photos_mode_controller_set_fullscreen (priv->controller, FALSE);
      return TRUE;
    }

  if (event->keyval == GDK_KEY_Escape || event->keyval == GDK_KEY_Back)
    {
      photos_mode_controller_set_window_mode (priv->controller, PHOTOS_WINDOW_MODE_OVERVIEW);
      return TRUE;
    }

  return FALSE;
}


static gboolean
photos_main_window_key_press_event (GtkWidget *widget, GdkEventKey *event)
{
  PhotosMainWindow *self = PHOTOS_MAIN_WINDOW (widget);
  PhotosMainWindowPrivate *priv = self->priv;
  PhotosWindowMode mode;

  if ((event->keyval == GDK_KEY_q) && ((event->state & GDK_CONTROL_MASK) != 0))
    {
      gtk_widget_destroy (widget);
      return TRUE;
    }

  mode = photos_mode_controller_get_window_mode (priv->controller);
  if (mode == PHOTOS_WINDOW_MODE_PREVIEW)
    return photos_main_window_handle_key_preview (self, event);
  else
    return photos_main_window_handle_key_overview (self, event);
}


static gboolean
photos_main_window_window_state_event (GtkWidget *widget, GdkEventWindowState *event)
{
  GdkWindow *window;
  GdkWindowState state;
  gboolean maximized;
  gboolean ret_val;

  ret_val = GTK_WIDGET_CLASS (photos_main_window_parent_class)->window_state_event (widget, event);

  window = gtk_widget_get_window (widget);
  state = gdk_window_get_state (window);

  if (state & GDK_WINDOW_STATE_FULLSCREEN)
    return ret_val;

  maximized = (state & GDK_WINDOW_STATE_MAXIMIZED);
  maximized;

  return ret_val;
}


static void
photos_main_window_dispose (GObject *object)
{
  PhotosMainWindow *self = PHOTOS_MAIN_WINDOW (object);
  PhotosMainWindowPrivate *priv = self->priv;

  if (priv->controller != NULL)
    {
      g_object_unref (priv->controller);
      priv->controller = NULL;
    }

  if (priv->configure_id != 0)
    {
      g_source_remove (priv->configure_id);
      priv->configure_id = 0;
    }

  G_OBJECT_CLASS (photos_main_window_parent_class)->dispose (object);
}


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

  priv->controller = photos_mode_controller_new ();
  g_signal_connect (priv->controller,
                    "fullscreen-changed",
                    G_CALLBACK (photos_main_window_fullscreen_changed),
                    self);
}


static void
photos_main_window_class_init (PhotosMainWindowClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->dispose = photos_main_window_dispose;
  widget_class->configure_event = photos_main_window_configure_event;
  widget_class->delete_event = photos_main_window_delete_event;
  widget_class->key_press_event = photos_main_window_key_press_event;
  widget_class->window_state_event = photos_main_window_window_state_event;

  g_type_class_add_private (class, sizeof (PhotosMainWindowPrivate));
}


GtkWidget *
photos_main_window_new (GtkApplication *application)
{
  g_return_val_if_fail (GTK_IS_APPLICATION (application), NULL);

  return g_object_new (PHOTOS_TYPE_MAIN_WINDOW,
                       "application", application,
                       "hide-titlebar-when-maximized", TRUE,
                       "title", _(PACKAGE_NAME),
                       "window-position", GTK_WIN_POS_CENTER,
                       NULL);
}
