/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2013 – 2021 Red Hat, Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/* Based on code from:
 *   + Documents
 */


#include "config.h"

#include <math.h>

#include <glib.h>
#include <gio/gio.h>
#include <glib/gi18n.h>

#include "photos-application.h"
#include "photos-base-item.h"
#include "photos-done-notification.h"
#include "photos-gesture-zoom.h"
#include "photos-image-view.h"
#include "photos-edit-palette.h"
#include "photos-operation-insta-common.h"
#include "photos-preview-nav-buttons.h"
#include "photos-preview-view.h"
#include "photos-search-context.h"
#include "photos-tool.h"
#include "photos-utils.h"


struct _PhotosPreviewView
{
  GtkBin parent_instance;
  GAction *zoom_begin_action;
  GAction *zoom_best_fit_action;
  GAction *zoom_end_action;
  GAction *zoom_in_action;
  GAction *zoom_out_action;
  GCancellable *cancellable;
  GeglNode *node;
  GtkWidget *overlay;
  GtkWidget *palette;
  GtkWidget *revealer;
  GtkWidget *stack;
  PhotosBaseManager *item_mngr;
  PhotosGestureZoom *gesture_zoom;
  PhotosModeController *mode_cntrlr;
  PhotosPreviewNavButtons *nav_buttons;
  PhotosTool *current_tool;
  gboolean grabbed;
  gdouble event_x_last;
  gdouble event_y_last;
  gdouble zoom_begin;
  gdouble zoom_best_fit;
};

enum
{
  PROP_0,
  PROP_OVERLAY
};


G_DEFINE_TYPE (PhotosPreviewView, photos_preview_view, GTK_TYPE_BIN);


static const gdouble ZOOM_BEST_FIT_FACTOR_TOUCH = 0.6;
static const gdouble ZOOM_FACTOR_1 = 2.8561;
static const gdouble ZOOM_FACTOR_2 = 1.69;
static const gdouble ZOOM_FACTOR_3 = 1.3;


static GtkWidget *photos_preview_view_create_view_with_container (PhotosPreviewView *self);


static GtkWidget *
photos_preview_view_get_view_from_view_container (GtkWidget *view_container)
{
  GtkWidget *child;
  GtkWidget *view;

  child = gtk_bin_get_child (GTK_BIN (view_container));

  if (GTK_IS_VIEWPORT (child))
    view = gtk_bin_get_child (GTK_BIN (child));
  else
    view = child;

  return view;
}


static gboolean
photos_preview_view_button_press_event (GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
  PhotosPreviewView *self = PHOTOS_PREVIEW_VIEW (user_data);
  GtkWidget *current_view;
  GtkWidget *current_view_container;
  gboolean ret_val = GDK_EVENT_PROPAGATE;

  current_view_container = gtk_stack_get_visible_child (GTK_STACK (self->stack));
  current_view = photos_preview_view_get_view_from_view_container (current_view_container);
  g_return_val_if_fail (widget == current_view, GDK_EVENT_PROPAGATE);

  if (self->current_tool == NULL)
    {
      switch (event->button.button)
        {
        case 1:
          {
            if (photos_utils_scrolled_window_can_scroll (GTK_SCROLLED_WINDOW (current_view_container)))
              {
                GdkCursor *cursor = NULL;
                GdkDisplay *display;
                GdkWindow *window;

                window = gtk_widget_get_window (widget);
                display = gdk_window_get_display (window);
                cursor = gdk_cursor_new_for_display (display, GDK_FLEUR);
                gdk_window_set_cursor (window, cursor);

                self->grabbed = TRUE;
                self->event_x_last = event->button.x;
                self->event_y_last = event->button.y;

                ret_val = GDK_EVENT_STOP;

                g_object_unref (cursor);
              }

            break;
          }

        default:
          break;
        }
    }
  else
    {
      switch (event->button.button)
        {
        case 1:
          ret_val = photos_tool_left_click_event (self->current_tool, &(event->button));
          break;

        default:
          break;
        }
    }

  return ret_val;
}


static gboolean
photos_preview_view_button_release_event (GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
  PhotosPreviewView *self = PHOTOS_PREVIEW_VIEW (user_data);
  GtkWidget *current_view;
  GtkWidget *current_view_container;
  gboolean ret_val = GDK_EVENT_PROPAGATE;

  current_view_container = gtk_stack_get_visible_child (GTK_STACK (self->stack));
  current_view = photos_preview_view_get_view_from_view_container (current_view_container);
  g_return_val_if_fail (widget == current_view, GDK_EVENT_PROPAGATE);

  if (self->current_tool == NULL)
    {
      if (!self->grabbed)
        goto out;

      switch (event->button.button)
        {
        case 1:
          {
            GdkWindow *window;

            window = gtk_widget_get_window (widget);
            gdk_window_set_cursor (window, NULL);
            self->grabbed = FALSE;
            self->event_x_last = -1.0;
            self->event_y_last = -1.0;
            break;
          }

        default:
          break;
        }
    }
  else
    {
      switch (event->button.button)
        {
        case 1:
          ret_val = photos_tool_left_unclick_event (self->current_tool, &(event->button));
          break;

        default:
          break;
        }
    }

 out:
  return ret_val;
}


static void
photos_preview_view_draw_overlay (PhotosPreviewView *self, cairo_t *cr, GdkRectangle *rect, gpointer user_data)
{
  if (self->current_tool == NULL)
    return;

  photos_tool_draw (self->current_tool, cr, rect);
}


static void
photos_preview_view_gesture_zoom_begin (PhotosPreviewView *self)
{
  GVariant *parameter;
  GtkGesture *gesture_zoom;

  gesture_zoom = photos_gesture_zoom_get_gesture (self->gesture_zoom);
  gtk_gesture_set_state (gesture_zoom, GTK_EVENT_SEQUENCE_CLAIMED);

  parameter = photos_utils_create_zoom_target_value (0.0, PHOTOS_ZOOM_EVENT_TOUCH);
  g_action_activate (self->zoom_begin_action, parameter);
}


static void
photos_preview_view_gesture_zoom_direction_changed (PhotosPreviewView *self)
{
  GVariant *parameter;

  parameter = photos_utils_create_zoom_target_value (0.0, PHOTOS_ZOOM_EVENT_TOUCH);
  g_action_activate (self->zoom_end_action, parameter);

  parameter = photos_utils_create_zoom_target_value (0.0, PHOTOS_ZOOM_EVENT_TOUCH);
  g_action_activate (self->zoom_begin_action, parameter);
}


static void
photos_preview_view_gesture_zoom_end (PhotosPreviewView *self)
{
  GVariant *parameter;

  parameter = photos_utils_create_zoom_target_value (0.0, PHOTOS_ZOOM_EVENT_TOUCH);
  g_action_activate (self->zoom_end_action, parameter);
}


static void
photos_preview_view_gesture_zoom_scale_changed (PhotosPreviewView *self,
                                                gdouble scale,
                                                PhotosGestureZoomDirection direction)
{
  GVariant *parameter;

  parameter = photos_utils_create_zoom_target_value (scale, PHOTOS_ZOOM_EVENT_TOUCH);

  switch (direction)
    {
    case PHOTOS_GESTURE_ZOOM_DIRECTION_DECREASING:
      g_action_activate (self->zoom_out_action, parameter);
      break;

    case PHOTOS_GESTURE_ZOOM_DIRECTION_INCREASING:
      g_action_activate (self->zoom_in_action, parameter);
      break;

    case PHOTOS_GESTURE_ZOOM_DIRECTION_NONE:
    default:
      g_assert_not_reached ();
      break;
    }
}


static GtkWidget *
photos_preview_view_get_invisible_child (PhotosPreviewView *self)
{
  GList *children;
  GList *l;
  GtkWidget *current_view_container;
  GtkWidget *next_view_container = NULL;

  current_view_container = gtk_stack_get_visible_child (GTK_STACK (self->stack));
  children = gtk_container_get_children (GTK_CONTAINER (self->stack));
  for (l = children; l != NULL; l = l->next)
    {
      GtkWidget *view_container = GTK_WIDGET (l->data);

      if (current_view_container != view_container)
        {
          next_view_container = view_container;
          break;
        }
    }

  g_list_free (children);
  return next_view_container;
}


static gboolean
photos_preview_view_motion_notify_event (GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
  PhotosPreviewView *self = PHOTOS_PREVIEW_VIEW (user_data);
  GtkWidget *current_view;
  GtkWidget *current_view_container;
  gboolean ret_val = GDK_EVENT_PROPAGATE;

  current_view_container = gtk_stack_get_visible_child (GTK_STACK (self->stack));
  current_view = photos_preview_view_get_view_from_view_container (current_view_container);
  g_return_val_if_fail (widget == current_view, GDK_EVENT_PROPAGATE);

  if (self->current_tool == NULL)
    {
      if (self->grabbed)
        {
          gdouble delta_x;
          gdouble delta_y;

          /* We are moving the content, not the view. Hence the
           * deltas are inverted.
           */
          delta_x = self->event_x_last - event->motion.x;
          delta_y = self->event_y_last - event->motion.y;

          self->event_x_last = event->motion.x;
          self->event_y_last = event->motion.y;

          photos_utils_scrolled_window_scroll (GTK_SCROLLED_WINDOW (current_view_container), delta_x, delta_y);
          ret_val = GDK_EVENT_STOP;
        }
    }
  else
    {
      ret_val = photos_tool_motion_event (self->current_tool, &(event->motion));
    }

  return ret_val;
}


static void
photos_preview_view_update_zoom_best_fit (PhotosPreviewView *self, PhotosImageView *view)
{
  GtkWidget *current_view;
  GtkWidget *current_view_container;
  gdouble zoom;

  current_view_container = gtk_stack_get_visible_child (GTK_STACK (self->stack));
  current_view = photos_preview_view_get_view_from_view_container (current_view_container);
  g_return_if_fail (view == PHOTOS_IMAGE_VIEW (current_view));

  zoom = photos_image_view_get_zoom (view);
  if (!photos_image_view_get_best_fit (view))
    return;

  self->zoom_best_fit = zoom;
}


static void
photos_preview_view_notify_best_fit (GObject *object, GParamSpec *pspec, gpointer user_data)
{
  PhotosPreviewView *self = PHOTOS_PREVIEW_VIEW (user_data);
  PhotosImageView *view = PHOTOS_IMAGE_VIEW (object);

  photos_preview_view_update_zoom_best_fit (self, view);
}


static void
photos_preview_view_notify_zoom (GObject *object, GParamSpec *pspec, gpointer user_data)
{
  PhotosPreviewView *self = PHOTOS_PREVIEW_VIEW (user_data);
  PhotosImageView *view = PHOTOS_IMAGE_VIEW (object);

  photos_preview_view_update_zoom_best_fit (self, view);
}


static void
photos_preview_view_navigate (PhotosPreviewView *self, gint position)
{
  GeglNode *node;
  GtkWidget *current_view_container;
  GtkWidget *new_view_container;
  GtkWidget *next_view;
  GtkWidget *next_view_container;

  next_view_container = photos_preview_view_get_invisible_child (self);
  next_view = photos_preview_view_get_view_from_view_container (next_view_container);
  node = photos_image_view_get_node (PHOTOS_IMAGE_VIEW (next_view));
  g_return_if_fail (node == NULL);

  current_view_container = gtk_stack_get_visible_child (GTK_STACK (self->stack));
  gtk_container_child_set (GTK_CONTAINER (self->stack), current_view_container, "position", position, NULL);

  gtk_stack_set_visible_child (GTK_STACK (self->stack), next_view_container);

  gtk_container_remove (GTK_CONTAINER (self->stack), current_view_container);

  new_view_container = photos_preview_view_create_view_with_container (self);
  gtk_container_add (GTK_CONTAINER (self->stack), new_view_container);
}


static void
photos_preview_view_navigate_next (PhotosPreviewView *self)
{
  photos_preview_view_navigate (self, 0);
}


static void
photos_preview_view_navigate_previous (PhotosPreviewView *self)
{
  photos_preview_view_navigate (self, -1);
}


static gboolean
photos_preview_view_scroll_event (GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
  PhotosPreviewView *self = PHOTOS_PREVIEW_VIEW (user_data);
  GVariant *parameter = NULL;
  GtkWidget *current_view;
  GtkWidget *current_view_container;
  gboolean ret_val = GDK_EVENT_PROPAGATE;
  gdouble delta_abs;

  current_view_container = gtk_stack_get_visible_child (GTK_STACK (self->stack));
  current_view = photos_preview_view_get_view_from_view_container (current_view_container);
  g_return_val_if_fail (widget == current_view, GDK_EVENT_PROPAGATE);

  if ((event->scroll.state & GDK_SHIFT_MASK) != 0 || (event->scroll.state & GDK_CONTROL_MASK) != 0)
    goto out;

  delta_abs = fabs (event->scroll.delta_y);
  parameter = photos_utils_create_zoom_target_value (delta_abs, PHOTOS_ZOOM_EVENT_SCROLL);
  parameter = g_variant_ref_sink (parameter);

  if (event->scroll.delta_y < 0.0)
    {
      if (g_action_get_enabled (self->zoom_in_action))
        {
          g_action_activate (self->zoom_in_action, parameter);
          ret_val = GDK_EVENT_STOP;
        }
    }
  else
    {
      if (g_action_get_enabled (self->zoom_out_action))
        {
          g_action_activate (self->zoom_out_action, parameter);
          ret_val = GDK_EVENT_STOP;
        }
    }

 out:
  g_clear_pointer (&parameter, g_variant_unref);
  return ret_val;
}


static GtkWidget *
photos_preview_view_create_view_with_container (PhotosPreviewView *self)
{
  GtkStyleContext *context;
  GtkWidget *sw;
  GtkWidget *view;

  sw = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw), GTK_SHADOW_IN);
  context = gtk_widget_get_style_context (sw);
  gtk_style_context_add_class (context, "documents-scrolledwin");

  view = photos_image_view_new ();
  gtk_container_add (GTK_CONTAINER (sw), view);
  g_signal_connect (view, "button-press-event", G_CALLBACK (photos_preview_view_button_press_event), self);
  g_signal_connect (view, "button-release-event", G_CALLBACK (photos_preview_view_button_release_event), self);
  g_signal_connect_swapped (view, "draw-overlay", G_CALLBACK (photos_preview_view_draw_overlay), self);
  g_signal_connect (view, "motion-notify-event", G_CALLBACK (photos_preview_view_motion_notify_event), self);
  g_signal_connect (view, "notify::best-fit", G_CALLBACK (photos_preview_view_notify_best_fit), self);
  g_signal_connect (view, "notify::zoom", G_CALLBACK (photos_preview_view_notify_zoom), self);
  g_signal_connect (view, "scroll-event", G_CALLBACK (photos_preview_view_scroll_event), self);

  /* It has to be visible to become the visible child of self->stack. */
  gtk_widget_show_all (sw);

  return sw;
}


static void
photos_preview_view_process (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  GError *error = NULL;
  PhotosBaseItem *item = PHOTOS_BASE_ITEM (source_object);

  photos_base_item_operation_add_finish (item, res, &error);
  if (error != NULL)
    {
      if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        {
          g_error_free (error);
          return;
        }
      else
        {
          g_warning ("Unable to process item: %s", error->message);
          g_error_free (error);
        }
    }
}


static void
photos_preview_view_blacks_exposure (PhotosPreviewView *self, GVariant *parameter)
{
  GVariantIter iter;
  PhotosBaseItem *item;
  const gchar *key;
  gdouble blacks = -G_MAXDOUBLE;
  gdouble exposure = -G_MAXDOUBLE;
  gdouble value;

  item = PHOTOS_BASE_ITEM (photos_base_manager_get_active_object (self->item_mngr));
  if (item == NULL)
    return;

  g_variant_iter_init (&iter, parameter);
  while (g_variant_iter_next (&iter, "{&sd}", &key, &value))
    {
      if (g_strcmp0 (key, "blacks") == 0)
        blacks = value;
      else if (g_strcmp0 (key, "exposure") == 0)
        exposure = value;
    }

  g_return_if_fail (blacks > -G_MAXDOUBLE);
  g_return_if_fail (exposure > -G_MAXDOUBLE);

  photos_base_item_operation_add_async (item,
                                        self->cancellable,
                                        photos_preview_view_process,
                                        self,
                                        "gegl:exposure",
                                        "black-level", blacks,
                                        "exposure", exposure,
                                        NULL);
}


static void
photos_preview_view_contrast (PhotosPreviewView *self, GVariant *parameter)
{
  PhotosBaseItem *item;
  gdouble contrast;

  item = PHOTOS_BASE_ITEM (photos_base_manager_get_active_object (self->item_mngr));
  if (item == NULL)
    return;

  contrast = g_variant_get_double (parameter);
  photos_base_item_operation_add_async (item,
                                        self->cancellable,
                                        photos_preview_view_process,
                                        self,
                                        "gegl:brightness-contrast",
                                        "contrast", contrast,
                                        NULL);
}


static void
photos_preview_view_shadows_highlights (PhotosPreviewView *self, GVariant *parameter)
{
  GVariantIter iter;
  PhotosBaseItem *item;
  const gchar *key;
  gdouble shadows = -G_MAXDOUBLE;
  gdouble highlights = -G_MAXDOUBLE;
  gdouble value;

  item = PHOTOS_BASE_ITEM (photos_base_manager_get_active_object (self->item_mngr));
  if (item == NULL)
    return;

  g_variant_iter_init (&iter, parameter);
  while (g_variant_iter_next (&iter, "{&sd}", &key, &value))
    {
      if (g_strcmp0 (key, "shadows") == 0)
        shadows = value;
      else if (g_strcmp0 (key, "highlights") == 0)
        highlights = value;
    }

  g_return_if_fail (shadows > -G_MAXDOUBLE);
  g_return_if_fail (highlights > -G_MAXDOUBLE);

  photos_base_item_operation_add_async (item,
                                        self->cancellable,
                                        photos_preview_view_process,
                                        self,
                                        "gegl:shadows-highlights",
                                        "shadows", shadows,
                                        "highlights", highlights,
                                        NULL);
}


static void
photos_preview_view_crop (PhotosPreviewView *self, GVariant *parameter)
{
  GVariantIter iter;
  PhotosBaseItem *item;
  const gchar *key;
  gdouble height = -1.0;
  gdouble width = -1.0;
  gdouble value;
  gdouble x = -1.0;
  gdouble y = -1.0;

  item = PHOTOS_BASE_ITEM (photos_base_manager_get_active_object (self->item_mngr));
  if (item == NULL)
    return;

  g_variant_iter_init (&iter, parameter);
  while (g_variant_iter_next (&iter, "{&sd}", &key, &value))
    {
      if (g_strcmp0 (key, "height") == 0)
        height = value;
      else if (g_strcmp0 (key, "width") == 0)
        width = value;
      else if (g_strcmp0 (key, "x") == 0)
        x = value;
      else if (g_strcmp0 (key, "y") == 0)
        y = value;
    }

  g_return_if_fail (height >= 0.0);
  g_return_if_fail (width >= 0.0);
  g_return_if_fail (x >= 0.0);
  g_return_if_fail (y >= 0.0);

  photos_base_item_operation_add_async (item,
                                        self->cancellable,
                                        photos_preview_view_process,
                                        self,
                                        "gegl:crop",
                                        "height", height,
                                        "width", width,
                                        "x", x,
                                        "y", y,
                                        NULL);
}


static void
photos_preview_view_denoise (PhotosPreviewView *self, GVariant *parameter)
{
  PhotosBaseItem *item;
  guint16 iterations;

  item = PHOTOS_BASE_ITEM (photos_base_manager_get_active_object (self->item_mngr));
  if (item == NULL)
    return;

  iterations = g_variant_get_uint16 (parameter);
  photos_base_item_operation_add_async (item,
                                        self->cancellable,
                                        photos_preview_view_process,
                                        self,
                                        "gegl:noise-reduction",
                                        "iterations", (gint) iterations,
                                        NULL);
}


static void
photos_preview_view_insta (PhotosPreviewView *self, GVariant *parameter)
{
  PhotosBaseItem *item;
  PhotosOperationInstaPreset preset;

  item = PHOTOS_BASE_ITEM (photos_base_manager_get_active_object (self->item_mngr));
  if (item == NULL)
    return;

  preset = (PhotosOperationInstaPreset) g_variant_get_int16 (parameter);
  photos_base_item_operation_add_async (item,
                                        self->cancellable,
                                        photos_preview_view_process,
                                        self,
                                        "photos:insta-filter",
                                        "preset", preset,
                                        NULL);
}


static void
photos_preview_view_saturation (PhotosPreviewView *self, GVariant *parameter)
{
  PhotosBaseItem *item;
  gdouble scale;

  item = PHOTOS_BASE_ITEM (photos_base_manager_get_active_object (self->item_mngr));
  if (item == NULL)
    return;

  scale = g_variant_get_double (parameter);
  photos_base_item_operation_add_async (item,
                                        self->cancellable,
                                        photos_preview_view_process,
                                        self,
                                        "photos:saturation",
                                        "scale", scale,
                                        NULL);
}


static void
photos_preview_view_sharpen (PhotosPreviewView *self, GVariant *parameter)
{
  PhotosBaseItem *item;
  gdouble scale;

  item = PHOTOS_BASE_ITEM (photos_base_manager_get_active_object (self->item_mngr));
  if (item == NULL)
    return;

  scale = g_variant_get_double (parameter);
  photos_base_item_operation_add_async (item,
                                        self->cancellable,
                                        photos_preview_view_process,
                                        self,
                                        "gegl:unsharp-mask",
                                        "scale", scale,
                                        NULL);
}


static void
photos_preview_view_tool_activated (PhotosTool *tool, gpointer user_data)
{
  PhotosPreviewView *self = PHOTOS_PREVIEW_VIEW (user_data);

  g_return_if_fail (self->current_tool == NULL);

  self->current_tool = tool;
  g_object_add_weak_pointer (G_OBJECT (self->current_tool), (gpointer *) &self->current_tool);
}


static void
photos_preview_view_tool_changed (PhotosPreviewView *self, PhotosTool *tool)
{
  if (self->current_tool == tool)
    return;

  if (self->current_tool != NULL)
    {
      photos_tool_deactivate (self->current_tool);
      g_object_remove_weak_pointer (G_OBJECT (self->current_tool), (gpointer *) &self->current_tool);
      g_signal_handlers_disconnect_by_func (self->current_tool, photos_preview_view_tool_activated, self);
      self->current_tool = NULL;
    }

  if (tool == NULL)
    {
      self->current_tool = NULL;
    }
  else
    {
      GtkWidget *view;
      GtkWidget *view_container;
      PhotosBaseItem *item;

      g_signal_connect_object (tool, "activated", G_CALLBACK (photos_preview_view_tool_activated), self, 0);

      item = PHOTOS_BASE_ITEM (photos_base_manager_get_active_object (self->item_mngr));
      view_container = gtk_stack_get_visible_child (GTK_STACK (self->stack));
      view = photos_preview_view_get_view_from_view_container (view_container);
      photos_tool_activate (tool, item, PHOTOS_IMAGE_VIEW (view));
    }
}


static void
photos_preview_view_edit_done_pipeline_save (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosPreviewView *self;
  PhotosBaseItem *item = PHOTOS_BASE_ITEM (source_object);
  GApplication *app;
  GError *error;

  error = NULL;
  if (!photos_base_item_pipeline_save_finish (item, res, &error))
    {
      if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Unable to save pipeline: Unexpected destruction of PhotosPreviewView");
      else
        g_warning ("Unable to save pipeline: %s", error->message);

      g_error_free (error);
      goto out;
    }

  self = PHOTOS_PREVIEW_VIEW (user_data);
  photos_mode_controller_go_back (self->mode_cntrlr);

  photos_done_notification_new (item);

 out:
  app = g_application_get_default ();
  photos_application_release (PHOTOS_APPLICATION (app));
}


static void
photos_preview_view_edit_done (PhotosPreviewView *self)
{
  PhotosBaseItem *item;
  GApplication *app;

  item = PHOTOS_BASE_ITEM (photos_base_manager_get_active_object (self->item_mngr));
  g_return_if_fail (item != NULL);

  /* We need to withdraw the palette's row before saving the pipeline so
   * that we don't miss the application of the current tool. Some
   * tools (eg., crop) use their deactivated virtual method to apply
   * themselves, and that happens when the row gets hidden.
   */
  photos_edit_palette_hide_details (PHOTOS_EDIT_PALETTE (self->palette));

  app = g_application_get_default ();
  photos_application_hold (PHOTOS_APPLICATION (app));

  photos_base_item_pipeline_save_async (item,
                                        self->cancellable,
                                        photos_preview_view_edit_done_pipeline_save,
                                        self);
}


static void
photos_preview_view_window_mode_changed (PhotosPreviewView *self, PhotosWindowMode mode, PhotosWindowMode old_mode)
{
  switch (mode)
    {
    case PHOTOS_WINDOW_MODE_COLLECTION_VIEW:
    case PHOTOS_WINDOW_MODE_COLLECTIONS:
    case PHOTOS_WINDOW_MODE_FAVORITES:
    case PHOTOS_WINDOW_MODE_OVERVIEW:
    case PHOTOS_WINDOW_MODE_SEARCH:
      gtk_revealer_set_reveal_child (GTK_REVEALER (self->revealer), FALSE);
      photos_edit_palette_hide_details (PHOTOS_EDIT_PALETTE (self->palette));
      photos_preview_nav_buttons_hide (self->nav_buttons);
      photos_preview_nav_buttons_set_mode (self->nav_buttons, PHOTOS_WINDOW_MODE_NONE);
      break;

    case PHOTOS_WINDOW_MODE_EDIT:
      gtk_revealer_set_reveal_child (GTK_REVEALER (self->revealer), TRUE);
      photos_edit_palette_show (PHOTOS_EDIT_PALETTE (self->palette));
      photos_preview_nav_buttons_hide (self->nav_buttons);
      break;

    case PHOTOS_WINDOW_MODE_IMPORT:
      break;

    case PHOTOS_WINDOW_MODE_PREVIEW:
      gtk_revealer_set_reveal_child (GTK_REVEALER (self->revealer), FALSE);
      photos_edit_palette_hide_details (PHOTOS_EDIT_PALETTE (self->palette));
      photos_preview_nav_buttons_set_auto_hide (self->nav_buttons, TRUE);
      photos_preview_nav_buttons_set_show_navigation (self->nav_buttons, TRUE);
      photos_preview_nav_buttons_show (self->nav_buttons);
      break;

    case PHOTOS_WINDOW_MODE_NONE:
    default:
      g_assert_not_reached ();
      break;
    }
}


static void
photos_preview_view_zoom_begin (PhotosPreviewView *self, GVariant *parameter)
{
  GtkWidget *view;
  GtkWidget *view_container;
  PhotosZoomEvent event;

  event = photos_utils_get_zoom_event (parameter);
  g_return_if_fail (event == PHOTOS_ZOOM_EVENT_TOUCH);

  view_container = gtk_stack_get_visible_child (GTK_STACK (self->stack));
  view = photos_preview_view_get_view_from_view_container (view_container);
  self->zoom_begin = photos_image_view_get_zoom (PHOTOS_IMAGE_VIEW (view));

  g_simple_action_set_enabled (G_SIMPLE_ACTION (self->zoom_begin_action), FALSE);
  g_simple_action_set_enabled (G_SIMPLE_ACTION (self->zoom_best_fit_action), TRUE);
  g_simple_action_set_enabled (G_SIMPLE_ACTION (self->zoom_end_action), TRUE);
  g_simple_action_set_enabled (G_SIMPLE_ACTION (self->zoom_out_action), TRUE);
  photos_preview_nav_buttons_set_auto_hide (self->nav_buttons, FALSE);
  photos_preview_nav_buttons_set_show_navigation (self->nav_buttons, FALSE);
}


static void
photos_preview_view_zoom_best_fit (PhotosPreviewView *self)
{
  GtkWidget *view;
  GtkWidget *view_container;

  view_container = gtk_stack_get_visible_child (GTK_STACK (self->stack));
  view = photos_preview_view_get_view_from_view_container (view_container);
  photos_image_view_set_best_fit (PHOTOS_IMAGE_VIEW (view), TRUE, TRUE);
  g_simple_action_set_enabled (G_SIMPLE_ACTION (self->zoom_best_fit_action), FALSE);
  g_simple_action_set_enabled (G_SIMPLE_ACTION (self->zoom_out_action), FALSE);
  photos_preview_nav_buttons_set_auto_hide (self->nav_buttons, TRUE);
  photos_preview_nav_buttons_set_show_navigation (self->nav_buttons, TRUE);
}


static void
photos_preview_view_zoom_end (PhotosPreviewView *self, GVariant *parameter)
{
  GtkWidget *view;
  GtkWidget *view_container;
  PhotosZoomEvent event;
  gdouble zoom;

  event = photos_utils_get_zoom_event (parameter);
  g_return_if_fail (event == PHOTOS_ZOOM_EVENT_TOUCH);

  view_container = gtk_stack_get_visible_child (GTK_STACK (self->stack));
  view = photos_preview_view_get_view_from_view_container (view_container);
  zoom = photos_image_view_get_zoom (PHOTOS_IMAGE_VIEW (view));

  if (zoom < self->zoom_best_fit || G_APPROX_VALUE (self->zoom_best_fit, zoom, PHOTOS_EPSILON))
    {
      photos_image_view_set_best_fit (PHOTOS_IMAGE_VIEW (view), TRUE, TRUE);
      g_simple_action_set_enabled (G_SIMPLE_ACTION (self->zoom_best_fit_action), FALSE);
      g_simple_action_set_enabled (G_SIMPLE_ACTION (self->zoom_out_action), FALSE);
    }

  self->zoom_begin = 0.0;

  g_simple_action_set_enabled (G_SIMPLE_ACTION (self->zoom_begin_action), TRUE);
  g_simple_action_set_enabled (G_SIMPLE_ACTION (self->zoom_end_action), FALSE);
  photos_preview_nav_buttons_set_auto_hide (self->nav_buttons, TRUE);
  photos_preview_nav_buttons_set_show_navigation (self->nav_buttons, TRUE);
}


static gdouble
photos_preview_view_calculate_zoom_for_non_touch (PhotosPreviewView *self,
                                                  gdouble delta,
                                                  PhotosZoomEvent event,
                                                  gboolean zoom_in)
{
  GtkWidget *view;
  GtkWidget *view_container;
  gboolean best_fit;
  gdouble zoom = 0.0;
  gdouble zoom_factor;
  gdouble zoom_factor_for_delta;

  g_return_val_if_fail (event != PHOTOS_ZOOM_EVENT_NONE, 0.0);
  g_return_val_if_fail (event != PHOTOS_ZOOM_EVENT_TOUCH, 0.0);
  g_return_val_if_fail ((event == PHOTOS_ZOOM_EVENT_KEYBOARD_ACCELERATOR
                         && G_APPROX_VALUE (delta, 1.0, PHOTOS_EPSILON))
                        || (event == PHOTOS_ZOOM_EVENT_MOUSE_CLICK
                            && G_APPROX_VALUE (delta, 1.0, PHOTOS_EPSILON))
                        || (event == PHOTOS_ZOOM_EVENT_SCROLL && delta >= 0.0),
                        0.0);

  view_container = gtk_stack_get_visible_child (GTK_STACK (self->stack));
  view = photos_preview_view_get_view_from_view_container (view_container);

  best_fit = photos_image_view_get_best_fit (PHOTOS_IMAGE_VIEW (view));

  switch (event)
    {
    case PHOTOS_ZOOM_EVENT_KEYBOARD_ACCELERATOR:
    case PHOTOS_ZOOM_EVENT_MOUSE_CLICK:
      zoom_factor = best_fit && zoom_in ? ZOOM_FACTOR_1 : ZOOM_FACTOR_2;
      break;

    case PHOTOS_ZOOM_EVENT_SCROLL:
      zoom_factor = best_fit && zoom_in ? ZOOM_FACTOR_2 : ZOOM_FACTOR_3;
      break;

    case PHOTOS_ZOOM_EVENT_NONE:
    case PHOTOS_ZOOM_EVENT_TOUCH:
    default:
      g_assert_not_reached ();
      break;
    }

  zoom_factor_for_delta = pow (zoom_factor, delta);
  zoom = photos_image_view_get_zoom (PHOTOS_IMAGE_VIEW (view));

  if (zoom_in)
    zoom *= zoom_factor_for_delta;
  else
    zoom /= zoom_factor_for_delta;

  return zoom;
}


static void
photos_preview_view_zoom_in (PhotosPreviewView *self, GVariant *parameter)
{
  GtkWidget *view;
  GtkWidget *view_container;
  PhotosZoomEvent event;
  gboolean enable_animation;
  gdouble delta;
  gdouble zoom;

  g_return_if_fail (self->zoom_best_fit > 0.0);

  event = photos_utils_get_zoom_event (parameter);
  g_return_if_fail (event != PHOTOS_ZOOM_EVENT_NONE);

  delta = photos_utils_get_zoom_delta (parameter);
  g_return_if_fail ((event == PHOTOS_ZOOM_EVENT_KEYBOARD_ACCELERATOR
                     && G_APPROX_VALUE (delta, 1.0, PHOTOS_EPSILON))
                    || (event == PHOTOS_ZOOM_EVENT_MOUSE_CLICK && G_APPROX_VALUE (delta, 1.0, PHOTOS_EPSILON))
                    || (event == PHOTOS_ZOOM_EVENT_SCROLL && delta >= 0.0)
                    || (event == PHOTOS_ZOOM_EVENT_TOUCH && delta >= 0.0 && self->zoom_begin > 0.0));

  switch (event)
    {
    case PHOTOS_ZOOM_EVENT_KEYBOARD_ACCELERATOR:
    case PHOTOS_ZOOM_EVENT_MOUSE_CLICK:
    case PHOTOS_ZOOM_EVENT_SCROLL:
      enable_animation = TRUE;
      zoom = photos_preview_view_calculate_zoom_for_non_touch (self, delta, event, TRUE);
      break;

    case PHOTOS_ZOOM_EVENT_TOUCH:
      enable_animation = FALSE;
      zoom = self->zoom_begin * delta;
      break;

    case PHOTOS_ZOOM_EVENT_NONE:
    default:
      g_assert_not_reached ();
      break;
    }

  view_container = gtk_stack_get_visible_child (GTK_STACK (self->stack));
  view = photos_preview_view_get_view_from_view_container (view_container);
  photos_image_view_set_zoom (PHOTOS_IMAGE_VIEW (view), zoom, enable_animation);

  g_simple_action_set_enabled (G_SIMPLE_ACTION (self->zoom_best_fit_action), TRUE);
  g_simple_action_set_enabled (G_SIMPLE_ACTION (self->zoom_out_action), TRUE);
  photos_preview_nav_buttons_set_auto_hide (self->nav_buttons, FALSE);
  photos_preview_nav_buttons_set_show_navigation (self->nav_buttons, FALSE);
}


static void
photos_preview_view_zoom_out (PhotosPreviewView *self, GVariant *parameter)
{
  GtkWidget *view;
  GtkWidget *view_container;
  PhotosZoomEvent event;
  gboolean enable_animation;
  gdouble delta;
  gdouble zoom;
  gdouble zoom_best_fit;
  gdouble zoom_best_fit_factor;

  g_return_if_fail (self->zoom_best_fit > 0.0);

  event = photos_utils_get_zoom_event (parameter);
  g_return_if_fail (event != PHOTOS_ZOOM_EVENT_NONE);

  delta = photos_utils_get_zoom_delta (parameter);
  g_return_if_fail ((event == PHOTOS_ZOOM_EVENT_KEYBOARD_ACCELERATOR
                     && G_APPROX_VALUE (delta, 1.0, PHOTOS_EPSILON))
                    || (event == PHOTOS_ZOOM_EVENT_MOUSE_CLICK && G_APPROX_VALUE (delta, 1.0, PHOTOS_EPSILON))
                    || (event == PHOTOS_ZOOM_EVENT_SCROLL && delta >= 0.0)
                    || (event == PHOTOS_ZOOM_EVENT_TOUCH && delta >= 0.0 && self->zoom_begin > 0.0));

  switch (event)
    {
    case PHOTOS_ZOOM_EVENT_KEYBOARD_ACCELERATOR:
    case PHOTOS_ZOOM_EVENT_MOUSE_CLICK:
    case PHOTOS_ZOOM_EVENT_SCROLL:
      enable_animation = TRUE;
      zoom = photos_preview_view_calculate_zoom_for_non_touch (self, delta, event, FALSE);
      zoom_best_fit_factor = 1.0;
      break;

    case PHOTOS_ZOOM_EVENT_TOUCH:
      enable_animation = FALSE;
      zoom = self->zoom_begin * delta;
      zoom_best_fit_factor = ZOOM_BEST_FIT_FACTOR_TOUCH;
      break;

    case PHOTOS_ZOOM_EVENT_NONE:
    default:
      g_assert_not_reached ();
      break;
    }

  view_container = gtk_stack_get_visible_child (GTK_STACK (self->stack));
  view = photos_preview_view_get_view_from_view_container (view_container);

  zoom_best_fit = self->zoom_best_fit * zoom_best_fit_factor;
  if (zoom < zoom_best_fit || G_APPROX_VALUE (zoom_best_fit, zoom, PHOTOS_EPSILON))
    {
      photos_image_view_set_best_fit (PHOTOS_IMAGE_VIEW (view), TRUE, TRUE);
      g_simple_action_set_enabled (G_SIMPLE_ACTION (self->zoom_best_fit_action), FALSE);
      g_simple_action_set_enabled (G_SIMPLE_ACTION (self->zoom_out_action), FALSE);
      photos_preview_nav_buttons_set_auto_hide (self->nav_buttons, TRUE);
      photos_preview_nav_buttons_set_show_navigation (self->nav_buttons, TRUE);
    }
  else
    {
      photos_image_view_set_zoom (PHOTOS_IMAGE_VIEW (view), zoom, enable_animation);
    }
}


static void
photos_preview_view_dispose (GObject *object)
{
  PhotosPreviewView *self = PHOTOS_PREVIEW_VIEW (object);

  if (self->cancellable != NULL)
    {
      g_cancellable_cancel (self->cancellable);
      g_clear_object (&self->cancellable);
    }

  g_clear_object (&self->node);
  g_clear_object (&self->item_mngr);
  g_clear_object (&self->gesture_zoom);
  g_clear_object (&self->mode_cntrlr);
  g_clear_object (&self->nav_buttons);

  G_OBJECT_CLASS (photos_preview_view_parent_class)->dispose (object);
}


static void
photos_preview_view_finalize (GObject *object)
{
  PhotosPreviewView *self = PHOTOS_PREVIEW_VIEW (object);

  if (self->current_tool != NULL)
    g_object_remove_weak_pointer (G_OBJECT (self->current_tool), (gpointer *) &self->current_tool);

  G_OBJECT_CLASS (photos_preview_view_parent_class)->finalize (object);
}


static void
photos_preview_view_constructed (GObject *object)
{
  PhotosPreviewView *self = PHOTOS_PREVIEW_VIEW (object);
  GtkGesture *gesture_zoom;

  G_OBJECT_CLASS (photos_preview_view_parent_class)->constructed (object);

  gesture_zoom = gtk_gesture_zoom_new (GTK_WIDGET (self));
  gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (gesture_zoom), GTK_PHASE_CAPTURE);
  g_signal_connect_swapped (gesture_zoom,
                            "begin",
                            G_CALLBACK (photos_preview_view_gesture_zoom_begin),
                            self);
  g_signal_connect_swapped (gesture_zoom,
                            "end",
                            G_CALLBACK (photos_preview_view_gesture_zoom_end),
                            self);

  self->gesture_zoom = photos_gesture_zoom_new (gesture_zoom);
  g_signal_connect_swapped (self->gesture_zoom,
                            "direction-changed",
                            G_CALLBACK (photos_preview_view_gesture_zoom_direction_changed),
                            self);
  g_signal_connect_swapped (self->gesture_zoom,
                            "scale-changed",
                            G_CALLBACK (photos_preview_view_gesture_zoom_scale_changed),
                            self);

  self->nav_buttons = photos_preview_nav_buttons_new (self, GTK_OVERLAY (self->overlay));
  g_signal_connect_swapped (self->nav_buttons, "load-next", G_CALLBACK (photos_preview_view_navigate_next), self);
  g_signal_connect_swapped (self->nav_buttons, "load-previous", G_CALLBACK (photos_preview_view_navigate_previous), self);

  gtk_widget_show_all (GTK_WIDGET (self));

  g_object_unref (gesture_zoom);
}


static void
photos_preview_view_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
  PhotosPreviewView *self = PHOTOS_PREVIEW_VIEW (object);

  switch (prop_id)
    {
    case PROP_OVERLAY:
      self->overlay = GTK_WIDGET (g_value_dup_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
photos_preview_view_init (PhotosPreviewView *self)
{
  GAction *action;
  GApplication *app;
  GtkWidget *grid;
  GtkWidget *sw;
  GtkWidget *view_container;
  PhotosSearchContextState *state;

  app = g_application_get_default ();
  state = photos_search_context_get_state (PHOTOS_SEARCH_CONTEXT (app));

  self->cancellable = g_cancellable_new ();
  self->item_mngr = g_object_ref (state->item_mngr);

  self->mode_cntrlr = g_object_ref (state->mode_cntrlr);
  g_signal_connect_object (self->mode_cntrlr,
                           "window-mode-changed",
                           G_CALLBACK (photos_preview_view_window_mode_changed),
                           self,
                           G_CONNECT_SWAPPED);

  gtk_widget_set_hexpand (GTK_WIDGET (self), TRUE);
  gtk_widget_set_vexpand (GTK_WIDGET (self), TRUE);

  grid = gtk_grid_new ();
  gtk_container_add (GTK_CONTAINER (self), grid);

  self->stack = gtk_stack_new ();
  gtk_widget_set_hexpand (self->stack, TRUE);
  gtk_widget_set_vexpand (self->stack, TRUE);
  gtk_container_add (GTK_CONTAINER (grid), self->stack);

  view_container = photos_preview_view_create_view_with_container (self);
  gtk_container_add (GTK_CONTAINER (self->stack), view_container);
  gtk_stack_set_visible_child (GTK_STACK (self->stack), view_container);

  view_container = photos_preview_view_create_view_with_container (self);
  gtk_container_add (GTK_CONTAINER (self->stack), view_container);

  self->revealer = gtk_revealer_new ();
  gtk_revealer_set_transition_type (GTK_REVEALER (self->revealer), GTK_REVEALER_TRANSITION_TYPE_SLIDE_LEFT);
  gtk_container_add (GTK_CONTAINER (grid), self->revealer);

  sw = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw), GTK_SHADOW_IN);
  gtk_container_add (GTK_CONTAINER (self->revealer), sw);

  self->palette = photos_edit_palette_new ();
  gtk_widget_set_hexpand (self->palette, FALSE);
  gtk_widget_set_vexpand (self->palette, TRUE);
  gtk_container_add (GTK_CONTAINER (sw), self->palette);
  g_signal_connect_swapped (self->palette, "tool-changed", G_CALLBACK (photos_preview_view_tool_changed), self);

  action = g_action_map_lookup_action (G_ACTION_MAP (app), "blacks-exposure-current");
  g_signal_connect_object (action,
                           "activate",
                           G_CALLBACK (photos_preview_view_blacks_exposure),
                           self,
                           G_CONNECT_SWAPPED);

  action = g_action_map_lookup_action (G_ACTION_MAP (app), "contrast-current");
  g_signal_connect_object (action,
                           "activate",
                           G_CALLBACK (photos_preview_view_contrast),
                           self,
                           G_CONNECT_SWAPPED);

  action = g_action_map_lookup_action (G_ACTION_MAP (app), "crop-current");
  g_signal_connect_object (action, "activate", G_CALLBACK (photos_preview_view_crop), self, G_CONNECT_SWAPPED);

  action = g_action_map_lookup_action (G_ACTION_MAP (app), "denoise-current");
  g_signal_connect_object (action, "activate", G_CALLBACK (photos_preview_view_denoise), self, G_CONNECT_SWAPPED);

  action = g_action_map_lookup_action (G_ACTION_MAP (app), "edit-done");
  g_signal_connect_object (action, "activate", G_CALLBACK (photos_preview_view_edit_done), self, G_CONNECT_SWAPPED);

  action = g_action_map_lookup_action (G_ACTION_MAP (app), "insta-current");
  g_signal_connect_object (action, "activate", G_CALLBACK (photos_preview_view_insta), self, G_CONNECT_SWAPPED);

  action = g_action_map_lookup_action (G_ACTION_MAP (app), "saturation-current");
  g_signal_connect_object (action,
                           "activate",
                           G_CALLBACK (photos_preview_view_saturation),
                           self,
                           G_CONNECT_SWAPPED);

  action = g_action_map_lookup_action (G_ACTION_MAP (app), "shadows-highlights-current");
  g_signal_connect_object (action,
                           "activate",
                           G_CALLBACK (photos_preview_view_shadows_highlights),
                           self,
                           G_CONNECT_SWAPPED);

  action = g_action_map_lookup_action (G_ACTION_MAP (app), "sharpen-current");
  g_signal_connect_object (action, "activate", G_CALLBACK (photos_preview_view_sharpen), self, G_CONNECT_SWAPPED);

  self->zoom_begin_action = g_action_map_lookup_action (G_ACTION_MAP (app), "zoom-begin");
  g_signal_connect_object (self->zoom_begin_action,
                           "activate",
                           G_CALLBACK (photos_preview_view_zoom_begin),
                           self,
                           G_CONNECT_SWAPPED);

  self->zoom_best_fit_action = g_action_map_lookup_action (G_ACTION_MAP (app), "zoom-best-fit");
  g_signal_connect_object (self->zoom_best_fit_action,
                           "activate",
                           G_CALLBACK (photos_preview_view_zoom_best_fit),
                           self,
                           G_CONNECT_SWAPPED);

  self->zoom_end_action = g_action_map_lookup_action (G_ACTION_MAP (app), "zoom-end");
  g_signal_connect_object (self->zoom_end_action,
                           "activate",
                           G_CALLBACK (photos_preview_view_zoom_end),
                           self,
                           G_CONNECT_SWAPPED);

  self->zoom_in_action = g_action_map_lookup_action (G_ACTION_MAP (app), "zoom-in");
  g_signal_connect_object (self->zoom_in_action,
                           "activate",
                           G_CALLBACK (photos_preview_view_zoom_in),
                           self,
                           G_CONNECT_SWAPPED);

  self->zoom_out_action = g_action_map_lookup_action (G_ACTION_MAP (app), "zoom-out");
  g_signal_connect_object (self->zoom_out_action,
                           "activate",
                           G_CALLBACK (photos_preview_view_zoom_out),
                           self,
                           G_CONNECT_SWAPPED);

  self->event_x_last = -1.0;
  self->event_y_last = -1.0;
}


static void
photos_preview_view_class_init (PhotosPreviewViewClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->constructed = photos_preview_view_constructed;
  object_class->dispose = photos_preview_view_dispose;
  object_class->finalize = photos_preview_view_finalize;
  object_class->set_property = photos_preview_view_set_property;

  g_object_class_install_property (object_class,
                                   PROP_OVERLAY,
                                   g_param_spec_object ("overlay",
                                                        "GtkOverlay object",
                                                        "The stack overlay widget",
                                                        GTK_TYPE_OVERLAY,
                                                        G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE));
}


GtkWidget *
photos_preview_view_new (GtkOverlay *overlay)
{
  return g_object_new (PHOTOS_TYPE_PREVIEW_VIEW, "overlay", overlay, NULL);
}


void
photos_preview_view_set_mode (PhotosPreviewView *self, PhotosWindowMode old_mode)
{
  g_return_if_fail (PHOTOS_IS_PREVIEW_VIEW (self));
  g_return_if_fail (old_mode != PHOTOS_WINDOW_MODE_NONE);
  g_return_if_fail (old_mode != PHOTOS_WINDOW_MODE_EDIT);
  g_return_if_fail (old_mode != PHOTOS_WINDOW_MODE_IMPORT);
  g_return_if_fail (old_mode != PHOTOS_WINDOW_MODE_PREVIEW);

  photos_preview_nav_buttons_set_auto_hide (self->nav_buttons, TRUE);
  photos_preview_nav_buttons_set_mode (self->nav_buttons, old_mode);
  photos_preview_nav_buttons_set_show_navigation (self->nav_buttons, TRUE);
  photos_preview_nav_buttons_show (self->nav_buttons);
}


void
photos_preview_view_set_node (PhotosPreviewView *self, GeglNode *node)
{
  GtkWidget *view_container;

  if (self->node == node)
    return;

  view_container = gtk_stack_get_visible_child (GTK_STACK (self->stack));
  self->zoom_best_fit = 0.0;
  g_clear_object (&self->node);

  if (node == NULL)
    {
      gtk_container_remove (GTK_CONTAINER (self->stack), view_container);

      view_container = photos_preview_view_create_view_with_container (self);
      gtk_container_add (GTK_CONTAINER (self->stack), view_container);
    }
  else
    {
      GtkWidget *view;

      self->node = g_object_ref (node);
      view = photos_preview_view_get_view_from_view_container (view_container);
      photos_image_view_set_node (PHOTOS_IMAGE_VIEW (view), self->node);
    }
}
