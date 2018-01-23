/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2018 Red Hat, Inc.
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

#include <gio/gio.h>
#include <libtracker-control/tracker-control.h>

#include "photos-base-manager.h"
#include "photos-debug.h"
#include "photos-device-item.h"
#include "photos-item-manager.h"
#include "photos-offset-import-controller.h"
#include "photos-query-builder.h"
#include "photos-search-context.h"
#include "photos-tracker-import-controller.h"
#include "photos-utils.h"


struct _PhotosTrackerImportController
{
  PhotosTrackerController parent_instance;
  GCancellable *cancellable;
  GQueue *pending_directories;
  PhotosBaseManager *item_mngr;
  PhotosBaseManager *src_mngr;
  PhotosOffsetController *offset_cntrlr;
  TrackerMinerManager *manager;
};


G_DEFINE_TYPE_WITH_CODE (PhotosTrackerImportController,
                         photos_tracker_import_controller,
                         PHOTOS_TYPE_TRACKER_CONTROLLER,
                         photos_utils_ensure_extension_points ();
                         g_io_extension_point_implement (PHOTOS_TRACKER_CONTROLLER_EXTENSION_POINT_NAME,
                                                         g_define_type_id,
                                                         "import",
                                                         0));


static const gchar *IMPORTABLE_MIME_TYPES[] =
{
  "image/jpeg",
  "image/png",
  "image/x-dcraw"
};


static void photos_tracker_import_controller_enumerate_children (GObject *source_object,
                                                                 GAsyncResult *res,
                                                                 gpointer user_data);


static void
photos_tracker_import_controller_index (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  g_autoptr (GFile) file = G_FILE (user_data);
  TrackerMinerManager *manager = TRACKER_MINER_MANAGER (source_object);

  {
    g_autoptr (GError) error = NULL;

    if (!tracker_miner_manager_index_file_for_process_finish (manager, res, &error))
      {
        if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
          {
            g_autofree gchar *uri = NULL;

            uri = g_file_get_uri (file);
            g_warning ("Unable to index %s: %s", uri, error->message);
          }
      }
  }
}


static void
photos_tracker_import_controller_next_files (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosTrackerImportController *self;
  GFileEnumerator *enumerator = G_FILE_ENUMERATOR (source_object);
  GList *infos = NULL;
  GList *l;

  {
    g_autoptr (GError) error = NULL;

    infos = g_file_enumerator_next_files_finish (enumerator, res, &error);
    if (error != NULL)
      {
        GFile *directory;
        g_autofree gchar *uri = NULL;

        if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
          goto out;

        directory = g_file_enumerator_get_container (enumerator);
        uri = g_file_get_uri (directory);
        g_warning ("Unable to read files from %s: %s", uri, error->message);
      }
  }

  self = PHOTOS_TRACKER_IMPORT_CONTROLLER (user_data);

  if (infos == NULL)
    {
      GFile *directory;
      GFile *queue_head;

      directory = g_file_enumerator_get_container (enumerator);
      queue_head = G_FILE (g_queue_pop_head (self->pending_directories));
      g_assert_true (g_file_equal (directory, queue_head));

      if (!g_queue_is_empty (self->pending_directories))
        {
          g_autofree gchar *uri = NULL;

          directory = G_FILE (g_queue_peek_head (self->pending_directories));

          uri = g_file_get_uri (directory);
          photos_debug (PHOTOS_DEBUG_IMPORT, "Enumerating device directory %s", uri);

          g_file_enumerate_children_async (directory,
                                           G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE","
                                           G_FILE_ATTRIBUTE_STANDARD_NAME","
                                           G_FILE_ATTRIBUTE_STANDARD_TYPE,
                                           G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                           G_PRIORITY_DEFAULT,
                                           self->cancellable,
                                           photos_tracker_import_controller_enumerate_children,
                                           self);
        }
    }
  else
    {
      for (l = infos; l != NULL; l = l->next)
        {
          g_autoptr (GFile) file = NULL;
          GFileInfo *info = G_FILE_INFO (l->data);
          GFileType file_type;
          g_autofree gchar *uri = NULL;

          file = g_file_enumerator_get_child (enumerator, info);
          uri = g_file_get_uri (file);

          file_type = g_file_info_get_file_type (info);

          switch (file_type)
            {
            case G_FILE_TYPE_DIRECTORY:
              photos_debug (PHOTOS_DEBUG_IMPORT, "Queued device directory %s", uri);
              g_queue_push_tail (self->pending_directories, g_object_ref (file));
              break;

            case G_FILE_TYPE_REGULAR:
              {
                const gchar *mime_type;
                gboolean indexing = FALSE;
                guint i;
                guint n_elements;

                mime_type = g_file_info_get_content_type (info);
                n_elements = G_N_ELEMENTS (IMPORTABLE_MIME_TYPES);
                for (i = 0; i < n_elements && !indexing; i++)
                  {
                    if (g_content_type_equals (mime_type, IMPORTABLE_MIME_TYPES[i])
                        || g_content_type_is_a (mime_type, IMPORTABLE_MIME_TYPES[i]))
                      {
                        tracker_miner_manager_index_file_for_process_async (self->manager,
                                                                            file,
                                                                            self->cancellable,
                                                                            photos_tracker_import_controller_index,
                                                                            g_object_ref (file));
                        indexing = TRUE;
                      }
                  }

                photos_debug (PHOTOS_DEBUG_IMPORT, "%s device file %s", indexing ? "Indexing" : "Skipped", uri);
                break;
              }

            case G_FILE_TYPE_UNKNOWN:
            case G_FILE_TYPE_MOUNTABLE:
            case G_FILE_TYPE_SHORTCUT:
            case G_FILE_TYPE_SPECIAL:
            case G_FILE_TYPE_SYMBOLIC_LINK:
            default:
              break;
            }
        }

      g_file_enumerator_next_files_async (enumerator,
                                          5,
                                          G_PRIORITY_DEFAULT,
                                          self->cancellable,
                                          photos_tracker_import_controller_next_files,
                                          self);
    }

 out:
  g_list_free_full (infos, g_object_unref);
}


static void
photos_tracker_import_controller_enumerate_children (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosTrackerImportController *self;
  GFile *directory = G_FILE (source_object);
  g_autoptr (GFileEnumerator) enumerator = NULL;

  {
    g_autoptr (GError) error = NULL;

    enumerator = g_file_enumerate_children_finish (directory, res, &error);
    if (error != NULL)
      {
        if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
          {
            g_autofree gchar *uri = NULL;

            uri = g_file_get_uri (directory);
            g_warning ("Unable to get an enumerator for %s: %s", uri, error->message);
          }

        goto out;
      }
  }

  self = PHOTOS_TRACKER_IMPORT_CONTROLLER (user_data);

  g_file_enumerator_next_files_async (enumerator,
                                      5,
                                      G_PRIORITY_DEFAULT,
                                      self->cancellable,
                                      photos_tracker_import_controller_next_files,
                                      self);

 out:
  return;
}


static void
photos_tracker_import_controller_source_active_changed (PhotosTrackerImportController *self, GObject *source)
{
  GMount *mount;
  gboolean frozen;

  g_return_if_fail (PHOTOS_IS_TRACKER_IMPORT_CONTROLLER (self));
  g_return_if_fail (PHOTOS_IS_SOURCE (source));
  g_return_if_fail (PHOTOS_IS_ITEM_MANAGER (self->item_mngr));

  mount = photos_source_get_mount (PHOTOS_SOURCE (source));
  frozen = mount == NULL;
  photos_tracker_controller_set_frozen (PHOTOS_TRACKER_CONTROLLER (self), frozen);

  if (mount == NULL)
    {
      g_cancellable_cancel (self->cancellable);
      g_clear_object (&self->cancellable);
      self->cancellable = g_cancellable_new ();

      g_queue_free_full (self->pending_directories, g_object_unref);
      self->pending_directories = g_queue_new ();

      photos_item_manager_clear (PHOTOS_ITEM_MANAGER (self->item_mngr), PHOTOS_WINDOW_MODE_IMPORT);
    }
  else
    {
      g_return_if_fail (g_queue_is_empty (self->pending_directories));

      if (G_LIKELY (self->manager != NULL))
        {
          g_autoptr (GFile) root = NULL;
          g_autofree gchar *uri = NULL;

          root = g_mount_get_root (mount);
          g_queue_push_tail (self->pending_directories, g_object_ref (root));

          uri = g_file_get_uri (root);
          photos_debug (PHOTOS_DEBUG_IMPORT, "Enumerating device directory %s", uri);

          g_file_enumerate_children_async (root,
                                           G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE","
                                           G_FILE_ATTRIBUTE_STANDARD_NAME","
                                           G_FILE_ATTRIBUTE_STANDARD_TYPE,
                                           G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                           G_PRIORITY_DEFAULT,
                                           self->cancellable,
                                           photos_tracker_import_controller_enumerate_children,
                                           self);
        }

      photos_tracker_controller_refresh_for_object (PHOTOS_TRACKER_CONTROLLER (self));
    }
}


static PhotosOffsetController *
photos_tracker_import_controller_get_offset_controller (PhotosTrackerController *trk_cntrlr)
{
  PhotosTrackerImportController *self = PHOTOS_TRACKER_IMPORT_CONTROLLER (trk_cntrlr);
  return g_object_ref (self->offset_cntrlr);
}


static PhotosQuery *
photos_tracker_import_controller_get_query (PhotosTrackerController *trk_cntrlr)
{
  PhotosTrackerImportController *self = PHOTOS_TRACKER_IMPORT_CONTROLLER (trk_cntrlr);
  GApplication *app;
  PhotosSearchContextState *state;

  app = g_application_get_default ();
  state = photos_search_context_get_state (PHOTOS_SEARCH_CONTEXT (app));

  return photos_query_builder_global_query (state, PHOTOS_QUERY_FLAGS_IMPORT, self->offset_cntrlr);
}


static GObject *
photos_tracker_import_controller_constructor (GType type,
                                              guint n_construct_params,
                                              GObjectConstructParam *construct_params)
{
  static GObject *self = NULL;

  if (self == NULL)
    {
      self = G_OBJECT_CLASS (photos_tracker_import_controller_parent_class)->constructor (type,
                                                                                          n_construct_params,
                                                                                          construct_params);
      g_object_add_weak_pointer (self, (gpointer) &self);
      return self;
    }

  return g_object_ref (self);
}


static void
photos_tracker_import_controller_dispose (GObject *object)
{
  PhotosTrackerImportController *self = PHOTOS_TRACKER_IMPORT_CONTROLLER (object);

  if (self->cancellable != NULL)
    {
      g_cancellable_cancel (self->cancellable);
      g_clear_object (&self->cancellable);
    }

  if (self->pending_directories != NULL)
    {
      g_queue_free_full (self->pending_directories, g_object_unref);
      self->pending_directories = NULL;
    }

  g_clear_object (&self->src_mngr);
  g_clear_object (&self->offset_cntrlr);
  g_clear_object (&self->manager);

  G_OBJECT_CLASS (photos_tracker_import_controller_parent_class)->dispose (object);
}


static void
photos_tracker_import_controller_finalize (GObject *object)
{
  PhotosTrackerImportController *self = PHOTOS_TRACKER_IMPORT_CONTROLLER (object);

  if (self->item_mngr != NULL)
    g_object_remove_weak_pointer (G_OBJECT (self->item_mngr), (gpointer *) &self->item_mngr);

  G_OBJECT_CLASS (photos_tracker_import_controller_parent_class)->finalize (object);
}


static void
photos_tracker_import_controller_init (PhotosTrackerImportController *self)
{
  GApplication *app;
  PhotosSearchContextState *state;

  app = g_application_get_default ();
  state = photos_search_context_get_state (PHOTOS_SEARCH_CONTEXT (app));

  self->cancellable = g_cancellable_new ();
  self->pending_directories = g_queue_new ();

  self->item_mngr = state->item_mngr;
  g_object_add_weak_pointer (G_OBJECT (self->item_mngr), (gpointer *) &self->item_mngr);

  self->src_mngr = g_object_ref (state->src_mngr);
  g_signal_connect_object (self->src_mngr,
                           "active-changed",
                           G_CALLBACK (photos_tracker_import_controller_source_active_changed),
                           self,
                           G_CONNECT_AFTER | G_CONNECT_SWAPPED);

  self->offset_cntrlr = photos_offset_import_controller_dup_singleton ();

  {
    g_autoptr (GError) error = NULL;

    self->manager = tracker_miner_manager_new_full (FALSE, &error);
    if (error != NULL)
      g_warning ("Unable to create a TrackerMinerManager, indexing attached devices won't work: %s", error->message);
  }
}


static void
photos_tracker_import_controller_class_init (PhotosTrackerImportControllerClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  PhotosTrackerControllerClass *tracker_controller_class = PHOTOS_TRACKER_CONTROLLER_CLASS (class);

  tracker_controller_class->base_item_type = PHOTOS_TYPE_DEVICE_ITEM;

  object_class->constructor = photos_tracker_import_controller_constructor;
  object_class->dispose = photos_tracker_import_controller_dispose;
  object_class->finalize = photos_tracker_import_controller_finalize;
  tracker_controller_class->get_offset_controller = photos_tracker_import_controller_get_offset_controller;
  tracker_controller_class->get_query = photos_tracker_import_controller_get_query;
}


PhotosTrackerController *
photos_tracker_import_controller_dup_singleton (void)
{
  return g_object_new (PHOTOS_TYPE_TRACKER_IMPORT_CONTROLLER,
                       "delay-start", TRUE,
                       "mode", PHOTOS_WINDOW_MODE_IMPORT,
                       NULL);
}
