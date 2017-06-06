/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2015 Alessandro Bono
 * Copyright © 2014 – 2015 Pranav Kant
 * Copyright © 2012 – 2017 Red Hat, Inc.
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

#include <stdlib.h>

#include <gegl.h>
#include <gexiv2/gexiv2.h>
#include <gio/gio.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <grilo.h>
#include <libgnome-desktop/gnome-bg.h>

#include "photos-application.h"
#include "photos-base-item.h"
#include "photos-camera-cache.h"
#include "photos-debug.h"
#include "photos-dlna-renderers-dialog.h"
#include "photos-export-dialog.h"
#include "photos-export-notification.h"
#include "photos-filterable.h"
#include "photos-gegl.h"
#include "photos-glib.h"
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
  GSimpleAction *brightness_contrast_action;
  GSimpleAction *crop_action;
  GSimpleAction *delete_action;
  GSimpleAction *denoise_action;
  GSimpleAction *edit_action;
  GSimpleAction *edit_cancel_action;
  GSimpleAction *edit_done_action;
  GSimpleAction *edit_revert_action;
  GSimpleAction *fs_action;
  GSimpleAction *gear_action;
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
  GSimpleAction *share_action;
  GSimpleAction *sharpen_action;
  GSimpleAction *zoom_best_fit_action;
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
  { "version", 0, G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, NULL, N_("Show the application's version"), NULL},
  { NULL }
};

static const gchar *REQUIRED_GEGL_OPS[] =
{
  "gegl:buffer-sink",
  "gegl:buffer-source",
  "gegl:crop",
  "gegl:exposure",
  "gegl:gray",
  "gegl:load",
  "gegl:nop",
  "gegl:pixbuf",
  "gegl:rotate-on-center",
  "gegl:save-pixbuf",
  "gegl:scale-ratio",

  /* Used by gegl:load */
  "gegl:jpg-load",
  "gegl:png-load",
  "gegl:raw-load",
  "gegl:text"
};

static const gchar *DESKTOP_BACKGROUND_SCHEMA = "org.gnome.desktop.background";
static const gchar *DESKTOP_SCREENSAVER_SCHEMA = "org.gnome.desktop.screensaver";
static const gchar *DESKTOP_KEY_PICTURE_URI = "picture-uri";
static const gchar *DESKTOP_KEY_PICTURE_OPTIONS = "picture-options";
static const gchar *DESKTOP_KEY_COLOR_SHADING_TYPE = "color-shading-type";
static const gchar *DESKTOP_KEY_PRIMARY_COLOR = "primary-color";
static const gchar *DESKTOP_KEY_SECONDARY_COLOR = "secondary-color";

typedef struct _PhotosApplicationCreateData PhotosApplicationCreateData;
typedef struct _PhotosApplicationRefreshData PhotosApplicationRefreshData;

struct _PhotosApplicationCreateData
{
  PhotosApplication *application;
  gchar *extension_name;
  gchar *miner_name;
};

struct _PhotosApplicationRefreshData
{
  PhotosApplication *application;
  GomMiner *miner;
};

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


static void
photos_application_action_toggle (GSimpleAction *simple, GVariant *parameter, gpointer user_data)
{
  GVariant *state;
  GVariant *new_state;
  gboolean state_value;

  state = g_action_get_state (G_ACTION (simple));
  g_return_if_fail (state != NULL);

  state_value = g_variant_get_boolean (state);
  new_state = g_variant_new ("b", !state_value);
  g_action_change_state (G_ACTION (simple), new_state);

  g_variant_unref (state);
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

  item = photos_application_get_selection_or_active_item (self);
  load_state = photos_item_manager_get_load_state (self->state->item_mngr);
  mode = photos_mode_controller_get_window_mode (self->state->mode_cntrlr);
  selection = photos_selection_controller_get_selection (self->sel_cntrlr);
  selection_mode = photos_utils_get_selection_mode ();

  g_simple_action_set_enabled (self->zoom_best_fit_action, FALSE);
  g_simple_action_set_enabled (self->zoom_out_action, FALSE);

  enable = (mode == PHOTOS_WINDOW_MODE_EDIT);
  g_simple_action_set_enabled (self->blacks_exposure_action, enable);
  g_simple_action_set_enabled (self->brightness_contrast_action, enable);
  g_simple_action_set_enabled (self->crop_action, enable);
  g_simple_action_set_enabled (self->denoise_action, enable);
  g_simple_action_set_enabled (self->edit_cancel_action, enable);
  g_simple_action_set_enabled (self->edit_done_action, enable);
  g_simple_action_set_enabled (self->insta_action, enable);
  g_simple_action_set_enabled (self->saturation_action, enable);
  g_simple_action_set_enabled (self->sharpen_action, enable);

  enable = (mode == PHOTOS_WINDOW_MODE_COLLECTIONS
            || mode == PHOTOS_WINDOW_MODE_FAVORITES
            || mode == PHOTOS_WINDOW_MODE_OVERVIEW
            || mode == PHOTOS_WINDOW_MODE_SEARCH);
  g_simple_action_set_enabled (self->search_match_action, enable);
  g_simple_action_set_enabled (self->search_source_action, enable);
  g_simple_action_set_enabled (self->search_type_action, enable);
  g_simple_action_set_enabled (self->sel_all_action, enable);
  g_simple_action_set_enabled (self->sel_none_action, enable);
  g_simple_action_set_enabled (self->selection_mode_action, enable);

  enable = (mode == PHOTOS_WINDOW_MODE_PREVIEW);
  g_simple_action_set_enabled (self->load_next_action, enable);
  g_simple_action_set_enabled (self->load_previous_action, enable);

  enable = (load_state == PHOTOS_LOAD_STATE_FINISHED && mode == PHOTOS_WINDOW_MODE_PREVIEW);
  g_simple_action_set_enabled (self->gear_action, enable);
  g_simple_action_set_enabled (self->set_bg_action, enable);
  g_simple_action_set_enabled (self->set_ss_action, enable);
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

      selected_item = PHOTOS_BASE_ITEM (photos_base_manager_get_object_by_id (self->state->item_mngr, urn));
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
  GError *error;
  TrackerExtractPriority *extract_priority = TRACKER_EXTRACT_PRIORITY (source_object);

  error = NULL;
  if (!tracker_extract_priority_call_clear_rdf_types_finish (extract_priority, res, &error))
    {
      g_warning ("Unable to call ClearRdfTypes: %s", error->message);
      g_error_free (error);
      goto out;
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
  GError *error;
  GomMiner *miner = NULL;

  error = NULL;
  miner = gom_miner_proxy_new_for_bus_finish (res, &error);
  if (error != NULL)
    {
      if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        {
          g_error_free (error);
          goto out;
        }
      else
        {
          g_warning ("Unable to create GomMiner proxy for %s: %s", data->miner_name, error->message);
          g_error_free (error);
          goto maybe_continue;
        }
    }

  g_object_set_data_full (G_OBJECT (miner), "provider-type", g_strdup (data->extension_name), g_free);
  self->miners = g_list_prepend (self->miners, g_object_ref (miner));

 maybe_continue:
  if (self->create_miners_count == 1)
    photos_application_start_miners_second (self);

 out:
  self->create_miners_count--;
  g_clear_object (&miner);
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
      PhotosBaseItemClass *base_item_class;

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


static gboolean
photos_application_sanity_check_gegl (PhotosApplication *self)
{
  gboolean ret_val = TRUE;
  guint i;

  for (i = 0; i < G_N_ELEMENTS (REQUIRED_GEGL_OPS); i++)
    {
      if (!gegl_has_operation (REQUIRED_GEGL_OPS[i]))
        {
          g_warning ("Unable to find GEGL operation %s: Check your GEGL install", REQUIRED_GEGL_OPS[i]);
          ret_val = FALSE;
          break;
        }
    }

  return ret_val;
}


static void
photos_application_tracker_set_rdf_types (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosApplication *self = PHOTOS_APPLICATION (user_data);
  GError *error;
  TrackerExtractPriority *extract_priority = TRACKER_EXTRACT_PRIORITY (source_object);

  error = NULL;
  if (!tracker_extract_priority_call_set_rdf_types_finish (extract_priority, res, &error))
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Unable to call SetRdfTypes: %s", error->message);
      g_error_free (error);
      goto out;
    }

 out:
  g_application_release (G_APPLICATION (self));
}


static void
photos_application_tracker_extract_priority (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosApplication *self = PHOTOS_APPLICATION (user_data);
  GError *error;
  const gchar *const rdf_types[] = {"nfo:Image", NULL};

  error = NULL;
  self->extract_priority = tracker_extract_priority_proxy_new_for_bus_finish (res, &error);
  if (error != NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Unable to create TrackerExtractPriority proxy: %s", error->message);
      g_error_free (error);
      goto out;
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
  gboolean gexiv2_initialized;
  gboolean gexiv2_registered_namespace;
  gboolean sanity_checked_gegl;

  if (self->main_window != NULL)
    return TRUE;

  gexiv2_initialized = gexiv2_initialize ();
  g_return_val_if_fail (gexiv2_initialized, FALSE);

  gexiv2_registered_namespace = gexiv2_metadata_register_xmp_namespace ("http://www.gnome.org/xmp", "gnome");
  g_return_val_if_fail (gexiv2_registered_namespace, FALSE);

  sanity_checked_gegl = photos_application_sanity_check_gegl (self);
  g_return_val_if_fail (sanity_checked_gegl, FALSE);

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
  if (!photos_application_create_window (self))
    return;

  photos_base_manager_set_active_object (self->state->item_mngr, item);
  g_application_activate (G_APPLICATION (self));

  /* TODO: Forward the search terms when we exit the preview */
}


static void
photos_application_activate_query_executed (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosApplication *self = PHOTOS_APPLICATION (user_data);
  GError *error = NULL;
  GObject *item;
  PhotosSingleItemJob *job = PHOTOS_SINGLE_ITEM_JOB (source_object);
  TrackerSparqlCursor *cursor = NULL;
  const gchar *identifier;

  cursor = photos_single_item_job_finish (job, res, &error);
  if (error != NULL)
    {
      g_warning ("Unable to query single item: %s", error->message);
      g_error_free (error);
      goto out;
    }

  if (cursor == NULL)
    goto out;

  photos_item_manager_add_item (PHOTOS_ITEM_MANAGER (self->state->item_mngr), cursor, TRUE);

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

  self->activation_timestamp = timestamp;

  item = photos_base_manager_get_object_by_id (self->state->item_mngr, identifier);
  if (item != NULL)
    photos_application_activate_item (self, item);
  else
    {
      PhotosSingleItemJob *job;

      job = photos_single_item_job_new (identifier);
      g_application_hold (G_APPLICATION (self));
      photos_single_item_job_run (job,
                                  self->state,
                                  PHOTOS_QUERY_FLAGS_UNFILTERED,
                                  NULL,
                                  photos_application_activate_query_executed,
                                  self);
      g_object_unref (job);
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
  GError *error = NULL;
  PhotosBaseItem *item = PHOTOS_BASE_ITEM (source_object);

  if (!photos_base_item_pipeline_revert_finish (item, res, &error))
    {
      g_warning ("Unable to process item: %s", error->message);
      g_error_free (error);
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
  GError *error;
  PhotosBaseItem *item = PHOTOS_BASE_ITEM (source_object);

  error = NULL;
  if (!photos_base_item_pipeline_save_finish (item, res, &error))
    {
      g_warning ("Unable to save pipeline: %s", error->message);
      g_error_free (error);
    }

  g_application_release (G_APPLICATION (self));
}


static void
photos_application_edit_revert_revert (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosApplication *self = PHOTOS_APPLICATION (user_data);
  GError *error;
  PhotosBaseItem *item = PHOTOS_BASE_ITEM (source_object);

  error = NULL;
  if (!photos_base_item_pipeline_revert_finish (item, res, &error))
    {
      g_warning ("Unable to process item: %s", error->message);
      g_error_free (error);
      goto out;
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
photos_application_launch_search (PhotosApplication *self, const gchar* const *terms, guint timestamp)
{
  GVariant *state;
  gchar *str;

  if (!photos_application_create_window (self))
    return;

  photos_mode_controller_set_window_mode (self->state->mode_cntrlr, PHOTOS_WINDOW_MODE_OVERVIEW);

  str = g_strjoinv (" ", (gchar **) terms);
  photos_search_controller_set_string (self->state->srch_cntrlr, str);
  g_free (str);

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
  GError *error;
  PhotosBaseItem *item = PHOTOS_BASE_ITEM (source_object);

  error = NULL;
  if (!photos_base_item_pipeline_save_finish (item, res, &error))
    {
      g_warning ("Unable to save pipeline: %s", error->message);
      g_error_free (error);
    }

  g_application_release (G_APPLICATION (self));
}


static void
photos_application_properties_revert_to_original (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosApplication *self = PHOTOS_APPLICATION (user_data);
  GError *error = NULL;
  PhotosBaseItem *item = PHOTOS_BASE_ITEM (source_object);

  if (!photos_base_item_pipeline_revert_to_original_finish (item, res, &error))
    {
      g_warning ("Unable to revert to original: %s", error->message);
      g_error_free (error);
      goto out;
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
  GError *error;
  GList *miner_link;
  GomMiner *miner = GOM_MINER (source_object);
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

  error = NULL;
  if (!gom_miner_call_refresh_db_finish (miner, res, &error))
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Unable to update the cache: %s", error->message);
      g_error_free (error);
      goto out;
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
  g_object_unref (miner);
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
photos_application_save_save (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosApplication *self = PHOTOS_APPLICATION (user_data);
  PhotosBaseItem *item = PHOTOS_BASE_ITEM (source_object);
  GError *error = NULL;
  GFile *file = NULL;
  GList *items = NULL;

  file = photos_base_item_save_finish (item, res, &error);
  if (error != NULL)
    {
      g_warning ("Unable to save: %s", error->message);
      photos_export_notification_new_with_error (error);
      g_error_free (error);
      goto out;
    }

  items = g_list_prepend (items, g_object_ref (item));
  photos_export_notification_new (items, file);

 out:
  g_application_release (G_APPLICATION (self));
  g_clear_object (&file);
  g_list_free_full (items, g_object_unref);
}


static void
photos_application_save_response (GtkDialog *dialog, gint response_id, gpointer user_data)
{
  PhotosApplication *self = PHOTOS_APPLICATION (user_data);
  GError *error;
  GFile *export = NULL;
  GFile *tmp;
  GVariant *new_state;
  PhotosBaseItem *item;
  const gchar *export_dir_name;
  const gchar *pictures_path;
  gchar *export_path = NULL;
  gdouble zoom;

  if (response_id != GTK_RESPONSE_OK)
    goto out;

  item = photos_application_get_selection_or_active_item (self);
  g_return_if_fail (item != NULL);

  new_state = g_variant_new ("b", FALSE);
  g_action_change_state (G_ACTION (self->selection_mode_action), new_state);

  pictures_path = g_get_user_special_dir (G_USER_DIRECTORY_PICTURES);
  export_path = g_build_filename (pictures_path, PHOTOS_EXPORT_SUBPATH, NULL);
  export = g_file_new_for_path (export_path);

  error = NULL;
  if (!photos_glib_make_directory_with_parents (export, NULL, &error))
    {
      g_warning ("Unable to create %s: %s", export_path, error->message);
      photos_export_notification_new_with_error (error);
      g_error_free (error);
      goto out;
    }

  export_dir_name = photos_export_dialog_get_dir_name (PHOTOS_EXPORT_DIALOG (dialog));

  error = NULL;
  tmp = g_file_get_child_for_display_name (export, export_dir_name, &error);
  if (error != NULL)
    {
      g_warning ("Unable to get a child for %s: %s", export_dir_name, error->message);
      photos_export_notification_new_with_error (error);
      g_error_free (error);
      goto out;
    }

  g_object_unref (export);
  export = tmp;

  error = NULL;
  if (!photos_glib_make_directory_with_parents (export, NULL, &error))
    {
      g_warning ("Unable to create %s: %s", export_path, error->message);
      photos_export_notification_new_with_error (error);
      g_error_free (error);
      goto out;
    }

  zoom = photos_export_dialog_get_zoom (PHOTOS_EXPORT_DIALOG (dialog));

  g_application_hold (G_APPLICATION (self));
  photos_base_item_save_async (item, export, zoom, NULL, photos_application_save_save, self);

 out:
  g_free (export_path);
  g_clear_object (&export);
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
photos_application_set_bg_common_download (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosBaseItem *item = PHOTOS_BASE_ITEM (source_object);
  GError *error;
  GSettings *settings = G_SETTINGS (user_data);
  gchar *filename = NULL;

  error = NULL;
  filename = photos_base_item_download_finish (item, res, &error);
  if (error != NULL)
    {
      const gchar *uri;

      uri = photos_base_item_get_uri (item);
      g_warning ("Unable to extract the local filename for %s", uri);
      g_error_free (error);
      goto out;
    }

  g_settings_set_string (settings, DESKTOP_KEY_PICTURE_URI, filename);
  g_settings_set_enum (settings, DESKTOP_KEY_PICTURE_OPTIONS, G_DESKTOP_BACKGROUND_STYLE_ZOOM);
  g_settings_set_enum (settings, DESKTOP_KEY_COLOR_SHADING_TYPE, G_DESKTOP_BACKGROUND_SHADING_SOLID);
  g_settings_set_string (settings, DESKTOP_KEY_PRIMARY_COLOR, "#000000000000");
  g_settings_set_string (settings, DESKTOP_KEY_SECONDARY_COLOR, "#000000000000");

 out:
  g_free (filename);
  g_object_unref (settings);
}


static void
photos_application_set_bg_common (PhotosApplication *self, GVariant *parameter, gpointer user_data)
{
  GSimpleAction *action = G_SIMPLE_ACTION (user_data);
  GSettings *settings;
  PhotosBaseItem *item;

  item = PHOTOS_BASE_ITEM (photos_base_manager_get_active_object (self->state->item_mngr));
  g_return_if_fail (item != NULL);

  settings = G_SETTINGS (g_object_get_data (G_OBJECT (action), "settings"));
  photos_base_item_download_async (item, NULL, photos_application_set_bg_common_download, g_object_ref (settings));
}


static void
photos_application_share_share (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosApplication *self = PHOTOS_APPLICATION (user_data);
  PhotosSharePoint *share_point = PHOTOS_SHARE_POINT (source_object);
  GError *error = NULL;

  photos_share_point_share_finish (share_point, res, &error);
  if (error != NULL)
    {
      g_warning ("Unable to share the image: %s", error->message);
      photos_share_notification_new_with_error (share_point, error);
      g_error_free (error);
      goto out;
    }

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
  photos_share_point_share_async (share_point, item, NULL, photos_application_share_share, self);

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
  gchar *theme;

  g_object_get (settings, "gtk-theme-name", &theme, NULL);
  screen = gdk_screen_get_default ();

  if (g_strcmp0 (theme, "Adwaita") == 0)
    {
      if (provider == NULL)
        {
          GFile *file;

          provider = gtk_css_provider_new ();
          file = g_file_new_for_uri ("resource:///org/gnome/Photos/Adwaita.css");
          gtk_css_provider_load_from_file (provider, file, NULL);
          g_object_unref (file);
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

  g_free (theme);
}


static void
photos_application_window_mode_changed (PhotosApplication *self)
{
  photos_application_actions_update (self);
}


static void
photos_application_selection_changed (PhotosApplication *self)
{
  photos_application_actions_update (self);
}


static void
photos_application_selection_mode_notify_state (PhotosApplication *self)
{
  if (photos_utils_get_selection_mode ())
    {
      const gchar *selection_mode_accels[2] = {"Escape", NULL};
      gtk_application_set_accels_for_action (GTK_APPLICATION (self), "app.selection-mode", selection_mode_accels);
    }
  else
    {
      const gchar *selection_mode_accels[1] = {NULL};
      gtk_application_set_accels_for_action (GTK_APPLICATION (self), "app.selection-mode", selection_mode_accels);
    }

  photos_application_actions_update (self);
}


static void
photos_application_activate (GApplication *application)
{
  PhotosApplication *self = PHOTOS_APPLICATION (application);

  if (self->main_window == NULL)
    {
      if (!photos_application_create_window (self))
        return;

      photos_mode_controller_set_window_mode (self->state->mode_cntrlr, PHOTOS_WINDOW_MODE_OVERVIEW);
    }

  gtk_window_present_with_time (GTK_WINDOW (self->main_window), self->activation_timestamp);
  self->activation_timestamp = GDK_CURRENT_TIME;
}


static gboolean
photos_application_dbus_register (GApplication *application,
                                  GDBusConnection *connection,
                                  const gchar *object_path,
                                  GError **error)
{
  PhotosApplication *self = PHOTOS_APPLICATION (application);
  gboolean ret_val = FALSE;
  gchar *search_provider_path = NULL;

  if (!G_APPLICATION_CLASS (photos_application_parent_class)->dbus_register (application,
                                                                             connection,
                                                                             object_path,
                                                                             error))
    goto out;

  search_provider_path = g_strconcat (object_path, PHOTOS_SEARCH_PROVIDER_PATH_SUFFIX, NULL);
  if (!photos_search_provider_dbus_export (self->search_provider, connection, search_provider_path, error))
    {
      g_clear_object (&self->search_provider);
      goto out;
    }

  ret_val = TRUE;

 out:
  g_free (search_provider_path);
  return ret_val;
}


static void
photos_application_dbus_unregister (GApplication *application,
                                    GDBusConnection *connection,
                                    const gchar *object_path)
{
  PhotosApplication *self = PHOTOS_APPLICATION (application);

  if (self->search_provider != NULL)
    {
      gchar *search_provider_path = NULL;

      search_provider_path = g_strconcat (object_path, PHOTOS_SEARCH_PROVIDER_PATH_SUFFIX, NULL);
      photos_search_provider_dbus_unexport (self->search_provider, connection, search_provider_path);
      g_clear_object (&self->search_provider);
      g_free (search_provider_path);
    }

  G_APPLICATION_CLASS (photos_application_parent_class)->dbus_unregister (application, connection, object_path);
}


static gint
photos_application_handle_local_options (GApplication *application, GVariantDict *options)
{
  gint ret_val = -1;

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
  GError *error;
  GSimpleAction *action;
  GrlRegistry *registry;
  GtkIconTheme *icon_theme;
  GtkSettings *settings;
  GVariant *state;
  GVariantType *parameter_type;
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

  G_APPLICATION_CLASS (photos_application_parent_class)->startup (application);

  gegl_init (NULL, NULL);

  grl_init (NULL, NULL);
  registry = grl_registry_get_default ();

  error = NULL;
  if (!grl_registry_load_all_plugins (registry, FALSE, &error))
    {
      g_warning ("Unable to load Grilo plugins: %s", error->message);
      g_error_free (error);
    }
  else
    {
      error = NULL;
      if (!grl_registry_activate_plugin_by_id (registry, "grl-flickr", &error))
        {
          g_warning ("Unable to activate Grilo's Flickr plugin: %s", error->message);
          g_error_free (error);
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

  action = g_simple_action_new ("about", NULL);
  g_signal_connect_swapped (action, "activate", G_CALLBACK (photos_application_about), self);
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (action));
  g_object_unref (action);

  parameter_type = g_variant_type_new ("a{sd}");
  self->blacks_exposure_action = g_simple_action_new ("blacks-exposure-current", parameter_type);
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (self->blacks_exposure_action));
  g_variant_type_free (parameter_type);

  parameter_type = g_variant_type_new ("a{sd}");
  self->brightness_contrast_action = g_simple_action_new ("brightness-contrast-current", parameter_type);
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (self->brightness_contrast_action));
  g_variant_type_free (parameter_type);

  parameter_type = g_variant_type_new ("a{sd}");
  self->crop_action = g_simple_action_new ("crop-current", parameter_type);
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (self->crop_action));
  g_variant_type_free (parameter_type);

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
  g_signal_connect (self->gear_action, "activate", G_CALLBACK (photos_application_action_toggle), self);
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (self->gear_action));

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

  action = g_simple_action_new ("quit", NULL);
  g_signal_connect_swapped (action, "activate", G_CALLBACK (photos_application_quit), self);
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (action));
  g_object_unref (action);

  action = g_simple_action_new ("remote-display-current", NULL);
  g_signal_connect_swapped (action, "activate", G_CALLBACK (photos_application_remote_display_current), self);
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (action));
  g_object_unref (action);

  self->saturation_action = g_simple_action_new ("saturation-current", G_VARIANT_TYPE_DOUBLE);
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (self->saturation_action));

  self->save_action = g_simple_action_new ("save-current", NULL);
  g_signal_connect_swapped (self->save_action, "activate", G_CALLBACK (photos_application_save), self);
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (self->save_action));

  state = g_variant_new ("b", FALSE);
  self->search_action = g_simple_action_new_stateful ("search", NULL, state);
  g_signal_connect (self->search_action, "activate", G_CALLBACK (photos_application_action_toggle), self);
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (self->search_action));

  state = g_variant_new ("s", PHOTOS_SEARCH_MATCH_STOCK_ALL);
  self->search_match_action = g_simple_action_new_stateful ("search-match", G_VARIANT_TYPE_STRING, state);
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (self->search_match_action));

  state = g_variant_new ("s", PHOTOS_SOURCE_STOCK_ALL);
  self->search_source_action = g_simple_action_new_stateful ("search-source", G_VARIANT_TYPE_STRING, state);
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (self->search_source_action));

  state = g_variant_new ("s", PHOTOS_SEARCH_TYPE_STOCK_ALL);
  self->search_type_action = g_simple_action_new_stateful ("search-type", G_VARIANT_TYPE_STRING, state);
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (self->search_type_action));

  self->sel_all_action = g_simple_action_new ("select-all", NULL);
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (self->sel_all_action));

  self->sel_none_action = g_simple_action_new ("select-none", NULL);
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (self->sel_none_action));

  state = g_variant_new ("b", FALSE);
  self->selection_mode_action = g_simple_action_new_stateful ("selection-mode", NULL, state);
  g_signal_connect (self->selection_mode_action, "activate", G_CALLBACK (photos_application_action_toggle), self);
  g_signal_connect_swapped (self->selection_mode_action,
                            "notify::state",
                            G_CALLBACK (photos_application_selection_mode_notify_state),
                            self);
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (self->selection_mode_action));

  self->set_bg_action = g_simple_action_new ("set-background", NULL);
  g_object_set_data_full (G_OBJECT (self->set_bg_action),
                          "settings",
                          g_object_ref (self->bg_settings),
                          g_object_unref);
  g_signal_connect_swapped (self->set_bg_action, "activate", G_CALLBACK (photos_application_set_bg_common), self);
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (self->set_bg_action));

  self->set_ss_action = g_simple_action_new ("set-screensaver", NULL);
  g_object_set_data_full (G_OBJECT (self->set_ss_action),
                          "settings",
                          g_object_ref (self->ss_settings),
                          g_object_unref);
  g_signal_connect_swapped (self->set_ss_action, "activate", G_CALLBACK (photos_application_set_bg_common), self);
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (self->set_ss_action));

  self->share_action = g_simple_action_new ("share-current", NULL);
  g_signal_connect_swapped (self->share_action, "activate", G_CALLBACK (photos_application_share_current), self);
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (self->share_action));

  self->sharpen_action = g_simple_action_new ("sharpen-current", G_VARIANT_TYPE_DOUBLE);
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (self->sharpen_action));

  self->zoom_best_fit_action = g_simple_action_new ("zoom-best-fit", NULL);
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (self->zoom_best_fit_action));

  self->zoom_in_action = g_simple_action_new ("zoom-in", NULL);
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (self->zoom_in_action));

  self->zoom_out_action = g_simple_action_new ("zoom-out", NULL);
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (self->zoom_out_action));

  g_signal_connect_swapped (self->state->mode_cntrlr,
                            "window-mode-changed",
                            G_CALLBACK (photos_application_window_mode_changed),
                            self);

  action = g_simple_action_new ("help", NULL);
  g_signal_connect_swapped (action, "activate", G_CALLBACK (photos_application_help), self);
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (action));
  g_object_unref (action);

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
  gtk_application_set_accels_for_action (GTK_APPLICATION (self), "app.zoom-in", zoom_in_accels);
  gtk_application_set_accels_for_action (GTK_APPLICATION (self), "app.zoom-out", zoom_out_accels);

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
photos_application_dispose (GObject *object)
{
  PhotosApplication *self = PHOTOS_APPLICATION (object);

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
  g_clear_object (&self->brightness_contrast_action);
  g_clear_object (&self->crop_action);
  g_clear_object (&self->delete_action);
  g_clear_object (&self->denoise_action);
  g_clear_object (&self->edit_action);
  g_clear_object (&self->edit_cancel_action);
  g_clear_object (&self->edit_done_action);
  g_clear_object (&self->edit_revert_action);
  g_clear_object (&self->fs_action);
  g_clear_object (&self->gear_action);
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
  g_clear_object (&self->share_action);
  g_clear_object (&self->sharpen_action);
  g_clear_object (&self->zoom_best_fit_action);
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
  photos_utils_ensure_builtins ();

  self->search_provider = photos_search_provider_new ();
  g_signal_connect_swapped (self->search_provider,
                            "activate-result",
                            G_CALLBACK (photos_application_activate_result),
                            self);
  g_signal_connect_swapped (self->search_provider,
                            "launch-search",
                            G_CALLBACK (photos_application_launch_search),
                            self);

  self->state = photos_search_context_state_new (PHOTOS_SEARCH_CONTEXT (self));
  self->activation_timestamp = GDK_CURRENT_TIME;

  g_application_add_main_option_entries (G_APPLICATION (self), COMMAND_LINE_OPTIONS);
}


static void
photos_application_class_init (PhotosApplicationClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GApplicationClass *application_class = G_APPLICATION_CLASS (class);

  object_class->dispose = photos_application_dispose;
  object_class->finalize = photos_application_finalize;
  application_class->activate = photos_application_activate;
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
