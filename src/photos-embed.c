/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2014 Pranav Kant
 * Copyright © 2012 – 2017 Red Hat, Inc.
 * Copyright © 2014 Saurav Agarwalla
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

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gegl.h>
#include <gio/gio.h>
#include <glib/gi18n.h>

#include "photos-base-item.h"
#include "photos-embed.h"
#include "photos-filterable.h"
#include "photos-item-manager.h"
#include "photos-notification-manager.h"
#include "photos-search-controller.h"
#include "photos-selection-toolbar.h"
#include "photos-spinner-box.h"
#include "photos-search-context.h"
#include "photos-search-type.h"
#include "photos-search-type-manager.h"
#include "photos-source.h"
#include "photos-source-manager.h"
#include "photos-source-notification.h"
#include "photos-tracker-controller.h"
#include "photos-tracker-overview-controller.h"
#include "photos-utils.h"
#include "photos-view-container.h"


typedef struct _PhotosEmbedSearchState PhotosEmbedSearchState;

struct _PhotosEmbedSearchState
{
  GObject *search_type;
  GObject *source;
  gboolean saved;
  gchar *str;
};

struct _PhotosEmbed
{
  GtkBox parent_instance;
  GAction *search_action;
  GHashTable *notifications;
  GIOExtensionPoint *extension_point;
  GtkWidget *collections;
  GtkWidget *favorites;
  GtkWidget *no_results;
  GtkWidget *ntfctn_mngr;
  GtkWidget *overview;
  GtkWidget *preview;
  GtkWidget *search;
  GtkWidget *spinner_box;
  GtkWidget *stack;
  GtkWidget *stack_overlay;
  GtkWidget *toolbar;
  PhotosBaseManager *item_mngr;
  PhotosBaseManager *src_mngr;
  PhotosBaseManager *srch_mngr;
  PhotosEmbedSearchState search_state;
  PhotosModeController *mode_cntrlr;
  PhotosSearchController *srch_cntrlr;
  PhotosTrackerController *trk_ovrvw_cntrlr;
  guint load_show_id;
};


G_DEFINE_TYPE (PhotosEmbed, photos_embed, GTK_TYPE_BOX);


static void photos_embed_search_changed (PhotosEmbed *self);


static void
photos_embed_block_search_changed (PhotosEmbed *self)
{
  g_signal_handlers_block_by_func (self->src_mngr, photos_embed_search_changed, self);
  g_signal_handlers_block_by_func (self->srch_mngr, photos_embed_search_changed, self);
  g_signal_handlers_block_by_func (self->srch_cntrlr, photos_embed_search_changed, self);
}


static void
photos_embed_unblock_search_changed (PhotosEmbed *self)
{
  g_signal_handlers_unblock_by_func (self->src_mngr, photos_embed_search_changed, self);
  g_signal_handlers_unblock_by_func (self->srch_mngr, photos_embed_search_changed, self);
  g_signal_handlers_unblock_by_func (self->srch_cntrlr, photos_embed_search_changed, self);
}


static void
photos_embed_clear_load_timer (PhotosEmbed *self)
{
  if (self->load_show_id != 0)
    {
      g_source_remove (self->load_show_id);
      self->load_show_id = 0;
    }
}


static void
photos_embed_clear_search (PhotosEmbed *self)
{
  g_clear_object (&self->search_state.search_type);
  g_clear_object (&self->search_state.source);
  g_clear_pointer (&self->search_state.str, g_free);
}


static GtkWidget*
photos_embed_get_view_container_from_mode (PhotosEmbed *self, PhotosWindowMode mode)
{
  GtkWidget *view_container;

  switch (mode)
    {
    case PHOTOS_WINDOW_MODE_COLLECTIONS:
      view_container = self->collections;
      break;

    case PHOTOS_WINDOW_MODE_FAVORITES:
      view_container = self->favorites;
      break;

    case PHOTOS_WINDOW_MODE_OVERVIEW:
      view_container = self->overview;
      break;


    case PHOTOS_WINDOW_MODE_SEARCH:
      view_container = self->search;
      break;

    case PHOTOS_WINDOW_MODE_NONE:
    case PHOTOS_WINDOW_MODE_EDIT:
    case PHOTOS_WINDOW_MODE_PREVIEW:
    default:
      g_assert_not_reached ();
      break;
    }

  return view_container;
}


static void
photos_embed_activate_result (PhotosEmbed *self)
{
  GtkWidget *view_container;
  PhotosWindowMode mode;

  mode = photos_mode_controller_get_window_mode (self->mode_cntrlr);
  view_container = photos_embed_get_view_container_from_mode (self, mode);
  photos_view_container_activate_result (PHOTOS_VIEW_CONTAINER (view_container));
}


static void
photos_embed_restore_search (PhotosEmbed *self)
{
  GVariant *state;

  if (!self->search_state.saved)
    return;

  photos_embed_block_search_changed (self);
  photos_base_manager_set_active_object (self->src_mngr, self->search_state.source);
  photos_base_manager_set_active_object (self->srch_mngr, self->search_state.search_type);
  photos_search_controller_set_string (self->srch_cntrlr, self->search_state.str);
  photos_embed_unblock_search_changed (self);

  self->search_state.saved = FALSE;
  photos_embed_clear_search (self);

  state = g_variant_new ("b", TRUE);
  g_action_change_state (self->search_action, state);
}


static void
photos_embed_save_search (PhotosEmbed *self)
{
  GVariant *state;

  if (self->search_state.saved)
    return;

  photos_embed_clear_search (self);

  self->search_state.source = g_object_ref (photos_base_manager_get_active_object (self->src_mngr));
  self->search_state.search_type = g_object_ref (photos_base_manager_get_active_object (self->srch_mngr));
  self->search_state.str = g_strdup (photos_search_controller_get_string (self->srch_cntrlr));
  self->search_state.saved = TRUE;

  photos_embed_block_search_changed (self);
  state = g_variant_new ("b", FALSE);
  g_action_change_state (self->search_action, state);
  photos_embed_unblock_search_changed (self);
}


static void
photos_embed_tracker_controllers_set_frozen (PhotosEmbed *self, gboolean frozen)
{
  GList *extensions;
  GList *l;

  extensions = g_io_extension_point_get_extensions (self->extension_point);
  for (l = extensions; l != NULL; l = l->next)
    {
      GIOExtension *extension = (GIOExtension *) l->data;
      GType type;
      PhotosTrackerController *trk_cntrlr;

      type = g_io_extension_get_type (extension);

      /* Strictly speaking, we need to set the "mode" too, but that
       * is a bit inconvenient, so we operate under the assumption
       * that the objects have already been constructed.
       */
      trk_cntrlr = PHOTOS_TRACKER_CONTROLLER (g_object_new (type, NULL));
      photos_tracker_controller_set_frozen (trk_cntrlr, frozen);
      g_object_unref (trk_cntrlr);
    }
}


static void
photos_embed_prepare_for_preview (PhotosEmbed *self, PhotosWindowMode old_mode)
{
  PhotosBaseItem *active_collection;
  PhotosBaseItem *item;

  /* TODO: SearchController,
   *       ErrorHandler
   */

  item = PHOTOS_BASE_ITEM (photos_base_manager_get_active_object (self->item_mngr));
  g_return_if_fail (PHOTOS_IS_BASE_ITEM (item));

  /* We want to freeze before saving the search state and to thaw
   * after restoring it. We could thaw it earlier too, but that would
   * lead to a bunch of needless queries from the TrackerControllers.
   *
   * Note that we don't want to freeze when showing a collection.
   */
  photos_embed_tracker_controllers_set_frozen (self, TRUE);

  active_collection = photos_item_manager_get_active_collection (PHOTOS_ITEM_MANAGER (self->item_mngr));
  if (old_mode == PHOTOS_WINDOW_MODE_SEARCH)
    {
      if (active_collection == NULL)
        photos_embed_save_search (self);
    }
  else
    {
      GVariant *state;

      /* Hide any empty search bars that might have been floating
       * around.
       */
      state = g_variant_new ("b", FALSE);
      g_action_change_state (self->search_action, state);
    }

  /* This is not needed when activated from the search provider, or
   * when returning from the edit mode.
   */
  if (old_mode != PHOTOS_WINDOW_MODE_NONE && old_mode != PHOTOS_WINDOW_MODE_EDIT)
    photos_preview_view_set_mode (PHOTOS_PREVIEW_VIEW (self->preview), old_mode);

  if (old_mode != PHOTOS_WINDOW_MODE_EDIT)
    photos_preview_view_set_node (PHOTOS_PREVIEW_VIEW (self->preview), NULL);

  photos_spinner_box_stop (PHOTOS_SPINNER_BOX (self->spinner_box));
  gtk_stack_set_visible_child_name (GTK_STACK (self->stack), "preview");
}


static void
photos_embed_load_finished (PhotosEmbed *self, PhotosBaseItem *item, GeglNode *node)
{
  photos_embed_clear_load_timer (self);
  photos_spinner_box_stop (PHOTOS_SPINNER_BOX (self->spinner_box));

  if (node == NULL)
    return;

  photos_preview_view_set_node (PHOTOS_PREVIEW_VIEW (self->preview), node);

  /* TODO: set toolbar model */
}


static gboolean
photos_embed_load_show_timeout (gpointer user_data)
{
  PhotosEmbed *self = PHOTOS_EMBED (user_data);

  self->load_show_id = 0;
  photos_spinner_box_start (PHOTOS_SPINNER_BOX (self->spinner_box));
  return G_SOURCE_REMOVE;
}


static void
photos_embed_load_started (PhotosEmbed *self, PhotosBaseItem *item)
{
  photos_embed_clear_load_timer (self);
  self->load_show_id = g_timeout_add (400, photos_embed_load_show_timeout, self);
}


static void
photos_embed_active_collection_changed (PhotosBaseManager *manager, PhotosBaseItem *collection, gpointer user_data)
{
  PhotosEmbed *self = PHOTOS_EMBED (user_data);
  PhotosWindowMode mode;

  mode = photos_mode_controller_get_window_mode (self->mode_cntrlr);
  if (mode != PHOTOS_WINDOW_MODE_SEARCH)
    return;

  if (collection == NULL)
    photos_embed_restore_search (self);
  else
    photos_embed_save_search (self);
}


static void
photos_embed_fullscreen_changed (PhotosModeController *mode_cntrlr, gboolean fullscreen, gpointer user_data)
{
}


static void
photos_embed_notify_visible_child (PhotosEmbed *self)
{
  GdkEvent *event = NULL;
  GtkWidget *visible_child;
  PhotosWindowMode mode = PHOTOS_WINDOW_MODE_NONE;

  event = gtk_get_current_event ();
  if (event == NULL)
    goto out;

  if (event->type != GDK_BUTTON_RELEASE && event->type != GDK_TOUCH_END)
    goto out;

  visible_child = gtk_stack_get_visible_child (GTK_STACK (self->stack));
  if (visible_child == self->overview)
    mode = PHOTOS_WINDOW_MODE_OVERVIEW;
  else if (visible_child == self->collections)
    mode = PHOTOS_WINDOW_MODE_COLLECTIONS;
  else if (visible_child == self->favorites)
    mode = PHOTOS_WINDOW_MODE_FAVORITES;

  if (mode == PHOTOS_WINDOW_MODE_NONE)
    goto out;

  photos_mode_controller_set_window_mode (self->mode_cntrlr, mode);

 out:
  g_clear_pointer (&event, (GDestroyNotify) gdk_event_free);
}


static void
photos_embed_prepare_for_collections (PhotosEmbed *self, PhotosWindowMode old_mode)
{
  if (old_mode == PHOTOS_WINDOW_MODE_PREVIEW)
    photos_embed_tracker_controllers_set_frozen (self, FALSE);

  photos_spinner_box_stop (PHOTOS_SPINNER_BOX (self->spinner_box));
  gtk_stack_set_visible_child_name (GTK_STACK (self->stack), "collections");
}


static void
photos_embed_prepare_for_favorites (PhotosEmbed *self, PhotosWindowMode old_mode)
{
  if (old_mode == PHOTOS_WINDOW_MODE_PREVIEW)
    photos_embed_tracker_controllers_set_frozen (self, FALSE);

  photos_spinner_box_stop (PHOTOS_SPINNER_BOX (self->spinner_box));
  gtk_stack_set_visible_child_name (GTK_STACK (self->stack), "favorites");
}


static void
photos_embed_prepare_for_overview (PhotosEmbed *self, PhotosWindowMode old_mode)
{
  if (old_mode == PHOTOS_WINDOW_MODE_PREVIEW)
    photos_embed_tracker_controllers_set_frozen (self, FALSE);

  photos_spinner_box_stop (PHOTOS_SPINNER_BOX (self->spinner_box));
  gtk_stack_set_visible_child_name (GTK_STACK (self->stack), "overview");
}


static void
photos_embed_prepare_for_search (PhotosEmbed *self, PhotosWindowMode old_mode)
{
  if (old_mode == PHOTOS_WINDOW_MODE_PREVIEW)
    {
      PhotosBaseItem *active_collection;

      active_collection = photos_item_manager_get_active_collection (PHOTOS_ITEM_MANAGER (self->item_mngr));
      if (active_collection == NULL)
        photos_embed_restore_search (self);

      photos_embed_tracker_controllers_set_frozen (self, FALSE);
    }

  photos_spinner_box_stop (PHOTOS_SPINNER_BOX (self->spinner_box));
  gtk_stack_set_visible_child_name (GTK_STACK (self->stack), "search");
}


static void
photos_embed_query_status_changed (PhotosEmbed *self, gboolean querying)
{
  if (querying)
    photos_spinner_box_start (PHOTOS_SPINNER_BOX (self->spinner_box));
  else
    photos_spinner_box_stop (PHOTOS_SPINNER_BOX (self->spinner_box));
}


static void
photos_embed_search_changed (PhotosEmbed *self)
{
  GObject *object;
  const gchar *search_type_id;
  const gchar *source_id;
  const gchar *str;

  /* Whenever a search constraint is specified we want to switch to
   * the search mode, and when all constraints have been lifted we
   * want to go back to the previous mode which can be either
   * collections, favorites or overview.
   */

  object = photos_base_manager_get_active_object (self->src_mngr);
  source_id = photos_filterable_get_id (PHOTOS_FILTERABLE (object));

  object = photos_base_manager_get_active_object (self->srch_mngr);
  search_type_id = photos_filterable_get_id (PHOTOS_FILTERABLE (object));

  str = photos_search_controller_get_string (self->srch_cntrlr);

  if (g_strcmp0 (search_type_id, PHOTOS_SEARCH_TYPE_STOCK_ALL) == 0
      && g_strcmp0 (source_id, PHOTOS_SOURCE_STOCK_ALL) == 0
      && (str == NULL || str [0] == '\0'))
    photos_mode_controller_go_back (self->mode_cntrlr);
  else
    photos_mode_controller_set_window_mode (self->mode_cntrlr, PHOTOS_WINDOW_MODE_SEARCH);
}


static void
photos_embed_source_manager_notification_hide (PhotosEmbed *self, PhotosSource *source)
{
  GtkWidget *ntfctn;
  gboolean removed;
  const gchar *id;

  g_return_if_fail (PHOTOS_IS_EMBED (self));
  g_return_if_fail (PHOTOS_IS_SOURCE (source));

  id = photos_filterable_get_id (PHOTOS_FILTERABLE (source));
  ntfctn = GTK_WIDGET (g_hash_table_lookup (self->notifications, id));
  if (ntfctn == NULL)
    return;

  gtk_widget_destroy (ntfctn);

  removed = g_hash_table_remove (self->notifications, id);
  g_return_if_fail (removed);
}


static void
photos_embed_source_notification_closed (PhotosSourceNotification *ntfctn, gpointer user_data)
{
  PhotosEmbed *self = PHOTOS_EMBED (user_data);
  PhotosSource *source;
  gboolean removed;
  const gchar *id;

  g_return_if_fail (PHOTOS_IS_SOURCE_NOTIFICATION (ntfctn));

  source = photos_source_notification_get_source (ntfctn);
  id = photos_filterable_get_id (PHOTOS_FILTERABLE (source));
  g_return_if_fail (g_hash_table_contains (self->notifications, id));

  gtk_widget_destroy (GTK_WIDGET (ntfctn));

  removed = g_hash_table_remove (self->notifications, id);
  g_return_if_fail (removed);
}


static void
photos_embed_source_manager_notification_show (PhotosEmbed *self, PhotosSource *source)
{
  GtkWidget *ntfctn;
  gboolean inserted;
  const gchar *id;

  g_return_if_fail (PHOTOS_IS_EMBED (self));
  g_return_if_fail (PHOTOS_IS_SOURCE (source));

  id = photos_filterable_get_id (PHOTOS_FILTERABLE (source));
  g_return_if_fail (!g_hash_table_contains (self->notifications, id));

  ntfctn = photos_source_notification_new (source);
  photos_notification_manager_add_notification (PHOTOS_NOTIFICATION_MANAGER (self->ntfctn_mngr), ntfctn);
  g_signal_connect (ntfctn, "closed", G_CALLBACK (photos_embed_source_notification_closed), self);

  inserted = g_hash_table_insert (self->notifications, g_strdup (id), g_object_ref_sink (ntfctn));
  g_return_if_fail (inserted);
}


static void
photos_embed_window_mode_changed (PhotosModeController *mode_cntrlr,
                                  PhotosWindowMode mode,
                                  PhotosWindowMode old_mode,
                                  gpointer user_data)
{
  PhotosEmbed *self = PHOTOS_EMBED (user_data);

  switch (mode)
    {
    case PHOTOS_WINDOW_MODE_COLLECTIONS:
    case PHOTOS_WINDOW_MODE_FAVORITES:
    case PHOTOS_WINDOW_MODE_OVERVIEW:
      if (!photos_main_toolbar_is_focus (PHOTOS_MAIN_TOOLBAR (self->toolbar)))
        {
          GVariant *state;

          photos_embed_block_search_changed (self);
          state = g_variant_new ("b", FALSE);
          g_action_change_state (self->search_action, state);
          photos_embed_unblock_search_changed (self);
        }
      break;

    case PHOTOS_WINDOW_MODE_EDIT:
    case PHOTOS_WINDOW_MODE_PREVIEW:
    case PHOTOS_WINDOW_MODE_SEARCH:
      break;

    case PHOTOS_WINDOW_MODE_NONE:
    default:
      g_assert_not_reached ();
      break;
    }

  photos_main_toolbar_reset_toolbar_mode (PHOTOS_MAIN_TOOLBAR (self->toolbar));

  switch (mode)
    {
    case PHOTOS_WINDOW_MODE_EDIT:
      break;

    case PHOTOS_WINDOW_MODE_COLLECTIONS:
      photos_embed_prepare_for_collections (self, old_mode);
      break;

    case PHOTOS_WINDOW_MODE_FAVORITES:
      photos_embed_prepare_for_favorites (self, old_mode);
      break;

    case PHOTOS_WINDOW_MODE_OVERVIEW:
      photos_embed_prepare_for_overview (self, old_mode);
      break;

    case PHOTOS_WINDOW_MODE_PREVIEW:
      photos_embed_prepare_for_preview (self, old_mode);
      break;

    case PHOTOS_WINDOW_MODE_SEARCH:
      photos_embed_prepare_for_search (self, old_mode);
      break;

    case PHOTOS_WINDOW_MODE_NONE:
    default:
      g_assert_not_reached ();
      break;
    }

  return;
}


static void
photos_embed_dispose (GObject *object)
{
  PhotosEmbed *self = PHOTOS_EMBED (object);

  photos_embed_clear_load_timer (self);
  photos_embed_clear_search (self);

  g_clear_object (&self->ntfctn_mngr);
  g_clear_object (&self->item_mngr);
  g_clear_object (&self->src_mngr);
  g_clear_object (&self->srch_mngr);
  g_clear_object (&self->mode_cntrlr);
  g_clear_object (&self->srch_cntrlr);
  g_clear_object (&self->trk_ovrvw_cntrlr);
  g_clear_pointer (&self->notifications, (GDestroyNotify) g_hash_table_unref);

  G_OBJECT_CLASS (photos_embed_parent_class)->dispose (object);
}


static void
photos_embed_init (PhotosEmbed *self)
{
  GApplication *app;
  GList *windows;
  PhotosSearchbar *searchbar;
  PhotosSearchContextState *state;
  gboolean querying;
  const gchar *name;

  gtk_widget_init_template (GTK_WIDGET (self));

  app = g_application_get_default ();
  state = photos_search_context_get_state (PHOTOS_SEARCH_CONTEXT (app));

  self->search_action = g_action_map_lookup_action (G_ACTION_MAP (app), "search");
  self->notifications = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);

  self->extension_point = g_io_extension_point_lookup (PHOTOS_TRACKER_CONTROLLER_EXTENSION_POINT_NAME);

  self->toolbar = photos_main_toolbar_new (GTK_OVERLAY (self->stack_overlay));
  photos_main_toolbar_set_stack (PHOTOS_MAIN_TOOLBAR (self->toolbar), GTK_STACK (self->stack));
  windows = gtk_application_get_windows (GTK_APPLICATION (app));
  gtk_window_set_titlebar (GTK_WINDOW (windows->data), self->toolbar);
  searchbar = photos_main_toolbar_get_searchbar (PHOTOS_MAIN_TOOLBAR (self->toolbar));
  g_signal_connect_swapped (searchbar, "activate-result", G_CALLBACK (photos_embed_activate_result), self);

  self->ntfctn_mngr = photos_notification_manager_dup_singleton ();
  gtk_overlay_add_overlay (GTK_OVERLAY (self->stack_overlay), self->ntfctn_mngr);

  self->overview = photos_view_container_new (PHOTOS_WINDOW_MODE_OVERVIEW, _("Photos"));
  name = photos_view_container_get_name (PHOTOS_VIEW_CONTAINER (self->overview));
  gtk_stack_add_titled (GTK_STACK (self->stack), self->overview, "overview", name);

  self->collections = photos_view_container_new (PHOTOS_WINDOW_MODE_COLLECTIONS, _("Albums"));
  name = photos_view_container_get_name (PHOTOS_VIEW_CONTAINER (self->collections));
  gtk_stack_add_titled (GTK_STACK (self->stack), self->collections, "collections", name);

  self->favorites = photos_view_container_new (PHOTOS_WINDOW_MODE_FAVORITES, _("Favorites"));
  name = photos_view_container_get_name (PHOTOS_VIEW_CONTAINER (self->favorites));
  gtk_stack_add_titled (GTK_STACK (self->stack), self->favorites, "favorites", name);

  self->search = photos_view_container_new (PHOTOS_WINDOW_MODE_SEARCH, _("Search"));
  gtk_stack_add_named (GTK_STACK (self->stack), self->search, "search");

  self->preview = photos_preview_view_new (GTK_OVERLAY (self->stack_overlay));
  gtk_stack_add_named (GTK_STACK (self->stack), self->preview, "preview");

  /* TODO: SearchBar.Dropdown, …
   */

  g_signal_connect_object (self->stack,
                           "notify::visible-child",
                           G_CALLBACK (photos_embed_notify_visible_child),
                           self,
                           G_CONNECT_SWAPPED);

  self->mode_cntrlr = g_object_ref (state->mode_cntrlr);
  g_signal_connect_object (self->mode_cntrlr,
                           "window-mode-changed",
                           G_CALLBACK (photos_embed_window_mode_changed),
                           self,
                           0);
  g_signal_connect_object (self->mode_cntrlr,
                           "fullscreen-changed",
                           G_CALLBACK (photos_embed_fullscreen_changed),
                           self,
                           0);

  self->trk_ovrvw_cntrlr = photos_tracker_overview_controller_dup_singleton ();
  g_signal_connect_object (self->trk_ovrvw_cntrlr,
                           "query-status-changed",
                           G_CALLBACK (photos_embed_query_status_changed),
                           self,
                           G_CONNECT_SWAPPED);

  self->item_mngr = g_object_ref (state->item_mngr);
  g_signal_connect_object (self->item_mngr,
                           "active-collection-changed",
                           G_CALLBACK (photos_embed_active_collection_changed),
                           self,
                           0);
  g_signal_connect_object (self->item_mngr,
                           "load-finished",
                           G_CALLBACK (photos_embed_load_finished),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (self->item_mngr,
                           "load-started",
                           G_CALLBACK (photos_embed_load_started),
                           self,
                           G_CONNECT_SWAPPED);

  self->src_mngr = g_object_ref (state->src_mngr);
  g_signal_connect_object (self->src_mngr,
                           "active-changed",
                           G_CALLBACK (photos_embed_search_changed),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (self->src_mngr,
                           "notification-hide",
                           G_CALLBACK (photos_embed_source_manager_notification_hide),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (self->src_mngr,
                           "notification-show",
                           G_CALLBACK (photos_embed_source_manager_notification_show),
                           self,
                           G_CONNECT_SWAPPED);

  self->srch_mngr = g_object_ref (state->srch_typ_mngr);
  g_signal_connect_object (self->srch_mngr,
                           "active-changed",
                           G_CALLBACK (photos_embed_search_changed),
                           self,
                           G_CONNECT_SWAPPED);

  querying = photos_tracker_controller_get_query_status (self->trk_ovrvw_cntrlr);
  photos_embed_query_status_changed (self, querying);

  self->srch_cntrlr = g_object_ref (state->srch_cntrlr);
  g_signal_connect_object (self->srch_cntrlr,
                           "search-string-changed",
                           G_CALLBACK (photos_embed_search_changed),
                           self,
                           G_CONNECT_SWAPPED);
}


static void
photos_embed_class_init (PhotosEmbedClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->dispose = photos_embed_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Photos/embed.ui");
  gtk_widget_class_bind_template_child (widget_class, PhotosEmbed, spinner_box);
  gtk_widget_class_bind_template_child (widget_class, PhotosEmbed, stack);
  gtk_widget_class_bind_template_child (widget_class, PhotosEmbed, stack_overlay);
}


GtkWidget *
photos_embed_new (void)
{
  return g_object_new (PHOTOS_TYPE_EMBED, NULL);
}


PhotosMainToolbar *
photos_embed_get_main_toolbar (PhotosEmbed *self)
{
  return PHOTOS_MAIN_TOOLBAR (self->toolbar);
}
