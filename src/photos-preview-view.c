/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2013, 2014, 2015, 2016 Red Hat, Inc.
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
#include <gio/gio.h>
#include <glib/gi18n.h>

#include "gegl-gtk-view.h"
#include "photos-base-item.h"
#include "photos-item-manager.h"
#include "photos-edit-palette.h"
#include "photos-operation-insta-common.h"
#include "photos-preview-nav-buttons.h"
#include "photos-preview-view.h"
#include "photos-search-context.h"
#include "photos-tool.h"


struct _PhotosPreviewView
{
  GtkBin parent_instance;
  GeglNode *node;
  GtkWidget *overlay;
  GtkWidget *palette;
  GtkWidget *revealer;
  GtkWidget *stack;
  PhotosBaseManager *item_mngr;
  PhotosModeController *mode_cntrlr;
  PhotosPreviewNavButtons *nav_buttons;
  PhotosTool *current_tool;
};

struct _PhotosPreviewViewClass
{
  GtkBinClass parent_class;
};

enum
{
  PROP_0,
  PROP_OVERLAY
};


G_DEFINE_TYPE (PhotosPreviewView, photos_preview_view, GTK_TYPE_BIN);


static GtkWidget *photos_preview_view_create_view_with_container (PhotosPreviewView *self);


static gboolean
photos_preview_view_button_press_event (PhotosPreviewView *self, GdkEvent *event)
{
  gboolean ret_val = GDK_EVENT_PROPAGATE;

  if (self->current_tool == NULL)
    goto out;

  switch (event->button.button)
    {
    case 1:
      ret_val = photos_tool_left_click_event (self->current_tool, &(event->button));
      break;

    default:
      break;
    }

 out:
  return ret_val;
}


static gboolean
photos_preview_view_button_release_event (PhotosPreviewView *self, GdkEvent *event)
{
  gboolean ret_val = GDK_EVENT_PROPAGATE;

  if (self->current_tool == NULL)
    goto out;

  switch (event->button.button)
    {
    case 1:
      ret_val = photos_tool_left_unclick_event (self->current_tool, &(event->button));
      break;

    default:
      break;
    }

 out:
  return ret_val;
}


static void
photos_preview_view_draw_background (PhotosPreviewView *self, cairo_t *cr, GdkRectangle *rect, gpointer user_data)
{
  GtkStyleContext *context;
  GtkStateFlags flags;
  GtkWidget *view = GTK_WIDGET (user_data);
  gint height;
  gint width;

  context = gtk_widget_get_style_context (view);
  flags = gtk_widget_get_state_flags (view);
  gtk_style_context_save (context);
  gtk_style_context_set_state (context, flags);
  height = gtk_widget_get_allocated_height (view);
  width = gtk_widget_get_allocated_width (view);
  gtk_render_background (context, cr, 0, 0, width, height);
  gtk_style_context_restore (context);
}


static void
photos_preview_view_draw_overlay (PhotosPreviewView *self, cairo_t *cr, GdkRectangle *rect, gpointer user_data)
{
  if (self->current_tool == NULL)
    return;

  photos_tool_draw (self->current_tool, cr, rect);
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
photos_preview_view_motion_notify_event (PhotosPreviewView *self, GdkEvent *event)
{
  gboolean ret_val = GDK_EVENT_PROPAGATE;

  if (self->current_tool == NULL)
    goto out;

  ret_val = photos_tool_motion_event (self->current_tool, &(event->motion));

 out:
  return ret_val;
}


static void
photos_preview_view_navigate (PhotosPreviewView *self, gint position)
{
  GtkWidget *current_view_container;
  GtkWidget *new_view_container;
  GtkWidget *next_view_container;

  current_view_container = gtk_stack_get_visible_child (GTK_STACK (self->stack));
  gtk_container_child_set (GTK_CONTAINER (self->stack), current_view_container, "position", position, NULL);

  next_view_container = photos_preview_view_get_invisible_child (self);
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

  view = GTK_WIDGET (gegl_gtk_view_new ());
  gtk_widget_add_events (view, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK);
  context = gtk_widget_get_style_context (view);
  gtk_style_context_add_class (context, GTK_STYLE_CLASS_VIEW);
  gtk_style_context_add_class (context, "content-view");
  gtk_container_add (GTK_CONTAINER (sw), view);
  g_signal_connect_swapped (view, "button-press-event", G_CALLBACK (photos_preview_view_button_press_event), self);
  g_signal_connect_swapped (view,
                            "button-release-event",
                            G_CALLBACK (photos_preview_view_button_release_event),
                            self);
  g_signal_connect_swapped (view, "motion-notify-event", G_CALLBACK (photos_preview_view_motion_notify_event), self);
  g_signal_connect_swapped (view, "draw-background", G_CALLBACK (photos_preview_view_draw_background), self);
  g_signal_connect_swapped (view, "draw-overlay", G_CALLBACK (photos_preview_view_draw_overlay), self);

  /* It has to be visible to become the visible child of self->stack. */
  gtk_widget_show_all (sw);

  return sw;
}


static GtkWidget *
photos_preview_view_get_view_from_view_container (GtkWidget *view_container)
{
  GtkWidget *view;
  GtkWidget *viewport;

  viewport = gtk_bin_get_child (GTK_BIN (view_container));
  view = gtk_bin_get_child (GTK_BIN (viewport));
  return view;
}


static void
photos_preview_view_process (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosPreviewView *self = PHOTOS_PREVIEW_VIEW (user_data);
  GError *error = NULL;
  GtkWidget *view_container;
  GtkWidget *view;
  PhotosBaseItem *item = PHOTOS_BASE_ITEM (source_object);

  photos_base_item_process_finish (item, res, &error);
  if (error != NULL)
    {
      g_warning ("Unable to process item: %s", error->message);
      g_error_free (error);
    }

  view_container = gtk_stack_get_visible_child (GTK_STACK (self->stack));
  view = photos_preview_view_get_view_from_view_container (view_container);
  gtk_widget_queue_draw (view);
}


static void
photos_preview_view_brightness_contrast (PhotosPreviewView *self, GVariant *parameter)
{
  GVariantIter iter;
  PhotosBaseItem *item;
  const gchar *key;
  gdouble brightness = -G_MAXDOUBLE;
  gdouble contrast = -G_MAXDOUBLE;
  gdouble value;

  item = PHOTOS_BASE_ITEM (photos_base_manager_get_active_object (self->item_mngr));
  if (item == NULL)
    return;

  g_variant_iter_init (&iter, parameter);
  while (g_variant_iter_next (&iter, "{&sd}", &key, &value))
    {
      if (g_strcmp0 (key, "brightness") == 0)
        brightness = value;
      else if (g_strcmp0 (key, "contrast") == 0)
        contrast = value;
    }

  g_return_if_fail (brightness > -G_MAXDOUBLE);
  g_return_if_fail (contrast > -G_MAXDOUBLE);

  photos_base_item_operation_add (item,
                                  "gegl:brightness-contrast",
                                  "brightness", brightness,
                                  "contrast", contrast,
                                  NULL);
  photos_base_item_process_async (item, NULL, photos_preview_view_process, self);
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

  photos_base_item_operation_add (item, "gegl:crop", "height", height, "width", width, "x", x, "y", y, NULL);
  photos_base_item_process_async (item, NULL, photos_preview_view_process, self);
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
  photos_base_item_operation_add (item, "gegl:noise-reduction", "iterations", (gint) iterations, NULL);
  photos_base_item_process_async (item, NULL, photos_preview_view_process, self);
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
  photos_base_item_operation_add (item, "photos:insta-filter", "preset", preset, NULL);
  photos_base_item_process_async (item, NULL, photos_preview_view_process, self);
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
  photos_base_item_operation_add (item, "photos:saturation", "scale", scale, NULL);
  photos_base_item_process_async (item, NULL, photos_preview_view_process, self);
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
  photos_base_item_operation_add (item, "gegl:unsharp-mask", "scale", scale, NULL);
  photos_base_item_process_async (item, NULL, photos_preview_view_process, self);
}


static void
photos_preview_view_tool_changed (PhotosPreviewView *self, PhotosTool *tool)
{
  GtkWidget *view_container;
  GtkWidget *view;

  if (self->current_tool == tool)
    return;

  if (self->current_tool != NULL)
    {
      photos_tool_deactivate (self->current_tool);
      g_object_remove_weak_pointer (G_OBJECT (self->current_tool), (gpointer *) &self->current_tool);
      self->current_tool = NULL;
    }

  view_container = gtk_stack_get_visible_child (GTK_STACK (self->stack));
  view = photos_preview_view_get_view_from_view_container (view_container);

  if (tool != NULL)
    {
      PhotosBaseItem *item;

      self->current_tool = tool;
      g_object_add_weak_pointer (G_OBJECT (self->current_tool), (gpointer *) &self->current_tool);

      item = PHOTOS_BASE_ITEM (photos_base_manager_get_active_object (self->item_mngr));
      photos_tool_activate (self->current_tool, item, GEGL_GTK_VIEW (view));
    }

  gtk_widget_queue_draw (view);
}


static void
photos_preview_view_undo (PhotosPreviewView *self)
{
  PhotosBaseItem *item;

  item = PHOTOS_BASE_ITEM (photos_base_manager_get_active_object (self->item_mngr));
  if (item == NULL)
    return;

  photos_base_item_operation_undo (item);
  photos_base_item_process_async (item, NULL, photos_preview_view_process, self);
}


static void
photos_preview_view_edit_done_pipeline_save (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosPreviewView *self = PHOTOS_PREVIEW_VIEW (user_data);
  PhotosBaseItem *item = PHOTOS_BASE_ITEM (source_object);
  GApplication *app;
  GError *error;

  error = NULL;
  if (!photos_base_item_pipeline_save_finish (item, res, &error))
    {
      g_warning ("Unable to save pipeline: %s", error->message);
      g_error_free (error);
      goto out;
    }

  photos_mode_controller_go_back (self->mode_cntrlr);

 out:
  app = g_application_get_default ();
  g_application_release (G_APPLICATION (app));
  g_object_unref (self);
}


static void
photos_preview_view_edit_done (PhotosPreviewView *self)
{
  PhotosBaseItem *item;
  GApplication *app;

  item = PHOTOS_BASE_ITEM (photos_base_manager_get_active_object (self->item_mngr));
  g_return_if_fail (item != NULL);

  app = g_application_get_default ();
  g_application_hold (app);

  photos_base_item_pipeline_save_async (item,
                                        NULL,
                                        photos_preview_view_edit_done_pipeline_save,
                                        g_object_ref (self));
}


static void
photos_preview_view_window_mode_changed (PhotosPreviewView *self, PhotosWindowMode mode, PhotosWindowMode old_mode)
{
  switch (mode)
    {
    case PHOTOS_WINDOW_MODE_COLLECTIONS:
    case PHOTOS_WINDOW_MODE_FAVORITES:
    case PHOTOS_WINDOW_MODE_OVERVIEW:
    case PHOTOS_WINDOW_MODE_SEARCH:
      gtk_revealer_set_reveal_child (GTK_REVEALER (self->revealer), FALSE);
      photos_edit_palette_hide_details (PHOTOS_EDIT_PALETTE (self->palette));
      photos_preview_nav_buttons_hide (self->nav_buttons);
      photos_preview_nav_buttons_set_model (self->nav_buttons, NULL, NULL);
      break;

    case PHOTOS_WINDOW_MODE_EDIT:
      gtk_revealer_set_reveal_child (GTK_REVEALER (self->revealer), TRUE);
      photos_edit_palette_show (PHOTOS_EDIT_PALETTE (self->palette));
      photos_preview_nav_buttons_hide (self->nav_buttons);
      break;

    case PHOTOS_WINDOW_MODE_PREVIEW:
      gtk_revealer_set_reveal_child (GTK_REVEALER (self->revealer), FALSE);
      photos_edit_palette_hide_details (PHOTOS_EDIT_PALETTE (self->palette));
      photos_preview_nav_buttons_show (self->nav_buttons);
      break;

    case PHOTOS_WINDOW_MODE_NONE:
    default:
      g_assert_not_reached ();
      break;
    }
}


static void
photos_preview_view_dispose (GObject *object)
{
  PhotosPreviewView *self = PHOTOS_PREVIEW_VIEW (object);

  g_clear_object (&self->node);
  g_clear_object (&self->item_mngr);
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

  G_OBJECT_CLASS (photos_preview_view_parent_class)->constructed (object);

  self->nav_buttons = photos_preview_nav_buttons_new (self, GTK_OVERLAY (self->overlay));
  gtk_widget_show_all (GTK_WIDGET (self));
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
  gtk_container_add (GTK_CONTAINER (sw), self->palette);
  g_signal_connect_swapped (self->palette, "tool-changed", G_CALLBACK (photos_preview_view_tool_changed), self);

  action = g_action_map_lookup_action (G_ACTION_MAP (app), "brightness-contrast-current");
  g_signal_connect_object (action,
                           "activate",
                           G_CALLBACK (photos_preview_view_brightness_contrast),
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

  action = g_action_map_lookup_action (G_ACTION_MAP (app), "load-next");
  g_signal_connect_swapped (action, "activate", G_CALLBACK (photos_preview_view_navigate_next), self);

  action = g_action_map_lookup_action (G_ACTION_MAP (app), "load-previous");
  g_signal_connect_swapped (action, "activate", G_CALLBACK (photos_preview_view_navigate_previous), self);

  action = g_action_map_lookup_action (G_ACTION_MAP (app), "saturation-current");
  g_signal_connect_object (action,
                           "activate",
                           G_CALLBACK (photos_preview_view_saturation),
                           self,
                           G_CONNECT_SWAPPED);

  action = g_action_map_lookup_action (G_ACTION_MAP (app), "sharpen-current");
  g_signal_connect_object (action, "activate", G_CALLBACK (photos_preview_view_sharpen), self, G_CONNECT_SWAPPED);

  action = g_action_map_lookup_action (G_ACTION_MAP (app), "undo-current");
  g_signal_connect_object (action, "activate", G_CALLBACK (photos_preview_view_undo), self, G_CONNECT_SWAPPED);
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
photos_preview_view_set_model (PhotosPreviewView *self, GtkTreeModel *model, GtkTreePath *current_path)
{
  g_return_if_fail (model != NULL);
  g_return_if_fail (current_path != NULL);

  photos_preview_nav_buttons_set_model (self->nav_buttons, model, current_path);
  photos_preview_nav_buttons_show (self->nav_buttons);
}


void
photos_preview_view_set_node (PhotosPreviewView *self, GeglNode *node)
{
  GtkWidget *view_container;;

  if (self->node == node)
    return;

  view_container = gtk_stack_get_visible_child (GTK_STACK (self->stack));
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
      gegl_gtk_view_set_node (GEGL_GTK_VIEW (view), self->node);
      gtk_widget_queue_draw (view);
    }
}
