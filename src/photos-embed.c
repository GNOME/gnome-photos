/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2012, 2013 Red Hat, Inc.
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

#include "photos-embed.h"
#include "photos-empty-results-box.h"
#include "photos-error-box.h"
#include "photos-indexing-notification.h"
#include "photos-item-manager.h"
#include "photos-main-toolbar.h"
#include "photos-mode-controller.h"
#include "photos-notification-manager.h"
#include "photos-offset-overview-controller.h"
#include "photos-preview-view.h"
#include "photos-selection-toolbar.h"
#include "photos-spinner-box.h"
#include "photos-tracker-change-monitor.h"
#include "photos-tracker-overview-controller.h"
#include "photos-view-container.h"


struct _PhotosEmbedPrivate
{
  GCancellable *loader_cancellable;
  GtkWidget *error_box;
  GtkWidget *favorites;
  GtkWidget *indexing_ntfctn;
  GtkWidget *no_results;
  GtkWidget *notebook;
  GtkWidget *notebook_overlay;
  GtkWidget *ntfctn_mngr;
  GtkWidget *overview;
  GtkWidget *preview;
  GtkWidget *selection_toolbar;
  GtkWidget *spinner_box;
  GtkWidget *toolbar;
  PhotosBaseManager *item_mngr;
  PhotosModeController *mode_cntrlr;
  PhotosOffsetController *offset_cntrlr;
  PhotosTrackerChangeMonitor *monitor;
  PhotosTrackerController *trk_ovrvw_cntrlr;
  gint error_page;
  gint favorites_page;
  gint no_results_page;
  gint preview_page;
  gint overview_page;
  gint spinner_page;
  gulong no_results_change_id;
};


G_DEFINE_TYPE (PhotosEmbed, photos_embed, GTK_TYPE_BOX);


static void
photos_embed_item_load (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosEmbed *self = PHOTOS_EMBED (user_data);
  PhotosEmbedPrivate *priv = self->priv;
  GeglNode *node;
  PhotosBaseItem *item = PHOTOS_BASE_ITEM (source_object);

  g_clear_object (&priv->loader_cancellable);
  node = photos_base_item_load_finish (item, res, NULL);
  if (node == NULL)
    return;

  photos_preview_view_set_node (PHOTOS_PREVIEW_VIEW (priv->preview), node);

  /* TODO: set toolbar model */

  photos_mode_controller_set_window_mode (priv->mode_cntrlr, PHOTOS_WINDOW_MODE_PREVIEW);
  photos_mode_controller_set_can_fullscreen (priv->mode_cntrlr, TRUE);
}


static void
photos_embed_active_changed (PhotosBaseManager *manager, GObject *object, gpointer user_data)
{
  PhotosEmbed *self = PHOTOS_EMBED (user_data);
  PhotosEmbedPrivate *priv = self->priv;

  if (object == NULL)
    return;

  /* TODO: CollectionManager */

  photos_spinner_box_start_delayed (PHOTOS_SPINNER_BOX (priv->spinner_box), 400);
  gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->notebook), priv->spinner_page);

  priv->loader_cancellable = g_cancellable_new ();
  photos_base_item_load_async (PHOTOS_BASE_ITEM (object),
                               priv->loader_cancellable,
                               photos_embed_item_load,
                               self);
}


static void
photos_embed_restore_last_page (PhotosEmbed *self)
{
  PhotosEmbedPrivate *priv = self->priv;
  PhotosWindowMode mode;
  gint page;

  mode = photos_mode_controller_get_window_mode (priv->mode_cntrlr);
  switch (mode)
    {
    case PHOTOS_WINDOW_MODE_FAVORITES:
      page = priv->overview_page;
      break;

    case PHOTOS_WINDOW_MODE_OVERVIEW:
      page = priv->favorites_page;
      break;

    case PHOTOS_WINDOW_MODE_PREVIEW:
      page = priv->preview_page;
      break;

    default:
      g_assert_not_reached ();
      break;
    }

  gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->notebook), page);
}


static void
photos_embed_hide_no_results_page (PhotosEmbed *self)
{
  PhotosEmbedPrivate *priv = self->priv;

  if (priv->no_results_change_id != 0)
    {
      g_signal_handler_disconnect (priv->monitor, priv->no_results_change_id);
      priv->no_results_change_id = 0;
    }

  photos_embed_restore_last_page (self);
}


static void
photos_embed_changes_pending (PhotosEmbed *self, GHashTable *changes)
{
  photos_embed_hide_no_results_page (self);
}


static void
photos_embed_count_changed (PhotosEmbed *self, gint count)
{
  PhotosEmbedPrivate *priv = self->priv;

  if (count == 0)
    {
      priv->no_results_change_id = g_signal_connect_swapped (priv->monitor,
                                                             "changes-pending",
                                                             G_CALLBACK (photos_embed_changes_pending),
                                                             self);
      gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->notebook), priv->no_results_page);
    }
  else
    photos_embed_hide_no_results_page (self);
}


static void
photos_embed_fullscreen_changed (PhotosModeController *mode_cntrlr, gboolean fullscreen, gpointer user_data)
{
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
  gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->notebook), priv->favorites_page);
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
  gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->notebook), priv->overview_page);
}


static void
photos_embed_prepare_for_preview (PhotosEmbed *self)
{
  PhotosEmbedPrivate *priv = self->priv;

  /* TODO: SearchController,
   *       ErrorHandler
   */

  photos_spinner_box_stop (PHOTOS_SPINNER_BOX (priv->spinner_box));
  gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->notebook), priv->preview_page);
}


void
photos_embed_set_error (PhotosEmbed *self, const gchar *primary, const gchar *secondary)
{
  PhotosEmbedPrivate *priv = self->priv;

  photos_error_box_update (PHOTOS_ERROR_BOX (priv->error_box), primary, secondary);
  gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->notebook), priv->error_page);
}


static void
photos_embed_query_error (PhotosEmbed *self, const gchar *primary, const gchar *secondary)
{
  photos_embed_set_error (self, primary, secondary);
}


void
photos_embed_query_status_changed (PhotosTrackerController *trk_cntrlr, gboolean querying, gpointer user_data)
{
  PhotosEmbed *self = PHOTOS_EMBED (user_data);
  PhotosEmbedPrivate *priv = self->priv;

  if (querying)
    {
      photos_spinner_box_start (PHOTOS_SPINNER_BOX (priv->spinner_box));
      gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->notebook), priv->spinner_page);
    }
  else
    {
      photos_spinner_box_stop (PHOTOS_SPINNER_BOX (priv->spinner_box));
      photos_embed_restore_last_page (self);
    }
}

static void
photos_embed_window_mode_changed (PhotosModeController *mode_cntrlr,
                                  PhotosWindowMode mode,
                                  PhotosWindowMode old_mode,
                                  gpointer user_data)
{
  PhotosEmbed *self = PHOTOS_EMBED (user_data);
  PhotosEmbedPrivate *priv = self->priv;

  if (mode == PHOTOS_WINDOW_MODE_FAVORITES)
    photos_embed_prepare_for_favorites (self);
  else if (mode == PHOTOS_WINDOW_MODE_OVERVIEW)
    photos_embed_prepare_for_overview (self);
  else
    photos_embed_prepare_for_preview (self);
}


static void
photos_embed_dispose (GObject *object)
{
  PhotosEmbed *self = PHOTOS_EMBED (object);
  PhotosEmbedPrivate *priv = self->priv;

  if (priv->no_results_change_id != 0)
    {
      g_signal_handler_disconnect (priv->monitor, priv->no_results_change_id);
      priv->no_results_change_id = 0;
    }

  g_clear_object (&priv->ntfctn_mngr);
  g_clear_object (&priv->loader_cancellable);
  g_clear_object (&priv->indexing_ntfctn);
  g_clear_object (&priv->item_mngr);
  g_clear_object (&priv->mode_cntrlr);
  g_clear_object (&priv->offset_cntrlr);
  g_clear_object (&priv->monitor);
  g_clear_object (&priv->trk_ovrvw_cntrlr);

  G_OBJECT_CLASS (photos_embed_parent_class)->dispose (object);
}

static void
photos_embed_init (PhotosEmbed *self)
{
  PhotosEmbedPrivate *priv;
  gboolean querying;
  GtkStyleContext *context;

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, PHOTOS_TYPE_EMBED, PhotosEmbedPrivate);
  priv = self->priv;

  gtk_orientable_set_orientation (GTK_ORIENTABLE (self), GTK_ORIENTATION_VERTICAL);
  gtk_widget_show (GTK_WIDGET (self));

  priv->notebook_overlay = gtk_overlay_new ();
  gtk_widget_show (priv->notebook_overlay);
  gtk_box_pack_end (GTK_BOX (self), priv->notebook_overlay, TRUE, TRUE, 0);

  priv->notebook = gtk_notebook_new ();
  gtk_notebook_set_show_border (GTK_NOTEBOOK (priv->notebook), FALSE);
  gtk_notebook_set_show_tabs (GTK_NOTEBOOK (priv->notebook), FALSE);
  gtk_widget_show (priv->notebook);
  gtk_container_add (GTK_CONTAINER (priv->notebook_overlay), priv->notebook);

  priv->toolbar = photos_main_toolbar_new ();
  gtk_box_pack_start (GTK_BOX (self), priv->toolbar, FALSE, FALSE, 0);

  priv->ntfctn_mngr = g_object_ref_sink (photos_notification_manager_new ());
  gtk_overlay_add_overlay (GTK_OVERLAY (priv->notebook_overlay), priv->ntfctn_mngr);

  priv->indexing_ntfctn = g_object_ref_sink (photos_indexing_notification_new ());

  priv->overview = photos_view_container_new (PHOTOS_WINDOW_MODE_OVERVIEW);
  priv->overview_page = gtk_notebook_append_page (GTK_NOTEBOOK (priv->notebook), priv->overview, NULL);

  priv->favorites = photos_view_container_new (PHOTOS_WINDOW_MODE_FAVORITES);
  priv->favorites_page = gtk_notebook_append_page (GTK_NOTEBOOK (priv->notebook), priv->favorites, NULL);

  priv->preview = photos_preview_view_new ();
  priv->preview_page = gtk_notebook_append_page (GTK_NOTEBOOK (priv->notebook), priv->preview, NULL);

  priv->spinner_box = photos_spinner_box_new ();
  priv->spinner_page = gtk_notebook_append_page (GTK_NOTEBOOK (priv->notebook), priv->spinner_box, NULL);

  priv->error_box = photos_error_box_new ();
  priv->error_page = gtk_notebook_append_page (GTK_NOTEBOOK (priv->notebook), priv->error_box, NULL);

  priv->no_results = photos_empty_results_box_new ();
  priv->no_results_page = gtk_notebook_append_page (GTK_NOTEBOOK (priv->notebook), priv->no_results, NULL);

  /* TODO: SearchBar.Dropdown,
   *       ...
   */

  priv->selection_toolbar = photos_selection_toolbar_new ();
  gtk_overlay_add_overlay (GTK_OVERLAY (priv->notebook_overlay), priv->selection_toolbar);

  priv->mode_cntrlr = photos_mode_controller_new ();
  g_signal_connect (priv->mode_cntrlr,
                    "window-mode-changed",
                    G_CALLBACK (photos_embed_window_mode_changed),
                    self);
  g_signal_connect (priv->mode_cntrlr,
                    "fullscreen-changed",
                    G_CALLBACK (photos_embed_fullscreen_changed),
                    self);

  priv->trk_ovrvw_cntrlr = photos_tracker_overview_controller_new ();
  g_signal_connect_swapped (priv->trk_ovrvw_cntrlr, "query-error", G_CALLBACK (photos_embed_query_error), self);
  g_signal_connect (priv->trk_ovrvw_cntrlr,
                    "query-status-changed",
                    G_CALLBACK (photos_embed_query_status_changed),
                    self);

  priv->offset_cntrlr = photos_offset_overview_controller_new ();
  g_signal_connect_swapped (priv->offset_cntrlr, "count-changed", G_CALLBACK (photos_embed_count_changed), self);

  priv->item_mngr = photos_item_manager_new ();
  g_signal_connect (priv->item_mngr, "active-changed", G_CALLBACK (photos_embed_active_changed), self);

  querying = photos_tracker_controller_get_query_status (priv->trk_ovrvw_cntrlr);
  photos_embed_query_status_changed (priv->trk_ovrvw_cntrlr, querying, self);

  priv->monitor = photos_tracker_change_monitor_new ();

  gtk_widget_show (GTK_WIDGET (self));
}


static void
photos_embed_class_init (PhotosEmbedClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->dispose = photos_embed_dispose;

  g_type_class_add_private (class, sizeof (PhotosEmbedPrivate));
}


GtkWidget *
photos_embed_new (void)
{
  return g_object_new (PHOTOS_TYPE_EMBED, NULL);
}
