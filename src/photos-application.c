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
#include <gio/gio.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <tracker-sparql.h>

#include "photos-application.h"
#include "photos-main-window.h"
#include "photos-mode-controller.h"


struct _PhotosApplicationPrivate
{
  GSimpleAction *fs_action;
  GtkWidget *main_window;
  PhotosModeController *controller;
  TrackerSparqlConnection *connection;
};


G_DEFINE_TYPE (PhotosApplication, photos_application, GTK_TYPE_APPLICATION)


static void
photos_application_about (GSimpleAction *simple, GVariant *parameter, gpointer user_data)
{
}


static void
photos_application_can_fullscreen_changed (PhotosModeController *controller, gpointer user_data)
{
  PhotosApplication *self = PHOTOS_APPLICATION (user_data);
  PhotosApplicationPrivate *priv = self->priv;
  gboolean can_fullscreen;

  can_fullscreen = photos_mode_controller_get_can_fullscreen (controller);
  g_simple_action_set_enabled (priv->fs_action, can_fullscreen);
}


static void
photos_application_fullscreen (GSimpleAction *simple, GVariant *parameter, gpointer user_data)
{
  PhotosApplication *self = PHOTOS_APPLICATION (user_data);
  PhotosApplicationPrivate *priv = self->priv;

  photos_mode_controller_toggle_fullscreen (priv->controller);
}


static void
photos_application_quit (GSimpleAction *simple, GVariant *parameter, gpointer user_data)
{
  PhotosApplication *self = PHOTOS_APPLICATION (user_data);
  PhotosApplicationPrivate *priv = self->priv;

  gtk_widget_destroy (priv->main_window);
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
  GError *error;
  GMenu *doc_actions;
  GMenu *menu;
  GSimpleAction *action;

  G_APPLICATION_CLASS (photos_application_parent_class)
    ->startup (application);

  if (gtk_clutter_init (NULL, NULL) != CLUTTER_INIT_SUCCESS)
    {
      g_warning ("Unable to initialize Clutter");
      return;
    }

  error = NULL;
  priv->connection = tracker_sparql_connection_get (NULL, &error);
  if (error != NULL)
    {
      g_warning ("Unable to connect to the Tracker database: %s", error->message);
      return;
    }

  priv->controller = photos_mode_controller_new ();
  photos_mode_controller_set_window_mode (priv->controller, PHOTOS_WINDOW_MODE_OVERVIEW);

  action = g_simple_action_new ("about", NULL);
  g_signal_connect (action, "activate", G_CALLBACK (photos_application_about), self);
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (action));
  g_object_unref (action);

  priv->fs_action = g_simple_action_new ("fullscreen", NULL);
  g_signal_connect (priv->fs_action, "activate", G_CALLBACK (photos_application_fullscreen), self);
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (priv->fs_action));

  g_signal_connect (priv->controller,
                    "can-fullscreen-changed",
                    G_CALLBACK (photos_application_can_fullscreen_changed),
                    self);

  action = g_simple_action_new ("quit", NULL);
  g_signal_connect (action, "activate", G_CALLBACK (photos_application_quit), self);
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (action));
  g_object_unref (action);

  menu = g_menu_new ();

  doc_actions = g_menu_new ();
  g_menu_append (doc_actions, _("Fullscreen"), "app.fullscreen");
  g_menu_append_section (menu, NULL, G_MENU_MODEL (doc_actions));

  g_menu_append (menu, _("About Photos"), "app.about");
  g_menu_append (menu, _("Quit"), "app.quit");

  gtk_application_set_app_menu (GTK_APPLICATION (self), G_MENU_MODEL (menu));

  priv->main_window = photos_main_window_new (GTK_APPLICATION (self));
}


static void
photos_application_dispose (GObject *object)
{
  PhotosApplication *self = PHOTOS_APPLICATION (object);
  PhotosApplicationPrivate *priv = self->priv;

  if (priv->fs_action != NULL)
    {
      g_object_unref (priv->fs_action);
      priv->fs_action = NULL;
    }

  if (priv->controller != NULL)
    {
      g_object_unref (priv->controller);
      priv->controller = NULL;
    }

  if (priv->connection != NULL)
    {
      g_object_unref (priv->connection);
      priv->connection = NULL;
    }

  G_OBJECT_CLASS (photos_application_parent_class)
    ->dispose (object);
}


static void
photos_application_init (PhotosApplication *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                            PHOTOS_TYPE_APPLICATION,
                                            PhotosApplicationPrivate);
}


static void
photos_application_class_init (PhotosApplicationClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GApplicationClass *application_class = G_APPLICATION_CLASS (class);

  object_class->dispose = photos_application_dispose;
  application_class->activate = photos_application_activate;
  application_class->startup = photos_application_startup;

  g_type_class_add_private (class, sizeof (PhotosApplicationPrivate));
}


GtkApplication *
photos_application_new (const gchar       *application_id,
                        GApplicationFlags  flags)
{
  g_return_val_if_fail (g_application_id_is_valid (application_id), NULL);

  g_type_init ();
  return g_object_new (PHOTOS_TYPE_APPLICATION,
                       "application-id", application_id,
                       "flags", flags,
                       NULL);
}
