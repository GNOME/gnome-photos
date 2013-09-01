/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2013 Red Hat, Inc.
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
#include <libgd/gd.h>

#include "photos-item-manager.h"
#include "photos-preview-model.h"
#include "photos-preview-nav-buttons.h"
#include "photos-view-model.h"


struct _PhotosPreviewNavButtonsPrivate
{
  GtkTreeModel *model;
  GtkTreePath *current_path;
  GtkWidget *favorite_button;
  GtkWidget *next_widget;
  GtkWidget *overlay;
  GtkWidget *prev_widget;
  GtkWidget *preview_view;
  GtkWidget *toolbar_widget;
  PhotosBaseManager *item_mngr;
  gboolean hover;
  gboolean visible;
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


static void photos_preview_nav_buttons_update_favorite (PhotosPreviewNavButtons *self, gboolean favorite);


static void
photos_preview_nav_buttons_active_changed (PhotosPreviewNavButtons *self, GObject *object)
{
  PhotosBaseItem *item = PHOTOS_BASE_ITEM (object);
  gboolean favorite;

  if (object == NULL)
    return;

  favorite = photos_base_item_is_favorite (item);
  photos_preview_nav_buttons_update_favorite (self, favorite);
}


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
photos_preview_nav_buttons_favorite_clicked (PhotosPreviewNavButtons *self)
{
  PhotosPreviewNavButtonsPrivate *priv = self->priv;
  PhotosBaseItem *item;
  gboolean favorite;

  item = PHOTOS_BASE_ITEM (photos_base_manager_get_active_object (priv->item_mngr));
  favorite = photos_base_item_is_favorite (item);
  photos_base_item_set_favorite (item, !favorite);

  photos_preview_nav_buttons_update_favorite (self, !favorite);
}


static gboolean
photos_preview_nav_buttons_auto_hide (PhotosPreviewNavButtons *self)
{
  PhotosPreviewNavButtonsPrivate *priv = self->priv;

  photos_preview_nav_buttons_fade_out_button (self, priv->prev_widget);
  photos_preview_nav_buttons_fade_out_button (self, priv->next_widget);
  photos_preview_nav_buttons_fade_out_button (self, priv->toolbar_widget);
  priv->auto_hide_id = 0;
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
  self->priv->hover = TRUE;
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
  self->priv->hover = FALSE;
  photos_preview_nav_buttons_queue_auto_hide (self);
  return FALSE;
}


static void
photos_preview_nav_buttons_update_favorite (PhotosPreviewNavButtons *self, gboolean favorite)
{
  PhotosPreviewNavButtonsPrivate *priv = self->priv;
  GtkStyleContext *context;
  gchar *favorite_label;

  g_signal_handlers_block_by_func (priv->favorite_button, photos_preview_nav_buttons_favorite_clicked, self);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->favorite_button), favorite);
  g_signal_handlers_unblock_by_func (priv->favorite_button, photos_preview_nav_buttons_favorite_clicked, self);

  context = gtk_widget_get_style_context (priv->favorite_button);
  if (favorite)
    {
      favorite_label = g_strdup (_("Remove from favorites"));
      gtk_style_context_add_class (context, "documents-favorite");
    }
  else
    {
      favorite_label = g_strdup (_("Add to favorites"));
      gtk_style_context_remove_class (context, "documents-favorite");
    }

  gtk_widget_reset_style (priv->favorite_button);
  gtk_widget_set_tooltip_text (priv->favorite_button, favorite_label);
  g_free (favorite_label);
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
      photos_preview_nav_buttons_fade_out_button (self, priv->prev_widget);
      photos_preview_nav_buttons_fade_out_button (self, priv->next_widget);
      photos_preview_nav_buttons_fade_out_button (self, priv->toolbar_widget);
      return;
    }

  photos_preview_nav_buttons_fade_in_button (self, priv->toolbar_widget);

  tmp = iter;
  if (gtk_tree_model_iter_previous (priv->model, &tmp))
      photos_preview_nav_buttons_fade_in_button (self, priv->prev_widget);
  else
      photos_preview_nav_buttons_fade_out_button (self, priv->prev_widget);

  tmp = iter;
  if (gtk_tree_model_iter_next (priv->model, &tmp))
      photos_preview_nav_buttons_fade_in_button (self, priv->next_widget);
  else
      photos_preview_nav_buttons_fade_out_button (self, priv->next_widget);

  if (!priv->hover)
    photos_preview_nav_buttons_queue_auto_hide (self);
}


static gboolean
photos_preview_nav_buttons_motion_notify_timeout (PhotosPreviewNavButtons *self)
{
  self->priv->motion_id = 0;
  photos_preview_nav_buttons_update_visibility (self);
  return G_SOURCE_REMOVE;
}


static gboolean
photos_preview_nav_buttons_motion_notify (PhotosPreviewNavButtons *self)
{
  PhotosPreviewNavButtonsPrivate *priv = self->priv;

  if (priv->motion_id != 0)
    return FALSE;

  priv->motion_id = g_idle_add_full (G_PRIORITY_DEFAULT,
                                     (GSourceFunc) photos_preview_nav_buttons_motion_notify_timeout,
                                     g_object_ref (self),
                                     g_object_unref);
  return FALSE;
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

  gtk_tree_model_get_iter (priv->model, &iter, priv->current_path);
  gtk_tree_model_filter_convert_iter_to_child_iter (GTK_TREE_MODEL_FILTER (priv->model), &child_iter, &iter);
  child_model = gtk_tree_model_filter_get_model (GTK_TREE_MODEL_FILTER (priv->model));
  gtk_tree_model_get (child_model, &child_iter, PHOTOS_VIEW_MODEL_URN, &id, -1);

  item = PHOTOS_BASE_ITEM (photos_base_manager_get_object_by_id (priv->item_mngr, id));
  photos_base_manager_set_active_object (priv->item_mngr, G_OBJECT (item));

  g_free (id);
}


static void
photos_preview_nav_buttons_next_clicked (PhotosPreviewNavButtons *self)
{
  gtk_tree_path_next (self->priv->current_path);
  photos_preview_nav_buttons_set_active_path (self);
}


static void
photos_preview_nav_buttons_prev_clicked (PhotosPreviewNavButtons *self)
{
  gtk_tree_path_prev (self->priv->current_path);
  photos_preview_nav_buttons_set_active_path (self);
}


static void
photos_preview_nav_buttons_dispose (GObject *object)
{
  PhotosPreviewNavButtons *self = PHOTOS_PREVIEW_NAV_BUTTONS (object);
  PhotosPreviewNavButtonsPrivate *priv = self->priv;

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
  GtkWidget *toolbar;
  gboolean is_rtl;
  const gchar *next_icon_name;
  const gchar *prev_icon_name;

  G_OBJECT_CLASS (photos_preview_nav_buttons_parent_class)->constructed (object);

  is_rtl = (gtk_widget_get_direction (priv->preview_view) == GTK_TEXT_DIR_RTL);
  prev_icon_name = is_rtl ? "go-next-symbolic" : "go-previous-symbolic";
  next_icon_name = is_rtl ? "go-previous-symbolic" : "go-next-symbolic";

  priv->prev_widget = gtk_revealer_new ();
  gtk_widget_set_halign (priv->prev_widget, GTK_ALIGN_START);
  gtk_widget_set_margin_left (priv->prev_widget, 30);
  gtk_widget_set_margin_right (priv->prev_widget, 30);
  gtk_widget_set_valign (priv->prev_widget, GTK_ALIGN_CENTER);
  gtk_revealer_set_transition_type (GTK_REVEALER (priv->prev_widget), GTK_REVEALER_TRANSITION_TYPE_CROSSFADE);
  gtk_overlay_add_overlay (GTK_OVERLAY (priv->overlay), priv->prev_widget);

  image = gtk_image_new_from_icon_name (prev_icon_name, GTK_ICON_SIZE_INVALID);
  gtk_image_set_pixel_size (GTK_IMAGE (image), 16);

  button = gtk_button_new ();
  gtk_container_add (GTK_CONTAINER (button), image);
  context = gtk_widget_get_style_context (button);
  gtk_style_context_add_class (context, "osd");
  gtk_container_add (GTK_CONTAINER (priv->prev_widget), button);
  g_signal_connect_swapped (button,
                            "clicked",
                            G_CALLBACK (photos_preview_nav_buttons_prev_clicked),
                            self);
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
  gtk_widget_set_margin_left (priv->next_widget, 30);
  gtk_widget_set_margin_right (priv->next_widget, 30);
  gtk_widget_set_valign (priv->next_widget, GTK_ALIGN_CENTER);
  gtk_revealer_set_transition_type (GTK_REVEALER (priv->next_widget), GTK_REVEALER_TRANSITION_TYPE_CROSSFADE);
  gtk_overlay_add_overlay (GTK_OVERLAY (priv->overlay), priv->next_widget);

  image = gtk_image_new_from_icon_name (next_icon_name, GTK_ICON_SIZE_INVALID);
  gtk_image_set_pixel_size (GTK_IMAGE (image), 16);

  button = gtk_button_new ();
  gtk_container_add (GTK_CONTAINER (button), image);
  context = gtk_widget_get_style_context (button);
  gtk_style_context_add_class (context, "osd");
  gtk_container_add (GTK_CONTAINER (priv->next_widget), button);
  g_signal_connect_swapped (button,
                            "clicked",
                            G_CALLBACK (photos_preview_nav_buttons_next_clicked),
                            self);
  g_signal_connect_swapped (button,
                            "enter-notify-event",
                            G_CALLBACK (photos_preview_nav_buttons_enter_notify),
                            self);
  g_signal_connect_swapped (button,
                            "leave-notify-event",
                            G_CALLBACK (photos_preview_nav_buttons_leave_notify),
                            self);

  priv->toolbar_widget = gtk_revealer_new ();
  gtk_widget_set_halign (priv->toolbar_widget, GTK_ALIGN_FILL);
  gtk_widget_set_valign (priv->toolbar_widget, GTK_ALIGN_END);
  gtk_revealer_set_transition_type (GTK_REVEALER (priv->toolbar_widget), GTK_REVEALER_TRANSITION_TYPE_CROSSFADE);
  gtk_overlay_add_overlay (GTK_OVERLAY (priv->overlay), priv->toolbar_widget);

  toolbar = gtk_header_bar_new ();
  context = gtk_widget_get_style_context (toolbar);
  gtk_style_context_add_class (context, "osd");
  gtk_container_add (GTK_CONTAINER (priv->toolbar_widget), toolbar);
  g_signal_connect_swapped (toolbar,
                            "enter-notify-event",
                            G_CALLBACK (photos_preview_nav_buttons_enter_notify),
                            self);
  g_signal_connect_swapped (toolbar,
                            "leave-notify-event",
                            G_CALLBACK (photos_preview_nav_buttons_leave_notify),
                            self);

  priv->favorite_button = gd_header_toggle_button_new ();
  gd_header_button_set_symbolic_icon_name (GD_HEADER_BUTTON (priv->favorite_button), "emblem-favorite-symbolic");
  gtk_header_bar_pack_end (GTK_HEADER_BAR (toolbar), priv->favorite_button);
  g_signal_connect_swapped (priv->favorite_button,
                            "clicked",
                            G_CALLBACK (photos_preview_nav_buttons_favorite_clicked),
                            self);

  g_signal_connect_swapped (priv->overlay,
                            "motion-notify-event",
                            G_CALLBACK (photos_preview_nav_buttons_motion_notify),
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

  self->priv = photos_preview_nav_buttons_get_instance_private (self);
  priv = self->priv;

  priv->item_mngr = photos_item_manager_dup_singleton ();
  g_signal_connect_swapped (priv->item_mngr,
                            "active-changed",
                            G_CALLBACK (photos_preview_nav_buttons_active_changed),
                            self);
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
  photos_preview_nav_buttons_fade_out_button (self, priv->prev_widget);
  photos_preview_nav_buttons_fade_out_button (self, priv->next_widget);
  photos_preview_nav_buttons_fade_out_button (self, priv->toolbar_widget);
}


void
photos_preview_nav_buttons_set_model (PhotosPreviewNavButtons *self, GtkTreeModel *model, GtkTreePath *current_path)
{
  PhotosPreviewNavButtonsPrivate *priv = self->priv;
  GtkTreePath *child_path = NULL;

  if (priv->model != NULL && gtk_tree_model_filter_get_model (GTK_TREE_MODEL_FILTER (priv->model)) == model)
    {
      if (priv->current_path != NULL)
        {
          child_path = gtk_tree_model_filter_convert_path_to_child_path (GTK_TREE_MODEL_FILTER (priv->model),
                                                                         priv->current_path);
          if (gtk_tree_path_compare (child_path, current_path) == 0)
            goto out;
        }
    }

  g_clear_object (&priv->model);
  if (model != NULL)
    priv->model = photos_preview_model_new (model);

  g_clear_pointer (&priv->current_path, (GDestroyNotify) gtk_tree_path_free);
  if (current_path != NULL)
    {
      priv->current_path = gtk_tree_model_filter_convert_child_path_to_path (GTK_TREE_MODEL_FILTER (priv->model),
                                                                             current_path);
    }

  photos_preview_nav_buttons_update_visibility (self);

 out:
  g_clear_pointer (&child_path, (GDestroyNotify) gtk_tree_path_free);
}


void
photos_preview_nav_buttons_show (PhotosPreviewNavButtons *self)
{
  PhotosPreviewNavButtonsPrivate *priv = self->priv;

  priv->visible = TRUE;
  photos_preview_nav_buttons_fade_in_button (self, priv->prev_widget);
  photos_preview_nav_buttons_fade_in_button (self, priv->next_widget);
  photos_preview_nav_buttons_fade_in_button (self, priv->toolbar_widget);
}
