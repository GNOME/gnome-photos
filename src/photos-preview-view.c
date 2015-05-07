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
#include <gio/gio.h>
#include <glib/gi18n.h>

#include "gegl-gtk-view.h"
#include "photos-item-manager.h"
#include "photos-preview-nav-buttons.h"
#include "photos-preview-view.h"
#include "photos-search-context.h"


struct _PhotosPreviewViewPrivate
{
  GeglNode *node;
  GtkWidget *overlay;
  GtkWidget *stack;
  GtkWidget *view;
  PhotosModeController *mode_cntrlr;
  PhotosPreviewNavButtons *nav_buttons;
};

enum
{
  PROP_0,
  PROP_OVERLAY
};


G_DEFINE_TYPE_WITH_PRIVATE (PhotosPreviewView, photos_preview_view, GTK_TYPE_SCROLLED_WINDOW);


static GtkWidget *photos_preview_view_create_view (PhotosPreviewView *self);


static void
photos_preview_view_draw_background (PhotosPreviewView *self, cairo_t *cr, GdkRectangle *rect, gpointer user_data)
{
  GtkStyleContext *context;
  GtkStateFlags flags;
  GtkWidget *view = GTK_WIDGET (user_data);

  context = gtk_widget_get_style_context (view);
  flags = gtk_widget_get_state_flags (view);
  gtk_style_context_save (context);
  gtk_style_context_set_state (context, flags);
  gtk_render_background (context, cr, 0, 0, rect->width, rect->height);
  gtk_style_context_restore (context);
}


static GtkWidget *
photos_preview_view_get_invisible_view (PhotosPreviewView *self)
{
  PhotosPreviewViewPrivate *priv = self->priv;
  GList *children;
  GList *l;
  GtkWidget *current_view;
  GtkWidget *next_view = NULL;

  current_view = gtk_stack_get_visible_child (GTK_STACK (priv->stack));
  children = gtk_container_get_children (GTK_CONTAINER (priv->stack));
  for (l = children; l != NULL; l = l->next)
    {
      GtkWidget *view = GTK_WIDGET (l->data);

      if (current_view != view)
        {
          next_view = view;
          break;
        }
    }

  g_list_free (children);
  return next_view;
}


static void
photos_preview_view_navigate (PhotosPreviewView *self, gint position)
{
  PhotosPreviewViewPrivate *priv = self->priv;
  GtkWidget *current_view;
  GtkWidget *new_view;
  GtkWidget *next_view;

  current_view = gtk_stack_get_visible_child (GTK_STACK (priv->stack));
  gtk_container_child_set (GTK_CONTAINER (priv->stack), current_view, "position", position, NULL);

  next_view = photos_preview_view_get_invisible_view (self);
  gtk_stack_set_visible_child (GTK_STACK (priv->stack), next_view);

  gtk_container_remove (GTK_CONTAINER (priv->stack), current_view);

  new_view = photos_preview_view_create_view (self);
  gtk_container_add (GTK_CONTAINER (priv->stack), new_view);
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
photos_preview_view_create_view (PhotosPreviewView *self)
{
  GtkStyleContext *context;
  GtkWidget *view;

  view = GTK_WIDGET (gegl_gtk_view_new ());
  gtk_widget_add_events (view, GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK);
  context = gtk_widget_get_style_context (view);
  gtk_style_context_add_class (context, GTK_STYLE_CLASS_VIEW);
  gtk_style_context_add_class (context, "content-view");
  g_signal_connect_swapped (view, "draw-background", G_CALLBACK (photos_preview_view_draw_background), self);

  /* It has to be visible to become the visible child of priv->stack. */
  gtk_widget_show (view);

  return view;
}


static void
photos_preview_view_window_mode_changed (PhotosPreviewView *self, PhotosWindowMode mode, PhotosWindowMode old_mode)
{
  PhotosPreviewViewPrivate *priv = self->priv;

  switch (mode)
    {
    case PHOTOS_WINDOW_MODE_COLLECTIONS:
    case PHOTOS_WINDOW_MODE_EDIT:
    case PHOTOS_WINDOW_MODE_FAVORITES:
    case PHOTOS_WINDOW_MODE_OVERVIEW:
    case PHOTOS_WINDOW_MODE_SEARCH:
      photos_preview_nav_buttons_hide (priv->nav_buttons);
      photos_preview_nav_buttons_set_model (priv->nav_buttons, NULL, NULL);
      break;

    case PHOTOS_WINDOW_MODE_PREVIEW:
      photos_preview_nav_buttons_show (priv->nav_buttons);
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
  PhotosPreviewViewPrivate *priv = self->priv;

  g_clear_object (&priv->node);
  g_clear_object (&priv->mode_cntrlr);

  G_OBJECT_CLASS (photos_preview_view_parent_class)->dispose (object);
}


static void
photos_preview_view_constructed (GObject *object)
{
  PhotosPreviewView *self = PHOTOS_PREVIEW_VIEW (object);
  PhotosPreviewViewPrivate *priv = self->priv;

  G_OBJECT_CLASS (photos_preview_view_parent_class)->constructed (object);

  /* Add the stack to the scrolled window after the default
   * adjustments have been created.
   */
  gtk_container_add (GTK_CONTAINER (self), priv->stack);

  priv->nav_buttons = photos_preview_nav_buttons_new (self, GTK_OVERLAY (priv->overlay));
  gtk_widget_show_all (GTK_WIDGET (self));
}


static void
photos_preview_view_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
  PhotosPreviewView *self = PHOTOS_PREVIEW_VIEW (object);
  PhotosPreviewViewPrivate *priv = self->priv;

  switch (prop_id)
    {
    case PROP_OVERLAY:
      priv->overlay = GTK_WIDGET (g_value_dup_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
photos_preview_view_init (PhotosPreviewView *self)
{
  PhotosPreviewViewPrivate *priv;
  GAction *action;
  GApplication *app;
  GtkStyleContext *context;
  GtkWidget *view;
  PhotosSearchContextState *state;

  self->priv = photos_preview_view_get_instance_private (self);
  priv = self->priv;

  app = g_application_get_default ();
  state = photos_search_context_get_state (PHOTOS_SEARCH_CONTEXT (app));

  priv->mode_cntrlr = g_object_ref (state->mode_cntrlr);
  g_signal_connect_object (priv->mode_cntrlr,
                           "window-mode-changed",
                           G_CALLBACK (photos_preview_view_window_mode_changed),
                           self,
                           G_CONNECT_SWAPPED);

  gtk_widget_set_hexpand (GTK_WIDGET (self), TRUE);
  gtk_widget_set_vexpand (GTK_WIDGET (self), TRUE);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (self), GTK_SHADOW_IN);
  context = gtk_widget_get_style_context (GTK_WIDGET (self));
  gtk_style_context_add_class (context, "documents-scrolledwin");

  priv->stack = gtk_stack_new ();

  view = photos_preview_view_create_view (self);
  gtk_container_add (GTK_CONTAINER (priv->stack), view);
  gtk_stack_set_visible_child (GTK_STACK (priv->stack), view);

  view = photos_preview_view_create_view (self);
  gtk_container_add (GTK_CONTAINER (priv->stack), view);

  action = g_action_map_lookup_action (G_ACTION_MAP (app), "load-next");
  g_signal_connect_swapped (action, "activate", G_CALLBACK (photos_preview_view_navigate_next), self);

  action = g_action_map_lookup_action (G_ACTION_MAP (app), "load-previous");
  g_signal_connect_swapped (action, "activate", G_CALLBACK (photos_preview_view_navigate_previous), self);
}


static void
photos_preview_view_class_init (PhotosPreviewViewClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->constructed = photos_preview_view_constructed;
  object_class->dispose = photos_preview_view_dispose;
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
  PhotosPreviewViewPrivate *priv = self->priv;

  g_return_if_fail (model != NULL);
  g_return_if_fail (current_path != NULL);

  photos_preview_nav_buttons_set_model (self->priv->nav_buttons, model, current_path);
  photos_preview_nav_buttons_show (priv->nav_buttons);
}


void
photos_preview_view_set_node (PhotosPreviewView *self, GeglNode *node)
{
  PhotosPreviewViewPrivate *priv = self->priv;
  GtkWidget *view;

  if (priv->node == node)
    return;

  view = gtk_stack_get_visible_child (GTK_STACK (priv->stack));
  g_clear_object (&priv->node);

  if (node == NULL)
    {
      gtk_container_remove (GTK_CONTAINER (priv->stack), view);

      view = photos_preview_view_create_view (self);
      gtk_container_add (GTK_CONTAINER (priv->stack), view);
    }
  else
    {
      priv->node = g_object_ref (node);

      /* Steals the reference to the GeglNode. */
      gegl_gtk_view_set_node (GEGL_GTK_VIEW (view), g_object_ref (priv->node));
      gtk_widget_queue_draw (view);
    }
}
