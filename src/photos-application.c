/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2012, 2013, 2014 Red Hat, Inc.
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
#include <grilo.h>
#include <libgnome-desktop/gnome-bg.h>

#include "eog-debug.h"
#include "photos-application.h"
#include "photos-base-item.h"
#include "photos-camera-cache.h"
#include "photos-dlna-renderers-dialog.h"
#include "photos-filterable.h"
#include "photos-gom-miner.h"
#include "photos-item-manager.h"
#include "photos-main-window.h"
#include "photos-mode-controller.h"
#include "photos-properties-dialog.h"
#include "photos-resources.h"
#include "photos-source-manager.h"


struct _PhotosApplicationPrivate
{
  GList *miners_running;
  GResource *resource;
  GSettings *settings;
  GSimpleAction *fs_action;
  GSimpleAction *gear_action;
  GSimpleAction *open_action;
  GSimpleAction *print_action;
  GSimpleAction *properties_action;
  GSimpleAction *search_action;
  GSimpleAction *sel_all_action;
  GSimpleAction *sel_none_action;
  GSimpleAction *set_bg_action;
  GSimpleAction *remote_display_action;
  GomMiner *facebook_miner;
  GomMiner *flickr_miner;
  GtkWidget *main_window;
  PhotosBaseManager *item_mngr;
  PhotosBaseManager *src_mngr;
  PhotosCameraCache *camera_cache;
  PhotosModeController *mode_cntrlr;
};


G_DEFINE_TYPE_WITH_PRIVATE (PhotosApplication, photos_application, GTK_TYPE_APPLICATION)


enum
{
  MINER_REFRESH_TIMEOUT = 60 /* s */
};

typedef struct _PhotosApplicationRefreshData PhotosApplicationRefreshData;

struct _PhotosApplicationRefreshData
{
  PhotosApplication *application;
  GomMiner *miner;
};

static gboolean photos_application_refresh_miner_now (PhotosApplication *self, GomMiner *miner);


static PhotosApplicationRefreshData *
photos_application_refresh_data_new (PhotosApplication *application, GomMiner *miner)
{
  PhotosApplicationRefreshData *data;

  data = g_slice_new0 (PhotosApplicationRefreshData);
  data->application = g_object_ref (application);
  data->miner = g_object_ref (miner);
  return data;
}


static void
photos_application_refresh_data_free (PhotosApplicationRefreshData *data)
{
  g_object_unref (data->application);
  g_object_unref (data->miner);
  g_slice_free (PhotosApplicationRefreshData, data);
}


static void
photos_application_about (PhotosApplication *self, GVariant *parameter)
{
  photos_main_window_show_about (PHOTOS_MAIN_WINDOW (self->priv->main_window));
}


static void
photos_application_action_toggle (GSimpleAction *simple, GVariant *parameter, gpointer user_data)
{
  GVariant *state;
  GVariant *new_state;

  state = g_action_get_state (G_ACTION (simple));
  if (state == NULL)
    return;

  new_state = g_variant_new ("b", !g_variant_get_boolean (state));
  g_action_change_state (G_ACTION (simple), new_state);
  g_variant_unref (state);
}


static void
photos_application_can_fullscreen_changed (PhotosApplication *self)
{
  PhotosApplicationPrivate *priv = self->priv;
  gboolean can_fullscreen;

  can_fullscreen = photos_mode_controller_get_can_fullscreen (priv->mode_cntrlr);
  g_simple_action_set_enabled (priv->fs_action, can_fullscreen);
}


static void
photos_application_fullscreen (PhotosApplication *self, GVariant *parameter)
{
  PhotosApplicationPrivate *priv = self->priv;

  photos_mode_controller_toggle_fullscreen (priv->mode_cntrlr);
}


static void
photos_application_open_current (PhotosApplication *self)
{
  PhotosApplicationPrivate *priv = self->priv;
  GdkScreen *screen;
  PhotosBaseItem *item;
  guint32 time;

  item = PHOTOS_BASE_ITEM (photos_base_manager_get_active_object (priv->item_mngr));
  if (item == NULL)
    return;

  screen = gtk_window_get_screen (GTK_WINDOW (priv->main_window));
  time = gtk_get_current_event_time ();
  photos_base_item_open (item, screen, time);
}


static void
photos_application_print_current (PhotosApplication *self)
{
  PhotosApplicationPrivate *priv = self->priv;
  PhotosBaseItem *item;

  item = PHOTOS_BASE_ITEM (photos_base_manager_get_active_object (priv->item_mngr));
  if (item == NULL)
    return;

  photos_base_item_print (item, priv->main_window);
}


static void
photos_application_properties (PhotosApplication *self)
{
  PhotosApplicationPrivate *priv = self->priv;
  GObject *item;
  GtkWidget *dialog;
  const gchar *id;

  item = photos_base_manager_get_active_object (priv->item_mngr);
  if (item == NULL)
    return;

  id = photos_filterable_get_id (PHOTOS_FILTERABLE (item));
  dialog = photos_properties_dialog_new (GTK_WINDOW (priv->main_window), id);
  gtk_widget_show_all (dialog);
  g_signal_connect (dialog, "response", G_CALLBACK (gtk_widget_destroy), NULL);
}


static gboolean
photos_application_refresh_miner_timeout (gpointer user_data)
{
  PhotosApplicationRefreshData *data = (PhotosApplicationRefreshData *) user_data;
  return photos_application_refresh_miner_now (data->application, data->miner);
}


static void
photos_application_refresh_db (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosApplication *self = PHOTOS_APPLICATION (user_data);
  PhotosApplicationPrivate *priv = self->priv;
  GError *error;
  GomMiner *miner = GOM_MINER (source_object);
  PhotosApplicationRefreshData *data;

  priv->miners_running = g_list_remove (priv->miners_running, miner);

  error = NULL;
  if (!gom_miner_call_refresh_db_finish (miner, res, &error))
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Unable to update the cache: %s", error->message);
      g_error_free (error);
      goto out;
    }

  data = photos_application_refresh_data_new (self, miner);
  g_timeout_add_seconds_full (G_PRIORITY_DEFAULT,
                              MINER_REFRESH_TIMEOUT,
                              photos_application_refresh_miner_timeout,
                              data,
                              (GDestroyNotify) photos_application_refresh_data_free);

 out:
  g_object_unref (self);
  g_object_unref (miner);
}


static gboolean
photos_application_refresh_miner_now (PhotosApplication *self, GomMiner *miner)
{
  PhotosApplicationPrivate *priv = self->priv;
  GCancellable *cancellable;

  if (g_getenv ("PHOTOS_DISABLE_MINERS") != NULL)
    goto out;

  priv->miners_running = g_list_prepend (priv->miners_running, g_object_ref (miner));

  cancellable = g_cancellable_new ();
  g_object_set_data_full (G_OBJECT (miner), "cancellable", cancellable, g_object_unref);
  gom_miner_call_refresh_db (miner, cancellable, photos_application_refresh_db, g_object_ref (self));

 out:
  return G_SOURCE_REMOVE;
}


static void
photos_application_refresh_miners (PhotosApplication *self)
{
  PhotosApplicationPrivate *priv = self->priv;

  if (photos_source_manager_has_provider_type (PHOTOS_SOURCE_MANAGER (priv->src_mngr), "facebook"))
    photos_application_refresh_miner_now (self, priv->facebook_miner);

  if (photos_source_manager_has_provider_type (PHOTOS_SOURCE_MANAGER (priv->src_mngr), "flickr"))
    photos_application_refresh_miner_now (self, priv->flickr_miner);
}


static void
photos_application_remote_display_current (PhotosApplication *self)
{
  PhotosApplicationPrivate *priv = self->priv;
  GObject *item;
  GtkWidget *dialog;
  const gchar *urn;

  item = photos_base_manager_get_active_object (priv->item_mngr);
  if (item == NULL)
    return;

  urn = photos_filterable_get_id (PHOTOS_FILTERABLE (item));
  dialog = photos_dlna_renderers_dialog_new (GTK_WINDOW (priv->main_window), urn);
  gtk_widget_show_all (dialog);
}


static void
photos_application_quit (PhotosApplication *self, GVariant *parameter)
{
  gtk_widget_destroy (self->priv->main_window);
}


static void
photos_application_set_bg_download (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosApplication *self = PHOTOS_APPLICATION (user_data);
  PhotosApplicationPrivate *priv = self->priv;
  PhotosBaseItem *item = PHOTOS_BASE_ITEM (source_object);
  GError *error;
  gchar *filename = NULL;

  error = NULL;
  filename = photos_base_item_download_finish (item, res, &error);
  if (error != NULL)
    {
      const gchar *uri;

      uri = photos_base_item_get_uri (item);
      g_warning ("Unable to extract the local filename for %s", uri);
      g_error_free (error);
      goto out;
    }


  g_settings_set_string (priv->settings, "picture-uri", filename);
  g_settings_set_enum (priv->settings, "picture-options", G_DESKTOP_BACKGROUND_STYLE_ZOOM);
  g_settings_set_enum (priv->settings, "color-shading-type", G_DESKTOP_BACKGROUND_SHADING_SOLID);
  g_settings_set_string (priv->settings, "primary-color", "#000000000000");
  g_settings_set_string (priv->settings, "secondary-color", "#000000000000");

 out:
  g_free (filename);
  g_object_unref (self);
}


static void
photos_application_set_bg (PhotosApplication *self)
{
  PhotosBaseItem *item;

  item = PHOTOS_BASE_ITEM (photos_base_manager_get_active_object (self->priv->item_mngr));
  if (item == NULL)
    return;

  photos_base_item_download_async (item, NULL, photos_application_set_bg_download, g_object_ref (self));
}


static void
photos_application_start_miners (PhotosApplication *self)
{
  PhotosApplicationPrivate *priv = self->priv;

  photos_application_refresh_miners (self);

  g_signal_connect_object (priv->src_mngr,
                           "object-added",
                           G_CALLBACK (photos_application_refresh_miners),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (priv->src_mngr,
                           "object-removed",
                           G_CALLBACK (photos_application_refresh_miners),
                           self,
                           G_CONNECT_SWAPPED);
}


static void
photos_application_stop_miners (PhotosApplication *self)
{
  PhotosApplicationPrivate *priv = self->priv;
  GList *l;

  for (l = priv->miners_running; l != NULL; l = l->next)
    {
      GomMiner *miner = GOM_MINER (l->data);
      GCancellable *cancellable;

      cancellable = g_object_get_data (G_OBJECT (miner), "cancellable");
      g_cancellable_cancel (cancellable);
    }
}


static void
photos_application_window_mode_changed (PhotosApplication *self, PhotosWindowMode mode, PhotosWindowMode old_mode)
{
  PhotosApplicationPrivate *priv = self->priv;
  gboolean enable;

  enable = (mode == PHOTOS_WINDOW_MODE_OVERVIEW
            || mode == PHOTOS_WINDOW_MODE_COLLECTIONS
            || mode == PHOTOS_WINDOW_MODE_FAVORITES);
  g_simple_action_set_enabled (priv->sel_all_action, enable);
  g_simple_action_set_enabled (priv->sel_none_action, enable);

  enable = (mode == PHOTOS_WINDOW_MODE_PREVIEW);
  g_simple_action_set_enabled (priv->gear_action, enable);
  g_simple_action_set_enabled (priv->open_action, enable);
  g_simple_action_set_enabled (priv->print_action, enable);
  g_simple_action_set_enabled (priv->properties_action, enable);
  g_simple_action_set_enabled (priv->set_bg_action, enable);
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
  GMenu *menu;
  GSimpleAction *action;
  GrlRegistry *registry;
  GtkBuilder *builder;
  GtkSettings *settings;
  GVariant *state;

  G_APPLICATION_CLASS (photos_application_parent_class)
    ->startup (application);

  gegl_init (NULL, NULL);

  grl_init (NULL, NULL);
  registry = grl_registry_get_default ();
  error = NULL;
  if (!grl_registry_load_plugin_by_id (registry, "grl-flickr", &error))
    {
      g_warning ("Unable to load Grilo's Flickr plugin: %s", error->message);
      g_error_free (error);
    }

  priv->settings = g_settings_new ("org.gnome.desktop.background");

  priv->resource = photos_get_resource ();
  g_resources_register (priv->resource);

  settings = gtk_settings_get_default ();
  g_object_set (settings, "gtk-application-prefer-dark-theme", TRUE, NULL);

  priv->facebook_miner = gom_miner_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                                           G_DBUS_PROXY_FLAGS_NONE,
                                                           "org.gnome.OnlineMiners.Facebook",
                                                           "/org/gnome/OnlineMiners/Facebook",
                                                           NULL,
                                                           NULL);

  priv->flickr_miner = gom_miner_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                                         G_DBUS_PROXY_FLAGS_NONE,
                                                         "org.gnome.OnlineMiners.Flickr",
                                                         "/org/gnome/OnlineMiners/Flickr",
                                                         NULL,
                                                         NULL);

  priv->item_mngr = photos_item_manager_dup_singleton ();
  priv->src_mngr = photos_source_manager_dup_singleton ();

  /* A dummy reference to keep it alive during the lifetime of the
   * application.
   */
  priv->camera_cache = photos_camera_cache_dup_singleton ();

  priv->mode_cntrlr = photos_mode_controller_dup_singleton ();

  action = g_simple_action_new ("about", NULL);
  g_signal_connect_swapped (action, "activate", G_CALLBACK (photos_application_about), self);
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (action));
  g_object_unref (action);

  priv->fs_action = g_simple_action_new ("fullscreen", NULL);
  g_signal_connect_swapped (priv->fs_action, "activate", G_CALLBACK (photos_application_fullscreen), self);
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (priv->fs_action));

  g_signal_connect_swapped (priv->mode_cntrlr,
                            "can-fullscreen-changed",
                            G_CALLBACK (photos_application_can_fullscreen_changed),
                            self);

  state = g_variant_new ("b", FALSE);
  priv->gear_action = g_simple_action_new_stateful ("gear-menu", NULL, state);
  g_signal_connect (priv->gear_action, "activate", G_CALLBACK (photos_application_action_toggle), self);
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (priv->gear_action));

  priv->open_action = g_simple_action_new ("open-current", NULL);
  g_signal_connect_swapped (priv->open_action, "activate", G_CALLBACK (photos_application_open_current), self);
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (priv->open_action));

  priv->print_action = g_simple_action_new ("print-current", NULL);
  g_signal_connect_swapped (priv->print_action, "activate", G_CALLBACK (photos_application_print_current), self);
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (priv->print_action));

  priv->properties_action = g_simple_action_new ("properties", NULL);
  g_signal_connect_swapped (priv->properties_action, "activate", G_CALLBACK (photos_application_properties), self);
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (priv->properties_action));

  priv->remote_display_action = g_simple_action_new ("remote-display-current", NULL);
  g_signal_connect_swapped (priv->remote_display_action, "activate", G_CALLBACK (photos_application_remote_display_current), self);
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (priv->remote_display_action));

  action = g_simple_action_new ("quit", NULL);
  g_signal_connect_swapped (action, "activate", G_CALLBACK (photos_application_quit), self);
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (action));
  g_object_unref (action);

  state = g_variant_new ("b", FALSE);
  priv->search_action = g_simple_action_new_stateful ("search", NULL, state);
  g_signal_connect (priv->search_action, "activate", G_CALLBACK (photos_application_action_toggle), self);
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (priv->search_action));

  priv->sel_all_action = g_simple_action_new ("select-all", NULL);
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (priv->sel_all_action));

  priv->sel_none_action = g_simple_action_new ("select-none", NULL);
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (priv->sel_none_action));

  priv->set_bg_action = g_simple_action_new ("set-background", NULL);
  g_signal_connect_swapped (priv->set_bg_action, "activate", G_CALLBACK (photos_application_set_bg), self);
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (priv->set_bg_action));

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
  gtk_application_add_accelerator (GTK_APPLICATION (self), "F10", "app.gear-menu", NULL);
  gtk_application_add_accelerator (GTK_APPLICATION (self), "<Primary>p", "app.print-current", NULL);
  gtk_application_add_accelerator (GTK_APPLICATION (self), "<Primary>f", "app.search", NULL);
  gtk_application_add_accelerator (GTK_APPLICATION (self), "<Primary>a", "app.select-all", NULL);

  priv->main_window = photos_main_window_new (GTK_APPLICATION (self));
  photos_mode_controller_set_window_mode (priv->mode_cntrlr, PHOTOS_WINDOW_MODE_OVERVIEW);

  photos_application_start_miners (self);
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

  if (priv->miners_running != NULL)
    {
      photos_application_stop_miners (self);
      g_list_free_full (priv->miners_running, g_object_unref);
      priv->miners_running = NULL;
    }

  if (priv->resource != NULL)
    {
      g_resources_unregister (priv->resource);
      g_resource_unref (priv->resource);
      priv->resource = NULL;
    }

  g_clear_object (&priv->settings);
  g_clear_object (&priv->fs_action);
  g_clear_object (&priv->gear_action);
  g_clear_object (&priv->open_action);
  g_clear_object (&priv->print_action);
  g_clear_object (&priv->properties_action);
  g_clear_object (&priv->search_action);
  g_clear_object (&priv->sel_all_action);
  g_clear_object (&priv->sel_none_action);
  g_clear_object (&priv->set_bg_action);
  g_clear_object (&priv->facebook_miner);
  g_clear_object (&priv->flickr_miner);
  g_clear_object (&priv->item_mngr);
  g_clear_object (&priv->src_mngr);
  g_clear_object (&priv->camera_cache);
  g_clear_object (&priv->mode_cntrlr);

  G_OBJECT_CLASS (photos_application_parent_class)->dispose (object);
}


static void
photos_application_init (PhotosApplication *self)
{
  self->priv = photos_application_get_instance_private (self);
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

  /* TODO: Add miners-changed signal */
}


GtkApplication *
photos_application_new (void)
{
  return g_object_new (PHOTOS_TYPE_APPLICATION,
                       "application-id", "org.gnome." PACKAGE_NAME,
                       "flags", G_APPLICATION_FLAGS_NONE,
                       NULL);
}
