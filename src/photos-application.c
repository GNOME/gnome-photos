/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2015 Alessandro Bono
 * Copyright © 2014 – 2015 Pranav Kant
 * Copyright © 2012 – 2017 Red Hat, Inc.
 * Copyright © 2016 Umang Jain
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

#include <locale.h>
#include <stdlib.h>

#include <gdesktop-enums.h>
#include <gegl.h>
#include <gexiv2/gexiv2.h>
#include <gio/gio.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <grilo.h>
#include <libtracker-control/tracker-control.h>

#include "photos-application.h"
#include "photos-base-item.h"
#include "photos-camera-cache.h"
#include "photos-create-collection-job.h"
#include "photos-debug.h"
#include "photos-dlna-renderers-dialog.h"
#include "photos-export-dialog.h"
#include "photos-export-notification.h"
#include "photos-filterable.h"
#include "photos-gegl.h"
#include "photos-glib.h"
#include "photos-import-dialog.h"
#include "photos-item-manager.h"
#include "photos-main-window.h"
#include "photos-properties-dialog.h"
#include "photos-query.h"
#include "photos-resources.h"
#include "photos-resources-gegl.h"
#include "photos-search-context.h"
#include "photos-search-controller.h"
#include "photos-search-match.h"
#include "photos-search-type.h"
#include "photos-search-provider.h"
#include "photos-selection-controller.h"
#include "photos-set-collection-job.h"
#include "photos-single-item-job.h"
#include "photos-source.h"
#include "photos-source-manager.h"
#include "photos-share-dialog.h"
#include "photos-share-notification.h"
#include "photos-share-point-manager.h"
#include "photos-thumbnail-factory.h"
#include "photos-tracker-extract-priority.h"
#include "photos-utils.h"


struct _PhotosApplication
{
  GtkApplication parent_instance;
  GCancellable *create_window_cancellable;
  GHashTable *refresh_miner_ids;
  GList *miners;
  GList *miners_running;
  GResource *resource;
  GResource *resource_gegl;
  GSettings *bg_settings;
  GSettings *ss_settings;
  GSimpleAction *blacks_exposure_action;
  GSimpleAction *contrast_action;
  GSimpleAction *crop_action;
  GSimpleAction *delete_action;
  GSimpleAction *denoise_action;
  GSimpleAction *edit_action;
  GSimpleAction *edit_cancel_action;
  GSimpleAction *edit_done_action;
  GSimpleAction *edit_revert_action;
  GSimpleAction *fs_action;
  GSimpleAction *gear_action;
  GSimpleAction *import_action;
  GSimpleAction *import_cancel_action;
  GSimpleAction *insta_action;
  GSimpleAction *load_next_action;
  GSimpleAction *load_previous_action;
  GSimpleAction *open_action;
  GSimpleAction *print_action;
  GSimpleAction *properties_action;
  GSimpleAction *saturation_action;
  GSimpleAction *save_action;
  GSimpleAction *search_action;
  GSimpleAction *search_match_action;
  GSimpleAction *search_source_action;
  GSimpleAction *search_type_action;
  GSimpleAction *sel_all_action;
  GSimpleAction *sel_none_action;
  GSimpleAction *selection_mode_action;
  GSimpleAction *set_bg_action;
  GSimpleAction *set_ss_action;
  GSimpleAction *shadows_highlights_action;
  GSimpleAction *share_action;
  GSimpleAction *sharpen_action;
  GSimpleAction *zoom_begin_action;
  GSimpleAction *zoom_best_fit_action;
  GSimpleAction *zoom_end_action;
  GSimpleAction *zoom_in_action;
  GSimpleAction *zoom_out_action;
  GtkWidget *main_window;
  PhotosBaseManager *shr_pnt_mngr;
  PhotosCameraCache *camera_cache;
  PhotosSearchContextState *state;
  PhotosSearchProvider *search_provider;
  PhotosSelectionController *sel_cntrlr;
  PhotosThumbnailFactory *factory;
  TrackerExtractPriority *extract_priority;
  gboolean empty_results;
  gboolean main_window_deleted;
  guint create_miners_count;
  guint init_fishes_id;
  guint use_count;
  guint32 activation_timestamp;
  gulong source_added_id;
  gulong source_removed_id;
};

enum
{
  MINERS_CHANGED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static void photos_application_search_context_iface_init (PhotosSearchContextInterface *iface);


G_DEFINE_TYPE_WITH_CODE (PhotosApplication, photos_application, GTK_TYPE_APPLICATION,
                         G_IMPLEMENT_INTERFACE (PHOTOS_TYPE_SEARCH_CONTEXT,
                                                photos_application_search_context_iface_init));


enum
{
  MINER_REFRESH_TIMEOUT = 60 /* s */
};

static const GOptionEntry COMMAND_LINE_OPTIONS[] =
{
  { "empty-results", 0, G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, NULL, N_("Show the empty state"), NULL },
  { "version", 0, G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, NULL, N_("Show the application's version"), NULL},
  { NULL }
};

static const gchar *DESKTOP_BACKGROUND_SCHEMA = "org.gnome.desktop.background";
static const gchar *DESKTOP_SCREENSAVER_SCHEMA = "org.gnome.desktop.screensaver";
static const gchar *DESKTOP_KEY_PICTURE_URI = "picture-uri";
static const gchar *DESKTOP_KEY_PICTURE_OPTIONS = "picture-options";
static const gchar *DESKTOP_KEY_COLOR_SHADING_TYPE = "color-shading-type";
static const gchar *DESKTOP_KEY_PRIMARY_COLOR = "primary-color";
static const gchar *DESKTOP_KEY_SECONDARY_COLOR = "secondary-color";

typedef struct _PhotosApplicationCreateData PhotosApplicationCreateData;
typedef struct _PhotosApplicationImportData PhotosApplicationImportData;
typedef struct _PhotosApplicationImportCopiedData PhotosApplicationImportCopiedData;
typedef struct _PhotosApplicationImportWaitForFileData PhotosApplicationImportWaitForFileData;
typedef struct _PhotosApplicationRefreshData PhotosApplicationRefreshData;
typedef struct _PhotosApplicationSetBackgroundData PhotosApplicationSetBackgroundData;

struct _PhotosApplicationCreateData
{
  PhotosApplication *application;
  gchar *extension_name;
  gchar *miner_name;
};

struct _PhotosApplicationImportData
{
  PhotosApplication *application;
  GFile *destination;
  GFile *import_sub_dir;
  GList *files;
  PhotosBaseItem *collection;
  TrackerMinerManager *manager;
  gchar *collection_urn;
  gint64 ctime_latest;
};

struct _PhotosApplicationRefreshData
{
  PhotosApplication *application;
  GomMiner *miner;
};

struct _PhotosApplicationSetBackgroundData
{
  PhotosApplication *application;
  GFile *file;
  GSettings *settings;
};

static void photos_application_import_file_copy (GObject *source_object, GAsyncResult *res, gpointer user_data);
static void photos_application_refresh_miner_now (PhotosApplication *self, GomMiner *miner);
static void photos_application_start_miners (PhotosApplication *self);
static void photos_application_start_miners_second (PhotosApplication *self);
static void photos_application_stop_miners (PhotosApplication *self);


static PhotosApplicationCreateData *
photos_application_create_data_new (PhotosApplication *application,
                                    const gchar *extension_name,
                                    const gchar *miner_name)
{
  PhotosApplicationCreateData *data;

  data = g_slice_new0 (PhotosApplicationCreateData);
  g_application_hold (G_APPLICATION (application));
  data->application = application;
  data->extension_name = g_strdup (extension_name);
  data->miner_name = g_strdup (miner_name);
  return data;
}


static void
photos_application_create_data_free (PhotosApplicationCreateData *data)
{
  g_application_release (G_APPLICATION (data->application));
  g_free (data->extension_name);
  g_free (data->miner_name);
  g_slice_free (PhotosApplicationCreateData, data);
}


static PhotosApplicationImportData *
photos_application_import_data_new (PhotosApplication *application,
                                    TrackerMinerManager *manager,
                                    GList *files,
                                    gint64 ctime_latest)
{
  PhotosApplicationImportData *data;

  data = g_slice_new0 (PhotosApplicationImportData);
  g_application_hold (G_APPLICATION (application));
  data->application = application;
  data->manager = g_object_ref (manager);
  data->files = g_list_copy_deep (files, (GCopyFunc) g_object_ref, NULL);
  data->ctime_latest = ctime_latest;
  return data;
}


static void
photos_application_import_data_free (PhotosApplicationImportData *data)
{
  g_application_release (G_APPLICATION (data->application));

  if (data->collection != NULL)
    {
      photos_base_item_unmark_busy (data->collection);
      g_object_unref (data->collection);
    }

  g_clear_object (&data->destination);
  g_clear_object (&data->import_sub_dir);
  g_list_free_full (data->files, g_object_unref);
  g_clear_object (&data->manager);
  g_free (data->collection_urn);
  g_slice_free (PhotosApplicationImportData, data);
}


G_DEFINE_AUTOPTR_CLEANUP_FUNC (PhotosApplicationImportData, photos_application_import_data_free);


static PhotosApplicationRefreshData *
photos_application_refresh_data_new (PhotosApplication *application, GomMiner *miner)
{
  PhotosApplicationRefreshData *data;

  data = g_slice_new0 (PhotosApplicationRefreshData);
  g_application_hold (G_APPLICATION (application));
  data->application = application;
  data->miner = g_object_ref (miner);
  return data;
}


static void
photos_application_refresh_data_free (PhotosApplicationRefreshData *data)
{
  g_application_release (G_APPLICATION (data->application));
  g_object_unref (data->miner);
  g_slice_free (PhotosApplicationRefreshData, data);
}


static PhotosApplicationSetBackgroundData *
photos_application_set_background_data_new (PhotosApplication *application, GFile *file, GSettings *settings)
{
  PhotosApplicationSetBackgroundData *data;

  data = g_slice_new0 (PhotosApplicationSetBackgroundData);
  g_application_hold (G_APPLICATION (application));
  data->application = application;
  data->file = g_object_ref (file);
  data->settings = g_object_ref (settings);
  return data;
}


static void
photos_application_set_background_data_free (PhotosApplicationSetBackgroundData *data)
{
  g_application_release (G_APPLICATION (data->application));
  g_object_unref (data->file);
  g_object_unref (data->settings);
  g_slice_free (PhotosApplicationSetBackgroundData, data);
}


static void
photos_application_help (PhotosApplication *self)
{
  GtkWindow *parent;
  guint32 time;

  parent = gtk_application_get_active_window (GTK_APPLICATION (self));
  time = gtk_get_current_event_time ();
  gtk_show_uri_on_window (parent, "help:gnome-photos", time, NULL);
}


static void
photos_application_about (PhotosApplication *self)
{
  photos_main_window_show_about (PHOTOS_MAIN_WINDOW (self->main_window));
}


static PhotosBaseItem *
photos_application_get_selection_or_active_item (PhotosApplication *self)
{
  PhotosBaseItem *item = NULL;

  if (photos_utils_get_selection_mode ())
    {
      GList *selection;
      const gchar *urn;

      selection = photos_selection_controller_get_selection (self->sel_cntrlr);
      if (selection == NULL || selection->next != NULL) /* length != 1 */
        goto out;

      urn = (gchar *) selection->data;
      item = PHOTOS_BASE_ITEM (photos_base_manager_get_object_by_id (self->state->item_mngr, urn));
    }
  else
    {
      item = PHOTOS_BASE_ITEM (photos_base_manager_get_active_object (self->state->item_mngr));
    }

 out:
  return item;
}


static void
photos_application_actions_update (PhotosApplication *self)
{
  PhotosBaseItem *item;
  GList *l;
  GList *selection;
  PhotosLoadState load_state;
  PhotosWindowMode mode;
  gboolean can_open;
  gboolean can_trash;
  gboolean enable;
  gboolean selection_mode;
  const gchar *cancel_accels[] = { "Escape", NULL };
  const gchar *null_accels[] = { NULL };
  guint n_items = 0;

  item = photos_application_get_selection_or_active_item (self);
  load_state = photos_item_manager_get_load_state (self->state->item_mngr);
  mode = photos_mode_controller_get_window_mode (self->state->mode_cntrlr);
  selection = photos_selection_controller_get_selection (self->sel_cntrlr);
  selection_mode = photos_utils_get_selection_mode ();

  if (selection_mode)
    {
      gtk_application_set_accels_for_action (GTK_APPLICATION (self), "app.edit-cancel", null_accels);

      switch (mode)
        {
        case PHOTOS_WINDOW_MODE_COLLECTION_VIEW:
        case PHOTOS_WINDOW_MODE_COLLECTIONS:
        case PHOTOS_WINDOW_MODE_FAVORITES:
        case PHOTOS_WINDOW_MODE_OVERVIEW:
        case PHOTOS_WINDOW_MODE_SEARCH:
          gtk_application_set_accels_for_action (GTK_APPLICATION (self), "app.import-cancel", null_accels);
          gtk_application_set_accels_for_action (GTK_APPLICATION (self), "app.selection-mode", cancel_accels);
          break;

        case PHOTOS_WINDOW_MODE_IMPORT:
          gtk_application_set_accels_for_action (GTK_APPLICATION (self), "app.import-cancel", cancel_accels);
          gtk_application_set_accels_for_action (GTK_APPLICATION (self), "app.selection-mode", null_accels);
          break;

        case PHOTOS_WINDOW_MODE_NONE:
        case PHOTOS_WINDOW_MODE_EDIT:
        case PHOTOS_WINDOW_MODE_PREVIEW:
        default:
          g_assert_not_reached ();
          break;
        }
    }
  else
    {
      gtk_application_set_accels_for_action (GTK_APPLICATION (self), "app.import-cancel", null_accels);
      gtk_application_set_accels_for_action (GTK_APPLICATION (self), "app.selection-mode", null_accels);

      switch (mode)
        {
        case PHOTOS_WINDOW_MODE_COLLECTION_VIEW:
        case PHOTOS_WINDOW_MODE_COLLECTIONS:
        case PHOTOS_WINDOW_MODE_FAVORITES:
        case PHOTOS_WINDOW_MODE_IMPORT:
        case PHOTOS_WINDOW_MODE_OVERVIEW:
        case PHOTOS_WINDOW_MODE_PREVIEW:
        case PHOTOS_WINDOW_MODE_SEARCH:
          gtk_application_set_accels_for_action (GTK_APPLICATION (self), "app.edit-cancel", null_accels);
          break;

        case PHOTOS_WINDOW_MODE_EDIT:
          gtk_application_set_accels_for_action (GTK_APPLICATION (self), "app.edit-cancel", cancel_accels);
          break;

        case PHOTOS_WINDOW_MODE_NONE:
        default:
          g_assert_not_reached ();
          break;
        }
    }

  if (mode == PHOTOS_WINDOW_MODE_COLLECTION_VIEW
      || mode == PHOTOS_WINDOW_MODE_COLLECTIONS
      || mode == PHOTOS_WINDOW_MODE_FAVORITES
      || mode == PHOTOS_WINDOW_MODE_IMPORT
      || mode == PHOTOS_WINDOW_MODE_OVERVIEW
      || mode == PHOTOS_WINDOW_MODE_SEARCH)
    {
      PhotosBaseManager *item_mngr_chld;

      item_mngr_chld = photos_item_manager_get_for_mode (PHOTOS_ITEM_MANAGER (self->state->item_mngr), mode);
      n_items = g_list_model_get_n_items (G_LIST_MODEL (item_mngr_chld));
    }

  g_simple_action_set_enabled (self->zoom_best_fit_action, FALSE);
  g_simple_action_set_enabled (self->zoom_end_action, FALSE);
  g_simple_action_set_enabled (self->zoom_out_action, FALSE);

  enable = (mode == PHOTOS_WINDOW_MODE_EDIT);
  g_simple_action_set_enabled (self->blacks_exposure_action, enable);
  g_simple_action_set_enabled (self->contrast_action, enable);
  g_simple_action_set_enabled (self->crop_action, enable);
  g_simple_action_set_enabled (self->denoise_action, enable);
  g_simple_action_set_enabled (self->edit_cancel_action, enable);
  g_simple_action_set_enabled (self->edit_done_action, enable);
  g_simple_action_set_enabled (self->insta_action, enable);
  g_simple_action_set_enabled (self->saturation_action, enable);
  g_simple_action_set_enabled (self->shadows_highlights_action, enable);
  g_simple_action_set_enabled (self->sharpen_action, enable);

  enable = (((mode == PHOTOS_WINDOW_MODE_COLLECTION_VIEW
              || mode == PHOTOS_WINDOW_MODE_COLLECTIONS
              || mode == PHOTOS_WINDOW_MODE_FAVORITES
              || mode == PHOTOS_WINDOW_MODE_OVERVIEW)
             && n_items > 0)
            || mode == PHOTOS_WINDOW_MODE_SEARCH);
  g_simple_action_set_enabled (self->search_action, enable);
  g_simple_action_set_enabled (self->search_match_action, enable);
  g_simple_action_set_enabled (self->search_source_action, enable);
  g_simple_action_set_enabled (self->search_type_action, enable);

  enable = ((mode == PHOTOS_WINDOW_MODE_COLLECTION_VIEW
             || mode == PHOTOS_WINDOW_MODE_COLLECTIONS
             || mode == PHOTOS_WINDOW_MODE_FAVORITES
             || mode == PHOTOS_WINDOW_MODE_IMPORT
             || mode == PHOTOS_WINDOW_MODE_OVERVIEW
             || mode == PHOTOS_WINDOW_MODE_SEARCH)
            && n_items > 0);
  g_simple_action_set_enabled (self->sel_all_action, enable);
  g_simple_action_set_enabled (self->sel_none_action, enable);
  g_simple_action_set_enabled (self->selection_mode_action, enable);

  enable = (mode == PHOTOS_WINDOW_MODE_IMPORT);
  g_simple_action_set_enabled (self->import_cancel_action, enable);

  enable = (mode == PHOTOS_WINDOW_MODE_IMPORT && selection != NULL);
  g_simple_action_set_enabled (self->import_action, enable);

  enable = (mode == PHOTOS_WINDOW_MODE_PREVIEW);
  g_simple_action_set_enabled (self->load_next_action, enable);
  g_simple_action_set_enabled (self->load_previous_action, enable);

  enable = (load_state == PHOTOS_LOAD_STATE_FINISHED && mode == PHOTOS_WINDOW_MODE_PREVIEW);
  g_simple_action_set_enabled (self->gear_action, enable);
  g_simple_action_set_enabled (self->set_bg_action, enable);
  g_simple_action_set_enabled (self->set_ss_action, enable);
  g_simple_action_set_enabled (self->zoom_begin_action, enable);
  g_simple_action_set_enabled (self->zoom_in_action, enable);

  enable = ((load_state == PHOTOS_LOAD_STATE_FINISHED && mode == PHOTOS_WINDOW_MODE_PREVIEW)
            || (selection_mode && item != NULL));
  g_simple_action_set_enabled (self->properties_action, enable);

  enable = ((load_state == PHOTOS_LOAD_STATE_FINISHED && mode == PHOTOS_WINDOW_MODE_PREVIEW)
            || (selection_mode && item != NULL && !photos_base_item_is_collection (item)));
  g_simple_action_set_enabled (self->print_action, enable);
  g_simple_action_set_enabled (self->save_action, enable);

  enable = (item != NULL
            && ((load_state == PHOTOS_LOAD_STATE_FINISHED && mode == PHOTOS_WINDOW_MODE_PREVIEW) || selection_mode)
            && photos_share_point_manager_can_share (PHOTOS_SHARE_POINT_MANAGER (self->shr_pnt_mngr), item));
  g_simple_action_set_enabled (self->share_action, enable);

  can_open = FALSE;
  can_trash = selection != NULL;
  for (l = selection; l != NULL; l = l->next)
    {
      PhotosBaseItem *selected_item;
      const gchar *urn = (gchar *) l->data;

      /* When PhotosItemManager::items-changed is emitted, a selected
       * item can potentially be removed from the item manager.
       */
      selected_item = PHOTOS_BASE_ITEM (photos_base_manager_get_object_by_id (self->state->item_mngr, urn));
      if (selected_item == NULL)
        continue;

      can_trash = can_trash && photos_base_item_can_trash (selected_item);

      if (photos_base_item_get_default_app_name (selected_item) != NULL)
        can_open = TRUE;
    }

  enable = ((load_state == PHOTOS_LOAD_STATE_FINISHED
             && mode == PHOTOS_WINDOW_MODE_PREVIEW
             && photos_base_item_can_trash (item))
            || (selection_mode && can_trash));
  g_simple_action_set_enabled (self->delete_action, enable);

  enable = ((load_state == PHOTOS_LOAD_STATE_FINISHED && mode == PHOTOS_WINDOW_MODE_PREVIEW)
            || (selection_mode && can_open));
  g_simple_action_set_enabled (self->open_action, enable);

  enable = (load_state == PHOTOS_LOAD_STATE_FINISHED
            && mode == PHOTOS_WINDOW_MODE_PREVIEW
            && photos_base_item_can_edit (item));
  g_simple_action_set_enabled (self->edit_action, enable);
}


static void
photos_application_tracker_clear_rdf_types (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosApplication *self = PHOTOS_APPLICATION (user_data);
  TrackerExtractPriority *extract_priority = TRACKER_EXTRACT_PRIORITY (source_object);

  {
    g_autoptr (GError) error = NULL;

    if (!tracker_extract_priority_call_clear_rdf_types_finish (extract_priority, res, &error))
      {
        g_warning ("Unable to call ClearRdfTypes: %s", error->message);
        goto out;
      }
  }

 out:
  g_application_release (G_APPLICATION (self));
}


static gboolean
photos_application_delete_event (PhotosApplication *self)
{
  gboolean ret_val = GDK_EVENT_PROPAGATE;

  self->main_window_deleted = TRUE;

  if (self->use_count > 0)
    ret_val = gtk_widget_hide_on_delete (self->main_window);

  return ret_val;
}


static void
photos_application_destroy (PhotosApplication *self)
{
  GHashTableIter iter;
  gpointer refresh_miner_id_data;

  self->main_window = NULL;

  g_hash_table_iter_init (&iter, self->refresh_miner_ids);
  while (g_hash_table_iter_next (&iter, NULL, &refresh_miner_id_data))
    {
      guint refresh_miner_id = GPOINTER_TO_UINT (refresh_miner_id_data);
      g_source_remove (refresh_miner_id);
    }

  g_hash_table_remove_all (self->refresh_miner_ids);

  g_cancellable_cancel (self->create_window_cancellable);
  g_clear_object (&self->create_window_cancellable);
  self->create_window_cancellable = g_cancellable_new ();

  photos_application_stop_miners (self);

  if (self->extract_priority != NULL)
    {
      g_application_hold (G_APPLICATION (self));
      tracker_extract_priority_call_clear_rdf_types (self->extract_priority,
                                                     NULL,
                                                     photos_application_tracker_clear_rdf_types,
                                                     self);
      g_clear_object (&self->extract_priority);
    }
}


static void
photos_application_gom_miner (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosApplicationCreateData *data = (PhotosApplicationCreateData *) user_data;
  PhotosApplication *self = data->application;
  g_autoptr (GomMiner) miner = NULL;

  {
    g_autoptr (GError) error = NULL;

    miner = gom_miner_proxy_new_for_bus_finish (res, &error);
    if (error != NULL)
      {
        if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
          {
            goto out;
          }
        else
          {
            g_warning ("Unable to create GomMiner proxy for %s: %s", data->miner_name, error->message);
            goto maybe_continue;
          }
      }
  }

  g_object_set_data_full (G_OBJECT (miner), "provider-type", g_strdup (data->extension_name), g_free);
  self->miners = g_list_prepend (self->miners, g_object_ref (miner));

 maybe_continue:
  if (self->create_miners_count == 1)
    photos_application_start_miners_second (self);

 out:
  self->create_miners_count--;
  photos_application_create_data_free (data);
}


static void
photos_application_create_miners (PhotosApplication *self)
{
  GIOExtensionPoint *extension_point;
  GList *extensions;
  GList *l;

  extension_point = g_io_extension_point_lookup (PHOTOS_BASE_ITEM_EXTENSION_POINT_NAME);
  extensions = g_io_extension_point_get_extensions (extension_point);
  for (l = extensions; l != NULL; l = l->next)
    {
      GIOExtension *extension = (GIOExtension *) l->data;
      PhotosApplicationCreateData *data;
      PhotosBaseItemClass *base_item_class; /* TODO: use g_autoptr */

      base_item_class = PHOTOS_BASE_ITEM_CLASS (g_io_extension_ref_class (extension));
      if (base_item_class->miner_name != NULL && base_item_class->miner_object_path != NULL)
        {
          const gchar *extension_name;

          extension_name = g_io_extension_get_name (extension);
          data = photos_application_create_data_new (self, extension_name, base_item_class->miner_name);
          gom_miner_proxy_new_for_bus (G_BUS_TYPE_SESSION,
                                       G_DBUS_PROXY_FLAGS_NONE,
                                       base_item_class->miner_name,
                                       base_item_class->miner_object_path,
                                       self->create_window_cancellable,
                                       photos_application_gom_miner,
                                       data);
          self->create_miners_count++;
        }

      g_type_class_unref (base_item_class);
    }
}


static gboolean
photos_application_gegl_init_fishes_idle (gpointer user_data)
{
  PhotosApplication *self = PHOTOS_APPLICATION (user_data);

  self->init_fishes_id = 0;
  photos_gegl_init_fishes ();
  return G_SOURCE_REMOVE;
}


static void
photos_application_tracker_set_rdf_types (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosApplication *self = PHOTOS_APPLICATION (user_data);
  TrackerExtractPriority *extract_priority = TRACKER_EXTRACT_PRIORITY (source_object);

  {
    g_autoptr (GError) error = NULL;

    if (!tracker_extract_priority_call_set_rdf_types_finish (extract_priority, res, &error))
      {
        if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
          g_warning ("Unable to call SetRdfTypes: %s", error->message);

        goto out;
      }
  }

 out:
  g_application_release (G_APPLICATION (self));
}


static void
photos_application_tracker_extract_priority (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosApplication *self = PHOTOS_APPLICATION (user_data);
  const gchar *const rdf_types[] = {"nfo:Image", NULL};

  {
    g_autoptr (GError) error = NULL;

    self->extract_priority = tracker_extract_priority_proxy_new_for_bus_finish (res, &error);
    if (error != NULL)
      {
        if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
          g_warning ("Unable to create TrackerExtractPriority proxy: %s", error->message);

        goto out;
      }
  }

  g_application_hold (G_APPLICATION (self));
  tracker_extract_priority_call_set_rdf_types (self->extract_priority,
                                               rdf_types,
                                               self->create_window_cancellable,
                                               photos_application_tracker_set_rdf_types,
                                               self);

 out:
  g_application_release (G_APPLICATION (self));
}


static gboolean
photos_application_create_window (PhotosApplication *self)
{
  gboolean gegl_sanity_checked;
  gboolean gexiv2_initialized;
  gboolean gexiv2_registered_namespace;

  if (self->main_window != NULL)
    return TRUE;

  gegl_sanity_checked = photos_gegl_sanity_check ();
  g_return_val_if_fail (gegl_sanity_checked, FALSE);

  gexiv2_initialized = gexiv2_initialize ();
  g_return_val_if_fail (gexiv2_initialized, FALSE);

  gexiv2_registered_namespace = gexiv2_metadata_register_xmp_namespace ("http://www.gnome.org/xmp", "gnome");
  g_return_val_if_fail (gexiv2_registered_namespace, FALSE);

  self->main_window = photos_main_window_new (GTK_APPLICATION (self));
  g_signal_connect_object (self->main_window,
                           "delete-event",
                           G_CALLBACK (photos_application_delete_event),
                           self,
                           G_CONNECT_AFTER | G_CONNECT_SWAPPED);
  g_signal_connect_swapped (self->main_window, "destroy", G_CALLBACK (photos_application_destroy), self);

  self->main_window_deleted = FALSE;
  self->factory = photos_thumbnail_factory_dup_singleton (NULL, NULL);

  if (self->init_fishes_id == 0)
    self->init_fishes_id = g_idle_add (photos_application_gegl_init_fishes_idle, self);

  g_application_hold (G_APPLICATION (self));
  tracker_extract_priority_proxy_new_for_bus (G_BUS_TYPE_SESSION,
                                              G_DBUS_PROXY_FLAGS_NONE,
                                              "org.freedesktop.Tracker1.Miner.Extract",
                                              "/org/freedesktop/Tracker1/Extract/Priority",
                                              self->create_window_cancellable,
                                              photos_application_tracker_extract_priority,
                                              self);

  photos_application_start_miners (self);
  return TRUE;
}


static void
photos_application_activate_item (PhotosApplication *self, GObject *item)
{
  PhotosWindowMode mode;
  gboolean can_activate;

  mode = photos_mode_controller_get_window_mode (self->state->mode_cntrlr);

  switch (mode)
    {
    case PHOTOS_WINDOW_MODE_NONE:
    case PHOTOS_WINDOW_MODE_COLLECTION_VIEW:
    case PHOTOS_WINDOW_MODE_COLLECTIONS:
    case PHOTOS_WINDOW_MODE_FAVORITES:
    case PHOTOS_WINDOW_MODE_IMPORT:
    case PHOTOS_WINDOW_MODE_OVERVIEW:
    case PHOTOS_WINDOW_MODE_SEARCH:
      can_activate = TRUE;
      break;

    case PHOTOS_WINDOW_MODE_EDIT:
    case PHOTOS_WINDOW_MODE_PREVIEW:
      can_activate = FALSE;
      break;

    default:
      g_assert_not_reached ();
      break;
    }

  if (!can_activate)
    {
      g_return_if_fail (PHOTOS_IS_MAIN_WINDOW (self->main_window));
      return;
    }

  if (!photos_application_create_window (self))
    return;

  switch (mode)
    {
    case PHOTOS_WINDOW_MODE_NONE:
      photos_mode_controller_set_window_mode (self->state->mode_cntrlr, PHOTOS_WINDOW_MODE_OVERVIEW);
      break;

    case PHOTOS_WINDOW_MODE_COLLECTION_VIEW:
    case PHOTOS_WINDOW_MODE_IMPORT:
      photos_mode_controller_go_back (self->state->mode_cntrlr);
      break;

    case PHOTOS_WINDOW_MODE_COLLECTIONS:
    case PHOTOS_WINDOW_MODE_FAVORITES:
    case PHOTOS_WINDOW_MODE_OVERVIEW:
    case PHOTOS_WINDOW_MODE_SEARCH:
      break;

    case PHOTOS_WINDOW_MODE_EDIT:
    case PHOTOS_WINDOW_MODE_PREVIEW:
    default:
      g_assert_not_reached ();
      break;
    }

  photos_base_manager_set_active_object (self->state->item_mngr, item);
  g_application_activate (G_APPLICATION (self));

  /* TODO: Forward the search terms when we exit the preview */
}


static void
photos_application_activate_query_executed (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosApplication *self = PHOTOS_APPLICATION (user_data);
  GObject *item;
  PhotosSingleItemJob *job = PHOTOS_SINGLE_ITEM_JOB (source_object);
  TrackerSparqlCursor *cursor = NULL; /* TODO: use g_autoptr */
  const gchar *identifier;

  {
    g_autoptr (GError) error = NULL;

    cursor = photos_single_item_job_finish (job, res, &error);
    if (error != NULL)
      {
        g_warning ("Unable to query single item: %s", error->message);
        goto out;
      }
  }

  if (cursor == NULL)
    goto out;

  photos_item_manager_add_item (PHOTOS_ITEM_MANAGER (self->state->item_mngr), G_TYPE_NONE, cursor, TRUE);

  identifier = tracker_sparql_cursor_get_string (cursor, PHOTOS_QUERY_COLUMNS_URN, NULL);
  item = photos_base_manager_get_object_by_id (self->state->item_mngr, identifier);

  photos_application_activate_item (self, item);

 out:
  g_clear_object (&cursor);
  g_application_release (G_APPLICATION (self));
}


static void
photos_application_activate_result (PhotosApplication *self,
                                    const gchar *identifier,
                                    const gchar *const *terms,
                                    guint timestamp)
{
  GObject *item;

  photos_debug (PHOTOS_DEBUG_APPLICATION, "PhotosApplication::activate_result");

  self->activation_timestamp = timestamp;

  item = photos_base_manager_get_object_by_id (self->state->item_mngr, identifier);
  if (item != NULL)
    photos_application_activate_item (self, item);
  else
    {
      g_autoptr (PhotosSingleItemJob) job = NULL;

      job = photos_single_item_job_new (identifier);
      g_application_hold (G_APPLICATION (self));
      photos_single_item_job_run (job,
                                  self->state,
                                  PHOTOS_QUERY_FLAGS_UNFILTERED,
                                  NULL,
                                  photos_application_activate_query_executed,
                                  self);
    }
}


static void
photos_application_can_fullscreen_changed (PhotosApplication *self)
{
  gboolean can_fullscreen;

  can_fullscreen = photos_mode_controller_get_can_fullscreen (self->state->mode_cntrlr);
  g_simple_action_set_enabled (self->fs_action, can_fullscreen);
}


static void
photos_application_edit_cancel_process (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosApplication *self = PHOTOS_APPLICATION (user_data);
  PhotosBaseItem *item = PHOTOS_BASE_ITEM (source_object);

  {
    g_autoptr (GError) error = NULL;

    if (!photos_base_item_pipeline_revert_finish (item, res, &error))
      g_warning ("Unable to process item: %s", error->message);
  }

  /* Go back, no matter what. The revert can only fail in very
   * exceptional circumstances (currently I don't know of any). So,
   * if and when it fails, we don't want to be stuck inside the EDIT
   * mode.
   */
  photos_mode_controller_go_back (self->state->mode_cntrlr);
  g_application_release (G_APPLICATION (self));
}


static void
photos_application_edit_cancel (PhotosApplication *self)
{
  PhotosBaseItem *item;

  item = PHOTOS_BASE_ITEM (photos_base_manager_get_active_object (self->state->item_mngr));
  g_return_if_fail (item != NULL);

  g_application_hold (G_APPLICATION (self));
  photos_base_item_pipeline_revert_async (item, NULL, photos_application_edit_cancel_process, self);
}


static void
photos_application_edit_current (PhotosApplication *self)
{
  PhotosBaseItem *item;

  item = PHOTOS_BASE_ITEM (photos_base_manager_get_active_object (self->state->item_mngr));
  g_return_if_fail (item != NULL);

  g_action_activate (G_ACTION (self->zoom_best_fit_action), NULL);

  photos_base_item_pipeline_snapshot (item);
  photos_mode_controller_set_window_mode (self->state->mode_cntrlr, PHOTOS_WINDOW_MODE_EDIT);
}


static void
photos_application_edit_revert_pipeline_save (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosApplication *self = PHOTOS_APPLICATION (user_data);
  PhotosBaseItem *item = PHOTOS_BASE_ITEM (source_object);

  {
    g_autoptr (GError) error = NULL;

    if (!photos_base_item_pipeline_save_finish (item, res, &error))
      g_warning ("Unable to save pipeline: %s", error->message);
  }

  g_application_release (G_APPLICATION (self));
}


static void
photos_application_edit_revert_revert (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosApplication *self = PHOTOS_APPLICATION (user_data);
  PhotosBaseItem *item = PHOTOS_BASE_ITEM (source_object);

  {
    g_autoptr (GError) error = NULL;

    if (!photos_base_item_pipeline_revert_finish (item, res, &error))
      {
        g_warning ("Unable to process item: %s", error->message);
        goto out;
      }
  }

  g_application_hold (G_APPLICATION (self));
  photos_base_item_pipeline_save_async (item, NULL, photos_application_edit_revert_pipeline_save, self);

 out:
  g_application_release (G_APPLICATION (self));
}


static void
photos_application_edit_revert (PhotosApplication *self, GVariant *parameter)
{
  PhotosBaseItem *item;
  const gchar *id;

  id = g_variant_get_string (parameter, NULL);
  g_return_if_fail (id != NULL && id[0] != '\0');

  item = PHOTOS_BASE_ITEM (photos_base_manager_get_object_by_id (self->state->item_mngr, id));
  g_return_if_fail (item != NULL);

  g_application_hold (G_APPLICATION (self));
  photos_base_item_pipeline_revert_async (item, NULL, photos_application_edit_revert_revert, self);
}


static void
photos_application_fullscreen (PhotosApplication *self)
{
  photos_mode_controller_toggle_fullscreen (self->state->mode_cntrlr);
}


static PhotosSearchContextState *
photos_application_get_state (PhotosSearchContext *context)
{
  PhotosApplication *self = PHOTOS_APPLICATION (context);
  return self->state;
}


static void
photos_application_import_index_file (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosApplication *self;
  g_autoptr (GFile) file = G_FILE (user_data);
  TrackerMinerManager *manager = TRACKER_MINER_MANAGER (source_object);

  self = PHOTOS_APPLICATION (g_application_get_default ());

  {
    g_autoptr (GError) error = NULL;

    if (!tracker_miner_manager_index_file_for_process_finish (manager, res, &error))
      {
        g_autofree gchar *uri = NULL;

        uri = g_file_get_uri (file);
        g_warning ("Unable to index %s: %s", uri, error->message);
      }
  }

  g_application_unmark_busy (G_APPLICATION (self));
  g_application_release (G_APPLICATION (self));
}


static void
photos_application_import_copy_next_file (PhotosApplication *self, gpointer user_data)
{
  g_autoptr (PhotosApplicationImportData) data = (PhotosApplicationImportData *) user_data;
  g_autoptr (GFile) destination = NULL;
  GFile *source;
  g_autofree gchar *destination_uri = NULL;
  g_autofree gchar *filename = NULL;
  g_autofree gchar *source_uri = NULL;

  g_assert_nonnull (data->files);
  g_assert_nonnull (data->files->data);
  g_object_unref (data->files->data);
  data->files = g_list_remove_link (data->files, data->files);

  if (data->files == NULL)
    {
      photos_debug (PHOTOS_DEBUG_IMPORT, "Finished importing");
      goto out;
    }

  source = G_FILE (data->files->data);
  filename = g_file_get_basename (source);
  destination = g_file_get_child (data->import_sub_dir, filename);

  destination_uri = g_file_get_uri (destination);
  source_uri = g_file_get_uri (source);
  photos_debug (PHOTOS_DEBUG_IMPORT, "Importing %s to %s", source_uri, destination_uri);

  g_application_mark_busy (G_APPLICATION (self));
  photos_glib_file_copy_async (source,
                               destination,
                               G_FILE_COPY_TARGET_DEFAULT_PERMS,
                               G_PRIORITY_DEFAULT,
                               NULL,
                               photos_application_import_file_copy,
                               g_steal_pointer (&data));

 out:
  return;
}


static void
photos_application_import_single_item (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  g_autoptr (PhotosApplicationImportData) data = (PhotosApplicationImportData *) user_data;
  PhotosApplication *self = data->application;
  PhotosBaseItem *collection;
  PhotosSingleItemJob *job = PHOTOS_SINGLE_ITEM_JOB (source_object);
  TrackerSparqlCursor *cursor = NULL; /* TODO: use g_autoptr */

  {
    g_autoptr (GError) error = NULL;

    cursor = photos_single_item_job_finish (job, res, &error);
    if (error != NULL)
      {
        g_warning ("Unable to query single item: %s", error->message);
        goto out;
      }
  }

  if (cursor == NULL)
    goto out;

  photos_item_manager_add_item_for_mode (PHOTOS_ITEM_MANAGER (self->state->item_mngr),
                                         G_TYPE_NONE,
                                         PHOTOS_WINDOW_MODE_COLLECTIONS,
                                         cursor);

  collection = PHOTOS_BASE_ITEM (photos_base_manager_get_object_by_id (self->state->item_mngr, data->collection_urn));
  if (collection != NULL && data->collection == NULL)
    {
      photos_base_item_mark_busy (collection);
      data->collection = g_object_ref (collection);
    }

  photos_application_import_copy_next_file (self, g_steal_pointer (&data));

 out:
  g_application_unmark_busy (G_APPLICATION (self));
}


static void
photos_application_import_set_collection (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  g_autoptr (PhotosApplicationImportData) data = (PhotosApplicationImportData *) user_data;
  PhotosApplication *self = data->application;
  PhotosBaseItem *collection;
  PhotosSetCollectionJob *set_collection_job = PHOTOS_SET_COLLECTION_JOB (source_object);

  {
    g_autoptr (GError) error = NULL;

    if (!photos_set_collection_job_finish (set_collection_job, res, &error))
      {
        g_autofree gchar *uri = NULL;

        uri = g_file_get_uri (data->destination);
        g_warning ("Unable to set collection for %s: %s", uri, error->message);
        goto out;
      }
  }

  collection = PHOTOS_BASE_ITEM (photos_base_manager_get_object_by_id (self->state->item_mngr, data->collection_urn));
  if (collection == NULL)
    {
      g_autoptr (PhotosSingleItemJob) single_item_job = NULL;

      single_item_job = photos_single_item_job_new (data->collection_urn);

      g_application_mark_busy (G_APPLICATION (self));
      photos_single_item_job_run (single_item_job,
                                  self->state,
                                  PHOTOS_QUERY_FLAGS_COLLECTIONS,
                                  NULL,
                                  photos_application_import_single_item,
                                  g_steal_pointer (&data));
    }
  else
    {
      if (data->collection == NULL)
        {
          photos_base_item_mark_busy (collection);
          data->collection = g_object_ref (collection);
        }

      photos_base_item_refresh (collection);
      photos_application_import_copy_next_file (self, g_steal_pointer (&data));
    }

 out:
  g_application_unmark_busy (G_APPLICATION (self));
}


static void
photos_application_import_wait_for_file (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  g_autoptr (PhotosApplicationImportData) data = (PhotosApplicationImportData *) user_data;
  PhotosApplication *self = data->application;
  g_autoptr (GList) urns = NULL;
  PhotosItemManager *item_mngr = PHOTOS_ITEM_MANAGER (source_object);
  g_autoptr (PhotosSetCollectionJob) job = NULL;
  g_autofree gchar *id = NULL;

  {
    g_autoptr (GError) error = NULL;

    id = photos_item_manager_wait_for_file_finish (item_mngr, res, &error);
    if (error != NULL)
      {
        g_autofree gchar *uri = NULL;

        uri = g_file_get_uri (data->destination);
        g_warning ("Unable to detect %s: %s", uri, error->message);
        goto out;
      }
  }

  photos_debug (PHOTOS_DEBUG_IMPORT, "Adding item %s to collection %s", id, data->collection_urn);

  job = photos_set_collection_job_new (data->collection_urn, TRUE);
  urns = g_list_prepend (urns, id);

  g_application_mark_busy (G_APPLICATION (self));
  photos_set_collection_job_run (job,
                                 self->state,
                                 urns,
                                 NULL,
                                 photos_application_import_set_collection,
                                 g_steal_pointer (&data));

 out:
  g_application_unmark_busy (G_APPLICATION (self));
}


static void
photos_application_import_file_copy (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  g_autoptr (PhotosApplicationImportData) data = (PhotosApplicationImportData *) user_data;
  PhotosApplication *self = data->application;
  g_autoptr (GFile) destination = NULL;
  GFile *source = G_FILE (source_object);
  TrackerMinerManager *manager = data->manager;

  {
    g_autoptr (GError) error = NULL;

    destination = photos_glib_file_copy_finish (source, res, &error);
    if (error != NULL)
      {
        g_autofree gchar *uri = NULL;

        uri = g_file_get_uri (source);
        g_warning ("Unable to copy %s: %s", uri, error->message);

        if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NO_SPACE))
          goto out;
      }
  }

  if (destination == NULL)
    {
      photos_application_import_copy_next_file (self, g_steal_pointer (&data));
    }
  else
    {
      g_autofree gchar *destination_uri = NULL;

      g_assert_true (G_IS_FILE (destination));
      g_set_object (&data->destination, destination);

      destination_uri = g_file_get_uri (destination);
      photos_debug (PHOTOS_DEBUG_IMPORT, "Indexing after import %s", destination_uri);

      g_application_mark_busy (G_APPLICATION (self));
      photos_item_manager_wait_for_file_async (PHOTOS_ITEM_MANAGER (self->state->item_mngr),
                                               destination,
                                               NULL,
                                               photos_application_import_wait_for_file,
                                               g_steal_pointer (&data));

      g_application_hold (G_APPLICATION (self));
      g_application_mark_busy (G_APPLICATION (self));
      tracker_miner_manager_index_file_for_process_async (manager,
                                                          destination,
                                                          NULL,
                                                          photos_application_import_index_file,
                                                          g_object_ref (destination));
    }

 out:
  g_application_unmark_busy (G_APPLICATION (self));
}


static void
photos_application_import_copy_first_file (PhotosApplication *self, gpointer user_data)
{
  g_autoptr (PhotosApplicationImportData) data = (PhotosApplicationImportData *) user_data;
  g_autoptr (GDateTime) date_created_latest = NULL;
  g_autoptr (GFile) destination = NULL;
  GFile *source;
  const gchar *pictures_path;
  g_autofree gchar *date_created_latest_str = NULL;
  g_autofree gchar *destination_uri = NULL;
  g_autofree gchar *filename = NULL;
  g_autofree gchar *import_sub_path = NULL;
  g_autofree gchar *source_uri = NULL;

  date_created_latest = g_date_time_new_from_unix_local (data->ctime_latest);

  /* Translators: this is the default sub-directory where photos will
   * be imported.
   */
  date_created_latest_str = g_date_time_format (date_created_latest, _("%-d %B %Y"));

  pictures_path = g_get_user_special_dir (G_USER_DIRECTORY_PICTURES);
  import_sub_path = g_build_filename (pictures_path, PHOTOS_IMPORT_SUBPATH, date_created_latest_str, NULL);
  g_mkdir_with_parents (import_sub_path, 0777);

  g_clear_object (&data->import_sub_dir);
  data->import_sub_dir = g_file_new_for_path (import_sub_path);

  source = G_FILE (data->files->data);
  filename = g_file_get_basename (source);
  destination = g_file_get_child (data->import_sub_dir, filename);

  destination_uri = g_file_get_uri (destination);
  source_uri = g_file_get_uri (source);
  photos_debug (PHOTOS_DEBUG_IMPORT, "Importing %s to %s", source_uri, destination_uri);

  g_application_mark_busy (G_APPLICATION (self));
  photos_glib_file_copy_async (source,
                               destination,
                               G_FILE_COPY_TARGET_DEFAULT_PERMS,
                               G_PRIORITY_DEFAULT,
                               NULL,
                               photos_application_import_file_copy,
                               g_steal_pointer (&data));
}


static void
photos_application_import_create_collection_executed (GObject *source_object,
                                                      GAsyncResult *res,
                                                      gpointer user_data)
{
  g_autoptr (PhotosApplicationImportData) data = (PhotosApplicationImportData *) user_data;
  PhotosApplication *self = data->application;
  PhotosCreateCollectionJob *col_job = PHOTOS_CREATE_COLLECTION_JOB (source_object);
  g_autofree gchar *created_urn = NULL;

  {
    g_autoptr (GError) error = NULL;

    created_urn = photos_create_collection_job_finish (col_job, res, &error);
    if (error != NULL)
      {
        g_warning ("Unable to create collection: %s", error->message);
        goto out;
      }
  }

  g_assert_null (data->collection_urn);
  data->collection_urn = g_steal_pointer (&created_urn);

  photos_application_import_copy_first_file (self, g_steal_pointer (&data));

 out:
  g_application_unmark_busy (G_APPLICATION (self));
  return;
}


static void
photos_application_import_response (GtkDialog *dialog, gint response_id, gpointer user_data)
{
  g_autoptr (PhotosApplicationImportData) data = (PhotosApplicationImportData *) user_data;
  PhotosApplication *self = data->application;
  PhotosBaseItem *collection;
  const gchar *name;
  g_autofree gchar *identifier_tag = NULL;

  g_assert_true (PHOTOS_IS_IMPORT_DIALOG (dialog));

  if (response_id != GTK_RESPONSE_OK)
    goto out;

  collection = photos_import_dialog_get_collection (PHOTOS_IMPORT_DIALOG (dialog));
  name = photos_import_dialog_get_name (PHOTOS_IMPORT_DIALOG (dialog), &identifier_tag);
  g_assert_true ((PHOTOS_IS_BASE_ITEM (collection) && name == NULL) || (collection == NULL && name != NULL));

  if (name != NULL)
    {
      g_autoptr (PhotosCreateCollectionJob) col_job = NULL;

      col_job = photos_create_collection_job_new (name, identifier_tag);

      g_application_mark_busy (G_APPLICATION (self));
      photos_create_collection_job_run (col_job,
                                        NULL,
                                        photos_application_import_create_collection_executed,
                                        g_steal_pointer (&data));
    }
  else if (collection != NULL)
    {
      const gchar *id;

      id = photos_filterable_get_id (PHOTOS_FILTERABLE (collection));

      g_assert_null (data->collection_urn);
      data->collection_urn = g_strdup (id);

      photos_application_import_copy_first_file (self, g_steal_pointer (&data));
    }
  else
    {
      g_assert_not_reached ();
    }

  g_action_activate (G_ACTION (self->import_cancel_action), NULL);
  photos_mode_controller_set_window_mode (self->state->mode_cntrlr, PHOTOS_WINDOW_MODE_COLLECTIONS);

 out:
  gtk_widget_destroy (GTK_WIDGET (dialog));
}


static void
photos_application_import (PhotosApplication *self)
{
  GList *files = NULL;
  GList *l;
  GList *selection;
  GMount *mount;
  GtkWidget *dialog;
  g_autoptr (PhotosApplicationImportData) data = NULL;
  PhotosSource *source;
  TrackerMinerManager *manager = NULL; /* TODO: use g_autoptr */
  gint64 ctime_latest = -1;

  source = PHOTOS_SOURCE (photos_base_manager_get_active_object (self->state->src_mngr));
  g_return_if_fail (PHOTOS_IS_SOURCE (source));

  mount = photos_source_get_mount (source);
  g_return_if_fail (G_IS_MOUNT (mount));

  g_return_if_fail (photos_utils_get_selection_mode ());

  selection = photos_selection_controller_get_selection (self->sel_cntrlr);
  g_return_if_fail (selection != NULL);

  {
    g_autoptr (GError) error = NULL;

    manager = tracker_miner_manager_new_full (FALSE, &error);
    if (error != NULL)
      {
        g_warning ("Unable to create a TrackerMinerManager, importing from attached devices won't work: %s",
                   error->message);
        goto out;
      }
  }

  for (l = selection; l != NULL; l = l->next)
    {
      g_autoptr (GFile) file = NULL;
      PhotosBaseItem *item;
      const gchar *uri;
      const gchar *urn = (gchar *) l->data;
      gint64 ctime;

      item = PHOTOS_BASE_ITEM (photos_base_manager_get_object_by_id (self->state->item_mngr, urn));

      ctime = photos_base_item_get_date_created (item);
      if (ctime < 0)
        ctime = photos_base_item_get_mtime (item);

      if (ctime > ctime_latest)
        ctime_latest = ctime;

      uri = photos_base_item_get_uri (item);
      file = g_file_new_for_uri (uri);
      files = g_list_prepend (files, g_object_ref (file));
    }

  g_assert_cmpint (ctime_latest, >=, 0);

  dialog = photos_import_dialog_new (GTK_WINDOW (self->main_window), ctime_latest);
  gtk_widget_show_all (dialog);

  data = photos_application_import_data_new (self, manager, files, ctime_latest);
  g_signal_connect (dialog,
                    "response",
                    G_CALLBACK (photos_application_import_response),
                    g_steal_pointer (&data));

 out:
  g_clear_object (&manager);
  g_list_free_full (files, g_object_unref);
}


static void
photos_application_import_cancel (PhotosApplication *self)
{
  PhotosWindowMode mode;

  photos_base_manager_set_active_object_by_id (self->state->src_mngr, PHOTOS_SOURCE_STOCK_ALL);

  mode = photos_mode_controller_get_window_mode (self->state->mode_cntrlr);
  g_return_if_fail (mode != PHOTOS_WINDOW_MODE_IMPORT);
  g_return_if_fail (mode == PHOTOS_WINDOW_MODE_COLLECTIONS
                    || mode == PHOTOS_WINDOW_MODE_FAVORITES
                    || mode == PHOTOS_WINDOW_MODE_OVERVIEW);
}


static void
photos_application_items_changed (PhotosApplication *self)
{
  photos_application_actions_update (self);
}


static void
photos_application_launch_search (PhotosApplication *self, const gchar* const *terms, guint timestamp)
{
  GVariant *state;
  PhotosWindowMode mode;
  gboolean can_launch;
  g_autofree gchar *str = NULL;

  photos_debug (PHOTOS_DEBUG_APPLICATION, "PhotosApplication::launch_search");

  mode = photos_mode_controller_get_window_mode (self->state->mode_cntrlr);

  switch (mode)
    {
    case PHOTOS_WINDOW_MODE_NONE:
    case PHOTOS_WINDOW_MODE_COLLECTION_VIEW:
    case PHOTOS_WINDOW_MODE_COLLECTIONS:
    case PHOTOS_WINDOW_MODE_FAVORITES:
    case PHOTOS_WINDOW_MODE_IMPORT:
    case PHOTOS_WINDOW_MODE_OVERVIEW:
    case PHOTOS_WINDOW_MODE_SEARCH:
      can_launch = TRUE;
      break;

    case PHOTOS_WINDOW_MODE_EDIT:
    case PHOTOS_WINDOW_MODE_PREVIEW:
      can_launch = FALSE;
      break;

    default:
      g_assert_not_reached ();
      break;
    }

  if (!can_launch)
    {
      g_return_if_fail (PHOTOS_IS_MAIN_WINDOW (self->main_window));
      return;
    }

  if (!photos_application_create_window (self))
    return;

  switch (mode)
    {
    case PHOTOS_WINDOW_MODE_NONE:
      photos_mode_controller_set_window_mode (self->state->mode_cntrlr, PHOTOS_WINDOW_MODE_OVERVIEW);
      break;

    case PHOTOS_WINDOW_MODE_COLLECTION_VIEW:
    case PHOTOS_WINDOW_MODE_COLLECTIONS:
    case PHOTOS_WINDOW_MODE_FAVORITES:
    case PHOTOS_WINDOW_MODE_OVERVIEW:
    case PHOTOS_WINDOW_MODE_SEARCH:
      break;

    case PHOTOS_WINDOW_MODE_IMPORT:
      photos_mode_controller_go_back (self->state->mode_cntrlr);
      break;

    case PHOTOS_WINDOW_MODE_EDIT:
    case PHOTOS_WINDOW_MODE_PREVIEW:
    default:
      g_assert_not_reached ();
      break;
    }

  str = g_strjoinv (" ", (gchar **) terms);
  photos_search_controller_set_string (self->state->srch_cntrlr, str);

  state = g_variant_new ("b", TRUE);
  g_action_change_state (G_ACTION (self->search_action), state);

  self->activation_timestamp = timestamp;
  g_application_activate (G_APPLICATION (self));
}


static void
photos_application_load_changed (PhotosApplication *self)
{
  photos_application_actions_update (self);
}


static void
photos_application_open_current (PhotosApplication *self)
{
  PhotosBaseItem *item;
  GVariant *new_state;
  guint32 time;

  time = gtk_get_current_event_time ();

  if (photos_utils_get_selection_mode ())
    {
      GList *l;
      GList *selection;

      selection = photos_selection_controller_get_selection (self->sel_cntrlr);
      for (l = selection; l != NULL; l = l->next)
        {
          const gchar *urn = (gchar *) l->data;

          item = PHOTOS_BASE_ITEM (photos_base_manager_get_object_by_id (self->state->item_mngr, urn));
          photos_base_item_open (item, GTK_WINDOW (self->main_window), time);
        }
    }
  else
    {
      item = PHOTOS_BASE_ITEM (photos_base_manager_get_active_object (self->state->item_mngr));
      g_return_if_fail (item != NULL);

      photos_base_item_open (item, GTK_WINDOW (self->main_window), time);
    }

  new_state = g_variant_new ("b", FALSE);
  g_action_change_state (G_ACTION (self->selection_mode_action), new_state);
}


static void
photos_application_print_current (PhotosApplication *self)
{
  PhotosBaseItem *item;

  item = photos_application_get_selection_or_active_item (self);
  g_return_if_fail (item != NULL);

  photos_base_item_print (item, self->main_window);
}


static void
photos_application_properties_pipeline_save (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosApplication *self = PHOTOS_APPLICATION (user_data);
  PhotosBaseItem *item = PHOTOS_BASE_ITEM (source_object);

  {
    g_autoptr (GError) error = NULL;

    if (!photos_base_item_pipeline_save_finish (item, res, &error))
      g_warning ("Unable to save pipeline: %s", error->message);
  }

  g_application_release (G_APPLICATION (self));
}


static void
photos_application_properties_revert_to_original (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosApplication *self = PHOTOS_APPLICATION (user_data);
  PhotosBaseItem *item = PHOTOS_BASE_ITEM (source_object);

  {
    g_autoptr (GError) error = NULL;

    if (!photos_base_item_pipeline_revert_to_original_finish (item, res, &error))
      {
        g_warning ("Unable to revert to original: %s", error->message);
        goto out;
      }
  }

  g_application_hold (G_APPLICATION (self));
  photos_base_item_pipeline_save_async (item, NULL, photos_application_properties_pipeline_save, self);

 out:
  g_application_release (G_APPLICATION (self));
}


static void
photos_application_properties_discard_all_edits (PhotosApplication *self)
{
  PhotosBaseItem *item;

  item = photos_application_get_selection_or_active_item (self);
  g_return_if_fail (item != NULL);

  g_application_hold (G_APPLICATION (self));
  photos_base_item_pipeline_revert_to_original_async (item,
                                                      NULL,
                                                      photos_application_properties_revert_to_original,
                                                      self);
}


static void
photos_application_properties_response (GtkDialog *dialog, gint response_id, gpointer user_data)
{
  PhotosApplication *self = PHOTOS_APPLICATION (user_data);
  GVariant *new_state;

  gtk_widget_destroy (GTK_WIDGET (dialog));

  new_state = g_variant_new ("b", FALSE);
  g_action_change_state (G_ACTION (self->selection_mode_action), new_state);
}


static void
photos_application_properties (PhotosApplication *self)
{
  GtkWidget *dialog;
  PhotosBaseItem *item;
  const gchar *id;

  item = photos_application_get_selection_or_active_item (self);
  g_return_if_fail (item != NULL);

  id = photos_filterable_get_id (PHOTOS_FILTERABLE (item));
  dialog = photos_properties_dialog_new (GTK_WINDOW (self->main_window), id);
  gtk_widget_show_all (dialog);
  g_signal_connect_swapped (dialog,
                            "discard-all-edits",
                            G_CALLBACK (photos_application_properties_discard_all_edits),
                            self);
  g_signal_connect (dialog, "response", G_CALLBACK (photos_application_properties_response), self);
}


static gboolean
photos_application_refresh_miner_timeout (gpointer user_data)
{
  PhotosApplicationRefreshData *data = (PhotosApplicationRefreshData *) user_data;
  PhotosApplication *self = data->application;

  g_hash_table_remove (self->refresh_miner_ids, data->miner);
  photos_application_refresh_miner_now (self, data->miner);
  return G_SOURCE_REMOVE;
}


static void
photos_application_refresh_db (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosApplication *self = PHOTOS_APPLICATION (user_data);
  GList *miner_link;
  g_autoptr (GomMiner) miner = GOM_MINER (source_object);
  PhotosApplicationRefreshData *data;
  const gchar *name;
  gpointer refresh_miner_id_data;
  guint refresh_miner_id;

  name = g_dbus_proxy_get_name (G_DBUS_PROXY (miner));
  photos_debug (PHOTOS_DEBUG_NETWORK, "Finished RefreshDB for %s (%p)", name, miner);

  refresh_miner_id_data = g_hash_table_lookup (self->refresh_miner_ids, miner);
  g_assert_null (refresh_miner_id_data);

  miner_link = g_list_find (self->miners_running, miner);
  g_assert_nonnull (miner_link);

  self->miners_running = g_list_remove_link (self->miners_running, miner_link);
  g_signal_emit (self, signals[MINERS_CHANGED], 0, self->miners_running);

  {
    g_autoptr (GError) error = NULL;

    if (!gom_miner_call_refresh_db_finish (miner, res, &error))
      {
        if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
          g_warning ("Unable to update the cache: %s", error->message);

        goto out;
      }
  }

  data = photos_application_refresh_data_new (self, miner);
  refresh_miner_id = g_timeout_add_seconds_full (G_PRIORITY_DEFAULT,
                                                 MINER_REFRESH_TIMEOUT,
                                                 photos_application_refresh_miner_timeout,
                                                 data,
                                                 (GDestroyNotify) photos_application_refresh_data_free);
  g_hash_table_insert (self->refresh_miner_ids, miner, GUINT_TO_POINTER (refresh_miner_id));

  photos_debug (PHOTOS_DEBUG_NETWORK, "Added timeout for %s (%p)", name, miner);

 out:
  g_application_release (G_APPLICATION (self));
}


static void
photos_application_refresh_miner_now (PhotosApplication *self, GomMiner *miner)
{
  GCancellable *cancellable;
  const gchar *const index_types[] = {"photos", NULL};
  const gchar *name;
  gpointer refresh_miner_id_data;

  if (g_getenv ("GNOME_PHOTOS_DISABLE_MINERS") != NULL)
    return;

  name = g_dbus_proxy_get_name (G_DBUS_PROXY (miner));

  if (g_list_find (self->miners_running, miner) != NULL)
    {
      photos_debug (PHOTOS_DEBUG_NETWORK, "Skipped %s (%p): already running", name, miner);
      return;
    }

  refresh_miner_id_data = g_hash_table_lookup (self->refresh_miner_ids, miner);
  if (refresh_miner_id_data != NULL)
    {
      guint refresh_miner_id = GPOINTER_TO_UINT (refresh_miner_id_data);

      g_source_remove (refresh_miner_id);
      g_hash_table_remove (self->refresh_miner_ids, miner);
      photos_debug (PHOTOS_DEBUG_NETWORK, "Removed timeout for %s (%p)", name, miner);
    }

  self->miners_running = g_list_prepend (self->miners_running, g_object_ref (miner));
  g_signal_emit (self, signals[MINERS_CHANGED], 0, self->miners_running);

  cancellable = g_cancellable_new ();
  g_object_set_data_full (G_OBJECT (miner), "cancellable", cancellable, g_object_unref);
  g_application_hold (G_APPLICATION (self));
  gom_miner_call_refresh_db (miner, index_types, cancellable, photos_application_refresh_db, self);

  photos_debug (PHOTOS_DEBUG_NETWORK, "Called RefreshDB for %s (%p)", name, miner);
}


static void
photos_application_refresh_miners (PhotosApplication *self)
{
  GList *l;

  for (l = self->miners; l != NULL; l = l->next)
    {
      GomMiner *miner = GOM_MINER (l->data);
      const gchar *provider_type;

      provider_type = g_object_get_data (G_OBJECT (miner), "provider-type");
      if (photos_source_manager_has_provider_type (PHOTOS_SOURCE_MANAGER (self->state->src_mngr), provider_type))
        photos_application_refresh_miner_now (self, miner);
    }
}


static void
photos_application_remote_display_current (PhotosApplication *self)
{
  GObject *item;
  GtkWidget *dialog;
  const gchar *urn;

  item = photos_base_manager_get_active_object (self->state->item_mngr);
  g_return_if_fail (item != NULL);

  urn = photos_filterable_get_id (PHOTOS_FILTERABLE (item));
  dialog = photos_dlna_renderers_dialog_new (GTK_WINDOW (self->main_window), urn);
  gtk_widget_show_all (dialog);
}


static void
photos_application_quit (PhotosApplication *self)
{
  gtk_window_close (GTK_WINDOW (self->main_window));
}


static void
photos_application_save_save_to_dir (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosApplication *self = PHOTOS_APPLICATION (user_data);
  PhotosBaseItem *item = PHOTOS_BASE_ITEM (source_object);
  g_autoptr (GFile) file = NULL;
  GList *items = NULL;

  {
    g_autoptr (GError) error = NULL;

    file = photos_base_item_save_to_dir_finish (item, res, &error);
    if (error != NULL)
      {
        g_warning ("Unable to save: %s", error->message);
        photos_export_notification_new_with_error (error);
        goto out;
      }
  }

  items = g_list_prepend (items, g_object_ref (item));
  photos_export_notification_new (items, file);

 out:
  g_application_release (G_APPLICATION (self));
  g_list_free_full (items, g_object_unref);
}


static void
photos_application_save_response (GtkDialog *dialog, gint response_id, gpointer user_data)
{
  PhotosApplication *self = PHOTOS_APPLICATION (user_data);
  g_autoptr (GFile) export_dir = NULL;
  g_autoptr (GFile) export_sub_dir = NULL;
  GVariant *new_state;
  PhotosBaseItem *item;
  const gchar *export_dir_name;
  const gchar *pictures_path;
  g_autofree gchar *export_path = NULL;
  gdouble zoom;

  if (response_id != GTK_RESPONSE_OK)
    goto out;

  item = photos_application_get_selection_or_active_item (self);
  g_return_if_fail (item != NULL);

  new_state = g_variant_new ("b", FALSE);
  g_action_change_state (G_ACTION (self->selection_mode_action), new_state);

  pictures_path = g_get_user_special_dir (G_USER_DIRECTORY_PICTURES);
  export_path = g_build_filename (pictures_path, PHOTOS_EXPORT_SUBPATH, NULL);
  export_dir = g_file_new_for_path (export_path);
  export_dir_name = photos_export_dialog_get_dir_name (PHOTOS_EXPORT_DIALOG (dialog));

  {
    g_autoptr (GError) error = NULL;

    export_sub_dir = g_file_get_child_for_display_name (export_dir, export_dir_name, &error);
    if (error != NULL)
      {
        g_warning ("Unable to get a child for %s: %s", export_dir_name, error->message);
        photos_export_notification_new_with_error (error);
        goto out;
      }
  }

  {
    g_autoptr (GError) error = NULL;

    if (!photos_glib_make_directory_with_parents (export_sub_dir, NULL, &error))
      {
        g_warning ("Unable to create %s: %s", export_path, error->message);
        photos_export_notification_new_with_error (error);
        goto out;
      }
  }

  zoom = photos_export_dialog_get_zoom (PHOTOS_EXPORT_DIALOG (dialog));

  g_application_hold (G_APPLICATION (self));
  photos_base_item_save_to_dir_async (item, export_sub_dir, zoom, NULL, photos_application_save_save_to_dir, self);

 out:
  gtk_widget_destroy (GTK_WIDGET (dialog));
}


static void
photos_application_save (PhotosApplication *self)
{
  GtkWidget *dialog;
  PhotosBaseItem *item;

  item = photos_application_get_selection_or_active_item (self);
  g_return_if_fail (item != NULL);
  g_return_if_fail (!photos_base_item_is_collection (item));

  dialog = photos_export_dialog_new (GTK_WINDOW (self->main_window), item);
  gtk_widget_show_all (dialog);
  g_signal_connect (dialog, "response", G_CALLBACK (photos_application_save_response), self);
}


static void
photos_application_set_bg_common_save_to_file (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosBaseItem *item = PHOTOS_BASE_ITEM (source_object);
  PhotosApplicationSetBackgroundData *data = (PhotosApplicationSetBackgroundData *) user_data;
  g_autofree gchar *path = NULL;

  {
    g_autoptr (GError) error = NULL;

    if (!photos_base_item_save_to_file_finish (item, res, &error))
      {
        g_warning ("Unable to set background: %s", error->message);
        goto out;
      }
  }

  path = g_file_get_path (data->file);

  g_settings_set_string (data->settings, DESKTOP_KEY_PICTURE_URI, path);
  g_settings_set_enum (data->settings, DESKTOP_KEY_PICTURE_OPTIONS, G_DESKTOP_BACKGROUND_STYLE_ZOOM);
  g_settings_set_enum (data->settings, DESKTOP_KEY_COLOR_SHADING_TYPE, G_DESKTOP_BACKGROUND_SHADING_SOLID);
  g_settings_set_string (data->settings, DESKTOP_KEY_PRIMARY_COLOR, "#000000000000");
  g_settings_set_string (data->settings, DESKTOP_KEY_SECONDARY_COLOR, "#000000000000");

 out:
  photos_application_set_background_data_free (data);
}


static void
photos_application_set_bg_common (PhotosApplication *self, GSettings *settings)
{
  g_autoptr (GFile) backgrounds_file = NULL;
  PhotosApplicationSetBackgroundData *data;
  PhotosBaseItem *item;
  const gchar *config_dir;
  const gchar *extension;
  const gchar *filename;
  const gchar *mime_type;
  g_autofree gchar *backgrounds_dir = NULL;
  g_autofree gchar *backgrounds_filename = NULL;
  g_autofree gchar *backgrounds_path = NULL;
  g_autofree gchar *basename = NULL;
  gint64 now;

  item = PHOTOS_BASE_ITEM (photos_base_manager_get_active_object (self->state->item_mngr));
  g_return_if_fail (item != NULL);

  config_dir = g_get_user_config_dir ();
  backgrounds_dir = g_build_filename (config_dir, PACKAGE_TARNAME, "backgrounds", NULL);
  g_mkdir_with_parents (backgrounds_dir, 0700);

  now = g_get_monotonic_time ();
  filename = photos_base_item_get_filename (item);
  basename = photos_glib_filename_strip_extension (filename);
  mime_type = photos_base_item_get_mime_type (item);
  extension = g_strcmp0 (mime_type, "image/png") == 0 ? ".png" : ".jpg";

  backgrounds_filename = g_strdup_printf ("%" G_GINT64_FORMAT "-%s%s", now, basename, extension);
  backgrounds_path = g_build_filename (backgrounds_dir, backgrounds_filename, NULL);
  backgrounds_file = g_file_new_for_path (backgrounds_path);

  data = photos_application_set_background_data_new (self, backgrounds_file, settings);
  photos_base_item_save_to_file_async (item,
                                       backgrounds_file,
                                       G_FILE_CREATE_PRIVATE | G_FILE_CREATE_REPLACE_DESTINATION,
                                       1.0,
                                       NULL,
                                       photos_application_set_bg_common_save_to_file,
                                       data);
}


static void
photos_application_set_background (PhotosApplication *self)
{
  photos_application_set_bg_common (self, self->bg_settings);
}


static void
photos_application_set_screensaver (PhotosApplication *self)
{
  photos_application_set_bg_common (self, self->ss_settings);
}


static void
photos_application_share_share (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosApplication *self;
  PhotosSharePoint *share_point = PHOTOS_SHARE_POINT (source_object);
  g_autoptr (PhotosBaseItem) item = PHOTOS_BASE_ITEM (user_data);
  g_autofree gchar *uri = NULL;

  self = PHOTOS_APPLICATION (g_application_get_default ());

  {
    g_autoptr (GError) error = NULL;

    photos_share_point_share_finish (share_point, res, &uri, &error);
    if (error != NULL)
      {
        g_warning ("Unable to share the image: %s", error->message);
        photos_share_notification_new_with_error (share_point, error);
        goto out;
      }
  }

  if (photos_share_point_needs_notification (share_point))
    photos_share_notification_new (share_point, item, uri);

 out:
  g_application_unmark_busy (G_APPLICATION (self));
  g_application_release (G_APPLICATION (self));
}


static void
photos_application_share_response (GtkDialog *dialog, gint response_id, gpointer user_data)
{
  PhotosApplication *self = PHOTOS_APPLICATION (user_data);
  GVariant *new_state;
  PhotosBaseItem *item;
  PhotosSharePoint *share_point;

  if (response_id != GTK_RESPONSE_OK)
    goto out;

  share_point = photos_share_dialog_get_selected_share_point (PHOTOS_SHARE_DIALOG (dialog));
  g_return_if_fail (share_point != NULL);

  item = photos_application_get_selection_or_active_item (self);
  g_return_if_fail (item != NULL);

  new_state = g_variant_new ("b", FALSE);
  g_action_change_state (G_ACTION (self->selection_mode_action), new_state);

  g_application_hold (G_APPLICATION (self));
  g_application_mark_busy (G_APPLICATION (self));
  photos_share_point_share_async (share_point, item, NULL, photos_application_share_share, g_object_ref (item));

 out:
  gtk_widget_destroy (GTK_WIDGET (dialog));
}


static void
photos_application_share_current (PhotosApplication *self)
{
  GtkWidget *dialog;
  PhotosBaseItem *item;

  item = photos_application_get_selection_or_active_item (self);
  g_return_if_fail (item != NULL);
  g_return_if_fail (!photos_base_item_is_collection (item));

  dialog = photos_share_dialog_new (GTK_WINDOW (self->main_window), item);
  gtk_widget_show_all (dialog);
  g_signal_connect (dialog, "response", G_CALLBACK (photos_application_share_response), self);
}


static void
photos_application_start_miners (PhotosApplication *self)
{
  photos_application_create_miners (self);
}


static void
photos_application_start_miners_second (PhotosApplication *self)
{
  photos_application_refresh_miners (self);

  self->source_added_id = g_signal_connect_object (self->state->src_mngr,
                                                   "object-added",
                                                   G_CALLBACK (photos_application_refresh_miners),
                                                   self,
                                                   G_CONNECT_SWAPPED);
  self->source_removed_id = g_signal_connect_object (self->state->src_mngr,
                                                     "object-removed",
                                                     G_CALLBACK (photos_application_refresh_miners),
                                                     self,
                                                     G_CONNECT_SWAPPED);
}


static void
photos_application_stop_miners (PhotosApplication *self)
{
  GList *l;

  for (l = self->miners_running; l != NULL; l = l->next)
    {
      GomMiner *miner = GOM_MINER (l->data);
      GCancellable *cancellable;

      cancellable = g_object_get_data (G_OBJECT (miner), "cancellable");
      g_cancellable_cancel (cancellable);
    }

  if (self->source_added_id != 0)
    {
      g_signal_handler_disconnect (self->state->src_mngr, self->source_added_id);
      self->source_added_id = 0;
    }

  if (self->source_removed_id != 0)
    {
      g_signal_handler_disconnect (self->state->src_mngr, self->source_removed_id);
      self->source_removed_id = 0;
    }

  g_list_free_full (self->miners, g_object_unref);
  self->miners = NULL;
}


static void
photos_application_theme_changed (GtkSettings *settings)
{
  static GtkCssProvider *provider;
  GdkScreen *screen;
  g_autofree gchar *theme = NULL;

  g_object_get (settings, "gtk-theme-name", &theme, NULL);
  screen = gdk_screen_get_default ();

  if (g_strcmp0 (theme, "Adwaita") == 0)
    {
      if (provider == NULL)
        {
          g_autoptr (GFile) file = NULL;

          provider = gtk_css_provider_new ();
          file = g_file_new_for_uri ("resource:///org/gnome/Photos/Adwaita.css");
          gtk_css_provider_load_from_file (provider, file, NULL);
        }

      gtk_style_context_add_provider_for_screen (screen,
                                                 GTK_STYLE_PROVIDER (provider),
                                                 GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    }
  else if (provider != NULL)
    {
      gtk_style_context_remove_provider_for_screen (screen, GTK_STYLE_PROVIDER (provider));
      g_clear_object (&provider);
    }
}


static void
photos_application_window_mode_changed (PhotosApplication *self, PhotosWindowMode mode, PhotosWindowMode old_mode)
{
  PhotosBaseManager *item_mngr_chld;

  g_return_if_fail (mode != PHOTOS_WINDOW_MODE_NONE);

  switch (old_mode)
    {
    case PHOTOS_WINDOW_MODE_NONE:
    case PHOTOS_WINDOW_MODE_EDIT:
    case PHOTOS_WINDOW_MODE_PREVIEW:
      break;

    case PHOTOS_WINDOW_MODE_COLLECTION_VIEW:
    case PHOTOS_WINDOW_MODE_COLLECTIONS:
    case PHOTOS_WINDOW_MODE_FAVORITES:
    case PHOTOS_WINDOW_MODE_IMPORT:
    case PHOTOS_WINDOW_MODE_OVERVIEW:
    case PHOTOS_WINDOW_MODE_SEARCH:
      item_mngr_chld = photos_item_manager_get_for_mode (PHOTOS_ITEM_MANAGER (self->state->item_mngr), old_mode);
      g_signal_handlers_disconnect_by_func (item_mngr_chld, photos_application_items_changed, self);
      break;

    default:
      g_assert_not_reached ();
      break;
    }

  photos_application_actions_update (self);

  switch (mode)
    {
    case PHOTOS_WINDOW_MODE_COLLECTION_VIEW:
    case PHOTOS_WINDOW_MODE_COLLECTIONS:
    case PHOTOS_WINDOW_MODE_FAVORITES:
    case PHOTOS_WINDOW_MODE_IMPORT:
    case PHOTOS_WINDOW_MODE_OVERVIEW:
    case PHOTOS_WINDOW_MODE_SEARCH:
      item_mngr_chld = photos_item_manager_get_for_mode (PHOTOS_ITEM_MANAGER (self->state->item_mngr), mode);
      g_signal_connect_swapped (item_mngr_chld,
                                "items-changed",
                                G_CALLBACK (photos_application_items_changed),
                                self);
      break;

    case PHOTOS_WINDOW_MODE_EDIT:
    case PHOTOS_WINDOW_MODE_PREVIEW:
      break;

    case PHOTOS_WINDOW_MODE_NONE:
    default:
      g_assert_not_reached ();
      break;
    }
}


static void
photos_application_search_notify_state (GSimpleAction *simple, GParamSpec *pspec, gpointer user_data)
{
  PhotosBaseManager *mngr = PHOTOS_BASE_MANAGER (user_data);
  g_autoptr (GVariant) state = NULL;
  const gchar *action_id;
  const gchar *id;
  const gchar *name;

  action_id = photos_base_manager_get_action_id (mngr);
  name = g_action_get_name (G_ACTION (simple));
  g_return_if_fail (g_strcmp0 (action_id, name) == 0);

  state = g_action_get_state (G_ACTION (simple));
  g_return_if_fail (state != NULL);

  id = g_variant_get_string (state, NULL);
  photos_base_manager_set_active_object_by_id (mngr, id);
}


static void
photos_application_selection_changed (PhotosApplication *self)
{
  photos_application_actions_update (self);
}


static void
photos_application_selection_mode_notify_state (PhotosApplication *self)
{
  photos_application_actions_update (self);
}


static void
photos_application_activate (GApplication *application)
{
  PhotosApplication *self = PHOTOS_APPLICATION (application);

  photos_debug (PHOTOS_DEBUG_APPLICATION, "PhotosApplication::activate");

  if (self->main_window == NULL)
    {
      if (!photos_application_create_window (self))
        return;

      photos_mode_controller_set_window_mode (self->state->mode_cntrlr, PHOTOS_WINDOW_MODE_OVERVIEW);
    }

  gtk_window_present_with_time (GTK_WINDOW (self->main_window), self->activation_timestamp);
  self->activation_timestamp = GDK_CURRENT_TIME;
}


static gint
photos_application_command_line (GApplication *application, GApplicationCommandLine *command_line)
{
  PhotosApplication *self = PHOTOS_APPLICATION (application);
  GVariantDict *options;
  gint ret_val = -1;

  photos_debug (PHOTOS_DEBUG_APPLICATION, "PhotosApplication::command_line");

  options = g_application_command_line_get_options_dict (command_line);
  if (g_variant_dict_contains (options, "empty-results"))
    {
      if (g_application_command_line_get_is_remote (command_line))
        {
          ret_val = EXIT_FAILURE;
          goto out;
        }

      self->empty_results = TRUE;
    }

  g_application_activate (application);

 out:
  return ret_val;
}


static gboolean
photos_application_dbus_register (GApplication *application,
                                  GDBusConnection *connection,
                                  const gchar *object_path,
                                  GError **error)
{
  PhotosApplication *self = PHOTOS_APPLICATION (application);
  gboolean ret_val = FALSE;
  g_autofree gchar *search_provider_path = NULL;

  photos_debug (PHOTOS_DEBUG_APPLICATION,
                "PhotosApplication::dbus_register: object_path: %s, search_provider: %p",
                object_path,
                self->search_provider);

  g_return_val_if_fail (self->search_provider == NULL, FALSE);

  if (!G_APPLICATION_CLASS (photos_application_parent_class)->dbus_register (application,
                                                                             connection,
                                                                             object_path,
                                                                             error))
    goto out;

  self->search_provider = photos_search_provider_new ();
  g_signal_connect_swapped (self->search_provider,
                            "activate-result",
                            G_CALLBACK (photos_application_activate_result),
                            self);
  g_signal_connect_swapped (self->search_provider,
                            "launch-search",
                            G_CALLBACK (photos_application_launch_search),
                            self);

  search_provider_path = g_strconcat (object_path, PHOTOS_SEARCH_PROVIDER_PATH_SUFFIX, NULL);
  if (!photos_search_provider_dbus_export (self->search_provider, connection, search_provider_path, error))
    {
      g_clear_object (&self->search_provider);
      goto out;
    }

  ret_val = TRUE;

 out:
  photos_debug (PHOTOS_DEBUG_APPLICATION,
                "PhotosApplication::dbus_register: Done: %d, search_provider: %p",
                ret_val,
                self->search_provider);

  return ret_val;
}


static void
photos_application_dbus_unregister (GApplication *application,
                                    GDBusConnection *connection,
                                    const gchar *object_path)
{
  PhotosApplication *self = PHOTOS_APPLICATION (application);

  photos_debug (PHOTOS_DEBUG_APPLICATION,
                "PhotosApplication::dbus_unregister: object_path: %s, search_provider: %p",
                object_path,
                self->search_provider);

  if (self->search_provider != NULL)
    {
      g_autofree gchar *search_provider_path = NULL;

      search_provider_path = g_strconcat (object_path, PHOTOS_SEARCH_PROVIDER_PATH_SUFFIX, NULL);
      photos_search_provider_dbus_unexport (self->search_provider, connection, search_provider_path);
      g_clear_object (&self->search_provider);
    }

  G_APPLICATION_CLASS (photos_application_parent_class)->dbus_unregister (application, connection, object_path);

  photos_debug (PHOTOS_DEBUG_APPLICATION,
                "PhotosApplication::dbus_unregister: Done: search_provider: %p",
                self->search_provider);
}


static gint
photos_application_handle_local_options (GApplication *application, GVariantDict *options)
{
  gint ret_val = -1;

  photos_debug (PHOTOS_DEBUG_APPLICATION, "PhotosApplication::handle_local_options");

  if (g_variant_dict_contains (options, "version"))
    {
      const gchar *version;

      version = photos_utils_get_version ();
      g_print ("%s %s\n", PACKAGE_TARNAME, version);
      ret_val = EXIT_SUCCESS;
    }

  return ret_val;
}


static void
photos_application_shutdown (GApplication *application)
{
  PhotosApplication *self = PHOTOS_APPLICATION (application);
  guint refresh_miner_ids_size;

  photos_debug (PHOTOS_DEBUG_APPLICATION, "PhotosApplication::shutdown");

  refresh_miner_ids_size = g_hash_table_size (self->refresh_miner_ids);
  g_assert (refresh_miner_ids_size == 0);

  if (self->init_fishes_id != 0)
    {
      g_source_remove (self->init_fishes_id);
      self->init_fishes_id = 0;
    }

  g_clear_pointer (&self->refresh_miner_ids, (GDestroyNotify) g_hash_table_unref);

  G_APPLICATION_CLASS (photos_application_parent_class)->shutdown (application);
}


static void
photos_application_startup (GApplication *application)
{
  PhotosApplication *self = PHOTOS_APPLICATION (application);
  GrlRegistry *registry;
  GtkIconTheme *icon_theme;
  GtkSettings *settings;
  GVariant *state;
  gboolean grl_plugins_loaded;
  const gchar *delete_accels[3] = {"Delete", "KP_Delete", NULL};
  const gchar *edit_accels[2] = {"<Primary>e", NULL};
  const gchar *fullscreen_accels[2] = {"F11", NULL};
  const gchar *gear_menu_accels[2] = {"F10", NULL};
  const gchar *help_menu_accels[2] = {"F1", NULL};
  const gchar *print_current_accels[2] = {"<Primary>p", NULL};
  const gchar *quit_accels[2] = {"<Primary>q", NULL};
  const gchar *save_accels[2] = {"<Primary>x", NULL};
  const gchar *search_accels[2] = {"<Primary>f", NULL};
  const gchar *select_all_accels[2] = {"<Primary>a", NULL};
  const gchar *zoom_best_fit_accels[3] = {"<Primary>0", NULL};
  const gchar *zoom_in_accels[3] = {"<Primary>plus", "<Primary>equal", NULL};
  const gchar *zoom_out_accels[2] = {"<Primary>minus", NULL};

  photos_debug (PHOTOS_DEBUG_APPLICATION, "PhotosApplication::startup");

  G_APPLICATION_CLASS (photos_application_parent_class)->startup (application);

  photos_gegl_init ();

  grl_init (NULL, NULL);
  registry = grl_registry_get_default ();

  {
    g_autoptr (GError) error = NULL;

    grl_plugins_loaded = grl_registry_load_all_plugins (registry, FALSE, &error);
    if (error != NULL)
      g_warning ("Unable to load Grilo plugins: %s", error->message);
  }

  if (grl_plugins_loaded)
    {
      {
        g_autoptr (GError) error = NULL;

        if (!grl_registry_activate_plugin_by_id (registry, "grl-flickr", &error))
          g_warning ("Unable to activate Grilo's Flickr plugin: %s", error->message);
      }
    }

  self->create_window_cancellable = g_cancellable_new ();
  self->refresh_miner_ids = g_hash_table_new (g_direct_hash, g_direct_equal);

  self->bg_settings = g_settings_new (DESKTOP_BACKGROUND_SCHEMA);
  self->ss_settings = g_settings_new (DESKTOP_SCREENSAVER_SCHEMA);

  self->resource = photos_get_resource ();
  g_resources_register (self->resource);

  self->resource_gegl = photos_gegl_get_resource ();
  g_resources_register (self->resource_gegl);

  icon_theme = gtk_icon_theme_get_default ();
  gtk_icon_theme_add_resource_path (icon_theme, "/org/gnome/Photos/icons");

  settings = gtk_settings_get_default ();
  g_object_set (settings, "gtk-application-prefer-dark-theme", TRUE, NULL);
  g_signal_connect (settings, "notify::gtk-theme-name", G_CALLBACK (photos_application_theme_changed), NULL);
  photos_application_theme_changed (settings);

  self->shr_pnt_mngr = photos_share_point_manager_dup_singleton ();

  /* A dummy reference to keep it alive during the lifetime of the
   * application.
   */
  self->camera_cache = photos_camera_cache_dup_singleton ();

  self->sel_cntrlr = photos_selection_controller_dup_singleton ();
  g_signal_connect_swapped (self->sel_cntrlr,
                            "selection-changed",
                            G_CALLBACK (photos_application_selection_changed),
                            self);

  {
    g_autoptr (GSimpleAction) action = NULL;

    action = g_simple_action_new ("about", NULL);
    g_signal_connect_swapped (action, "activate", G_CALLBACK (photos_application_about), self);
    g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (action));
  }

  self->blacks_exposure_action = g_simple_action_new ("blacks-exposure-current", G_VARIANT_TYPE ("a{sd}"));
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (self->blacks_exposure_action));

  self->contrast_action = g_simple_action_new ("contrast-current", G_VARIANT_TYPE_DOUBLE);
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (self->contrast_action));

  self->crop_action = g_simple_action_new ("crop-current", G_VARIANT_TYPE ("a{sd}"));
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (self->crop_action));

  self->delete_action = g_simple_action_new ("delete", NULL);
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (self->delete_action));

  self->denoise_action = g_simple_action_new ("denoise-current", G_VARIANT_TYPE_UINT16);
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (self->denoise_action));

  self->edit_cancel_action = g_simple_action_new ("edit-cancel", NULL);
  g_signal_connect_swapped (self->edit_cancel_action,
                            "activate",
                            G_CALLBACK (photos_application_edit_cancel),
                            self);
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (self->edit_cancel_action));

  self->edit_action = g_simple_action_new ("edit-current", NULL);
  g_signal_connect_swapped (self->edit_action, "activate", G_CALLBACK (photos_application_edit_current), self);
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (self->edit_action));

  self->edit_done_action = g_simple_action_new ("edit-done", NULL);
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (self->edit_done_action));

  self->edit_revert_action = g_simple_action_new ("edit-revert", G_VARIANT_TYPE_STRING);
  g_simple_action_set_enabled (self->edit_revert_action, FALSE);
  g_signal_connect_swapped (self->edit_revert_action,
                            "activate",
                            G_CALLBACK (photos_application_edit_revert),
                            self);
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (self->edit_revert_action));

  self->fs_action = g_simple_action_new ("fullscreen", NULL);
  g_signal_connect_swapped (self->fs_action, "activate", G_CALLBACK (photos_application_fullscreen), self);
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (self->fs_action));

  g_signal_connect_swapped (self->state->mode_cntrlr,
                            "can-fullscreen-changed",
                            G_CALLBACK (photos_application_can_fullscreen_changed),
                            self);

  state = g_variant_new ("b", FALSE);
  self->gear_action = g_simple_action_new_stateful ("gear-menu", NULL, state);
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (self->gear_action));

  self->import_action = g_simple_action_new ("import-current", NULL);
  g_signal_connect_swapped (self->import_action, "activate", G_CALLBACK (photos_application_import), self);
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (self->import_action));

  self->import_cancel_action = g_simple_action_new ("import-cancel", NULL);
  g_signal_connect_swapped (self->import_cancel_action,
                            "activate",
                            G_CALLBACK (photos_application_import_cancel),
                            self);
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (self->import_cancel_action));

  self->insta_action = g_simple_action_new ("insta-current", G_VARIANT_TYPE_INT16);
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (self->insta_action));

  self->load_next_action = g_simple_action_new ("load-next", NULL);
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (self->load_next_action));

  self->load_previous_action = g_simple_action_new ("load-previous", NULL);
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (self->load_previous_action));

  self->open_action = g_simple_action_new ("open-current", NULL);
  g_signal_connect_swapped (self->open_action, "activate", G_CALLBACK (photos_application_open_current), self);
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (self->open_action));

  self->print_action = g_simple_action_new ("print-current", NULL);
  g_signal_connect_swapped (self->print_action, "activate", G_CALLBACK (photos_application_print_current), self);
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (self->print_action));

  self->properties_action = g_simple_action_new ("properties", NULL);
  g_signal_connect_swapped (self->properties_action, "activate", G_CALLBACK (photos_application_properties), self);
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (self->properties_action));

  {
    g_autoptr (GSimpleAction) action = NULL;

    action = g_simple_action_new ("quit", NULL);
    g_signal_connect_swapped (action, "activate", G_CALLBACK (photos_application_quit), self);
    g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (action));
  }

  {
    g_autoptr (GSimpleAction) action = NULL;

    action = g_simple_action_new ("remote-display-current", NULL);
    g_signal_connect_swapped (action, "activate", G_CALLBACK (photos_application_remote_display_current), self);
    g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (action));
  }

  self->saturation_action = g_simple_action_new ("saturation-current", G_VARIANT_TYPE_DOUBLE);
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (self->saturation_action));

  self->save_action = g_simple_action_new ("save-current", NULL);
  g_signal_connect_swapped (self->save_action, "activate", G_CALLBACK (photos_application_save), self);
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (self->save_action));

  state = g_variant_new ("b", FALSE);
  self->search_action = g_simple_action_new_stateful ("search", NULL, state);
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (self->search_action));

  state = g_variant_new ("s", PHOTOS_SEARCH_MATCH_STOCK_ALL);
  self->search_match_action = g_simple_action_new_stateful ("search-match", G_VARIANT_TYPE_STRING, state);
  g_signal_connect (self->search_match_action,
                    "notify::state",
                    G_CALLBACK (photos_application_search_notify_state),
                    self->state->srch_mtch_mngr);
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (self->search_match_action));

  state = g_variant_new ("s", PHOTOS_SOURCE_STOCK_ALL);
  self->search_source_action = g_simple_action_new_stateful ("search-source", G_VARIANT_TYPE_STRING, state);
  g_signal_connect (self->search_source_action,
                    "notify::state",
                    G_CALLBACK (photos_application_search_notify_state),
                    self->state->src_mngr);
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (self->search_source_action));

  state = g_variant_new ("s", PHOTOS_SEARCH_TYPE_STOCK_ALL);
  self->search_type_action = g_simple_action_new_stateful ("search-type", G_VARIANT_TYPE_STRING, state);
  g_signal_connect (self->search_type_action,
                    "notify::state",
                    G_CALLBACK (photos_application_search_notify_state),
                    self->state->srch_typ_mngr);
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (self->search_type_action));

  self->sel_all_action = g_simple_action_new ("select-all", NULL);
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (self->sel_all_action));

  self->sel_none_action = g_simple_action_new ("select-none", NULL);
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (self->sel_none_action));

  state = g_variant_new ("b", FALSE);
  self->selection_mode_action = g_simple_action_new_stateful ("selection-mode", NULL, state);
  g_signal_connect_swapped (self->selection_mode_action,
                            "notify::state",
                            G_CALLBACK (photos_application_selection_mode_notify_state),
                            self);
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (self->selection_mode_action));

  self->set_bg_action = g_simple_action_new ("set-background", NULL);
  g_signal_connect_swapped (self->set_bg_action, "activate", G_CALLBACK (photos_application_set_background), self);
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (self->set_bg_action));

  self->set_ss_action = g_simple_action_new ("set-screensaver", NULL);
  g_signal_connect_swapped (self->set_ss_action, "activate", G_CALLBACK (photos_application_set_screensaver), self);
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (self->set_ss_action));

  self->shadows_highlights_action = g_simple_action_new ("shadows-highlights-current", G_VARIANT_TYPE ("a{sd}"));
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (self->shadows_highlights_action));

  self->share_action = g_simple_action_new ("share-current", NULL);
  g_signal_connect_swapped (self->share_action, "activate", G_CALLBACK (photos_application_share_current), self);
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (self->share_action));

  self->sharpen_action = g_simple_action_new ("sharpen-current", G_VARIANT_TYPE_DOUBLE);
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (self->sharpen_action));

  self->zoom_begin_action = g_simple_action_new ("zoom-begin", G_VARIANT_TYPE_VARDICT);
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (self->zoom_begin_action));

  self->zoom_best_fit_action = g_simple_action_new ("zoom-best-fit", NULL);
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (self->zoom_best_fit_action));

  self->zoom_end_action = g_simple_action_new ("zoom-end", G_VARIANT_TYPE_VARDICT);
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (self->zoom_end_action));

  self->zoom_in_action = g_simple_action_new ("zoom-in", G_VARIANT_TYPE_VARDICT);
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (self->zoom_in_action));

  self->zoom_out_action = g_simple_action_new ("zoom-out", G_VARIANT_TYPE_VARDICT);
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (self->zoom_out_action));

  g_signal_connect_swapped (self->state->mode_cntrlr,
                            "window-mode-changed",
                            G_CALLBACK (photos_application_window_mode_changed),
                            self);

  {
    g_autoptr (GSimpleAction) action = NULL;

    action = g_simple_action_new ("help", NULL);
    g_signal_connect_swapped (action, "activate", G_CALLBACK (photos_application_help), self);
    g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (action));
  }

  gtk_application_set_accels_for_action (GTK_APPLICATION (self), "app.quit", quit_accels);
  gtk_application_set_accels_for_action (GTK_APPLICATION (self), "app.delete", delete_accels);
  gtk_application_set_accels_for_action (GTK_APPLICATION (self), "app.edit-current", edit_accels);
  gtk_application_set_accels_for_action (GTK_APPLICATION (self), "app.fullscreen", fullscreen_accels);
  gtk_application_set_accels_for_action (GTK_APPLICATION (self), "app.gear-menu", gear_menu_accels);
  gtk_application_set_accels_for_action (GTK_APPLICATION (self), "app.help", help_menu_accels);
  gtk_application_set_accels_for_action (GTK_APPLICATION (self), "app.print-current", print_current_accels);
  gtk_application_set_accels_for_action (GTK_APPLICATION (self), "app.save-current", save_accels);
  gtk_application_set_accels_for_action (GTK_APPLICATION (self), "app.search", search_accels);
  gtk_application_set_accels_for_action (GTK_APPLICATION (self), "app.select-all", select_all_accels);
  gtk_application_set_accels_for_action (GTK_APPLICATION (self), "app.zoom-best-fit", zoom_best_fit_accels);

  {
    g_autofree gchar *detailed_action_name = NULL;

    detailed_action_name = photos_utils_print_zoom_action_detailed_name ("app.zoom-in",
                                                                         1.0,
                                                                         PHOTOS_ZOOM_EVENT_KEYBOARD_ACCELERATOR);
    gtk_application_set_accels_for_action (GTK_APPLICATION (self), detailed_action_name, zoom_in_accels);
  }

  {
    g_autofree gchar *detailed_action_name = NULL;

    detailed_action_name = photos_utils_print_zoom_action_detailed_name ("app.zoom-out",
                                                                         1.0,
                                                                         PHOTOS_ZOOM_EVENT_KEYBOARD_ACCELERATOR);
    gtk_application_set_accels_for_action (GTK_APPLICATION (self), detailed_action_name, zoom_out_accels);
  }

  g_signal_connect_swapped (self->state->item_mngr,
                            "load-finished",
                            G_CALLBACK (photos_application_load_changed),
                            self);
  g_signal_connect_swapped (self->state->item_mngr,
                            "load-started",
                            G_CALLBACK (photos_application_load_changed),
                            self);
}


static void
photos_application_constructed (GObject *object)
{
  PhotosApplication *self = PHOTOS_APPLICATION (object);
  const gchar *app_id;

  G_OBJECT_CLASS (photos_application_parent_class)->constructed (object);

  app_id = g_application_get_application_id (G_APPLICATION (self));
  g_set_prgname (app_id);
}


static void
photos_application_dispose (GObject *object)
{
  PhotosApplication *self = PHOTOS_APPLICATION (object);

  g_assert_null (self->search_provider);

  if (self->miners_running != NULL)
    {
      g_list_free_full (self->miners_running, g_object_unref);
      self->miners_running = NULL;
    }

  if (self->miners != NULL)
    {
      g_list_free_full (self->miners, g_object_unref);
      self->miners = NULL;
    }

  if (self->resource != NULL)
    {
      g_resources_unregister (self->resource);
      self->resource = NULL;
    }

  if (self->resource_gegl != NULL)
    {
      g_resources_unregister (self->resource_gegl);
      self->resource_gegl = NULL;
    }

  g_clear_object (&self->create_window_cancellable);
  g_clear_object (&self->bg_settings);
  g_clear_object (&self->ss_settings);
  g_clear_object (&self->blacks_exposure_action);
  g_clear_object (&self->contrast_action);
  g_clear_object (&self->crop_action);
  g_clear_object (&self->delete_action);
  g_clear_object (&self->denoise_action);
  g_clear_object (&self->edit_action);
  g_clear_object (&self->edit_cancel_action);
  g_clear_object (&self->edit_done_action);
  g_clear_object (&self->edit_revert_action);
  g_clear_object (&self->fs_action);
  g_clear_object (&self->gear_action);
  g_clear_object (&self->import_action);
  g_clear_object (&self->import_cancel_action);
  g_clear_object (&self->insta_action);
  g_clear_object (&self->load_next_action);
  g_clear_object (&self->load_previous_action);
  g_clear_object (&self->open_action);
  g_clear_object (&self->print_action);
  g_clear_object (&self->properties_action);
  g_clear_object (&self->saturation_action);
  g_clear_object (&self->save_action);
  g_clear_object (&self->search_action);
  g_clear_object (&self->search_match_action);
  g_clear_object (&self->search_source_action);
  g_clear_object (&self->search_type_action);
  g_clear_object (&self->sel_all_action);
  g_clear_object (&self->sel_none_action);
  g_clear_object (&self->selection_mode_action);
  g_clear_object (&self->set_bg_action);
  g_clear_object (&self->set_ss_action);
  g_clear_object (&self->shadows_highlights_action);
  g_clear_object (&self->share_action);
  g_clear_object (&self->sharpen_action);
  g_clear_object (&self->zoom_begin_action);
  g_clear_object (&self->zoom_best_fit_action);
  g_clear_object (&self->zoom_end_action);
  g_clear_object (&self->zoom_in_action);
  g_clear_object (&self->zoom_out_action);
  g_clear_object (&self->shr_pnt_mngr);
  g_clear_object (&self->camera_cache);
  g_clear_object (&self->sel_cntrlr);
  g_clear_object (&self->factory);
  g_clear_object (&self->extract_priority);

  if (self->state != NULL)
    {
      photos_search_context_state_free (self->state);
      self->state = NULL;
    }

  G_OBJECT_CLASS (photos_application_parent_class)->dispose (object);
}


static void
photos_application_finalize (GObject *object)
{
  PhotosApplication *self = PHOTOS_APPLICATION (object);

  g_assert (self->create_miners_count == 0);

  if (g_application_get_is_registered (G_APPLICATION (self)) && !g_application_get_is_remote (G_APPLICATION (self)))
    gegl_exit ();

  G_OBJECT_CLASS (photos_application_parent_class)->finalize (object);
}


static void
photos_application_init (PhotosApplication *self)
{
  setlocale (LC_ALL, "");

  bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);

  g_set_application_name (_("Photos"));

  photos_utils_ensure_builtins ();

  self->state = photos_search_context_state_new (PHOTOS_SEARCH_CONTEXT (self));
  self->activation_timestamp = GDK_CURRENT_TIME;

  g_application_add_main_option_entries (G_APPLICATION (self), COMMAND_LINE_OPTIONS);
  g_application_set_flags (G_APPLICATION (self), G_APPLICATION_HANDLES_COMMAND_LINE);
}


static void
photos_application_class_init (PhotosApplicationClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GApplicationClass *application_class = G_APPLICATION_CLASS (class);

  object_class->constructed = photos_application_constructed;
  object_class->dispose = photos_application_dispose;
  object_class->finalize = photos_application_finalize;
  application_class->activate = photos_application_activate;
  application_class->command_line = photos_application_command_line;
  application_class->dbus_register = photos_application_dbus_register;
  application_class->dbus_unregister = photos_application_dbus_unregister;
  application_class->handle_local_options = photos_application_handle_local_options;
  application_class->shutdown = photos_application_shutdown;
  application_class->startup = photos_application_startup;

  signals[MINERS_CHANGED] = g_signal_new ("miners-changed",
                                          G_TYPE_FROM_CLASS (class),
                                          G_SIGNAL_RUN_LAST,
                                          0,
                                          NULL, /* accumulator */
                                          NULL, /* accu_data */
                                          g_cclosure_marshal_VOID__POINTER,
                                          G_TYPE_NONE,
                                          1,
                                          G_TYPE_POINTER);
}


static void
photos_application_search_context_iface_init (PhotosSearchContextInterface *iface)
{
  iface->get_state = photos_application_get_state;
}


GApplication *
photos_application_new (void)
{
  return g_object_new (PHOTOS_TYPE_APPLICATION,
                       "application-id", "org.gnome." PACKAGE_NAME,
                       NULL);
}


gboolean
photos_application_get_empty_results (PhotosApplication *self)
{
  g_return_val_if_fail (PHOTOS_IS_APPLICATION (self), FALSE);
  return self->empty_results;
}


GomMiner *
photos_application_get_miner (PhotosApplication *self, const gchar *provider_type)
{
  GList *l;
  GomMiner *ret_val = NULL;

  for (l = self->miners; l != NULL; l = l->next)
    {
      GomMiner *miner = GOM_MINER (l->data);
      const gchar *miner_provider_type;

      miner_provider_type = g_object_get_data (G_OBJECT (miner), "provider-type");
      if (g_strcmp0 (provider_type, miner_provider_type) == 0)
        {
          ret_val = miner;
          break;
        }
    }

  return ret_val;
}


GList *
photos_application_get_miners_running (PhotosApplication *self)
{
  return self->miners_running;
}


gint
photos_application_get_scale_factor (PhotosApplication *self)
{
  GList *windows;
  gint ret_val = 1;

  /* We do not use self->main_window to allow widgets to use this
   * method while they are being constructed. The widget hierarchy is
   * created in PhotosMainWindow:constructed and at that point
   * self->main_window is NULL.
   */
  windows = gtk_application_get_windows (GTK_APPLICATION (self));
  if (windows == NULL)
    goto out;

  ret_val = gtk_widget_get_scale_factor (GTK_WIDGET (windows->data));

 out:
  return ret_val;
}


void
photos_application_hold (PhotosApplication *self)
{
  g_return_if_fail (PHOTOS_IS_APPLICATION (self));
  self->use_count++;
}


void
photos_application_release (PhotosApplication *self)
{
  g_return_if_fail (PHOTOS_IS_APPLICATION (self));
  g_return_if_fail (self->use_count > 0);

  self->use_count--;
  if (self->main_window_deleted && self->use_count == 0)
    gtk_widget_destroy (self->main_window);
}
