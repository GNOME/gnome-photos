/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2014 Pranav Kant
 * Copyright © 2012 – 2016 Red Hat, Inc.
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
#include "photos-tracker-controller.h"
#include "photos-tracker-overview-controller.h"
#include "photos-utils.h"
#include "photos-view-container.h"
#include "photos-view-model.h"


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
  GIOExtensionPoint *extension_point;
  GtkWidget *collections;
  GtkWidget *favorites;
  GtkWidget *no_results;
  GtkWidget *ntfctn_mngr;
  GtkWidget *overview;
  GtkWidget *preview;
  GtkWidget *search;
  GtkWidget *selection_toolbar;
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

struct _PhotosEmbedClass
{
  GtkBoxClass parent_class;
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
    }

  return view_container;
}


static void
photos_embed_activate_result (PhotosEmbed *self)
{
  GtkTreeIter iter;
  GtkListStore *store;
  GtkWidget *view_container;
  PhotosWindowMode mode;

  mode = photos_mode_controller_get_window_mode (self->mode_cntrlr);
  view_container = photos_embed_get_view_container_from_mode (self, mode);
  store = photos_view_container_get_model (PHOTOS_VIEW_CONTAINER (view_container));

  if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (store), &iter))
    {
      GObject *item;
      gchar *id;

      gtk_tree_model_get (GTK_TREE_MODEL (store), &iter, PHOTOS_VIEW_MODEL_URN, &id, -1);
      item = photos_base_manager_get_object_by_id (self->item_mngr, id);
      photos_base_manager_set_active_object (self->item_mngr, item);
      g_free (id);
    }
}


static void
photos_embed_restore_search (PhotosEmbed *self)
{
  GVariant *state;

  if (!self->search_state.saved)
    return;

  photos_base_manager_set_active_object (self->src_mngr, self->search_state.source);
  photos_base_manager_set_active_object (self->srch_mngr, self->search_state.search_type);
  photos_search_controller_set_string (self->srch_cntrlr, self->search_state.str);
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

  state = g_variant_new ("b", FALSE);
  g_action_change_state (self->search_action, state);
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

  /* TODO: SearchController,
   *       ErrorHandler
   */

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
    {
      GtkListStore *model;
      GtkTreePath *current_path;
      GtkWidget *view_container;

      view_container = photos_embed_get_view_container_from_mode (self, old_mode);
      current_path = photos_view_container_get_current_path (PHOTOS_VIEW_CONTAINER (view_container));
      model = photos_view_container_get_model (PHOTOS_VIEW_CONTAINER (view_container));
      photos_preview_view_set_model (PHOTOS_PREVIEW_VIEW (self->preview), GTK_TREE_MODEL (model), current_path);
    }

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
    {
      photos_embed_restore_search (self);
    }
  else
    {
      PhotosSearchType *search_type;
      const gchar *str;
      const gchar *id;

      photos_embed_save_search (self);

      search_type = PHOTOS_SEARCH_TYPE (photos_base_manager_get_active_object (self->srch_mngr));
      str = photos_search_controller_get_string (self->srch_cntrlr);
      id = photos_filterable_get_id (PHOTOS_FILTERABLE (search_type));

      if (g_strcmp0 (str, "") != 0 || g_strcmp0 (id, "all") != 0)
        {
          photos_base_manager_set_active_object_by_id (self->srch_mngr, "all");
          photos_search_controller_set_string (self->srch_cntrlr, "");
        }
    }
}


static void
photos_embed_fullscreen_changed (PhotosModeController *mode_cntrlr, gboolean fullscreen, gpointer user_data)
{
}


static void
photos_embed_notify_visible_child (PhotosEmbed *self)
{
  GtkWidget *visible_child;
  GVariant *state;
  PhotosWindowMode mode = PHOTOS_WINDOW_MODE_NONE;

  visible_child = gtk_stack_get_visible_child (GTK_STACK (self->stack));
  if (visible_child == self->overview)
    mode = PHOTOS_WINDOW_MODE_OVERVIEW;
  else if (visible_child == self->collections)
    mode = PHOTOS_WINDOW_MODE_COLLECTIONS;
  else if (visible_child == self->favorites)
    mode = PHOTOS_WINDOW_MODE_FAVORITES;

  if (mode == PHOTOS_WINDOW_MODE_NONE)
    return;

  if (!photos_main_toolbar_is_focus (PHOTOS_MAIN_TOOLBAR (self->toolbar)))
    {
      photos_embed_block_search_changed (self);
      state = g_variant_new ("b", FALSE);
      g_action_change_state (self->search_action, state);
      photos_embed_unblock_search_changed (self);
    }

  photos_mode_controller_set_window_mode (self->mode_cntrlr, mode);
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
photos_embed_row_changed (PhotosEmbed *self)
{
  PhotosWindowMode mode;

  mode = photos_mode_controller_get_window_mode (self->mode_cntrlr);

  if (mode == PHOTOS_WINDOW_MODE_COLLECTIONS
      || mode == PHOTOS_WINDOW_MODE_FAVORITES
      || mode == PHOTOS_WINDOW_MODE_OVERVIEW
      || mode == PHOTOS_WINDOW_MODE_SEARCH)
    {
      GtkListStore *model;
      GtkWidget *view_container;

      view_container = photos_embed_get_view_container_from_mode (self, mode);
      model = photos_view_container_get_model (PHOTOS_VIEW_CONTAINER (view_container));
      photos_main_toolbar_set_view_model (PHOTOS_MAIN_TOOLBAR (self->toolbar), PHOTOS_VIEW_MODEL (model));
    }
}


static void
photos_embed_search_changed (PhotosEmbed *self)
{
  GObject *object;
  PhotosWindowMode mode;
  const gchar *search_type_id;
  const gchar *source_id;
  const gchar *str;

  /* Whenever a search constraint is specified we want to switch to
   * the search mode, and when all constraints have been lifted we
   * want to go back to the previous mode which can be either
   * collections, favorites or overview.
   *
   * However there are some exceptions, which are taken care of
   * elsewhere:
   *  - when moving from search to preview or collection view
   *  - when in preview
   */
  object = photos_base_manager_get_active_object (self->item_mngr);
  mode = photos_mode_controller_get_window_mode (self->mode_cntrlr);
  if (mode == PHOTOS_WINDOW_MODE_SEARCH && object != NULL)
    return;
  if (mode == PHOTOS_WINDOW_MODE_PREVIEW)
    return;

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
photos_embed_window_mode_changed (PhotosModeController *mode_cntrlr,
                                  PhotosWindowMode mode,
                                  PhotosWindowMode old_mode,
                                  gpointer user_data)
{
  PhotosEmbed *self = PHOTOS_EMBED (user_data);
  GtkListStore *model;
  GtkWidget *view_container;

  photos_main_toolbar_reset_toolbar_mode (PHOTOS_MAIN_TOOLBAR (self->toolbar));

  switch (mode)
    {
    case PHOTOS_WINDOW_MODE_EDIT:
      break;

    case PHOTOS_WINDOW_MODE_COLLECTIONS:
      photos_embed_prepare_for_collections (self, old_mode);
      goto set_toolbar_model;

    case PHOTOS_WINDOW_MODE_FAVORITES:
      photos_embed_prepare_for_favorites (self, old_mode);
      goto set_toolbar_model;

    case PHOTOS_WINDOW_MODE_OVERVIEW:
      photos_embed_prepare_for_overview (self, old_mode);
      goto set_toolbar_model;

    case PHOTOS_WINDOW_MODE_PREVIEW:
      photos_embed_prepare_for_preview (self, old_mode);
      break;

    case PHOTOS_WINDOW_MODE_SEARCH:
      photos_embed_prepare_for_search (self, old_mode);
      goto set_toolbar_model;

    case PHOTOS_WINDOW_MODE_NONE:
    default:
      g_assert_not_reached ();
      break;
    }

  return;

 set_toolbar_model:
  view_container = photos_embed_get_view_container_from_mode (self, mode);
  model = photos_view_container_get_model (PHOTOS_VIEW_CONTAINER (view_container));
  photos_main_toolbar_set_view_model (PHOTOS_MAIN_TOOLBAR (self->toolbar), PHOTOS_VIEW_MODEL (model));
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

  /* GtkStack triggers notify::visible-child during dispose and this means that
   * we have to explicitly disconnect the signal handler before calling up to
   * the parent implementation, or photos_embed_notify_visible_child() will
   * get called while we're in a inconsistent state
   *
   * See https://bugzilla.gnome.org/show_bug.cgi?id=749012
   */
  if (self->stack != NULL)
    {
      g_signal_handlers_disconnect_by_func (self->stack, photos_embed_notify_visible_child, self);
      self->stack = NULL;
    }

  G_OBJECT_CLASS (photos_embed_parent_class)->dispose (object);
}


static void
photos_embed_init (PhotosEmbed *self)
{
  GApplication *app;
  GList *windows;
  GtkListStore *model;
  PhotosSearchbar *searchbar;
  PhotosSearchContextState *state;
  gboolean querying;
  const gchar *name;

  app = g_application_get_default ();
  state = photos_search_context_get_state (PHOTOS_SEARCH_CONTEXT (app));

  self->search_action = g_action_map_lookup_action (G_ACTION_MAP (app), "search");

  gtk_orientable_set_orientation (GTK_ORIENTABLE (self), GTK_ORIENTATION_VERTICAL);
  gtk_widget_show (GTK_WIDGET (self));

  self->extension_point = g_io_extension_point_lookup (PHOTOS_TRACKER_CONTROLLER_EXTENSION_POINT_NAME);

  self->selection_toolbar = photos_selection_toolbar_new ();
  gtk_box_pack_end (GTK_BOX (self), self->selection_toolbar, FALSE, FALSE, 0);

  self->stack_overlay = gtk_overlay_new ();
  gtk_widget_show (self->stack_overlay);
  gtk_box_pack_end (GTK_BOX (self), self->stack_overlay, TRUE, TRUE, 0);

  self->stack = gtk_stack_new ();
  gtk_stack_set_homogeneous (GTK_STACK (self->stack), TRUE);
  gtk_stack_set_transition_type (GTK_STACK (self->stack), GTK_STACK_TRANSITION_TYPE_CROSSFADE);
  gtk_widget_show (self->stack);
  gtk_container_add (GTK_CONTAINER (self->stack_overlay), self->stack);

  self->toolbar = photos_main_toolbar_new (GTK_OVERLAY (self->stack_overlay));
  photos_main_toolbar_set_stack (PHOTOS_MAIN_TOOLBAR (self->toolbar), GTK_STACK (self->stack));
  windows = gtk_application_get_windows (GTK_APPLICATION (app));
  gtk_window_set_titlebar (GTK_WINDOW (windows->data), self->toolbar);
  searchbar = photos_main_toolbar_get_searchbar (PHOTOS_MAIN_TOOLBAR (self->toolbar));
  g_signal_connect_swapped (searchbar, "activate-result", G_CALLBACK (photos_embed_activate_result), self);

  self->ntfctn_mngr = g_object_ref_sink (photos_notification_manager_dup_singleton ());
  gtk_overlay_add_overlay (GTK_OVERLAY (self->stack_overlay), self->ntfctn_mngr);

  self->overview = photos_view_container_new (PHOTOS_WINDOW_MODE_OVERVIEW, _("Photos"));
  name = photos_view_container_get_name (PHOTOS_VIEW_CONTAINER (self->overview));
  gtk_stack_add_titled (GTK_STACK (self->stack), self->overview, "overview", name);
  model = photos_view_container_get_model (PHOTOS_VIEW_CONTAINER (self->overview));
  g_signal_connect_object (model, "row-inserted", G_CALLBACK (photos_embed_row_changed), self, G_CONNECT_SWAPPED);
  g_signal_connect_object (model, "row-deleted", G_CALLBACK (photos_embed_row_changed), self, G_CONNECT_SWAPPED);

  self->collections = photos_view_container_new (PHOTOS_WINDOW_MODE_COLLECTIONS, _("Albums"));
  name = photos_view_container_get_name (PHOTOS_VIEW_CONTAINER (self->collections));
  gtk_stack_add_titled (GTK_STACK (self->stack), self->collections, "collections", name);
  model = photos_view_container_get_model (PHOTOS_VIEW_CONTAINER (self->collections));
  g_signal_connect_object (model, "row-inserted", G_CALLBACK (photos_embed_row_changed), self, G_CONNECT_SWAPPED);
  g_signal_connect_object (model, "row-deleted", G_CALLBACK (photos_embed_row_changed), self, G_CONNECT_SWAPPED);

  self->favorites = photos_view_container_new (PHOTOS_WINDOW_MODE_FAVORITES, _("Favorites"));
  name = photos_view_container_get_name (PHOTOS_VIEW_CONTAINER (self->favorites));
  gtk_stack_add_titled (GTK_STACK (self->stack), self->favorites, "favorites", name);
  model = photos_view_container_get_model (PHOTOS_VIEW_CONTAINER (self->favorites));
  g_signal_connect_object (model, "row-inserted", G_CALLBACK (photos_embed_row_changed), self, G_CONNECT_SWAPPED);
  g_signal_connect_object (model, "row-deleted", G_CALLBACK (photos_embed_row_changed), self, G_CONNECT_SWAPPED);

  self->search = photos_view_container_new (PHOTOS_WINDOW_MODE_SEARCH, _("Search"));
  gtk_stack_add_named (GTK_STACK (self->stack), self->search, "search");
  model = photos_view_container_get_model (PHOTOS_VIEW_CONTAINER (self->search));
  g_signal_connect_object (model, "row-inserted", G_CALLBACK (photos_embed_row_changed), self, G_CONNECT_SWAPPED);
  g_signal_connect_object (model, "row-deleted", G_CALLBACK (photos_embed_row_changed), self, G_CONNECT_SWAPPED);

  self->preview = photos_preview_view_new (GTK_OVERLAY (self->stack_overlay));
  gtk_stack_add_named (GTK_STACK (self->stack), self->preview, "preview");

  self->spinner_box = photos_spinner_box_new ();
  gtk_overlay_add_overlay (GTK_OVERLAY (self->stack_overlay), self->spinner_box);

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

  gtk_widget_show (GTK_WIDGET (self));
}


static void
photos_embed_class_init (PhotosEmbedClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->dispose = photos_embed_dispose;
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
