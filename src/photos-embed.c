/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2014 Pranav Kant
 * Copyright © 2012 – 2017 Red Hat, Inc.
 * Copyright © 2014 Saurav Agarwalla
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
#include "photos-search-match.h"
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
  GObject *search_match;
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
  GtkWidget *collection_view;
  GtkWidget *collections;
  GtkWidget *favorites;
  GtkWidget *import;
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
  PhotosBaseManager *srch_mtch_mngr;
  PhotosBaseManager *srch_typ_mngr;
  PhotosEmbedSearchState search_state;
  PhotosModeController *mode_cntrlr;
  PhotosSearchController *srch_cntrlr;
  PhotosTrackerController *trk_ovrvw_cntrlr;
  gboolean search_changed;
  guint load_show_id;
};


G_DEFINE_TYPE (PhotosEmbed, photos_embed, GTK_TYPE_BOX);


static void photos_embed_search_changed (PhotosEmbed *self);


static void
photos_embed_block_search_changed (PhotosEmbed *self)
{
  g_signal_handlers_block_by_func (self->src_mngr, photos_embed_search_changed, self);
  g_signal_handlers_block_by_func (self->srch_cntrlr, photos_embed_search_changed, self);
  g_signal_handlers_block_by_func (self->srch_typ_mngr, photos_embed_search_changed, self);
}


static void
photos_embed_unblock_search_changed (PhotosEmbed *self)
{
  g_signal_handlers_unblock_by_func (self->src_mngr, photos_embed_search_changed, self);
  g_signal_handlers_unblock_by_func (self->srch_cntrlr, photos_embed_search_changed, self);
  g_signal_handlers_unblock_by_func (self->srch_typ_mngr, photos_embed_search_changed, self);
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
    case PHOTOS_WINDOW_MODE_COLLECTION_VIEW:
      view_container = self->collection_view;
      break;

    case PHOTOS_WINDOW_MODE_COLLECTIONS:
      view_container = self->collections;
      break;

    case PHOTOS_WINDOW_MODE_FAVORITES:
      view_container = self->favorites;
      break;

    case PHOTOS_WINDOW_MODE_IMPORT:
      view_container = self->import;
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


static gboolean
photos_embed_is_search_constraint_present (PhotosEmbed *self)
{
  GObject *object;
  const gchar *search_type_id;
  const gchar *source_id;
  const gchar *str;

  object = photos_base_manager_get_active_object (self->src_mngr);
  source_id = photos_filterable_get_id (PHOTOS_FILTERABLE (object));

  object = photos_base_manager_get_active_object (self->srch_typ_mngr);
  search_type_id = photos_filterable_get_id (PHOTOS_FILTERABLE (object));

  str = photos_search_controller_get_string (self->srch_cntrlr);

  return g_strcmp0 (search_type_id, PHOTOS_SEARCH_TYPE_STOCK_ALL) != 0
    || g_strcmp0 (source_id, PHOTOS_SOURCE_STOCK_ALL) != 0
    || (str != NULL && str [0] != '\0');
}


static void
photos_embed_restore_search (PhotosEmbed *self)
{
  GVariant *state;
  PhotosSource *source;
  const gchar *id;

  if (!self->search_state.saved)
    return;

  photos_embed_block_search_changed (self);

  id = photos_filterable_get_id (PHOTOS_FILTERABLE (self->search_state.source));
  source = PHOTOS_SOURCE (photos_base_manager_get_object_by_id (self->src_mngr, id));
  if (source == NULL)
    id = PHOTOS_SOURCE_STOCK_ALL;

  photos_base_manager_set_active_object_by_id (self->src_mngr, id);

  photos_base_manager_set_active_object (self->srch_mtch_mngr, self->search_state.search_match);
  photos_base_manager_set_active_object (self->srch_typ_mngr, self->search_state.search_type);
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
  self->search_state.search_match = g_object_ref (photos_base_manager_get_active_object (self->srch_mtch_mngr));
  self->search_state.search_type = g_object_ref (photos_base_manager_get_active_object (self->srch_typ_mngr));
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
  PhotosBaseItem *item;

  /* TODO: SearchController,
   *       ErrorHandler
   */

  item = PHOTOS_BASE_ITEM (photos_base_manager_get_active_object (self->item_mngr));
  g_return_if_fail (PHOTOS_IS_BASE_ITEM (item));

  /* We want to freeze before saving the search state and to thaw
   * after restoring it. We could thaw it earlier too, but that would
   * lead to a bunch of needless queries from the TrackerControllers.
   */
  photos_embed_tracker_controllers_set_frozen (self, TRUE);

  if (old_mode == PHOTOS_WINDOW_MODE_SEARCH)
    {
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
  g_clear_pointer (&event, gdk_event_free);
}


static void
photos_embed_prepare_for_collection_view (PhotosEmbed *self, PhotosWindowMode old_mode)
{
  switch (old_mode)
    {
    case PHOTOS_WINDOW_MODE_COLLECTIONS:
      {
        GVariant *state;

        /* Hide any empty search bars that might have been floating
         * around.
         */
        state = g_variant_new ("b", FALSE);
        g_action_change_state (self->search_action, state);
        break;
      }

    case PHOTOS_WINDOW_MODE_PREVIEW:
      photos_embed_tracker_controllers_set_frozen (self, FALSE);
      break;

    case PHOTOS_WINDOW_MODE_SEARCH:
      photos_embed_save_search (self);
      break;

    case PHOTOS_WINDOW_MODE_NONE:
      break;

    case PHOTOS_WINDOW_MODE_COLLECTION_VIEW:
    case PHOTOS_WINDOW_MODE_EDIT:
    case PHOTOS_WINDOW_MODE_FAVORITES:
    case PHOTOS_WINDOW_MODE_IMPORT:
    case PHOTOS_WINDOW_MODE_OVERVIEW:
    default:
      g_assert_not_reached ();
      break;
    }

  photos_spinner_box_stop (PHOTOS_SPINNER_BOX (self->spinner_box));
  gtk_stack_set_visible_child_name (GTK_STACK (self->stack), "collection-view");
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
photos_embed_prepare_for_import (PhotosEmbed *self, PhotosWindowMode old_mode)
{
  if (old_mode == PHOTOS_WINDOW_MODE_PREVIEW)
    photos_embed_tracker_controllers_set_frozen (self, FALSE);

  photos_spinner_box_stop (PHOTOS_SPINNER_BOX (self->spinner_box));
  gtk_stack_set_visible_child_name (GTK_STACK (self->stack), "import");
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
  photos_embed_restore_search (self);

  if (old_mode == PHOTOS_WINDOW_MODE_PREVIEW)
    photos_embed_tracker_controllers_set_frozen (self, FALSE);

  if (photos_embed_is_search_constraint_present (self))
    {
      photos_spinner_box_stop (PHOTOS_SPINNER_BOX (self->spinner_box));
      gtk_stack_set_visible_child_name (GTK_STACK (self->stack), "search");
    }
  else
    {
      photos_mode_controller_go_back (self->mode_cntrlr);
    }
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
  GMount *mount;
  PhotosSource *source;

  source = PHOTOS_SOURCE (photos_base_manager_get_active_object (self->src_mngr));

  mount = photos_source_get_mount (source);
  if (mount != NULL)
    {
      GObject *object;
      PhotosWindowMode mode;
      const gchar *search_match_id;
      const gchar *search_type_id;

      mode = photos_mode_controller_get_window_mode (self->mode_cntrlr);
      g_return_if_fail (mode != PHOTOS_WINDOW_MODE_NONE);
      g_return_if_fail (mode != PHOTOS_WINDOW_MODE_EDIT);
      g_return_if_fail (mode != PHOTOS_WINDOW_MODE_IMPORT);
      g_return_if_fail (mode != PHOTOS_WINDOW_MODE_PREVIEW);

      photos_embed_block_search_changed (self);

      if (mode == PHOTOS_WINDOW_MODE_COLLECTION_VIEW)
        photos_mode_controller_go_back (self->mode_cntrlr);

      mode = photos_mode_controller_get_window_mode (self->mode_cntrlr);
      if (mode == PHOTOS_WINDOW_MODE_SEARCH)
        photos_mode_controller_go_back (self->mode_cntrlr);

      photos_base_manager_set_active_object (self->src_mngr, G_OBJECT (source));

      object = photos_base_manager_get_active_object (self->srch_mtch_mngr);
      search_match_id = photos_filterable_get_id (PHOTOS_FILTERABLE (object));
      g_assert_cmpstr (search_match_id, ==, PHOTOS_SEARCH_MATCH_STOCK_ALL);

      object = photos_base_manager_get_active_object (self->srch_typ_mngr);
      search_type_id = photos_filterable_get_id (PHOTOS_FILTERABLE (object));
      g_assert_cmpstr (search_type_id, ==, PHOTOS_SEARCH_TYPE_STOCK_ALL);

      photos_mode_controller_set_window_mode (self->mode_cntrlr, PHOTOS_WINDOW_MODE_IMPORT);

      photos_embed_unblock_search_changed (self);
    }
  else
    {
      /* Whenever a search constraint is specified, we switch to the
       * search mode. Search is always global. If we are in
       * collection-view, we go back to the previous top-level mode and
       * then enter the search mode.
       *
       * When all constraints have been lifted we want to go back to the
       * previous top-level mode which can be either collections,
       * favorites or overview.
       *
       * The constraints are saved when entering collection-view or
       * preview from the search mode. They are restored when going back.
       * Saving and restoring doesn't cause any further mode changes.
       */

      self->search_changed = TRUE;

      if (photos_embed_is_search_constraint_present (self))
        {
          PhotosWindowMode mode;

          mode = photos_mode_controller_get_window_mode (self->mode_cntrlr);
          if (mode == PHOTOS_WINDOW_MODE_COLLECTION_VIEW)
            photos_mode_controller_go_back (self->mode_cntrlr);

          photos_mode_controller_set_window_mode (self->mode_cntrlr, PHOTOS_WINDOW_MODE_SEARCH);
        }
      else
        {
          photos_mode_controller_go_back (self->mode_cntrlr);
        }

      self->search_changed = FALSE;
    }
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
      if (!self->search_changed)
        {
          GVariant *state;

          photos_embed_block_search_changed (self);
          state = g_variant_new ("b", FALSE);
          g_action_change_state (self->search_action, state);
          photos_embed_unblock_search_changed (self);
        }
      break;

    case PHOTOS_WINDOW_MODE_COLLECTION_VIEW:
    case PHOTOS_WINDOW_MODE_EDIT:
    case PHOTOS_WINDOW_MODE_IMPORT:
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

    case PHOTOS_WINDOW_MODE_COLLECTION_VIEW:
      photos_embed_prepare_for_collection_view (self, old_mode);
      break;

    case PHOTOS_WINDOW_MODE_COLLECTIONS:
      photos_embed_prepare_for_collections (self, old_mode);
      break;

    case PHOTOS_WINDOW_MODE_FAVORITES:
      photos_embed_prepare_for_favorites (self, old_mode);
      break;

    case PHOTOS_WINDOW_MODE_IMPORT:
      photos_embed_prepare_for_import (self, old_mode);
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
  g_clear_object (&self->srch_mtch_mngr);
  g_clear_object (&self->srch_typ_mngr);
  g_clear_object (&self->mode_cntrlr);
  g_clear_object (&self->srch_cntrlr);
  g_clear_object (&self->trk_ovrvw_cntrlr);
  g_clear_pointer (&self->notifications, g_hash_table_unref);

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

  self->toolbar = photos_main_toolbar_new ();
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

  self->collection_view = photos_view_container_new (PHOTOS_WINDOW_MODE_COLLECTION_VIEW, _("Collection View"));
  gtk_stack_add_named (GTK_STACK (self->stack), self->collection_view, "collection-view");

  self->collections = photos_view_container_new (PHOTOS_WINDOW_MODE_COLLECTIONS, _("Albums"));
  name = photos_view_container_get_name (PHOTOS_VIEW_CONTAINER (self->collections));
  gtk_stack_add_titled (GTK_STACK (self->stack), self->collections, "collections", name);

  self->favorites = photos_view_container_new (PHOTOS_WINDOW_MODE_FAVORITES, _("Favorites"));
  name = photos_view_container_get_name (PHOTOS_VIEW_CONTAINER (self->favorites));
  gtk_stack_add_titled (GTK_STACK (self->stack), self->favorites, "favorites", name);

  self->import = photos_view_container_new (PHOTOS_WINDOW_MODE_IMPORT, _("Import"));
  gtk_stack_add_named (GTK_STACK (self->stack), self->import, "import");

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

  self->srch_mtch_mngr = g_object_ref (state->srch_mtch_mngr);

  self->srch_typ_mngr = g_object_ref (state->srch_typ_mngr);
  g_signal_connect_object (self->srch_typ_mngr,
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
