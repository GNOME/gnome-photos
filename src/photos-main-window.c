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

#include <gio/gio.h>
#include <glib.h>
#include <glib/gi18n.h>

#include "photos-about-data.h"
#include "photos-embed.h"
#include "photos-item-manager.h"
#include "photos-main-window.h"
#include "photos-mode-controller.h"
#include "photos-preview-view.h"
#include "photos-search-context.h"
#include "photos-selection-controller.h"
#include "photos-settings.h"


struct _PhotosMainWindowPrivate
{
  GtkWidget *embed;
  GSettings *settings;
  PhotosBaseManager *item_mngr;
  PhotosModeController *mode_cntrlr;
  PhotosSelectionController *sel_cntrlr;
  guint configure_id;
};


G_DEFINE_TYPE_WITH_PRIVATE (PhotosMainWindow, photos_main_window, GTK_TYPE_APPLICATION_WINDOW);


enum
{
  CONFIGURE_ID_TIMEOUT = 100, /* ms */
  WINDOW_MIN_HEIGHT = 600,
  WINDOW_MIN_WIDTH = 640,
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

  self->priv->configure_id = 0;
  photos_main_window_save_geometry (self);

  return G_SOURCE_REMOVE;
}


static gboolean
photos_main_window_configure_event (GtkWidget *widget, GdkEventConfigure *event)
{
  PhotosMainWindow *self = PHOTOS_MAIN_WINDOW (widget);
  PhotosMainWindowPrivate *priv = self->priv;
  gboolean ret_val;

  ret_val = GTK_WIDGET_CLASS (photos_main_window_parent_class)->configure_event (widget, event);

  if (photos_mode_controller_get_fullscreen (priv->mode_cntrlr))
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
  return GDK_EVENT_PROPAGATE;
}


static void
photos_main_window_fullscreen_changed (PhotosMainWindow *self, gboolean fullscreen)
{
  if (fullscreen)
    gtk_window_fullscreen (GTK_WINDOW (self));
  else
    gtk_window_unfullscreen (GTK_WINDOW (self));
}


static gboolean
photos_main_window_go_back (PhotosMainWindow *self)
{
  PhotosMainWindowPrivate *priv = self->priv;
  PhotosBaseItem *active_collection;
  PhotosWindowMode mode;
  gboolean handled = TRUE;

  mode = photos_mode_controller_get_window_mode (priv->mode_cntrlr);
  active_collection = photos_item_manager_get_active_collection (PHOTOS_ITEM_MANAGER (priv->item_mngr));

  switch (mode)
    {
    case PHOTOS_WINDOW_MODE_PREVIEW:
      photos_base_manager_set_active_object (priv->item_mngr, NULL);
      photos_mode_controller_go_back (priv->mode_cntrlr);
      break;

    case PHOTOS_WINDOW_MODE_COLLECTIONS:
    case PHOTOS_WINDOW_MODE_FAVORITES:
    case PHOTOS_WINDOW_MODE_SEARCH:
      if (active_collection != NULL)
        photos_item_manager_activate_previous_collection (PHOTOS_ITEM_MANAGER (self->priv->item_mngr));
      break;

    default:
      handled = FALSE;
      break;
    }

  return handled;
}


static gboolean
photos_main_window_is_back_key (PhotosMainWindow *self, GdkEventKey *event)
{
  GtkTextDirection direction;
  gboolean is_back;

  direction = gtk_widget_get_direction (GTK_WIDGET (self));
  is_back = (((event->state & GDK_MOD1_MASK) != 0
              && ((direction == GTK_TEXT_DIR_LTR && event->keyval == GDK_KEY_Left)
                  || (direction == GTK_TEXT_DIR_RTL && event->keyval == GDK_KEY_Right)))
             || event->keyval == GDK_KEY_Back);

  return is_back;
}


static gboolean
photos_main_window_handle_back_key (PhotosMainWindow *self, GdkEventKey *event)
{
  if (!photos_main_window_is_back_key (self, event))
    return FALSE;

  return photos_main_window_go_back (self);
}


static gboolean
photos_main_window_handle_key_overview (PhotosMainWindow *self, GdkEventKey *event)
{
  PhotosMainWindowPrivate *priv = self->priv;
  gboolean handled = FALSE;

  if (photos_selection_controller_get_selection_mode (priv->sel_cntrlr) && event->keyval == GDK_KEY_Escape)
    {
      photos_selection_controller_set_selection_mode (priv->sel_cntrlr, FALSE);
      handled = TRUE;
    }

  return handled;
}


static gboolean
photos_main_window_handle_key_preview (PhotosMainWindow *self, GdkEventKey *event)
{
  PhotosMainWindowPrivate *priv = self->priv;
  PhotosPreviewView *preview;
  gboolean fullscreen;
  gboolean handled = FALSE;

  preview = photos_embed_get_preview (PHOTOS_EMBED (priv->embed));
  fullscreen = photos_mode_controller_get_fullscreen (priv->mode_cntrlr);

  switch (event->keyval)
    {
    case GDK_KEY_Escape:
      if (fullscreen)
        {
          photos_base_manager_set_active_object (priv->item_mngr, NULL);
          photos_mode_controller_go_back (priv->mode_cntrlr);
        }
      break;

    case GDK_KEY_Left:
      photos_preview_view_load_previous (preview);
      handled = TRUE;
      break;

    case GDK_KEY_Right:
      photos_preview_view_load_next (preview);
      handled = TRUE;
      break;
    }

  return handled;
}


static gboolean
photos_main_window_key_press_event (GtkWidget *widget, GdkEventKey *event)
{
  PhotosMainWindow *self = PHOTOS_MAIN_WINDOW (widget);
  PhotosMainWindowPrivate *priv = self->priv;
  PhotosMainToolbar *toolbar;
  PhotosWindowMode mode;
  gboolean handled = GDK_EVENT_PROPAGATE;

  handled = photos_main_window_handle_back_key (self, event);
  if (handled)
    goto out;

  toolbar = photos_embed_get_main_toolbar (PHOTOS_EMBED (priv->embed));
  handled = photos_main_toolbar_handle_event (toolbar, event);
  if (handled)
    goto out;

  mode = photos_mode_controller_get_window_mode (priv->mode_cntrlr);

  switch (mode)
    {
    case PHOTOS_WINDOW_MODE_NONE:
      handled = GDK_EVENT_PROPAGATE;
      break;

    case PHOTOS_WINDOW_MODE_PREVIEW:
      handled = photos_main_window_handle_key_preview (self, event);
      break;

    case PHOTOS_WINDOW_MODE_COLLECTIONS:
    case PHOTOS_WINDOW_MODE_FAVORITES:
    case PHOTOS_WINDOW_MODE_OVERVIEW:
    case PHOTOS_WINDOW_MODE_SEARCH:
      handled = photos_main_window_handle_key_overview (self, event);
      break;
    }

 out:
  if (!handled)
    handled = GTK_WIDGET_CLASS (photos_main_window_parent_class)->key_press_event (widget, event);

  return handled;
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
photos_main_window_constructed (GObject *object)
{
  PhotosMainWindow *self = PHOTOS_MAIN_WINDOW (object);
  PhotosMainWindowPrivate *priv = self->priv;
  GApplication *app;

  G_OBJECT_CLASS (photos_main_window_parent_class)->constructed (object);

  /* HACK: Since GtkWindow:application is a non-construct property it
   * will be set after constructed has finished. We explicitly add
   * the window to the application here before creating the rest of
   * the widget hierarchy. This ensures that we can use
   * photos_application_get_scale_factor while constructing the
   * widgets.
   */
  app = g_application_get_default ();
  gtk_application_add_window (GTK_APPLICATION (app), GTK_WINDOW (self));

  priv->embed = photos_embed_new ();
  gtk_container_add (GTK_CONTAINER (self), priv->embed);
}


static void
photos_main_window_dispose (GObject *object)
{
  PhotosMainWindow *self = PHOTOS_MAIN_WINDOW (object);
  PhotosMainWindowPrivate *priv = self->priv;

  g_clear_object (&priv->settings);
  g_clear_object (&priv->item_mngr);
  g_clear_object (&priv->mode_cntrlr);
  g_clear_object (&priv->sel_cntrlr);

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
  GApplication *app;
  GVariant *variant;
  PhotosSearchContextState *state;
  gboolean maximized;
  const gint32 *position;
  const gint32 *size;
  gsize n_elements;

  self->priv = photos_main_window_get_instance_private (self);
  priv = self->priv;

  app = g_application_get_default ();
  state = photos_search_context_get_state (PHOTOS_SEARCH_CONTEXT (app));

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

  priv->item_mngr = g_object_ref (state->item_mngr);

  priv->mode_cntrlr = photos_mode_controller_dup_singleton ();
  g_signal_connect_swapped (priv->mode_cntrlr,
                            "fullscreen-changed",
                            G_CALLBACK (photos_main_window_fullscreen_changed),
                            self);

  priv->sel_cntrlr = photos_selection_controller_dup_singleton ();
}


static void
photos_main_window_class_init (PhotosMainWindowClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->constructed = photos_main_window_constructed;
  object_class->dispose = photos_main_window_dispose;
  widget_class->configure_event = photos_main_window_configure_event;
  widget_class->delete_event = photos_main_window_delete_event;
  widget_class->key_press_event = photos_main_window_key_press_event;
  widget_class->window_state_event = photos_main_window_window_state_event;
}


GtkWidget *
photos_main_window_new (GtkApplication *application)
{
  g_return_val_if_fail (GTK_IS_APPLICATION (application), NULL);

  return g_object_new (PHOTOS_TYPE_MAIN_WINDOW,
                       "width_request", WINDOW_MIN_WIDTH,
                       "height_request", WINDOW_MIN_HEIGHT,
                       "application", application,
                       "title", _(PACKAGE_NAME),
                       "window-position", GTK_WIN_POS_CENTER,
                       NULL);
}


void
photos_main_window_show_about (PhotosMainWindow *self)
{
  gtk_show_about_dialog (GTK_WINDOW (self),
                         "artists", PHOTOS_ARTISTS,
                         "authors", PHOTOS_AUTHORS,
                         "comments", _("Access, organize and share your photos on GNOME"),
                         "copyright", _("Copyright © 2013 Intel Corporation. All rights reserved.\n"
                                        "Copyright © 2014, 2015 Pranav Kant\n"
                                        "Copyright © 2012, 2013, 2014 Red Hat, Inc."),
                         "license-type", GTK_LICENSE_GPL_2_0,
                         "logo-icon-name", PACKAGE_TARNAME,
                         "program-name", _(PACKAGE_NAME),
                         "version", _(PACKAGE_VERSION),
                         "website", PACKAGE_URL,
                         "wrap-license", TRUE,
                         /* Translators: Put your names here */
                         "translator-credits", _("translator-credits"),
                         NULL);
}
