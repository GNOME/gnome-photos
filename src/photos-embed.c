/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2014 Pranav Kant
 * Copyright © 2012, 2013, 2014 Red Hat, Inc.
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

#include "photos-collection-manager.h"
#include "photos-embed.h"
#include "photos-filterable.h"
#include "photos-item-manager.h"
#include "photos-mode-controller.h"
#include "photos-notification-manager.h"
#include "photos-search-controller.h"
#include "photos-selection-toolbar.h"
#include "photos-spinner-box.h"
#include "photos-search-context.h"
#include "photos-search-type.h"
#include "photos-search-type-manager.h"
#include "photos-source.h"
#include "photos-source-manager.h"
#include "photos-tracker-overview-controller.h"
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

struct _PhotosEmbedPrivate
{
  GAction *search_action;
  GCancellable *loader_cancellable;
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
  PhotosBaseManager *col_mngr;
  PhotosBaseManager *item_mngr;
  PhotosBaseManager *src_mngr;
  PhotosBaseManager *srch_mngr;
  PhotosEmbedSearchState search_state;
  PhotosModeController *mode_cntrlr;
  PhotosSearchController *srch_cntrlr;
  PhotosTrackerController *trk_ovrvw_cntrlr;
  guint load_show_id;
};


G_DEFINE_TYPE_WITH_PRIVATE (PhotosEmbed, photos_embed, GTK_TYPE_BOX);


static void photos_embed_search_changed (PhotosEmbed *self);


static void
photos_embed_block_search_changed (PhotosEmbed *self)
{
  PhotosEmbedPrivate *priv = self->priv;

  g_signal_handlers_block_by_func (priv->src_mngr, photos_embed_search_changed, self);
  g_signal_handlers_block_by_func (priv->srch_mngr, photos_embed_search_changed, self);
  g_signal_handlers_block_by_func (priv->srch_cntrlr, photos_embed_search_changed, self);
}


static void
photos_embed_unblock_search_changed (PhotosEmbed *self)
{
  PhotosEmbedPrivate *priv = self->priv;

  g_signal_handlers_unblock_by_func (priv->src_mngr, photos_embed_search_changed, self);
  g_signal_handlers_unblock_by_func (priv->srch_mngr, photos_embed_search_changed, self);
  g_signal_handlers_unblock_by_func (priv->srch_cntrlr, photos_embed_search_changed, self);
}


static void
photos_embed_clear_load_timer (PhotosEmbed *self)
{
  PhotosEmbedPrivate *priv = self->priv;

  if (priv->load_show_id != 0)
    {
      g_source_remove (priv->load_show_id);
      priv->load_show_id = 0;
    }
}


static void
photos_embed_clear_search (PhotosEmbed *self)
{
  PhotosEmbedPrivate *priv = self->priv;

  g_clear_object (&priv->search_state.search_type);
  g_clear_object (&priv->search_state.source);
  g_clear_pointer (&priv->search_state.str, g_free);
}


static GtkWidget*
photos_embed_get_view_container_from_mode (PhotosEmbed *self, PhotosWindowMode mode)
{
  PhotosEmbedPrivate *priv = self->priv;
  GtkWidget *view_container;

  switch (mode)
    {
    case PHOTOS_WINDOW_MODE_COLLECTIONS:
      view_container = priv->collections;
      break;

    case PHOTOS_WINDOW_MODE_FAVORITES:
      view_container = priv->favorites;
      break;

    case PHOTOS_WINDOW_MODE_OVERVIEW:
      view_container = priv->overview;
      break;


    case PHOTOS_WINDOW_MODE_SEARCH:
      view_container = priv->search;
      break;

    default:
      g_assert_not_reached ();
    }

  return view_container;
}


static void
photos_embed_activate_result (PhotosEmbed *self)
{
  PhotosEmbedPrivate *priv = self->priv;
  GtkTreeIter iter;
  GtkListStore *store;
  GtkWidget *view_container;
  PhotosWindowMode mode;

  mode = photos_mode_controller_get_window_mode (priv->mode_cntrlr);
  view_container = photos_embed_get_view_container_from_mode (self, mode);
  store = photos_view_container_get_model (PHOTOS_VIEW_CONTAINER (view_container));

  if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (store), &iter))
    {
      GObject *item;
      gchar *id;

      gtk_tree_model_get (GTK_TREE_MODEL (store), &iter, PHOTOS_VIEW_MODEL_URN, &id, -1);
      item = photos_base_manager_get_object_by_id (priv->item_mngr, id);
      photos_base_manager_set_active_object (priv->item_mngr, item);
      g_free (id);
    }
}


static void
photos_embed_prepare_for_preview (PhotosEmbed *self)
{
  PhotosEmbedPrivate *priv = self->priv;

  /* TODO: SearchController,
   *       ErrorHandler
   */

  photos_spinner_box_stop (PHOTOS_SPINNER_BOX (priv->spinner_box));
  gtk_stack_set_visible_child_name (GTK_STACK (priv->stack), "preview");
}


static void
photos_embed_item_load (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosEmbed *self = PHOTOS_EMBED (user_data);
  PhotosEmbedPrivate *priv = self->priv;
  GError *error;
  GeglNode *node;
  GtkListStore *model;
  GtkTreePath *current_path;
  PhotosBaseItem *item = PHOTOS_BASE_ITEM (source_object);
  PhotosWindowMode mode;

  photos_embed_clear_load_timer (self);
  g_clear_object (&priv->loader_cancellable);

  error = NULL;
  node = photos_base_item_load_finish (item, res, &error);
  if (error != NULL)
    {
      g_warning ("Unable to load the item: %s", error->message);
      g_error_free (error);
      goto out;
    }

  mode = photos_mode_controller_get_window_mode (priv->mode_cntrlr);

  /* This is not needed when:
   *  - activated from the search provider
   *  - already in the preview and navigating using the buttons
   */
  if (mode == PHOTOS_WINDOW_MODE_COLLECTIONS
      || mode == PHOTOS_WINDOW_MODE_FAVORITES
      || mode == PHOTOS_WINDOW_MODE_OVERVIEW
      || mode == PHOTOS_WINDOW_MODE_SEARCH)
    {
      GtkWidget *view_container;

      view_container = photos_embed_get_view_container_from_mode (self, mode);
      current_path = photos_view_container_get_current_path (PHOTOS_VIEW_CONTAINER (view_container));
      model = photos_view_container_get_model (PHOTOS_VIEW_CONTAINER (view_container));
      photos_preview_view_set_model (PHOTOS_PREVIEW_VIEW (priv->preview), GTK_TREE_MODEL (model), current_path);
    }

  photos_preview_view_set_node (PHOTOS_PREVIEW_VIEW (priv->preview), node);

  /* TODO: set toolbar model */

  /* If we are already in the preview and navigating using the
   * buttons, then the window-mode-changed signal won't be fired. So
   * we need to prepare it ourselves.
   */
  if (mode != PHOTOS_WINDOW_MODE_PREVIEW)
    photos_mode_controller_set_window_mode (priv->mode_cntrlr, PHOTOS_WINDOW_MODE_PREVIEW);
  else
    photos_embed_prepare_for_preview (self);

  photos_mode_controller_set_can_fullscreen (priv->mode_cntrlr, TRUE);

 out:
  g_clear_object (&node);
  g_object_unref (self);
}


static gboolean
photos_embed_load_show_timeout (gpointer user_data)
{
  PhotosEmbed *self = PHOTOS_EMBED (user_data);
  PhotosEmbedPrivate *priv = self->priv;

  priv->load_show_id = 0;
  gtk_stack_set_visible_child_name (GTK_STACK (priv->stack), "spinner");
  photos_spinner_box_start (PHOTOS_SPINNER_BOX (priv->spinner_box));
  g_object_unref (self);
  return G_SOURCE_REMOVE;
}


static void
photos_embed_restore_search (PhotosEmbed *self)
{
  PhotosEmbedPrivate *priv = self->priv;

  if (!priv->search_state.saved)
    return;

  photos_base_manager_set_active_object (priv->src_mngr, priv->search_state.source);
  photos_base_manager_set_active_object (priv->srch_mngr, priv->search_state.search_type);
  photos_search_controller_set_string (priv->srch_cntrlr, priv->search_state.str);
  priv->search_state.saved = FALSE;

  photos_embed_clear_search (self);
}


static void
photos_embed_save_search (PhotosEmbed *self)
{
  PhotosEmbedPrivate *priv = self->priv;

  if (priv->search_state.saved)
    return;

  photos_embed_clear_search (self);

  priv->search_state.source = g_object_ref (photos_base_manager_get_active_object (priv->src_mngr));
  priv->search_state.search_type = g_object_ref (photos_base_manager_get_active_object (priv->srch_mngr));
  priv->search_state.str = g_strdup (photos_search_controller_get_string (priv->srch_cntrlr));
  priv->search_state.saved = TRUE;
}


static void
photos_embed_active_changed (PhotosBaseManager *manager, GObject *object, gpointer user_data)
{
  PhotosEmbed *self = PHOTOS_EMBED (user_data);
  PhotosEmbedPrivate *priv = self->priv;
  PhotosWindowMode mode;
  GObject *active_collection;
  GObject *active_item;
  GVariant *state;
  gboolean show_search;

  /* Hide the search bar when we are moving from the search to the
   * preview or collection viewin. Restore it when we are back.
   */

  active_collection = photos_base_manager_get_active_object (priv->col_mngr);
  active_item = photos_base_manager_get_active_object (priv->item_mngr);
  mode = photos_mode_controller_get_window_mode (priv->mode_cntrlr);
  show_search = (mode == PHOTOS_WINDOW_MODE_PREVIEW && active_item == NULL && active_collection == NULL)
                || (mode == PHOTOS_WINDOW_MODE_SEARCH && active_item == NULL);

  if (show_search)
    photos_embed_restore_search (self);
  else
    photos_embed_save_search (self);

  state = g_variant_new ("b", show_search);
  g_action_change_state (priv->search_action, state);

  if (object == NULL)
    return;

  photos_embed_clear_load_timer (self);

  if (photos_base_item_is_collection (PHOTOS_BASE_ITEM (object)))
    return;

  priv->load_show_id = g_timeout_add (400, photos_embed_load_show_timeout, g_object_ref (self));

  priv->loader_cancellable = g_cancellable_new ();
  photos_base_item_load_async (PHOTOS_BASE_ITEM (object),
                               priv->loader_cancellable,
                               photos_embed_item_load,
                               g_object_ref (self));
}


static void
photos_embed_restore_last_page (PhotosEmbed *self)
{
  PhotosEmbedPrivate *priv = self->priv;
  PhotosWindowMode mode;
  const gchar *page;

  mode = photos_mode_controller_get_window_mode (priv->mode_cntrlr);
  switch (mode)
    {
    case PHOTOS_WINDOW_MODE_COLLECTIONS:
      page = "collections";
      break;

    case PHOTOS_WINDOW_MODE_FAVORITES:
      page = "favorites";
      break;

    case PHOTOS_WINDOW_MODE_OVERVIEW:
      page = "overview";
      break;

    case PHOTOS_WINDOW_MODE_PREVIEW:
      page = "preview";
      break;

    case PHOTOS_WINDOW_MODE_SEARCH:
      page = "search";
      break;

    default:
      g_assert_not_reached ();
      break;
    }

  gtk_stack_set_visible_child_name (GTK_STACK (priv->stack), page);
}


static void
photos_embed_fullscreen_changed (PhotosModeController *mode_cntrlr, gboolean fullscreen, gpointer user_data)
{
}


static void
photos_embed_notify_visible_child (PhotosEmbed *self)
{
  PhotosEmbedPrivate *priv = self->priv;
  GtkWidget *visible_child;
  GVariant *state;
  PhotosWindowMode mode = PHOTOS_WINDOW_MODE_NONE;

  visible_child = gtk_stack_get_visible_child (GTK_STACK (priv->stack));
  if (visible_child == priv->overview)
    mode = PHOTOS_WINDOW_MODE_OVERVIEW;
  else if (visible_child == priv->collections)
    mode = PHOTOS_WINDOW_MODE_COLLECTIONS;
  else if (visible_child == priv->favorites)
    mode = PHOTOS_WINDOW_MODE_FAVORITES;

  if (mode == PHOTOS_WINDOW_MODE_NONE)
    return;

  if (!photos_main_toolbar_is_focus (PHOTOS_MAIN_TOOLBAR (priv->toolbar)))
    {
      photos_embed_block_search_changed (self);
      state = g_variant_new ("b", FALSE);
      g_action_change_state (priv->search_action, state);
      photos_embed_unblock_search_changed (self);
    }

  photos_mode_controller_set_window_mode (priv->mode_cntrlr, mode);
}


static void
photos_embed_prepare_for_collections (PhotosEmbed *self)
{
  PhotosEmbedPrivate *priv = self->priv;

  photos_base_manager_set_active_object (priv->item_mngr, NULL);

  if (priv->loader_cancellable != NULL)
    {
      g_cancellable_cancel (priv->loader_cancellable);
      g_clear_object (&priv->loader_cancellable);
    }

  photos_spinner_box_stop (PHOTOS_SPINNER_BOX (priv->spinner_box));
  gtk_stack_set_visible_child_name (GTK_STACK (priv->stack), "collections");
}


static void
photos_embed_prepare_for_favorites (PhotosEmbed *self)
{
  PhotosEmbedPrivate *priv = self->priv;

  photos_base_manager_set_active_object (priv->item_mngr, NULL);

  if (priv->loader_cancellable != NULL)
    {
      g_cancellable_cancel (priv->loader_cancellable);
      g_clear_object (&priv->loader_cancellable);
    }

  photos_spinner_box_stop (PHOTOS_SPINNER_BOX (priv->spinner_box));
  gtk_stack_set_visible_child_name (GTK_STACK (priv->stack), "favorites");
}


static void
photos_embed_prepare_for_overview (PhotosEmbed *self)
{
  PhotosEmbedPrivate *priv = self->priv;

  photos_base_manager_set_active_object (priv->item_mngr, NULL);

  if (priv->loader_cancellable != NULL)
    {
      g_cancellable_cancel (priv->loader_cancellable);
      g_clear_object (&priv->loader_cancellable);
    }

  photos_spinner_box_stop (PHOTOS_SPINNER_BOX (priv->spinner_box));
  gtk_stack_set_visible_child_name (GTK_STACK (priv->stack), "overview");
}


static void
photos_embed_prepare_for_search (PhotosEmbed *self)
{
  PhotosEmbedPrivate *priv = self->priv;

  photos_base_manager_set_active_object (priv->item_mngr, NULL);

  if (priv->loader_cancellable != NULL)
    {
      g_cancellable_cancel (priv->loader_cancellable);
      g_clear_object (&priv->loader_cancellable);
    }

  photos_spinner_box_stop (PHOTOS_SPINNER_BOX (priv->spinner_box));
  gtk_stack_set_visible_child_name (GTK_STACK (priv->stack), "search");
}


static void
photos_embed_query_status_changed (PhotosEmbed *self, gboolean querying)
{
  PhotosEmbedPrivate *priv = self->priv;

  if (querying)
    {
      photos_spinner_box_start (PHOTOS_SPINNER_BOX (priv->spinner_box));
      gtk_stack_set_visible_child_name (GTK_STACK (priv->stack), "spinner");
    }
  else
    {
      photos_spinner_box_stop (PHOTOS_SPINNER_BOX (priv->spinner_box));
      photos_embed_restore_last_page (self);
    }
}


static void
photos_embed_row_changed (PhotosEmbed *self)
{
  PhotosEmbedPrivate *priv = self->priv;
  PhotosWindowMode mode;

  mode = photos_mode_controller_get_window_mode (priv->mode_cntrlr);

  if (mode == PHOTOS_WINDOW_MODE_COLLECTIONS
      || mode == PHOTOS_WINDOW_MODE_FAVORITES
      || mode == PHOTOS_WINDOW_MODE_OVERVIEW
      || mode == PHOTOS_WINDOW_MODE_SEARCH)
    {
      GtkListStore *model;
      GtkWidget *view_container;

      view_container = photos_embed_get_view_container_from_mode (self, mode);
      model = photos_view_container_get_model (PHOTOS_VIEW_CONTAINER (view_container));
      photos_main_toolbar_set_view_model (PHOTOS_MAIN_TOOLBAR (priv->toolbar), PHOTOS_VIEW_MODEL (model));
    }
}


static void
photos_embed_search_changed (PhotosEmbed *self)
{
  PhotosEmbedPrivate *priv = self->priv;
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
  object = photos_base_manager_get_active_object (priv->item_mngr);
  mode = photos_mode_controller_get_window_mode (priv->mode_cntrlr);
  if (mode == PHOTOS_WINDOW_MODE_SEARCH && object != NULL)
    return;
  if (mode == PHOTOS_WINDOW_MODE_PREVIEW)
    return;

  object = photos_base_manager_get_active_object (priv->src_mngr);
  source_id = photos_filterable_get_id (PHOTOS_FILTERABLE (object));

  object = photos_base_manager_get_active_object (priv->srch_mngr);
  search_type_id = photos_filterable_get_id (PHOTOS_FILTERABLE (object));

  str = photos_search_controller_get_string (priv->srch_cntrlr);

  if (g_strcmp0 (search_type_id, PHOTOS_SEARCH_TYPE_STOCK_ALL) == 0
      && g_strcmp0 (source_id, PHOTOS_SOURCE_STOCK_ALL) == 0
      && (str == NULL || str [0] == '\0'))
    photos_mode_controller_go_back (priv->mode_cntrlr);
  else
    photos_mode_controller_set_window_mode (priv->mode_cntrlr, PHOTOS_WINDOW_MODE_SEARCH);
}


static void
photos_embed_window_mode_changed (PhotosModeController *mode_cntrlr,
                                  PhotosWindowMode mode,
                                  PhotosWindowMode old_mode,
                                  gpointer user_data)
{
  PhotosEmbed *self = PHOTOS_EMBED (user_data);
  PhotosEmbedPrivate *priv = self->priv;

  photos_main_toolbar_reset_toolbar_mode (PHOTOS_MAIN_TOOLBAR (priv->toolbar));

  if (mode == PHOTOS_WINDOW_MODE_PREVIEW)
    photos_embed_prepare_for_preview (self);
  else
    {
      GtkListStore *model;
      GtkWidget *view_container;

      if (mode == PHOTOS_WINDOW_MODE_COLLECTIONS)
        photos_embed_prepare_for_collections (self);
      else if (mode == PHOTOS_WINDOW_MODE_FAVORITES)
        photos_embed_prepare_for_favorites (self);
      else if (mode == PHOTOS_WINDOW_MODE_OVERVIEW)
        photos_embed_prepare_for_overview (self);
      else if (mode == PHOTOS_WINDOW_MODE_SEARCH)
        photos_embed_prepare_for_search (self);

      view_container = photos_embed_get_view_container_from_mode (self, mode);
      model = photos_view_container_get_model (PHOTOS_VIEW_CONTAINER (view_container));
      photos_main_toolbar_set_view_model (PHOTOS_MAIN_TOOLBAR (priv->toolbar), PHOTOS_VIEW_MODEL (model));
    }
}


static void
photos_embed_dispose (GObject *object)
{
  PhotosEmbed *self = PHOTOS_EMBED (object);
  PhotosEmbedPrivate *priv = self->priv;

  photos_embed_clear_search (self);

  g_clear_object (&priv->ntfctn_mngr);
  g_clear_object (&priv->loader_cancellable);
  g_clear_object (&priv->col_mngr);
  g_clear_object (&priv->item_mngr);
  g_clear_object (&priv->src_mngr);
  g_clear_object (&priv->srch_mngr);
  g_clear_object (&priv->mode_cntrlr);
  g_clear_object (&priv->srch_cntrlr);
  g_clear_object (&priv->trk_ovrvw_cntrlr);

  /* GdStack triggers notify::visible-child during dispose and this means that
   * we have to explicitly disconnect the signal handler before calling up to
   * the parent implementation, or photos_embed_notify_visible_child() will
   * get called while we're in a inconsistent state
   */
  if (priv->stack != NULL)
    {
      g_signal_handlers_disconnect_by_func (priv->stack, photos_embed_notify_visible_child, self);
      priv->stack = NULL;
    }

  G_OBJECT_CLASS (photos_embed_parent_class)->dispose (object);
}


static void
photos_embed_init (PhotosEmbed *self)
{
  PhotosEmbedPrivate *priv;
  GApplication *app;
  GList *windows;
  GtkListStore *model;
  PhotosSearchbar *searchbar;
  PhotosSearchContextState *state;
  gboolean querying;
  const gchar *name;

  self->priv = photos_embed_get_instance_private (self);
  priv = self->priv;

  app = g_application_get_default ();
  state = photos_search_context_get_state (PHOTOS_SEARCH_CONTEXT (app));

  priv->search_action = g_action_map_lookup_action (G_ACTION_MAP (app), "search");

  gtk_orientable_set_orientation (GTK_ORIENTABLE (self), GTK_ORIENTATION_VERTICAL);
  gtk_widget_show (GTK_WIDGET (self));

  priv->selection_toolbar = photos_selection_toolbar_new ();
  gtk_box_pack_end (GTK_BOX (self), priv->selection_toolbar, FALSE, FALSE, 0);

  priv->stack_overlay = gtk_overlay_new ();
  gtk_widget_show (priv->stack_overlay);
  gtk_box_pack_end (GTK_BOX (self), priv->stack_overlay, TRUE, TRUE, 0);

  priv->stack = gtk_stack_new ();
  gtk_stack_set_homogeneous (GTK_STACK (priv->stack), TRUE);
  gtk_stack_set_transition_type (GTK_STACK (priv->stack), GTK_STACK_TRANSITION_TYPE_CROSSFADE);
  gtk_widget_show (priv->stack);
  gtk_container_add (GTK_CONTAINER (priv->stack_overlay), priv->stack);

  priv->toolbar = photos_main_toolbar_new (GTK_OVERLAY (priv->stack_overlay));
  photos_main_toolbar_set_stack (PHOTOS_MAIN_TOOLBAR (priv->toolbar), GTK_STACK (priv->stack));
  windows = gtk_application_get_windows (GTK_APPLICATION (app));
  gtk_window_set_titlebar (GTK_WINDOW (windows->data), priv->toolbar);
  searchbar = photos_main_toolbar_get_searchbar (PHOTOS_MAIN_TOOLBAR (priv->toolbar));
  g_signal_connect_swapped (searchbar, "activate-result", G_CALLBACK (photos_embed_activate_result), self);

  priv->ntfctn_mngr = g_object_ref_sink (photos_notification_manager_dup_singleton ());
  gtk_overlay_add_overlay (GTK_OVERLAY (priv->stack_overlay), priv->ntfctn_mngr);

  priv->overview = photos_view_container_new (PHOTOS_WINDOW_MODE_OVERVIEW, _("Recent"));
  name = photos_view_container_get_name (PHOTOS_VIEW_CONTAINER (priv->overview));
  gtk_stack_add_titled (GTK_STACK (priv->stack), priv->overview, "overview", name);
  model = photos_view_container_get_model (PHOTOS_VIEW_CONTAINER (priv->overview));
  g_signal_connect_swapped (model, "row-inserted", G_CALLBACK (photos_embed_row_changed), self);
  g_signal_connect_swapped (model, "row-deleted", G_CALLBACK (photos_embed_row_changed), self);

  priv->collections = photos_view_container_new (PHOTOS_WINDOW_MODE_COLLECTIONS, _("Albums"));
  name = photos_view_container_get_name (PHOTOS_VIEW_CONTAINER (priv->collections));
  gtk_stack_add_titled (GTK_STACK (priv->stack), priv->collections, "collections", name);
  model = photos_view_container_get_model (PHOTOS_VIEW_CONTAINER (priv->collections));
  g_signal_connect_swapped (model, "row-inserted", G_CALLBACK (photos_embed_row_changed), self);
  g_signal_connect_swapped (model, "row-deleted", G_CALLBACK (photos_embed_row_changed), self);

  priv->favorites = photos_view_container_new (PHOTOS_WINDOW_MODE_FAVORITES, _("Favorites"));
  name = photos_view_container_get_name (PHOTOS_VIEW_CONTAINER (priv->favorites));
  gtk_stack_add_titled (GTK_STACK (priv->stack), priv->favorites, "favorites", name);
  model = photos_view_container_get_model (PHOTOS_VIEW_CONTAINER (priv->favorites));
  g_signal_connect_swapped (model, "row-inserted", G_CALLBACK (photos_embed_row_changed), self);
  g_signal_connect_swapped (model, "row-deleted", G_CALLBACK (photos_embed_row_changed), self);

  priv->search = photos_view_container_new (PHOTOS_WINDOW_MODE_SEARCH, _("Search"));
  gtk_stack_add_named (GTK_STACK (priv->stack), priv->search, "search");
  model = photos_view_container_get_model (PHOTOS_VIEW_CONTAINER (priv->search));
  g_signal_connect_swapped (model, "row-inserted", G_CALLBACK (photos_embed_row_changed), self);
  g_signal_connect_swapped (model, "row-deleted", G_CALLBACK (photos_embed_row_changed), self);

  priv->preview = photos_preview_view_new (GTK_OVERLAY (priv->stack_overlay));
  gtk_stack_add_named (GTK_STACK (priv->stack), priv->preview, "preview");

  priv->spinner_box = photos_spinner_box_new ();
  gtk_stack_add_named (GTK_STACK (priv->stack), priv->spinner_box, "spinner");

  /* TODO: SearchBar.Dropdown, …
   */

  g_signal_connect_object (priv->stack, "notify::visible-child",
                           G_CALLBACK (photos_embed_notify_visible_child),
                           self, G_CONNECT_SWAPPED);

  priv->mode_cntrlr = photos_mode_controller_dup_singleton ();
  g_signal_connect (priv->mode_cntrlr,
                    "window-mode-changed",
                    G_CALLBACK (photos_embed_window_mode_changed),
                    self);
  g_signal_connect (priv->mode_cntrlr,
                    "fullscreen-changed",
                    G_CALLBACK (photos_embed_fullscreen_changed),
                    self);

  priv->trk_ovrvw_cntrlr = photos_tracker_overview_controller_dup_singleton ();
  g_signal_connect_swapped (priv->trk_ovrvw_cntrlr,
                            "query-status-changed",
                            G_CALLBACK (photos_embed_query_status_changed),
                            self);

  priv->col_mngr = g_object_ref (state->col_mngr);
  g_signal_connect (priv->col_mngr, "active-changed", G_CALLBACK (photos_embed_active_changed), self);

  priv->item_mngr = photos_item_manager_dup_singleton ();
  g_signal_connect (priv->item_mngr, "active-changed", G_CALLBACK (photos_embed_active_changed), self);

  priv->src_mngr = g_object_ref (state->src_mngr);
  g_signal_connect_swapped (priv->src_mngr, "active-changed", G_CALLBACK (photos_embed_search_changed), self);

  priv->srch_mngr = g_object_ref (state->srch_typ_mngr);
  g_signal_connect_swapped (priv->srch_mngr, "active-changed", G_CALLBACK (photos_embed_search_changed), self);

  querying = photos_tracker_controller_get_query_status (priv->trk_ovrvw_cntrlr);
  photos_embed_query_status_changed (self, querying);

  priv->srch_cntrlr = g_object_ref (state->srch_cntrlr);
  g_signal_connect_swapped (priv->srch_cntrlr,
                            "search-string-changed",
                            G_CALLBACK (photos_embed_search_changed),
                            self);

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
  return PHOTOS_MAIN_TOOLBAR (self->priv->toolbar);
}


PhotosPreviewView *
photos_embed_get_preview (PhotosEmbed *self)
{
  return PHOTOS_PREVIEW_VIEW (self->priv->preview);
}
