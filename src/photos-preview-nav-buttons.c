/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2013 – 2017 Red Hat, Inc.
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
#include "photos-delete-notification.h"
#include "photos-icons.h"
#include "photos-preview-nav-buttons.h"
#include "photos-search-context.h"
#include "photos-zoom-controls.h"


struct _PhotosPreviewNavButtons
{
  GObject parent_instance;
  GAction *load_next;
  GAction *load_previous;
  GtkGesture *long_press_gesture;
  GtkGesture *tap_gesture;
  GtkWidget *next_widget;
  GtkWidget *overlay;
  GtkWidget *prev_widget;
  GtkWidget *preview_view;
  GtkWidget *zoom_revealer;
  GtkWidget *zoom_widget;
  PhotosBaseManager *item_mngr;
  PhotosModeController *mode_cntrlr;
  PhotosWindowMode old_mode;
  gboolean auto_hide;
  gboolean enable_next;
  gboolean enable_prev;
  gboolean show_navigation;
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

enum
{
  LOAD_NEXT,
  LOAD_PREVIOUS,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };


G_DEFINE_TYPE (PhotosPreviewNavButtons, photos_preview_nav_buttons, G_TYPE_OBJECT);


static void
photos_preview_nav_buttons_delete (PhotosPreviewNavButtons *self)
{
  GList *items = NULL;
  PhotosBaseItem *item;
  PhotosWindowMode mode;

  mode = photos_mode_controller_get_window_mode (self->mode_cntrlr);
  if (mode != PHOTOS_WINDOW_MODE_PREVIEW)
    return;

  item = PHOTOS_BASE_ITEM (photos_base_manager_get_active_object (self->item_mngr));
  if (item == NULL)
    return;

  if (self->enable_next)
    g_action_activate (self->load_next, NULL);
  else if (self->enable_prev)
    g_action_activate (self->load_previous, NULL);
  else
    photos_mode_controller_go_back (self->mode_cntrlr);

  items = g_list_prepend (items, g_object_ref (item));
  photos_item_manager_hide_item (PHOTOS_ITEM_MANAGER (self->item_mngr), item);
  photos_delete_notification_new (items);
  g_list_free_full (items, g_object_unref);
}


static void
photos_preview_nav_buttons_fade_in_button (PhotosPreviewNavButtons *self, GtkWidget *widget)
{
  PhotosBaseItem *item;

  if (self->old_mode == PHOTOS_WINDOW_MODE_NONE)
    return;

  item = PHOTOS_BASE_ITEM (photos_base_manager_get_active_object (self->item_mngr));
  if (item == NULL)
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


static PhotosBaseItem *
photos_preview_nav_buttons_get_next_item (PhotosPreviewNavButtons *self)
{
  PhotosBaseItem *item;
  PhotosBaseItem *next_item;
  PhotosBaseManager *item_mngr_chld;

  item = PHOTOS_BASE_ITEM (photos_base_manager_get_active_object (self->item_mngr));
  item_mngr_chld = photos_item_manager_get_for_mode (PHOTOS_ITEM_MANAGER (self->item_mngr), self->old_mode);

  do
    {
      next_item = PHOTOS_BASE_ITEM (photos_base_manager_get_next_object (item_mngr_chld, G_OBJECT (item)));
      item = next_item;
    } while (item != NULL && photos_base_item_is_collection (item));

  return next_item;
}


static PhotosBaseItem *
photos_preview_nav_buttons_get_previous_item (PhotosPreviewNavButtons *self)
{
  PhotosBaseItem *item;
  PhotosBaseItem *previous_item;
  PhotosBaseManager *item_mngr_chld;

  item = PHOTOS_BASE_ITEM (photos_base_manager_get_active_object (self->item_mngr));
  item_mngr_chld = photos_item_manager_get_for_mode (PHOTOS_ITEM_MANAGER (self->item_mngr), self->old_mode);

  do
    {
      previous_item = PHOTOS_BASE_ITEM (photos_base_manager_get_previous_object (item_mngr_chld, G_OBJECT (item)));
      item = previous_item;
    } while (item != NULL && photos_base_item_is_collection (item));

  return previous_item;
}


static void
photos_preview_nav_buttons_update_visibility (PhotosPreviewNavButtons *self)
{
  PhotosBaseItem *item;
  gboolean enable_zoom = TRUE;

  item = PHOTOS_BASE_ITEM (photos_base_manager_get_active_object (self->item_mngr));

  if (self->old_mode == PHOTOS_WINDOW_MODE_NONE || item == NULL || !self->visible)
    {
      self->enable_prev = FALSE;
      self->enable_next = FALSE;
    }
  else
    {
      PhotosBaseItem *next_item;
      PhotosBaseItem *previous_item;

      previous_item = photos_preview_nav_buttons_get_previous_item (self);
      self->enable_prev = previous_item != NULL;

      next_item = photos_preview_nav_buttons_get_next_item (self);
      self->enable_next = next_item != NULL;
    }

  if (item == NULL || !self->visible)
    enable_zoom = FALSE;

  if (self->visible_internal && self->show_navigation && self->enable_next)
    photos_preview_nav_buttons_fade_in_button (self, self->next_widget);
  else
    photos_preview_nav_buttons_fade_out_button (self, self->next_widget);

  if (self->visible_internal && self->show_navigation && self->enable_prev)
    photos_preview_nav_buttons_fade_in_button (self, self->prev_widget);
  else
    photos_preview_nav_buttons_fade_out_button (self, self->prev_widget);

  if (self->visible_internal && enable_zoom)
    photos_preview_nav_buttons_fade_in_button (self, self->zoom_revealer);
  else
    photos_preview_nav_buttons_fade_out_button (self, self->zoom_revealer);

  g_simple_action_set_enabled (G_SIMPLE_ACTION (self->load_next), self->enable_next);
  g_simple_action_set_enabled (G_SIMPLE_ACTION (self->load_previous), self->enable_prev);
}


static gboolean
photos_preview_nav_buttons_auto_hide (PhotosPreviewNavButtons *self)
{
  self->auto_hide_id = 0;
  self->visible_internal = FALSE;
  photos_preview_nav_buttons_update_visibility (self);
  return G_SOURCE_REMOVE;
}


static void
photos_preview_nav_buttons_unqueue_auto_hide (PhotosPreviewNavButtons *self)
{
  if (self->auto_hide_id == 0)
    return;

  g_source_remove (self->auto_hide_id);
  self->auto_hide_id = 0;
}


static gboolean
photos_preview_nav_buttons_enter_notify (PhotosPreviewNavButtons *self)
{
  photos_preview_nav_buttons_unqueue_auto_hide (self);
  return FALSE;
}


static void
photos_preview_nav_buttons_items_changed (PhotosPreviewNavButtons *self)
{
  photos_preview_nav_buttons_update_visibility (self);
}


static void
photos_preview_nav_buttons_queue_auto_hide (PhotosPreviewNavButtons *self)
{
  photos_preview_nav_buttons_unqueue_auto_hide (self);

  if (self->auto_hide)
    self->auto_hide_id = g_timeout_add_seconds (2, (GSourceFunc) photos_preview_nav_buttons_auto_hide, self);
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
  self->motion_id = 0;
  self->visible_internal = TRUE;
  photos_preview_nav_buttons_update_visibility (self);

  if (!photos_zoom_controls_get_hover (PHOTOS_ZOOM_CONTROLS (self->zoom_widget)))
    photos_preview_nav_buttons_queue_auto_hide (self);

  return G_SOURCE_REMOVE;
}


static gboolean
photos_preview_nav_buttons_motion_notify (PhotosPreviewNavButtons *self, GdkEventMotion *event)
{
  GdkDevice *device;
  GdkInputSource input_source;

  if (self->motion_id != 0)
    return FALSE;

  if (!self->auto_hide)
    return FALSE;

  device = gdk_event_get_source_device ((GdkEvent *) event);
  input_source = gdk_device_get_source (device);
  if (input_source == GDK_SOURCE_TOUCHSCREEN)
    return FALSE;

  self->motion_id = g_idle_add_full (G_PRIORITY_DEFAULT,
                                     (GSourceFunc) photos_preview_nav_buttons_motion_notify_timeout,
                                     g_object_ref (self),
                                     g_object_unref);
  return FALSE;
}


static void
photos_preview_nav_buttons_long_press_pressed (PhotosPreviewNavButtons *self)
{
  gtk_gesture_set_state (self->long_press_gesture, GTK_EVENT_SEQUENCE_DENIED);
}


static void
photos_preview_nav_buttons_multi_press_end (PhotosPreviewNavButtons *self, GdkEventSequence *sequence)
{
  GtkEventSequenceState state;

  state = gtk_gesture_get_sequence_state (self->tap_gesture, sequence);
  if (state == GTK_EVENT_SEQUENCE_DENIED)
    return;

  gtk_gesture_set_state (self->tap_gesture, GTK_EVENT_SEQUENCE_CLAIMED);

  if (self->auto_hide)
    {
      self->visible_internal = !self->visible_internal;
      photos_preview_nav_buttons_unqueue_auto_hide (self);
      photos_preview_nav_buttons_update_visibility (self);
    }
}


static void
photos_preview_nav_buttons_next (PhotosPreviewNavButtons *self)
{
  PhotosBaseItem *item;
  PhotosBaseItem *next_item;

  if (!self->enable_next)
    return;

  item = PHOTOS_BASE_ITEM (photos_base_manager_get_active_object (self->item_mngr));
  g_return_if_fail (PHOTOS_IS_BASE_ITEM (item));

  next_item = photos_preview_nav_buttons_get_next_item (self);
  g_return_if_fail (PHOTOS_IS_BASE_ITEM (next_item));

  g_signal_emit (self, signals[LOAD_NEXT], 0);

  photos_base_manager_set_active_object (self->item_mngr, G_OBJECT (next_item));
  photos_preview_nav_buttons_update_visibility (self);
}


static void
photos_preview_nav_buttons_previous (PhotosPreviewNavButtons *self)
{
  PhotosBaseItem *item;
  PhotosBaseItem *previous_item;

  if (!self->enable_prev)
    return;

  item = PHOTOS_BASE_ITEM (photos_base_manager_get_active_object (self->item_mngr));
  g_return_if_fail (PHOTOS_IS_BASE_ITEM (item));

  previous_item = photos_preview_nav_buttons_get_previous_item (self);
  g_return_if_fail (PHOTOS_IS_BASE_ITEM (previous_item));

  g_signal_emit (self, signals[LOAD_PREVIOUS], 0);

  photos_base_manager_set_active_object (self->item_mngr, G_OBJECT (previous_item));
  photos_preview_nav_buttons_update_visibility (self);
}


static void
photos_preview_nav_buttons_zoom_notify_hover (PhotosPreviewNavButtons *self)
{
  if (photos_zoom_controls_get_hover (PHOTOS_ZOOM_CONTROLS (self->zoom_widget)))
    photos_preview_nav_buttons_unqueue_auto_hide (self);
  else
    photos_preview_nav_buttons_leave_notify (self);
}


static void
photos_preview_nav_buttons_dispose (GObject *object)
{
  PhotosPreviewNavButtons *self = PHOTOS_PREVIEW_NAV_BUTTONS (object);

  photos_preview_nav_buttons_unqueue_auto_hide (self);

  g_clear_object (&self->long_press_gesture);
  g_clear_object (&self->tap_gesture);
  g_clear_object (&self->next_widget);
  g_clear_object (&self->prev_widget);
  g_clear_object (&self->zoom_revealer);
  g_clear_object (&self->zoom_widget);
  g_clear_object (&self->item_mngr);
  g_clear_object (&self->mode_cntrlr);

  G_OBJECT_CLASS (photos_preview_nav_buttons_parent_class)->dispose (object);
}


static void
photos_preview_nav_buttons_constructed (GObject *object)
{
  PhotosPreviewNavButtons *self = PHOTOS_PREVIEW_NAV_BUTTONS (object);
  GtkStyleContext *context;
  GtkWidget *button;
  GtkWidget *image;

  G_OBJECT_CLASS (photos_preview_nav_buttons_parent_class)->constructed (object);

  self->zoom_revealer = g_object_ref_sink (gtk_revealer_new ());
  gtk_widget_set_halign (self->zoom_revealer, GTK_ALIGN_END);
  gtk_widget_set_margin_bottom (self->zoom_revealer, 30);
  gtk_widget_set_margin_end (self->zoom_revealer, 30);
  gtk_widget_set_margin_start (self->zoom_revealer, 30);
  gtk_widget_set_margin_top (self->zoom_revealer, 30);
  gtk_widget_set_valign (self->zoom_revealer, GTK_ALIGN_END);
  gtk_revealer_set_transition_type (GTK_REVEALER (self->zoom_revealer), GTK_REVEALER_TRANSITION_TYPE_CROSSFADE);
  gtk_overlay_add_overlay (GTK_OVERLAY (self->overlay), self->zoom_revealer);

  self->zoom_widget = g_object_ref_sink (photos_zoom_controls_new ());
  context = gtk_widget_get_style_context (self->zoom_widget);
  gtk_style_context_add_class (context, "osd");
  gtk_container_add (GTK_CONTAINER (self->zoom_revealer), self->zoom_widget);
  g_signal_connect_swapped (self->zoom_widget,
                            "notify::hover",
                            G_CALLBACK (photos_preview_nav_buttons_zoom_notify_hover),
                            self);

  self->prev_widget = g_object_ref_sink (gtk_revealer_new ());
  gtk_widget_set_halign (self->prev_widget, GTK_ALIGN_START);
  gtk_widget_set_margin_start (self->prev_widget, 30);
  gtk_widget_set_margin_end (self->prev_widget, 30);
  gtk_widget_set_valign (self->prev_widget, GTK_ALIGN_CENTER);
  gtk_revealer_set_transition_type (GTK_REVEALER (self->prev_widget), GTK_REVEALER_TRANSITION_TYPE_CROSSFADE);
  gtk_overlay_add_overlay (GTK_OVERLAY (self->overlay), self->prev_widget);

  image = gtk_image_new_from_icon_name (PHOTOS_ICON_GO_PREVIOUS_SYMBOLIC, GTK_ICON_SIZE_INVALID);
  gtk_image_set_pixel_size (GTK_IMAGE (image), 16);

  button = gtk_button_new ();
  gtk_button_set_image (GTK_BUTTON (button), image);
  gtk_actionable_set_action_name (GTK_ACTIONABLE (button), "app.load-previous");
  context = gtk_widget_get_style_context (button);
  gtk_style_context_add_class (context, "osd");
  gtk_container_add (GTK_CONTAINER (self->prev_widget), button);
  g_signal_connect_swapped (button,
                            "enter-notify-event",
                            G_CALLBACK (photos_preview_nav_buttons_enter_notify),
                            self);
  g_signal_connect_swapped (button,
                            "leave-notify-event",
                            G_CALLBACK (photos_preview_nav_buttons_leave_notify),
                            self);

  self->next_widget = g_object_ref_sink (gtk_revealer_new ());
  gtk_widget_set_halign (self->next_widget, GTK_ALIGN_END);
  gtk_widget_set_margin_start (self->next_widget, 30);
  gtk_widget_set_margin_end (self->next_widget, 30);
  gtk_widget_set_valign (self->next_widget, GTK_ALIGN_CENTER);
  gtk_revealer_set_transition_type (GTK_REVEALER (self->next_widget), GTK_REVEALER_TRANSITION_TYPE_CROSSFADE);
  gtk_overlay_add_overlay (GTK_OVERLAY (self->overlay), self->next_widget);

  image = gtk_image_new_from_icon_name (PHOTOS_ICON_GO_NEXT_SYMBOLIC, GTK_ICON_SIZE_INVALID);
  gtk_image_set_pixel_size (GTK_IMAGE (image), 16);

  button = gtk_button_new ();
  gtk_button_set_image (GTK_BUTTON (button), image);
  gtk_actionable_set_action_name (GTK_ACTIONABLE (button), "app.load-next");
  context = gtk_widget_get_style_context (button);
  gtk_style_context_add_class (context, "osd");
  gtk_container_add (GTK_CONTAINER (self->next_widget), button);
  g_signal_connect_swapped (button,
                            "enter-notify-event",
                            G_CALLBACK (photos_preview_nav_buttons_enter_notify),
                            self);
  g_signal_connect_swapped (button,
                            "leave-notify-event",
                            G_CALLBACK (photos_preview_nav_buttons_leave_notify),
                            self);

  g_signal_connect_swapped (self->overlay,
                            "motion-notify-event",
                            G_CALLBACK (photos_preview_nav_buttons_motion_notify),
                            self);

  self->long_press_gesture = gtk_gesture_long_press_new (self->preview_view);
  gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (self->long_press_gesture), GTK_PHASE_CAPTURE);
  gtk_gesture_single_set_touch_only (GTK_GESTURE_SINGLE (self->long_press_gesture), TRUE);
  g_signal_connect_swapped (self->long_press_gesture,
                            "pressed",
                            G_CALLBACK (photos_preview_nav_buttons_long_press_pressed),
                            self);

  self->tap_gesture = gtk_gesture_multi_press_new (self->preview_view);
  gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (self->tap_gesture), GTK_PHASE_CAPTURE);
  gtk_gesture_single_set_touch_only (GTK_GESTURE_SINGLE (self->tap_gesture), TRUE);
  gtk_gesture_group (self->long_press_gesture, self->tap_gesture);
  g_signal_connect_swapped (self->tap_gesture,
                            "end",
                            G_CALLBACK (photos_preview_nav_buttons_multi_press_end),
                            self);

  /* We will not need them any more */
  self->overlay = NULL;
  self->preview_view = NULL;
}


static void
photos_preview_nav_buttons_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
  PhotosPreviewNavButtons *self = PHOTOS_PREVIEW_NAV_BUTTONS (object);

  switch (prop_id)
    {
    case PROP_OVERLAY:
      self->overlay = GTK_WIDGET (g_value_get_object (value)); /* overlay is owned by preview_view */
      break;

    case PROP_PREVIEW_VIEW:
      self->preview_view = GTK_WIDGET (g_value_get_object (value)); /* self is owned by preview_view */
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
photos_preview_nav_buttons_init (PhotosPreviewNavButtons *self)
{
  GAction *action;
  GApplication *app;
  PhotosSearchContextState *state;

  app = g_application_get_default ();
  state = photos_search_context_get_state (PHOTOS_SEARCH_CONTEXT (app));

  self->load_next = g_action_map_lookup_action (G_ACTION_MAP (app), "load-next");
  g_signal_connect_object (self->load_next,
                           "activate",
                           G_CALLBACK (photos_preview_nav_buttons_next),
                           self,
                           G_CONNECT_SWAPPED);

  self->load_previous = g_action_map_lookup_action (G_ACTION_MAP (app), "load-previous");
  g_signal_connect_object (self->load_previous,
                           "activate",
                           G_CALLBACK (photos_preview_nav_buttons_previous),
                           self,
                           G_CONNECT_SWAPPED);

  self->item_mngr = g_object_ref (state->item_mngr);
  g_signal_connect_object (self->item_mngr,
                           "items-changed",
                           G_CALLBACK (photos_preview_nav_buttons_items_changed),
                           self,
                           G_CONNECT_SWAPPED);

  self->mode_cntrlr = g_object_ref (state->mode_cntrlr);

  action = g_action_map_lookup_action (G_ACTION_MAP (app), "delete");
  g_signal_connect_object (action,
                           "activate",
                           G_CALLBACK (photos_preview_nav_buttons_delete),
                           self,
                           G_CONNECT_SWAPPED);
}


static void
photos_preview_nav_buttons_class_init (PhotosPreviewNavButtonsClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->constructed = photos_preview_nav_buttons_constructed;
  object_class->dispose = photos_preview_nav_buttons_dispose;
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

  signals[LOAD_NEXT] = g_signal_new ("load-next",
                                     G_TYPE_FROM_CLASS (class),
                                     G_SIGNAL_RUN_LAST,
                                     0,
                                     NULL, /*accumulator */
                                     NULL, /*accu_data */
                                     g_cclosure_marshal_VOID__VOID,
                                     G_TYPE_NONE,
                                     0);

  signals[LOAD_PREVIOUS] = g_signal_new ("load-previous",
                                         G_TYPE_FROM_CLASS (class),
                                         G_SIGNAL_RUN_LAST,
                                         0,
                                         NULL, /*accumulator */
                                         NULL, /*accu_data */
                                         g_cclosure_marshal_VOID__VOID,
                                         G_TYPE_NONE,
                                         0);
}


PhotosPreviewNavButtons *
photos_preview_nav_buttons_new (PhotosPreviewView *preview_view, GtkOverlay *overlay)
{
  g_return_val_if_fail (PHOTOS_IS_PREVIEW_VIEW (preview_view), NULL);
  g_return_val_if_fail (GTK_IS_OVERLAY (overlay), NULL);

  return g_object_new (PHOTOS_TYPE_PREVIEW_NAV_BUTTONS, "preview-view", preview_view, "overlay", overlay, NULL);
}


void
photos_preview_nav_buttons_hide (PhotosPreviewNavButtons *self)
{
  g_return_if_fail (PHOTOS_IS_PREVIEW_NAV_BUTTONS (self));

  self->visible = FALSE;
  self->visible_internal = FALSE;
  photos_preview_nav_buttons_update_visibility (self);
}


void
photos_preview_nav_buttons_set_auto_hide (PhotosPreviewNavButtons *self, gboolean auto_hide)
{
  g_return_if_fail (PHOTOS_IS_PREVIEW_NAV_BUTTONS (self));

  if (self->auto_hide == auto_hide)
    return;

  self->auto_hide = auto_hide;
  self->visible_internal = self->visible;
  photos_preview_nav_buttons_update_visibility (self);

  if (self->auto_hide)
    photos_preview_nav_buttons_queue_auto_hide (self);
  else
    photos_preview_nav_buttons_unqueue_auto_hide (self);
}


void
photos_preview_nav_buttons_set_mode (PhotosPreviewNavButtons *self, PhotosWindowMode old_mode)
{
  g_return_if_fail (PHOTOS_IS_PREVIEW_NAV_BUTTONS (self));
  g_return_if_fail (old_mode != PHOTOS_WINDOW_MODE_EDIT);
  g_return_if_fail (old_mode != PHOTOS_WINDOW_MODE_PREVIEW);

  self->old_mode = old_mode;
}


void
photos_preview_nav_buttons_set_show_navigation (PhotosPreviewNavButtons *self, gboolean show_navigation)
{
  g_return_if_fail (PHOTOS_IS_PREVIEW_NAV_BUTTONS (self));

  if (self->show_navigation == show_navigation)
    return;

  self->show_navigation = show_navigation;
  photos_preview_nav_buttons_update_visibility (self);
}


void
photos_preview_nav_buttons_show (PhotosPreviewNavButtons *self)
{
  g_return_if_fail (PHOTOS_IS_PREVIEW_NAV_BUTTONS (self));

  self->visible = TRUE;
  self->visible_internal = TRUE;
  photos_preview_nav_buttons_update_visibility (self);
  photos_preview_nav_buttons_queue_auto_hide (self);
}
