/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2012 – 2018 Red Hat, Inc.
 * Copyright © 2017 Umang Jain
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

#include <libgd/gd.h>

#include "photos-empty-results-box.h"
#include "photos-enums.h"
#include "photos-error-box.h"
#include "photos-item-manager.h"
#include "photos-offset-controller.h"
#include "photos-remote-display-manager.h"
#include "photos-search-context.h"
#include "photos-selection-controller.h"
#include "photos-tracker-controller.h"
#include "photos-utils.h"
#include "photos-view-container.h"


struct _PhotosViewContainer
{
  GtkStack parent_instance;
  GAction *selection_mode_action;
  GtkWidget *error_box;
  GtkWidget *no_results;
  GtkWidget *sw;
  GtkWidget *view;
  PhotosBaseManager *item_mngr;
  PhotosModeController *mode_cntrlr;
  PhotosOffsetController *offset_cntrlr;
  PhotosRemoteDisplayManager *remote_mngr;
  PhotosSelectionController *sel_cntrlr;
  PhotosTrackerController *trk_cntrlr;
  PhotosWindowMode mode;
  gboolean disposed;
  gchar *name;
};

enum
{
  PROP_0,
  PROP_MODE,
  PROP_NAME
};


G_DEFINE_TYPE (PhotosViewContainer, photos_view_container, GTK_TYPE_STACK);


static void
photos_view_container_edge_reached (PhotosViewContainer *self, GtkPositionType pos)
{
  if (pos == GTK_POS_BOTTOM)
    photos_offset_controller_increase_offset (self->offset_cntrlr);
}


static void
photos_view_container_connect_view (PhotosViewContainer *self)
{
  g_signal_connect_swapped (self->sw, "edge-reached", G_CALLBACK (photos_view_container_edge_reached), self);
}


static void
photos_view_container_count_changed (PhotosViewContainer *self, gint count)
{
  if (count == 0)
    gtk_stack_set_visible_child_name (GTK_STACK (self), "no-results");
  else
    gtk_stack_set_visible_child_name (GTK_STACK (self), "view");
}


static void
photos_view_container_disconnect_view (PhotosViewContainer *self)
{
  g_signal_handlers_disconnect_by_func (self->sw, photos_view_container_edge_reached, self);
}


static gboolean
photos_view_container_get_show_primary_text (PhotosViewContainer *self)
{
  gboolean ret_val;

  switch (self->mode)
    {
    case PHOTOS_WINDOW_MODE_COLLECTION_VIEW:
      ret_val = FALSE;
      break;

    case PHOTOS_WINDOW_MODE_COLLECTIONS:
      ret_val = TRUE;
      break;

    case PHOTOS_WINDOW_MODE_FAVORITES:
      ret_val = FALSE;
      break;

    case PHOTOS_WINDOW_MODE_IMPORT:
      ret_val = FALSE;
      break;

    case PHOTOS_WINDOW_MODE_OVERVIEW:
      ret_val = FALSE;
      break;

    case PHOTOS_WINDOW_MODE_SEARCH:
      ret_val = TRUE;
      break;

    case PHOTOS_WINDOW_MODE_NONE:
    case PHOTOS_WINDOW_MODE_EDIT:
    case PHOTOS_WINDOW_MODE_PREVIEW:
    default:
      g_assert_not_reached ();
      break;
    }

  return ret_val;
}


static void
photos_view_container_item_activated (PhotosViewContainer *self, GdMainBoxItem *box_item)
{
  PhotosBaseItem *item;
  const gchar *id;

  id = gd_main_box_item_get_id (box_item);
  item = PHOTOS_BASE_ITEM (photos_base_manager_get_object_by_id (self->item_mngr, id));
  g_return_if_fail ((gpointer) box_item == (gpointer) item);

  if (!photos_base_item_is_collection (item) &&
      photos_remote_display_manager_is_active (self->remote_mngr))
    photos_remote_display_manager_render (self->remote_mngr, item);
  else
    photos_base_manager_set_active_object (self->item_mngr, G_OBJECT (item));
}


static void
photos_view_container_set_error (PhotosViewContainer *self, const gchar *primary, const gchar *secondary)
{
  photos_error_box_update (PHOTOS_ERROR_BOX (self->error_box), primary, secondary);
  gtk_stack_set_visible_child_name (GTK_STACK (self), "error");
}


static void
photos_view_container_query_error (PhotosViewContainer *self, const gchar *primary, const gchar *secondary)
{
  photos_view_container_set_error (self, primary, secondary);
}


static void
photos_view_container_query_status_changed (PhotosViewContainer *self, gboolean query_status)
{
  if (!query_status)
    {
      PhotosBaseManager *item_mngr_chld;

      item_mngr_chld = photos_item_manager_get_for_mode (PHOTOS_ITEM_MANAGER (self->item_mngr), self->mode);
      gd_main_box_set_model (GD_MAIN_BOX (self->view), G_LIST_MODEL (item_mngr_chld));
      photos_selection_controller_freeze_selection (self->sel_cntrlr, FALSE);
      /* TODO: update selection */
    }
  else
    {
      photos_selection_controller_freeze_selection (self->sel_cntrlr, TRUE);
      gd_main_box_set_model (GD_MAIN_BOX (self->view), NULL);
    }
}


static void
photos_view_container_select_all (PhotosViewContainer *self)
{
  PhotosWindowMode mode;
  GVariant *new_state;

  mode = photos_mode_controller_get_window_mode (self->mode_cntrlr);
  if (self->mode != mode)
    return;

  new_state = g_variant_new ("b", TRUE);
  g_action_change_state (self->selection_mode_action, new_state);

  gd_main_box_select_all (GD_MAIN_BOX (self->view));
}


static void
photos_view_container_select_none (PhotosViewContainer *self)
{
  PhotosWindowMode mode;

  mode = photos_mode_controller_get_window_mode (self->mode_cntrlr);
  if (self->mode != mode)
    return;

  gd_main_box_unselect_all (GD_MAIN_BOX (self->view));
}


static void
photos_view_container_selection_mode_request (PhotosViewContainer *self)
{
  GVariant *new_state;

  new_state = g_variant_new ("b", TRUE);
  g_action_change_state (self->selection_mode_action, new_state);
}


static void
photos_view_container_selection_changed (PhotosViewContainer *self)
{
  GList *selected_urns;
  GList *selection;

  selection = gd_main_box_get_selection (GD_MAIN_BOX (self->view));
  selected_urns = photos_utils_get_urns_from_items (selection);
  photos_selection_controller_set_selection (self->sel_cntrlr, selected_urns);

  g_list_free_full (selected_urns, g_free);
  g_list_free_full (selection, g_object_unref);
}


static void
photos_view_container_set_selection_mode (PhotosViewContainer *self, gboolean selection_mode)
{
  PhotosWindowMode window_mode;

  window_mode = photos_mode_controller_get_window_mode (self->mode_cntrlr);
  if (self->mode != window_mode)
    return;

  gd_main_box_set_selection_mode (GD_MAIN_BOX (self->view), selection_mode);
}


static void
photos_view_container_selection_mode_notify_state (PhotosViewContainer *self)
{
  gboolean selection_mode;

  selection_mode = photos_utils_get_selection_mode ();
  photos_view_container_set_selection_mode (self, selection_mode);
}


static void
photos_view_container_window_mode_changed (PhotosViewContainer *self,
                                           PhotosWindowMode mode,
                                           PhotosWindowMode old_mode)
{
  photos_view_container_disconnect_view (self);

  switch (mode)
    {
    case PHOTOS_WINDOW_MODE_NONE:
      g_assert_not_reached ();
      break;

    case PHOTOS_WINDOW_MODE_COLLECTION_VIEW:
    case PHOTOS_WINDOW_MODE_COLLECTIONS:
    case PHOTOS_WINDOW_MODE_FAVORITES:
    case PHOTOS_WINDOW_MODE_IMPORT:
    case PHOTOS_WINDOW_MODE_OVERVIEW:
    case PHOTOS_WINDOW_MODE_SEARCH:
      photos_view_container_connect_view (self);
      break;

    case PHOTOS_WINDOW_MODE_EDIT:
    case PHOTOS_WINDOW_MODE_PREVIEW:
    default:
      break;
    }
}


static void
photos_view_container_constructed (GObject *object)
{
  PhotosViewContainer *self = PHOTOS_VIEW_CONTAINER (object);
  AtkObject *accessible;
  GAction *action;
  GApplication *app;
  GtkStyleContext *context;
  PhotosSearchContextState *state;
  gboolean selection_mode;
  gboolean show_primary_text;
  gboolean status;

  G_OBJECT_CLASS (photos_view_container_parent_class)->constructed (object);

  app = g_application_get_default ();
  state = photos_search_context_get_state (PHOTOS_SEARCH_CONTEXT (app));

  self->selection_mode_action = g_action_map_lookup_action (G_ACTION_MAP (app), "selection-mode");
  g_signal_connect_object (self->selection_mode_action,
                           "notify::state",
                           G_CALLBACK (photos_view_container_selection_mode_notify_state),
                           self,
                           G_CONNECT_SWAPPED);

  accessible = gtk_widget_get_accessible (GTK_WIDGET (self));
  if (accessible != NULL)
    atk_object_set_name (accessible, self->name);

  self->sw = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_set_hexpand (self->sw, TRUE);
  gtk_widget_set_vexpand (self->sw, TRUE);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (self->sw), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (self->sw), GTK_SHADOW_IN);
  context = gtk_widget_get_style_context (GTK_WIDGET (self->sw));
  gtk_style_context_add_class (context, "documents-scrolledwin");
  gtk_stack_add_named (GTK_STACK (self), self->sw, "view");

  self->view = gd_main_box_new (GD_MAIN_BOX_ICON);
  show_primary_text = photos_view_container_get_show_primary_text (self);
  gd_main_box_set_show_primary_text (GD_MAIN_BOX (self->view), show_primary_text);
  gtk_container_add (GTK_CONTAINER (self->sw), self->view);

  self->no_results = photos_empty_results_box_new (self->mode);
  gtk_stack_add_named (GTK_STACK (self), self->no_results, "no-results");

  self->error_box = photos_error_box_new ();
  gtk_stack_add_named (GTK_STACK (self), self->error_box, "error");

  gtk_widget_show_all (GTK_WIDGET (self));

  gtk_stack_set_visible_child_full (GTK_STACK (self), "view", GTK_STACK_TRANSITION_TYPE_NONE);

  g_signal_connect_swapped (self->view, "item-activated", G_CALLBACK (photos_view_container_item_activated), self);
  g_signal_connect_swapped (self->view,
                            "selection-mode-request",
                            G_CALLBACK (photos_view_container_selection_mode_request),
                            self);
  g_signal_connect_swapped (self->view,
                            "selection-changed",
                            G_CALLBACK (photos_view_container_selection_changed),
                            self);

  self->item_mngr = g_object_ref (state->item_mngr);
  self->sel_cntrlr = photos_selection_controller_dup_singleton ();

  self->mode_cntrlr = g_object_ref (state->mode_cntrlr);
  g_signal_connect_object (self->mode_cntrlr,
                           "window-mode-changed",
                           G_CALLBACK (photos_view_container_window_mode_changed),
                           self,
                           G_CONNECT_SWAPPED);

  self->remote_mngr = photos_remote_display_manager_dup_singleton ();
  photos_utils_get_controller (self->mode, &self->offset_cntrlr, &self->trk_cntrlr);

  action = g_action_map_lookup_action (G_ACTION_MAP (app), "select-all");
  g_signal_connect_object (action,
                           "activate",
                           G_CALLBACK (photos_view_container_select_all),
                           self,
                           G_CONNECT_SWAPPED);

  action = g_action_map_lookup_action (G_ACTION_MAP (app), "select-none");
  g_signal_connect_object (action,
                           "activate",
                           G_CALLBACK (photos_view_container_select_none),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->offset_cntrlr,
                           "count-changed",
                           G_CALLBACK (photos_view_container_count_changed),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->trk_cntrlr,
                           "query-error",
                           G_CALLBACK (photos_view_container_query_error),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (self->trk_cntrlr,
                           "query-status-changed",
                           G_CALLBACK (photos_view_container_query_status_changed),
                           self,
                           G_CONNECT_SWAPPED);

  selection_mode = photos_utils_get_selection_mode ();
  photos_view_container_set_selection_mode (self, selection_mode);

  photos_tracker_controller_start (self->trk_cntrlr);

  status = photos_tracker_controller_get_query_status (self->trk_cntrlr);
  photos_view_container_query_status_changed (self, status);
}


static void
photos_view_container_dispose (GObject *object)
{
  PhotosViewContainer *self = PHOTOS_VIEW_CONTAINER (object);

  if (!self->disposed)
    {
      photos_view_container_disconnect_view (self);
      self->disposed = TRUE;
    }

  g_clear_object (&self->item_mngr);
  g_clear_object (&self->mode_cntrlr);
  g_clear_object (&self->offset_cntrlr);
  g_clear_object (&self->remote_mngr);
  g_clear_object (&self->sel_cntrlr);
  g_clear_object (&self->trk_cntrlr);

  G_OBJECT_CLASS (photos_view_container_parent_class)->dispose (object);
}


static void
photos_view_container_finalize (GObject *object)
{
  PhotosViewContainer *self = PHOTOS_VIEW_CONTAINER (object);

  g_free (self->name);

  G_OBJECT_CLASS (photos_view_container_parent_class)->finalize (object);
}


static void
photos_view_container_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
  PhotosViewContainer *self = PHOTOS_VIEW_CONTAINER (object);

  switch (prop_id)
    {
    case PROP_NAME:
      g_value_set_string (value, self->name);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
photos_view_container_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
  PhotosViewContainer *self = PHOTOS_VIEW_CONTAINER (object);

  switch (prop_id)
    {
    case PROP_MODE:
      self->mode = (PhotosWindowMode) g_value_get_enum (value);
      break;

    case PROP_NAME:
      self->name = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
photos_view_container_init (PhotosViewContainer *self)
{
}


static void
photos_view_container_class_init (PhotosViewContainerClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->constructed = photos_view_container_constructed;
  object_class->dispose = photos_view_container_dispose;
  object_class->finalize = photos_view_container_finalize;
  object_class->get_property = photos_view_container_get_property;
  object_class->set_property = photos_view_container_set_property;

  g_object_class_install_property (object_class,
                                   PROP_MODE,
                                   g_param_spec_enum ("mode",
                                                      "PhotosWindowMode enum",
                                                      "The mode for which the widget is a view",
                                                      PHOTOS_TYPE_WINDOW_MODE,
                                                      PHOTOS_WINDOW_MODE_NONE,
                                                      G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE));

  g_object_class_install_property (object_class,
                                   PROP_NAME,
                                   g_param_spec_string ("name",
                                                        "Title",
                                                        "The string used to identify this view",
                                                        NULL,
                                                        G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE));
}


GtkWidget *
photos_view_container_new (PhotosWindowMode mode, const gchar *name)
{
  return g_object_new (PHOTOS_TYPE_VIEW_CONTAINER,
                       "homogeneous", TRUE,
                       "mode", mode,
                       "name", name,
                       "transition-type", GTK_STACK_TRANSITION_TYPE_CROSSFADE,
                       NULL);
}


void
photos_view_container_activate_result (PhotosViewContainer *self)
{
  PhotosBaseItem *item;
  PhotosBaseManager *item_mngr_chld;

  item_mngr_chld = photos_item_manager_get_for_mode (PHOTOS_ITEM_MANAGER (self->item_mngr), self->mode);
  item = PHOTOS_BASE_ITEM (g_list_model_get_object (G_LIST_MODEL (item_mngr_chld), 0));
  photos_base_manager_set_active_object (self->item_mngr, G_OBJECT (item));
}


const gchar *
photos_view_container_get_name (PhotosViewContainer *self)
{
  return self->name;
}
