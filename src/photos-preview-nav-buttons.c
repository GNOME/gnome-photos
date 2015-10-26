/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2013, 2014, 2015 Red Hat, Inc.
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

#include <glib.h>
#include <glib/gi18n.h>

#include "photos-base-manager.h"
#include "photos-icons.h"
#include "photos-preview-model.h"
#include "photos-preview-nav-buttons.h"
#include "photos-search-context.h"
#include "photos-view-model.h"


struct _PhotosPreviewNavButtonsPrivate
{
  GAction *load_next;
  GAction *load_previous;
  GtkGesture *tap_gesture;
  GtkTreeModel *model;
  GtkTreePath *current_path;
  GtkWidget *next_widget;
  GtkWidget *overlay;
  GtkWidget *prev_widget;
  GtkWidget *preview_view;
  PhotosBaseManager *item_mngr;
  gboolean enable_next;
  gboolean enable_prev;
  gboolean visible;
  gboolean visible_internal;
  guint auto_hide_id;
  guint motion_id;
};

enum
{
  PROP_0,
  PROP_OVERLAY,
  PROP_PREVIEW_VIEW
};


G_DEFINE_TYPE_WITH_PRIVATE (PhotosPreviewNavButtons, photos_preview_nav_buttons, G_TYPE_OBJECT);


static void
photos_preview_nav_buttons_fade_in_button (PhotosPreviewNavButtons *self, GtkWidget *widget)
{
  PhotosPreviewNavButtonsPrivate *priv = self->priv;

  if (priv->model == NULL || priv->current_path == NULL)
    return;

  gtk_widget_show_all (widget);
  gtk_revealer_set_reveal_child (GTK_REVEALER (widget), TRUE);
}


static void
photos_preview_nav_buttons_notify_child_revealed (PhotosPreviewNavButtons *self,
                                                  GParamSpec *pspec,
                                                  gpointer user_data)
{
  GtkWidget *widget = GTK_WIDGET (user_data);

  if (!gtk_revealer_get_child_revealed (GTK_REVEALER (widget)))
    gtk_widget_hide (widget);

  g_signal_handlers_disconnect_by_func (widget, photos_preview_nav_buttons_notify_child_revealed, self);
}


static void
photos_preview_nav_buttons_fade_out_button (PhotosPreviewNavButtons *self, GtkWidget *widget)
{
  gtk_revealer_set_reveal_child (GTK_REVEALER (widget), FALSE);
  g_signal_connect_swapped (widget,
                            "notify::child-revealed",
                            G_CALLBACK (photos_preview_nav_buttons_notify_child_revealed),
                            self);
}


static void
photos_preview_nav_buttons_update_visibility (PhotosPreviewNavButtons *self)
{
  PhotosPreviewNavButtonsPrivate *priv = self->priv;
  GtkTreeIter iter;
  GtkTreeIter tmp;

  if (priv->model == NULL
      || priv->current_path == NULL
      || !priv->visible
      || !gtk_tree_model_get_iter (priv->model, &iter, priv->current_path))
    {
      priv->enable_prev = FALSE;
      priv->enable_next = FALSE;
      goto out;
    }

  tmp = iter;
  priv->enable_prev = gtk_tree_model_iter_previous (priv->model, &tmp);

  tmp = iter;
  priv->enable_next = gtk_tree_model_iter_next (priv->model, &tmp);

 out:
  if (priv->visible_internal && priv->enable_next)
    photos_preview_nav_buttons_fade_in_button (self, priv->next_widget);
  else
    photos_preview_nav_buttons_fade_out_button (self, priv->next_widget);

  if (priv->visible_internal && priv->enable_prev)
    photos_preview_nav_buttons_fade_in_button (self, priv->prev_widget);
  else
    photos_preview_nav_buttons_fade_out_button (self, priv->prev_widget);

  g_simple_action_set_enabled (G_SIMPLE_ACTION (priv->load_next), priv->enable_next);
  g_simple_action_set_enabled (G_SIMPLE_ACTION (priv->load_previous), priv->enable_prev);
}


static gboolean
photos_preview_nav_buttons_auto_hide (PhotosPreviewNavButtons *self)
{
  PhotosPreviewNavButtonsPrivate *priv = self->priv;

  priv->auto_hide_id = 0;
  priv->visible_internal = FALSE;
  photos_preview_nav_buttons_update_visibility (self);
  return G_SOURCE_REMOVE;
}


static void
photos_preview_nav_buttons_unqueue_auto_hide (PhotosPreviewNavButtons *self)
{
  PhotosPreviewNavButtonsPrivate *priv = self->priv;

  if (priv->auto_hide_id == 0)
    return;

  g_source_remove (priv->auto_hide_id);
  priv->auto_hide_id = 0;
}


static gboolean
photos_preview_nav_buttons_enter_notify (PhotosPreviewNavButtons *self)
{
  photos_preview_nav_buttons_unqueue_auto_hide (self);
  return FALSE;
}


static void
photos_preview_nav_buttons_queue_auto_hide (PhotosPreviewNavButtons *self)
{
  PhotosPreviewNavButtonsPrivate *priv = self->priv;

  photos_preview_nav_buttons_unqueue_auto_hide (self);
  priv->auto_hide_id = g_timeout_add_seconds_full (G_PRIORITY_DEFAULT,
                                                   2,
                                                   (GSourceFunc) photos_preview_nav_buttons_auto_hide,
                                                   g_object_ref (self),
                                                   g_object_unref);
}


static gboolean
photos_preview_nav_buttons_leave_notify (PhotosPreviewNavButtons *self)
{
  photos_preview_nav_buttons_queue_auto_hide (self);
  return FALSE;
}


static gboolean
photos_preview_nav_buttons_motion_notify_timeout (PhotosPreviewNavButtons *self)
{
  PhotosPreviewNavButtonsPrivate *priv = self->priv;

  priv->motion_id = 0;
  priv->visible_internal = TRUE;
  photos_preview_nav_buttons_update_visibility (self);
  photos_preview_nav_buttons_queue_auto_hide (self);
  return G_SOURCE_REMOVE;
}


static gboolean
photos_preview_nav_buttons_motion_notify (PhotosPreviewNavButtons *self, GdkEventMotion *event)
{
  PhotosPreviewNavButtonsPrivate *priv = self->priv;
  GdkDevice *device;
  GdkInputSource input_source;

  if (priv->motion_id != 0)
    return FALSE;

  device = gdk_event_get_source_device ((GdkEvent *) event);
  input_source = gdk_device_get_source (device);
  if (input_source == GDK_SOURCE_TOUCHSCREEN)
    return FALSE;

  priv->motion_id = g_idle_add_full (G_PRIORITY_DEFAULT,
                                     (GSourceFunc) photos_preview_nav_buttons_motion_notify_timeout,
                                     g_object_ref (self),
                                     g_object_unref);
  return FALSE;
}


static void
photos_preview_nav_buttons_multi_press_released (PhotosPreviewNavButtons *self)
{
  PhotosPreviewNavButtonsPrivate *priv = self->priv;

  gtk_gesture_set_state (GTK_GESTURE (priv->tap_gesture), GTK_EVENT_SEQUENCE_CLAIMED);
  priv->visible_internal = !priv->visible_internal;
  photos_preview_nav_buttons_unqueue_auto_hide (self);
  photos_preview_nav_buttons_update_visibility (self);
}


static void
photos_preview_nav_buttons_multi_press_stopped (PhotosPreviewNavButtons *self)
{
  gtk_gesture_set_state (GTK_GESTURE (self->priv->tap_gesture), GTK_EVENT_SEQUENCE_DENIED);
}


static void
photos_preview_nav_buttons_set_active_path (PhotosPreviewNavButtons *self)
{
  PhotosPreviewNavButtonsPrivate *priv = self->priv;
  GtkTreeIter child_iter;
  GtkTreeIter iter;
  GtkTreeModel *child_model;
  PhotosBaseItem *item;
  gchar *id;

  if (!gtk_tree_model_get_iter (priv->model, &iter, priv->current_path))
    return;

  gtk_tree_model_filter_convert_iter_to_child_iter (GTK_TREE_MODEL_FILTER (priv->model), &child_iter, &iter);
  child_model = gtk_tree_model_filter_get_model (GTK_TREE_MODEL_FILTER (priv->model));
  gtk_tree_model_get (child_model, &child_iter, PHOTOS_VIEW_MODEL_URN, &id, -1);

  item = PHOTOS_BASE_ITEM (photos_base_manager_get_object_by_id (priv->item_mngr, id));
  photos_base_manager_set_active_object (priv->item_mngr, G_OBJECT (item));

  g_free (id);
}


static void
photos_preview_nav_buttons_next (PhotosPreviewNavButtons *self)
{
  PhotosPreviewNavButtonsPrivate *priv = self->priv;

  if (!priv->enable_next)
    return;

  gtk_tree_path_next (priv->current_path);
  photos_preview_nav_buttons_set_active_path (self);
  photos_preview_nav_buttons_update_visibility (self);
}


static void
photos_preview_nav_buttons_previous (PhotosPreviewNavButtons *self)
{
  PhotosPreviewNavButtonsPrivate *priv = self->priv;

  if (!priv->enable_prev)
    return;

  gtk_tree_path_prev (priv->current_path);
  photos_preview_nav_buttons_set_active_path (self);
  photos_preview_nav_buttons_update_visibility (self);
}


static void
photos_preview_nav_buttons_dispose (GObject *object)
{
  PhotosPreviewNavButtons *self = PHOTOS_PREVIEW_NAV_BUTTONS (object);
  PhotosPreviewNavButtonsPrivate *priv = self->priv;

  g_clear_object (&priv->tap_gesture);
  g_clear_object (&priv->model);
  g_clear_object (&priv->overlay);
  g_clear_object (&priv->preview_view);
  g_clear_object (&priv->item_mngr);

  G_OBJECT_CLASS (photos_preview_nav_buttons_parent_class)->dispose (object);
}


static void
photos_preview_nav_buttons_finalize (GObject *object)
{
  PhotosPreviewNavButtons *self = PHOTOS_PREVIEW_NAV_BUTTONS (object);
  PhotosPreviewNavButtonsPrivate *priv = self->priv;

  g_clear_pointer (&priv->current_path, (GDestroyNotify) gtk_tree_path_free);

  G_OBJECT_CLASS (photos_preview_nav_buttons_parent_class)->finalize (object);
}


static void
photos_preview_nav_buttons_constructed (GObject *object)
{
  PhotosPreviewNavButtons *self = PHOTOS_PREVIEW_NAV_BUTTONS (object);
  PhotosPreviewNavButtonsPrivate *priv = self->priv;
  GtkStyleContext *context;
  GtkWidget *button;
  GtkWidget *image;

  G_OBJECT_CLASS (photos_preview_nav_buttons_parent_class)->constructed (object);

  priv->prev_widget = gtk_revealer_new ();
  gtk_widget_set_halign (priv->prev_widget, GTK_ALIGN_START);
  gtk_widget_set_margin_start (priv->prev_widget, 30);
  gtk_widget_set_margin_end (priv->prev_widget, 30);
  gtk_widget_set_valign (priv->prev_widget, GTK_ALIGN_CENTER);
  gtk_revealer_set_transition_type (GTK_REVEALER (priv->prev_widget), GTK_REVEALER_TRANSITION_TYPE_CROSSFADE);
  gtk_overlay_add_overlay (GTK_OVERLAY (priv->overlay), priv->prev_widget);

  image = gtk_image_new_from_icon_name (PHOTOS_ICON_GO_PREVIOUS_SYMBOLIC, GTK_ICON_SIZE_INVALID);
  gtk_image_set_pixel_size (GTK_IMAGE (image), 16);

  button = gtk_button_new ();
  gtk_button_set_image (GTK_BUTTON (button), image);
  gtk_actionable_set_action_name (GTK_ACTIONABLE (button), "app.load-previous");
  context = gtk_widget_get_style_context (button);
  gtk_style_context_add_class (context, "osd");
  gtk_container_add (GTK_CONTAINER (priv->prev_widget), button);
  g_signal_connect_swapped (button,
                            "enter-notify-event",
                            G_CALLBACK (photos_preview_nav_buttons_enter_notify),
                            self);
  g_signal_connect_swapped (button,
                            "leave-notify-event",
                            G_CALLBACK (photos_preview_nav_buttons_leave_notify),
                            self);

  priv->next_widget = gtk_revealer_new ();
  gtk_widget_set_halign (priv->next_widget, GTK_ALIGN_END);
  gtk_widget_set_margin_start (priv->next_widget, 30);
  gtk_widget_set_margin_end (priv->next_widget, 30);
  gtk_widget_set_valign (priv->next_widget, GTK_ALIGN_CENTER);
  gtk_revealer_set_transition_type (GTK_REVEALER (priv->next_widget), GTK_REVEALER_TRANSITION_TYPE_CROSSFADE);
  gtk_overlay_add_overlay (GTK_OVERLAY (priv->overlay), priv->next_widget);

  image = gtk_image_new_from_icon_name (PHOTOS_ICON_GO_NEXT_SYMBOLIC, GTK_ICON_SIZE_INVALID);
  gtk_image_set_pixel_size (GTK_IMAGE (image), 16);

  button = gtk_button_new ();
  gtk_button_set_image (GTK_BUTTON (button), image);
  gtk_actionable_set_action_name (GTK_ACTIONABLE (button), "app.load-next");
  context = gtk_widget_get_style_context (button);
  gtk_style_context_add_class (context, "osd");
  gtk_container_add (GTK_CONTAINER (priv->next_widget), button);
  g_signal_connect_swapped (button,
                            "enter-notify-event",
                            G_CALLBACK (photos_preview_nav_buttons_enter_notify),
                            self);
  g_signal_connect_swapped (button,
                            "leave-notify-event",
                            G_CALLBACK (photos_preview_nav_buttons_leave_notify),
                            self);

  g_signal_connect_swapped (priv->overlay,
                            "motion-notify-event",
                            G_CALLBACK (photos_preview_nav_buttons_motion_notify),
                            self);

  priv->tap_gesture = gtk_gesture_multi_press_new (priv->preview_view);
  gtk_gesture_single_set_touch_only (GTK_GESTURE_SINGLE (priv->tap_gesture), TRUE);
  g_signal_connect_swapped (priv->tap_gesture,
                            "released",
                            G_CALLBACK (photos_preview_nav_buttons_multi_press_released),
                            self);
  g_signal_connect_swapped (priv->tap_gesture,
                            "stopped",
                            G_CALLBACK (photos_preview_nav_buttons_multi_press_stopped),
                            self);
}


static void
photos_preview_nav_buttons_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
  PhotosPreviewNavButtons *self = PHOTOS_PREVIEW_NAV_BUTTONS (object);
  PhotosPreviewNavButtonsPrivate *priv = self->priv;

  switch (prop_id)
    {
    case PROP_OVERLAY:
      priv->overlay = GTK_WIDGET (g_value_dup_object (value));
      break;

    case PROP_PREVIEW_VIEW:
      priv->preview_view = GTK_WIDGET (g_value_dup_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
photos_preview_nav_buttons_init (PhotosPreviewNavButtons *self)
{
  PhotosPreviewNavButtonsPrivate *priv;
  GApplication *app;
  PhotosSearchContextState *state;

  self->priv = photos_preview_nav_buttons_get_instance_private (self);
  priv = self->priv;

  app = g_application_get_default ();
  state = photos_search_context_get_state (PHOTOS_SEARCH_CONTEXT (app));

  priv->load_next = g_action_map_lookup_action (G_ACTION_MAP (app), "load-next");
  g_signal_connect_object (priv->load_next,
                           "activate",
                           G_CALLBACK (photos_preview_nav_buttons_next),
                           self,
                           G_CONNECT_SWAPPED);

  priv->load_previous = g_action_map_lookup_action (G_ACTION_MAP (app), "load-previous");
  g_signal_connect_object (priv->load_previous,
                           "activate",
                           G_CALLBACK (photos_preview_nav_buttons_previous),
                           self,
                           G_CONNECT_SWAPPED);

  priv->item_mngr = g_object_ref (state->item_mngr);
}


static void
photos_preview_nav_buttons_class_init (PhotosPreviewNavButtonsClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->constructed = photos_preview_nav_buttons_constructed;
  object_class->dispose = photos_preview_nav_buttons_dispose;
  object_class->finalize = photos_preview_nav_buttons_finalize;
  object_class->set_property = photos_preview_nav_buttons_set_property;

  g_object_class_install_property (object_class,
                                   PROP_OVERLAY,
                                   g_param_spec_object ("overlay",
                                                        "GtkOverlay object",
                                                        "The stack overlay widget",
                                                        GTK_TYPE_OVERLAY,
                                                        G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE));

  g_object_class_install_property (object_class,
                                   PROP_PREVIEW_VIEW,
                                   g_param_spec_object ("preview-view",
                                                        "PhotosPreviewView object",
                                                        "The widget used for showing the preview",
                                                        PHOTOS_TYPE_PREVIEW_VIEW,
                                                        G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE));
}


PhotosPreviewNavButtons *
photos_preview_nav_buttons_new (PhotosPreviewView *preview_view, GtkOverlay *overlay)
{
  return g_object_new (PHOTOS_TYPE_PREVIEW_NAV_BUTTONS, "preview-view", preview_view, "overlay", overlay, NULL);
}


void
photos_preview_nav_buttons_hide (PhotosPreviewNavButtons *self)
{
  PhotosPreviewNavButtonsPrivate *priv = self->priv;

  priv->visible = FALSE;
  priv->visible_internal = FALSE;
  photos_preview_nav_buttons_update_visibility (self);
}


void
photos_preview_nav_buttons_set_model (PhotosPreviewNavButtons *self,
                                      GtkTreeModel *child_model,
                                      GtkTreePath *current_child_path)
{
  PhotosPreviewNavButtonsPrivate *priv = self->priv;

  g_clear_object (&priv->model);
  if (child_model != NULL)
    priv->model = photos_preview_model_new (child_model);

  g_clear_pointer (&priv->current_path, (GDestroyNotify) gtk_tree_path_free);
  if (child_model != NULL && current_child_path != NULL)
    {
      priv->current_path = gtk_tree_model_filter_convert_child_path_to_path (GTK_TREE_MODEL_FILTER (priv->model),
                                                                             current_child_path);
    }
}


void
photos_preview_nav_buttons_show (PhotosPreviewNavButtons *self)
{
  PhotosPreviewNavButtonsPrivate *priv = self->priv;

  priv->visible = TRUE;
  priv->visible_internal = TRUE;
  photos_preview_nav_buttons_update_visibility (self);
  photos_preview_nav_buttons_queue_auto_hide (self);
}
