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

#include <clutter/clutter.h>
#include <clutter-gtk/clutter-gtk.h>
#include <glib.h>
#include <glib/gi18n.h>

#include "photos-embed.h"
#include "photos-main-window.h"
#include "photos-mode-controller.h"
#include "photos-settings.h"


struct _PhotosMainWindowPrivate
{
  GtkWidget *embed;
  GSettings *settings;
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
  PhotosMainWindowPrivate *priv = self->priv;
  GVariant *variant;
  GdkWindow *window;
  GdkWindowState state;
  gint32 position[2];
  gint32 size[2];

  window = gtk_widget_get_window (GTK_WIDGET (self));
  state = gdk_window_get_state (window);
  if (state & GDK_WINDOW_STATE_MAXIMIZED)
    return;

  gtk_window_get_size (GTK_WINDOW (self), (gint *) &size[0], (gint *) &size[1]);
  variant = g_variant_new_fixed_array (G_VARIANT_TYPE_INT32, size, 2, sizeof (size[0]));
  g_settings_set_value (priv->settings, "window-size", variant);

  gtk_window_get_position (GTK_WINDOW (self), (gint *) &position[0], (gint *) &position[1]);
  variant = g_variant_new_fixed_array (G_VARIANT_TYPE_INT32, position, 2, sizeof (position[0]));
  g_settings_set_value (priv->settings, "window-position", variant);
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
  GtkTextDirection direction;
  gboolean fullscreen;

  direction = gtk_widget_get_direction (GTK_WIDGET (self));
  fullscreen = photos_mode_controller_get_fullscreen (priv->controller);

  if ((event->keyval == GDK_KEY_f || event->keyval == GDK_KEY_F11) && (event->state & GDK_CONTROL_MASK) == 0)
    {
      photos_mode_controller_toggle_fullscreen (priv->controller);
      return TRUE;
    }

  if ((fullscreen && event->keyval == GDK_KEY_Escape)
      || ((event->state & GDK_MOD1_MASK) != 0
          && ((direction == GTK_TEXT_DIR_LTR && event->keyval == GDK_KEY_Left)
              || (direction == GTK_TEXT_DIR_RTL && event->keyval == GDK_KEY_Right)))
      || event->keyval == GDK_KEY_BackSpace
      || event->keyval == GDK_KEY_Back)
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
  PhotosMainWindow *self = PHOTOS_MAIN_WINDOW (widget);
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
  g_settings_set_boolean (self->priv->settings, "window-maximized", maximized);

  return ret_val;
}


static void
photos_main_window_dispose (GObject *object)
{
  PhotosMainWindow *self = PHOTOS_MAIN_WINDOW (object);
  PhotosMainWindowPrivate *priv = self->priv;

  g_clear_object (&priv->settings);

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
  ClutterActor *stage;
  ClutterConstraint *constraint;
  GVariant *variant;
  gboolean maximized;
  const gint32 *position;
  const gint32 *size;
  gsize n_elements;

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, PHOTOS_TYPE_MAIN_WINDOW, PhotosMainWindowPrivate);
  priv = self->priv;

  priv->settings = photos_settings_new ();

  variant = g_settings_get_value (priv->settings, "window-size");
  size = g_variant_get_fixed_array (variant, &n_elements, sizeof (gint32));
  if (n_elements == 2)
    gtk_window_set_default_size (GTK_WINDOW (self), size[0], size[1]);
  g_variant_unref (variant);

  variant = g_settings_get_value (priv->settings, "window-position");
  position = g_variant_get_fixed_array (variant, &n_elements, sizeof (gint32));
  if (n_elements == 2)
    gtk_window_move (GTK_WINDOW (self), position[0], position[1]);
  g_variant_unref (variant);

  maximized = g_settings_get_boolean (priv->settings, "window-maximized");
  if (maximized)
    gtk_window_maximize (GTK_WINDOW (self));

  priv->controller = photos_mode_controller_new ();
  g_signal_connect (priv->controller,
                    "fullscreen-changed",
                    G_CALLBACK (photos_main_window_fullscreen_changed),
                    self);

  priv->embed = photos_embed_new ();
  gtk_container_add (GTK_CONTAINER (self), priv->embed);
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
