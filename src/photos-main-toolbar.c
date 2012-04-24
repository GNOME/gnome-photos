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
#include "photos-selection-controller.h"


struct _PhotosMainToolbarPrivate
{
  ClutterActor *actor;
  GtkWidget *widget;
  PhotosModeController *mode_cntrlr;
  PhotosSelectionController *sel_cntrlr;
  gulong selection_changed_id;
  gulong selection_mode_id;
  gulong window_mode_id;
};


G_DEFINE_TYPE (PhotosMainToolbar, photos_main_toolbar, G_TYPE_OBJECT);


static void
photos_main_toolbar_clear_request (GdMainToolbar *toolbar, gpointer user_data)
{
  PhotosMainToolbar *self = PHOTOS_MAIN_TOOLBAR (user_data);
  PhotosMainToolbarPrivate *priv = self->priv;

  if (priv->selection_changed_id != 0)
    {
      g_signal_handler_disconnect (priv->sel_cntrlr, priv->selection_changed_id);
      priv->selection_changed_id = 0;
    }
}


static void
photos_main_toolbar_destroy (GtkWidget *widget, gpointer user_data)
{
  PhotosMainToolbar *self = PHOTOS_MAIN_TOOLBAR (user_data);
  PhotosMainToolbarPrivate *priv = self->priv;

  photos_main_toolbar_clear_request (GD_MAIN_TOOLBAR (widget), self);

  if (priv->window_mode_id != 0)
    {
      g_signal_handler_disconnect (priv->mode_cntrlr, priv->window_mode_id);
      priv->window_mode_id = 0;
    }

  if (priv->selection_mode_id != 0)
    {
      g_signal_handler_disconnect (priv->sel_cntrlr, priv->selection_mode_id);
      priv->selection_mode_id = 0;
    }
}


static void
photos_main_toolbar_go_back_request (GdMainToolbar *toolbar, gpointer user_data)
{
  PhotosMainToolbar *self = PHOTOS_MAIN_TOOLBAR (user_data);
  PhotosMainToolbarPrivate *priv = self->priv;
  PhotosWindowMode mode;

  mode = photos_mode_controller_get_window_mode (priv->mode_cntrlr);
  if (mode == PHOTOS_WINDOW_MODE_PREVIEW)
    photos_mode_controller_set_window_mode (priv->mode_cntrlr, PHOTOS_WINDOW_MODE_OVERVIEW);
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
photos_main_toolbar_set_toolbar_title (PhotosSelectionController *sel_cntrlr, gpointer user_data)
{
  PhotosMainToolbar *self = PHOTOS_MAIN_TOOLBAR (user_data);
  PhotosMainToolbarPrivate *priv = self->priv;
  GdMainToolbarMode mode;

  mode = gd_main_toolbar_get_mode (GD_MAIN_TOOLBAR (priv->widget));
  if (mode == GD_MAIN_TOOLBAR_MODE_OVERVIEW)
    {
    }
  else if (mode == GD_MAIN_TOOLBAR_MODE_PREVIEW)
    {
    }
  else if (mode == GD_MAIN_TOOLBAR_MODE_SELECTION)
    {
    }
}


static void
photos_main_toolbar_populate_for_selection_mode (PhotosMainToolbar *self)
{
  PhotosMainToolbarPrivate *priv = self->priv;

  gd_main_toolbar_set_mode (GD_MAIN_TOOLBAR (priv->widget), GD_MAIN_TOOLBAR_MODE_SELECTION);
  priv->selection_changed_id = g_signal_connect (priv->sel_cntrlr,
                                                 "selection-changed",
                                                 G_CALLBACK (photos_main_toolbar_set_toolbar_title),
                                                 self);

  photos_main_toolbar_set_toolbar_title (priv->sel_cntrlr, self);
  gtk_widget_show_all (priv->widget);
}


static void
photos_main_toolbar_selection_mode_changed (PhotosSelectionController *sel_cntrlr,
                                            gboolean mode,
                                            gpointer user_data)
{
  PhotosMainToolbar *self = PHOTOS_MAIN_TOOLBAR (user_data);
  PhotosMainToolbarPrivate *priv = self->priv;
  PhotosWindowMode window_mode;

  window_mode = photos_mode_controller_get_window_mode (priv->mode_cntrlr);
  if (window_mode != PHOTOS_WINDOW_MODE_OVERVIEW)
    return;

  if (mode)
    photos_main_toolbar_populate_for_selection_mode (self);
  else
    photos_main_toolbar_populate_for_overview (self);
}


static void
photos_main_toolbar_selection_mode_request (GdMainToolbar *toolbar, gboolean request_mode, gpointer user_data)
{
  PhotosMainToolbar *self = PHOTOS_MAIN_TOOLBAR (user_data);
  photos_selection_controller_set_selection_mode (self->priv->sel_cntrlr, request_mode);
}


static void
photos_main_toolbar_window_mode_changed (PhotosModeController *mode_cntrlr,
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

  if (priv->mode_cntrlr != NULL)
    {
      g_object_unref (priv->mode_cntrlr);
      priv->mode_cntrlr = NULL;
    }

  if (priv->sel_cntrlr != NULL)
    {
      g_object_unref (priv->sel_cntrlr);
      priv->sel_cntrlr = NULL;
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

  priv->mode_cntrlr = photos_mode_controller_new ();
  priv->window_mode_id = g_signal_connect (priv->mode_cntrlr,
                                           "window-mode-changed",
                                           G_CALLBACK (photos_main_toolbar_window_mode_changed),
                                           self);

  priv->sel_cntrlr = photos_selection_controller_new ();
  priv->selection_mode_id = g_signal_connect (priv->sel_cntrlr,
                                              "selection-mode-changed",
                                              G_CALLBACK (photos_main_toolbar_selection_mode_changed),
                                              self);

  photos_main_toolbar_window_mode_changed (priv->mode_cntrlr,
                                           photos_mode_controller_get_window_mode (priv->mode_cntrlr),
                                           PHOTOS_WINDOW_MODE_NONE, /* unused */
                                           self);

  g_signal_connect (priv->widget, "destroy", G_CALLBACK (photos_main_toolbar_destroy), self);
  g_signal_connect (priv->widget,
                    "selection-mode-request",
                    G_CALLBACK (photos_main_toolbar_selection_mode_request),
                    self);
  g_signal_connect (priv->widget, "go-back-request", G_CALLBACK (photos_main_toolbar_go_back_request), self);
  g_signal_connect (priv->widget, "clear-request", G_CALLBACK (photos_main_toolbar_clear_request), self);
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
