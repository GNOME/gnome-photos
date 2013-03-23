/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2012, 2013 Red Hat, Inc.
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

#include <gegl.h>
#include <gio/gio.h>
#include <glib.h>
#include <glib/gi18n.h>

#include "eog-debug.h"
#include "photos-application.h"
#include "photos-main-window.h"
#include "photos-mode-controller.h"
#include "photos-resources.h"


struct _PhotosApplicationPrivate
{
  GResource *resource;
  GSimpleAction *fs_action;
  GSimpleAction *sel_all_action;
  GSimpleAction *sel_none_action;
  GtkWidget *main_window;
  PhotosModeController *mode_cntrlr;
};


G_DEFINE_TYPE (PhotosApplication, photos_application, GTK_TYPE_APPLICATION)


static void
photos_application_can_fullscreen_changed (PhotosModeController *mode_cntrlr, gpointer user_data)
{
  PhotosApplication *self = PHOTOS_APPLICATION (user_data);
  PhotosApplicationPrivate *priv = self->priv;
  gboolean can_fullscreen;

  can_fullscreen = photos_mode_controller_get_can_fullscreen (mode_cntrlr);
  g_simple_action_set_enabled (priv->fs_action, can_fullscreen);
}


static void
photos_application_fullscreen (GSimpleAction *simple, GVariant *parameter, gpointer user_data)
{
  PhotosApplication *self = PHOTOS_APPLICATION (user_data);
  PhotosApplicationPrivate *priv = self->priv;

  photos_mode_controller_toggle_fullscreen (priv->mode_cntrlr);
}


static void
photos_application_quit (GSimpleAction *simple, GVariant *parameter, gpointer user_data)
{
  PhotosApplication *self = PHOTOS_APPLICATION (user_data);
  PhotosApplicationPrivate *priv = self->priv;

  gtk_widget_destroy (priv->main_window);
}


static void
photos_application_window_mode_changed (PhotosApplication *self, PhotosWindowMode mode, PhotosWindowMode old_mode)
{
  PhotosApplicationPrivate *priv = self->priv;
  gboolean enable;

  enable = (mode == PHOTOS_WINDOW_MODE_OVERVIEW || mode == PHOTOS_WINDOW_MODE_FAVORITES);
  g_simple_action_set_enabled (priv->sel_all_action, enable);
  g_simple_action_set_enabled (priv->sel_none_action, enable);
}


static void
photos_application_activate (GApplication *application)
{
  PhotosApplication *self = PHOTOS_APPLICATION (application);
  PhotosApplicationPrivate *priv = self->priv;

  gtk_window_present (GTK_WINDOW (priv->main_window));
}


static void
photos_application_startup (GApplication *application)
{
  PhotosApplication *self = PHOTOS_APPLICATION (application);
  PhotosApplicationPrivate *priv = self->priv;
  GMenu *menu;
  GSimpleAction *action;
  GtkBuilder *builder;
  GtkSettings *settings;

  G_APPLICATION_CLASS (photos_application_parent_class)
    ->startup (application);

  gegl_init (NULL, NULL);

  priv->resource = photos_get_resource ();
  g_resources_register (priv->resource);

  settings = gtk_settings_get_default ();
  g_object_set (settings, "gtk-application-prefer-dark-theme", TRUE, NULL);

  priv->mode_cntrlr = photos_mode_controller_new ();

  action = g_simple_action_new ("about", NULL);
  g_signal_connect_swapped (action, "activate", G_CALLBACK (photos_main_window_show_about), priv->main_window);
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (action));
  g_object_unref (action);

  priv->fs_action = g_simple_action_new ("fullscreen", NULL);
  g_signal_connect (priv->fs_action, "activate", G_CALLBACK (photos_application_fullscreen), self);
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (priv->fs_action));

  g_signal_connect (priv->mode_cntrlr,
                    "can-fullscreen-changed",
                    G_CALLBACK (photos_application_can_fullscreen_changed),
                    self);

  action = g_simple_action_new ("quit", NULL);
  g_signal_connect (action, "activate", G_CALLBACK (photos_application_quit), self);
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (action));
  g_object_unref (action);

  priv->sel_all_action = g_simple_action_new ("select-all", NULL);
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (priv->sel_all_action));

  priv->sel_none_action = g_simple_action_new ("select-none", NULL);
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (priv->sel_none_action));

  g_signal_connect_swapped (priv->mode_cntrlr,
                            "window-mode-changed",
                            G_CALLBACK (photos_application_window_mode_changed),
                            self);

  builder = gtk_builder_new ();
  gtk_builder_add_from_resource (builder, "/org/gnome/photos/app-menu.ui", NULL);

  menu = G_MENU (gtk_builder_get_object (builder, "app-menu"));
  gtk_application_set_app_menu (GTK_APPLICATION (self), G_MENU_MODEL (menu));
  g_object_unref (builder);

  gtk_application_add_accelerator (GTK_APPLICATION (self), "<Primary>q", "app.quit", NULL);
  gtk_application_add_accelerator (GTK_APPLICATION (self), "F11", "app.fullscreen", NULL);
  gtk_application_add_accelerator (GTK_APPLICATION (self), "<Primary>a", "app.select-all", NULL);

  priv->main_window = photos_main_window_new (GTK_APPLICATION (self));
  photos_mode_controller_set_window_mode (priv->mode_cntrlr, PHOTOS_WINDOW_MODE_OVERVIEW);
}


static GObject *
photos_application_constructor (GType type, guint n_construct_params, GObjectConstructParam *construct_params)
{
  static GObject *self = NULL;

  if (self == NULL)
    {
      self = G_OBJECT_CLASS (photos_application_parent_class)->constructor (type,
                                                                            n_construct_params,
                                                                            construct_params);
      g_object_add_weak_pointer (self, (gpointer) &self);
      return self;
    }

  return g_object_ref (self);
}


static void
photos_application_dispose (GObject *object)
{
  PhotosApplication *self = PHOTOS_APPLICATION (object);
  PhotosApplicationPrivate *priv = self->priv;

  if (priv->resource != NULL)
    {
      g_resources_unregister (priv->resource);
      g_resource_unref (priv->resource);
      priv->resource = NULL;
    }

  g_clear_object (&priv->fs_action);
  g_clear_object (&priv->sel_all_action);
  g_clear_object (&priv->sel_none_action);
  g_clear_object (&priv->mode_cntrlr);

  G_OBJECT_CLASS (photos_application_parent_class)
    ->dispose (object);
}


static void
photos_application_init (PhotosApplication *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                            PHOTOS_TYPE_APPLICATION,
                                            PhotosApplicationPrivate);
  eog_debug_init ();
}


static void
photos_application_class_init (PhotosApplicationClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GApplicationClass *application_class = G_APPLICATION_CLASS (class);

  object_class->constructor = photos_application_constructor;
  object_class->dispose = photos_application_dispose;
  application_class->activate = photos_application_activate;
  application_class->startup = photos_application_startup;

  g_type_class_add_private (class, sizeof (PhotosApplicationPrivate));
}


GtkApplication *
photos_application_new (void)
{
  return g_object_new (PHOTOS_TYPE_APPLICATION,
                       "application-id", "org.gnome." PACKAGE_NAME,
                       "flags", G_APPLICATION_FLAGS_NONE,
                       NULL);
}
