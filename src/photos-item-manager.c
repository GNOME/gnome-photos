/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2012 – 2021 Red Hat, Inc.
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

#include <string.h>

#include <gio/gio.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <tracker-sparql.h>

#include "photos-application.h"
#include "photos-debug.h"
#include "photos-device-item.h"
#include "photos-enums.h"
#include "photos-filterable.h"
#include "photos-item-manager.h"
#include "photos-local-item.h"
#include "photos-marshalers.h"
#include "photos-query.h"
#include "photos-search-context.h"
#include "photos-single-item-job.h"
#include "photos-tracker-queue.h"
#include "photos-utils.h"


struct _PhotosItemManager
{
  PhotosBaseManager parent_instance;
  GObject *active_object;
  GCancellable *loader_cancellable;
  GHashTable *collections;
  GHashTable *hidden_items;
  GHashTable *wait_for_changes_table;
  GIOExtensionPoint *extension_point;
  GQueue *history;
  PhotosBaseItem *active_collection;
  PhotosBaseManager **item_mngr_chldrn;
  PhotosLoadState load_state;
  PhotosTrackerQueue *queue;
  PhotosWindowMode mode;
  TrackerNotifier *notifier;
  gboolean fullscreen;
  gboolean *constrain_additions;
  guint wait_for_changes_id;
};

enum
{
  ACTIVE_COLLECTION_CHANGED,
  CAN_FULLSCREEN_CHANGED,
  FULLSCREEN_CHANGED,
  LOAD_ERROR,
  LOAD_FINISHED,
  LOAD_STARTED,
  WINDOW_MODE_CHANGED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static void photos_item_manager_list_model_iface_init (GListModelInterface *iface);


G_DEFINE_TYPE_WITH_CODE (PhotosItemManager, photos_item_manager, PHOTOS_TYPE_BASE_MANAGER,
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, photos_item_manager_list_model_iface_init));

typedef struct _PhotosItemManagerHiddenItem PhotosItemManagerHiddenItem;

struct _PhotosItemManagerHiddenItem
{
  PhotosBaseItem *item;
  gboolean *modes;
  guint n_modes;
};


enum
{
  WAIT_FOR_CHANGES_TIMEOUT = 1 /* s */
};


static gboolean photos_item_manager_wait_for_changes_timeout (gpointer user_data);


static PhotosItemManagerHiddenItem *
photos_item_manager_hidden_item_new (PhotosBaseItem *item)
{
  GEnumClass *window_mode_class; /* TODO: use g_autoptr */
  PhotosItemManagerHiddenItem *hidden_item;

  hidden_item = g_slice_new0 (PhotosItemManagerHiddenItem);
  hidden_item->item = g_object_ref (item);

  window_mode_class = G_ENUM_CLASS (g_type_class_ref (PHOTOS_TYPE_WINDOW_MODE));
  hidden_item->n_modes = window_mode_class->n_values;
  hidden_item->modes = (gboolean *) g_malloc0_n (hidden_item->n_modes, sizeof (gboolean));

  g_type_class_unref (window_mode_class);
  return hidden_item;
}


static void
photos_item_manager_hidden_item_free (PhotosItemManagerHiddenItem *hidden_item)
{
  g_free (hidden_item->modes);
  g_object_unref (hidden_item->item);
  g_slice_free (PhotosItemManagerHiddenItem, hidden_item);
}


static void
photos_item_manager_add_object (PhotosBaseManager *mngr, GObject *object)
{
  g_assert_not_reached ();
}


static gboolean
photos_item_manager_can_add_mtime_for_mode (PhotosItemManager *self, gint64 mtime, PhotosWindowMode mode)
{
  g_autoptr (PhotosBaseItem) oldest_item = NULL;
  PhotosBaseManager *item_mngr_chld;
  gboolean ret_val = TRUE;
  gint64 oldest_mtime;
  guint n_items;

  if (!self->constrain_additions[mode])
    goto out;

  item_mngr_chld = self->item_mngr_chldrn[mode];
  n_items = g_list_model_get_n_items (G_LIST_MODEL (item_mngr_chld));
  if (n_items == 0)
    goto out;

  oldest_item = PHOTOS_BASE_ITEM (g_list_model_get_object (G_LIST_MODEL (item_mngr_chld), n_items - 1));
  oldest_mtime = photos_base_item_get_mtime (oldest_item);

  if (mtime > oldest_mtime)
    goto out;

  ret_val = FALSE;

 out:
  return ret_val;
}


static gboolean
photos_item_manager_can_add_cursor_for_mode (PhotosItemManager *self,
                                             TrackerSparqlCursor *cursor,
                                             PhotosWindowMode mode)
{
  gboolean ret_val = TRUE;
  gint64 mtime;

  mtime = photos_utils_get_mtime_from_sparql_cursor (cursor);
  ret_val = photos_item_manager_can_add_mtime_for_mode (self, mtime, mode);
  return ret_val;
}


static gboolean
photos_item_manager_can_add_item_for_mode (PhotosItemManager *self, PhotosBaseItem *item, PhotosWindowMode mode)
{
  gboolean ret_val = TRUE;
  gint64 mtime;

  mtime = photos_base_item_get_mtime (item);
  ret_val = photos_item_manager_can_add_mtime_for_mode (self, mtime, mode);
  return ret_val;
}


static gboolean
photos_item_manager_cursor_is_collection (TrackerSparqlCursor *cursor)
{
  gboolean ret_val = FALSE;
  const gchar *rdf_type;

  rdf_type = tracker_sparql_cursor_get_string (cursor, PHOTOS_QUERY_COLUMNS_RDF_TYPE, NULL);
  if (rdf_type == NULL)
    goto out;

  ret_val = strstr (rdf_type, "nfo#DataContainer") != NULL;

 out:
  return ret_val;
}


static gboolean
photos_item_manager_try_to_add_item_for_mode (PhotosItemManager *self,
                                              PhotosBaseItem *item,
                                              PhotosWindowMode mode)
{
  gboolean ret_val = FALSE;

  g_return_val_if_fail (PHOTOS_IS_ITEM_MANAGER (self), FALSE);
  g_return_val_if_fail (PHOTOS_IS_BASE_ITEM (item), FALSE);
  g_return_val_if_fail (mode != PHOTOS_WINDOW_MODE_NONE, FALSE);
  g_return_val_if_fail (mode != PHOTOS_WINDOW_MODE_EDIT, FALSE);
  g_return_val_if_fail (mode != PHOTOS_WINDOW_MODE_PREVIEW, FALSE);

  if (!photos_item_manager_can_add_item_for_mode (self, item, mode))
    goto out;

  photos_base_manager_add_object (self->item_mngr_chldrn[mode], G_OBJECT (item));
  ret_val = TRUE;

 out:
  return ret_val;
}


static void
photos_item_manager_info_updated (PhotosBaseItem *item, gpointer user_data)
{
  PhotosItemManager *self = PHOTOS_ITEM_MANAGER (user_data);
  GType base_item_type;
  PhotosBaseItem *updated_item;
  gboolean is_collection;
  gboolean is_favorite;
  const gchar *id;

  g_return_if_fail (PHOTOS_IS_BASE_ITEM (item));

  id = photos_filterable_get_id (PHOTOS_FILTERABLE (item));
  updated_item = PHOTOS_BASE_ITEM (photos_base_manager_get_object_by_id (PHOTOS_BASE_MANAGER (self), id));
  g_return_if_fail (updated_item == item);

  base_item_type = G_OBJECT_TYPE (item);
  if (base_item_type == PHOTOS_TYPE_DEVICE_ITEM)
    goto out;

  is_collection = photos_base_item_is_collection (item);
  is_favorite = photos_base_item_is_favorite (item);

  if (is_collection)
    {
      photos_item_manager_try_to_add_item_for_mode (self, item, PHOTOS_WINDOW_MODE_COLLECTIONS);
    }
  else
    {
      if (is_favorite)
        photos_item_manager_try_to_add_item_for_mode (self, item, PHOTOS_WINDOW_MODE_FAVORITES);

      photos_item_manager_try_to_add_item_for_mode (self, item, PHOTOS_WINDOW_MODE_OVERVIEW);
    }

  if (is_collection)
    {
      photos_base_manager_remove_object (self->item_mngr_chldrn[PHOTOS_WINDOW_MODE_COLLECTION_VIEW],
                                         G_OBJECT (item));
      photos_base_manager_remove_object (self->item_mngr_chldrn[PHOTOS_WINDOW_MODE_FAVORITES], G_OBJECT (item));
      photos_base_manager_remove_object (self->item_mngr_chldrn[PHOTOS_WINDOW_MODE_OVERVIEW], G_OBJECT (item));
    }
  else
    {
      photos_base_manager_remove_object (self->item_mngr_chldrn[PHOTOS_WINDOW_MODE_COLLECTIONS], G_OBJECT (item));

      if (!is_favorite)
        photos_base_manager_remove_object (self->item_mngr_chldrn[PHOTOS_WINDOW_MODE_FAVORITES], G_OBJECT (item));
    }

 out:
  return;
}


static void
photos_item_manager_add_cursor_for_mode (PhotosItemManager *self,
                                         GType base_item_type,
                                         TrackerSparqlCursor *cursor,
                                         PhotosWindowMode mode,
                                         gboolean force)
{
  g_autoptr (PhotosBaseItem) item = NULL;
  PhotosBaseManager *item_mngr_chld;
  gboolean is_collection;
  const gchar *id;

  g_return_if_fail (PHOTOS_IS_ITEM_MANAGER (self));
  g_return_if_fail (base_item_type == G_TYPE_NONE
                    || (base_item_type != PHOTOS_TYPE_BASE_ITEM
                        && g_type_is_a (base_item_type, PHOTOS_TYPE_BASE_ITEM)));
  g_return_if_fail (TRACKER_IS_SPARQL_CURSOR (cursor));
  g_return_if_fail (mode != PHOTOS_WINDOW_MODE_NONE);
  g_return_if_fail (mode != PHOTOS_WINDOW_MODE_EDIT);
  g_return_if_fail (mode != PHOTOS_WINDOW_MODE_PREVIEW);

  is_collection = photos_item_manager_cursor_is_collection (cursor);
  g_return_if_fail ((is_collection
                     && (mode == PHOTOS_WINDOW_MODE_COLLECTIONS || mode == PHOTOS_WINDOW_MODE_SEARCH))
                    || (!is_collection && mode != PHOTOS_WINDOW_MODE_COLLECTIONS));

  if (!force && !photos_item_manager_can_add_cursor_for_mode (self, cursor, mode))
    goto out;

  item_mngr_chld = self->item_mngr_chldrn[mode];
  id = tracker_sparql_cursor_get_string (cursor, PHOTOS_QUERY_COLUMNS_URN, NULL);

  item = PHOTOS_BASE_ITEM (photos_base_manager_get_object_by_id (item_mngr_chld, id));
  if (item != NULL)
    {
      g_object_ref (item);
    }
  else
    {
      gboolean already_present = FALSE;

      item = PHOTOS_BASE_ITEM (photos_base_manager_get_object_by_id (PHOTOS_BASE_MANAGER (self), id));
      if (item != NULL)
        {
          g_object_ref (item);
          already_present = TRUE;
        }
      else
        {
          item = photos_item_manager_create_item (self, base_item_type, cursor, TRUE);
          if (photos_base_item_is_collection (item))
            g_hash_table_insert (self->collections, g_strdup (id), g_object_ref (item));

          g_signal_connect_object (item, "info-updated", G_CALLBACK (photos_item_manager_info_updated), self, 0);
        }

      photos_base_manager_add_object (item_mngr_chld, G_OBJECT (item));
      photos_base_manager_add_object (self->item_mngr_chldrn[0], G_OBJECT (item));

      if (!already_present)
        g_signal_emit_by_name (self, "object-added", G_OBJECT (item));
    }

 out:
  return;
}


static void
photos_item_manager_check_wait_for_changes (PhotosItemManager *self, const gchar *id, const gchar *uri)
{
  GList *l;
  GList *tasks;

  g_return_if_fail (id != NULL && id[0] != '\0');
  g_return_if_fail (uri != NULL && uri[0] != '\0');

  photos_debug (PHOTOS_DEBUG_TRACKER, "Detected changes to %s", uri);

  tasks = (GList *) g_hash_table_lookup (self->wait_for_changes_table, uri);
  for (l = tasks; l != NULL; l = l->next)
    {
      GTask *task = G_TASK (l->data);
      g_task_return_pointer (task, g_strdup (id), g_free);
    }

  g_hash_table_remove (self->wait_for_changes_table, uri);
}


static void
photos_item_manager_item_created_executed_import (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  g_autoptr (PhotosItemManager) self = PHOTOS_ITEM_MANAGER (user_data);
  PhotosSingleItemJob *job_import = PHOTOS_SINGLE_ITEM_JOB (source_object);
  TrackerSparqlCursor *cursor = NULL; /* TODO: use g_autoptr */

  {
    g_autoptr (GError) error = NULL;

    cursor = photos_single_item_job_finish (job_import, res, &error);
    if (error != NULL)
      {
        g_warning ("Unable to query single item: %s", error->message);
        goto out;
      }
  }

  if (cursor == NULL)
    goto out;

  photos_item_manager_add_item_for_mode (self, PHOTOS_TYPE_DEVICE_ITEM, PHOTOS_WINDOW_MODE_IMPORT, cursor);

 out:
  g_clear_object (&cursor);
}


static void
photos_item_manager_item_created_executed_overview (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  g_autoptr (PhotosItemManager) self = PHOTOS_ITEM_MANAGER (user_data);
  PhotosSingleItemJob *job_overview = PHOTOS_SINGLE_ITEM_JOB (source_object);
  TrackerSparqlCursor *cursor = NULL; /* TODO: use g_autoptr */

  {
    g_autoptr (GError) error = NULL;

    cursor = photos_single_item_job_finish (job_overview, res, &error);
    if (error != NULL)
      {
        g_warning ("Unable to query single item: %s", error->message);
        goto out;
      }
  }

  if (cursor == NULL)
    goto out;

  photos_item_manager_add_item (self, G_TYPE_NONE, cursor, FALSE);

 out:
  g_clear_object (&cursor);
}


static void
photos_item_manager_item_created_executed_wait_for_changes (GObject *source_object,
                                                            GAsyncResult *res,
                                                            gpointer user_data)
{
  g_autoptr (PhotosItemManager) self = PHOTOS_ITEM_MANAGER (user_data);
  PhotosSingleItemJob *job_wait_for_changes = PHOTOS_SINGLE_ITEM_JOB (source_object);
  TrackerSparqlCursor *cursor = NULL; /* TODO: use g_autoptr */

  {
    g_autoptr (GError) error = NULL;

    cursor = photos_single_item_job_finish (job_wait_for_changes, res, &error);
    if (error != NULL)
      {
        g_warning ("Unable to query single item: %s", error->message);
        goto out;
      }
  }

  if (cursor == NULL)
    goto out;

  if (!photos_item_manager_cursor_is_collection (cursor))
    {
      const gchar *id;
      const gchar *uri;

      id = tracker_sparql_cursor_get_string (cursor, PHOTOS_QUERY_COLUMNS_URN, NULL);
      uri = tracker_sparql_cursor_get_string (cursor, PHOTOS_QUERY_COLUMNS_URI, NULL);
      if (id != NULL && id[0] != '\0' && uri != NULL && uri[0] != '\0')
        photos_item_manager_check_wait_for_changes (self, id, uri);
    }

 out:
  g_clear_object (&cursor);
}


static void
photos_item_manager_item_created (PhotosItemManager *self, const gchar *urn)
{
  GApplication *app;
  PhotosItemManagerHiddenItem *old_hidden_item;
  PhotosSearchContextState *state;
  g_autoptr (PhotosSingleItemJob) job_import = NULL;
  g_autoptr (PhotosSingleItemJob) job_overview = NULL;
  guint wait_for_changes_size;

  old_hidden_item = (PhotosItemManagerHiddenItem *) g_hash_table_lookup (self->hidden_items, urn);
  g_return_if_fail (old_hidden_item == NULL);

  app = g_application_get_default ();
  state = photos_search_context_get_state (PHOTOS_SEARCH_CONTEXT (app));

  job_import = photos_single_item_job_new (urn);
  photos_single_item_job_run (job_import,
                              state,
                              PHOTOS_QUERY_FLAGS_IMPORT,
                              NULL,
                              photos_item_manager_item_created_executed_import,
                              g_object_ref (self));

  job_overview = photos_single_item_job_new (urn);
  photos_single_item_job_run (job_overview,
                              state,
                              PHOTOS_QUERY_FLAGS_NONE,
                              NULL,
                              photos_item_manager_item_created_executed_overview,
                              g_object_ref (self));

  wait_for_changes_size = g_hash_table_size (self->wait_for_changes_table);
  if (wait_for_changes_size > 0)
    {
      g_autoptr (PhotosSingleItemJob) job_wait_for_changes = NULL;

      job_wait_for_changes = photos_single_item_job_new (urn);
      photos_single_item_job_run (job_wait_for_changes,
                                  state,
                                  PHOTOS_QUERY_FLAGS_UNFILTERED,
                                  NULL,
                                  photos_item_manager_item_created_executed_wait_for_changes,
                                  g_object_ref (self));
    }
}


static void
photos_item_manager_notifier_events_foreach (gpointer data, gpointer user_data)
{
  PhotosItemManager *self = PHOTOS_ITEM_MANAGER (user_data);
  TrackerNotifierEvent *event = (TrackerNotifierEvent *) data;
  TrackerNotifierEventType event_type;
  const gchar *event_urn;

  event_type = tracker_notifier_event_get_event_type (event);
  event_urn = tracker_notifier_event_get_urn (event);
  photos_debug (PHOTOS_DEBUG_TRACKER, "Received TrackerNotifierEvent (%d): URN %s", event_type, event_urn);

  g_return_if_fail (event_urn != NULL && event_urn[0] != '\0');

  if (event_type == TRACKER_NOTIFIER_EVENT_UPDATE)
    {
      GObject *object;

      object = photos_base_manager_get_object_by_id (PHOTOS_BASE_MANAGER (self), event_urn);
      if (object != NULL)
        {
          photos_base_item_refresh (PHOTOS_BASE_ITEM (object));

          if (!photos_base_item_is_collection (PHOTOS_BASE_ITEM (object)))
            {
              const gchar *uri;

              uri = photos_base_item_get_uri (PHOTOS_BASE_ITEM (object));
              photos_item_manager_check_wait_for_changes (self, event_urn, uri);
            }
        }
    }
  else if (event_type == TRACKER_NOTIFIER_EVENT_CREATE)
    {
      photos_item_manager_item_created (self, event_urn);
    }
  else if (event_type == TRACKER_NOTIFIER_EVENT_DELETE)
    {
      GObject *object;

      object = photos_base_manager_get_object_by_id (PHOTOS_BASE_MANAGER (self), event_urn);
      if (object != NULL)
        {
          photos_base_item_destroy (PHOTOS_BASE_ITEM (object));
          g_hash_table_remove (self->hidden_items, event_urn);
          photos_base_manager_remove_object_by_id (PHOTOS_BASE_MANAGER (self), event_urn);
        }
    }
}


static void
photos_item_manager_notifier_events (PhotosItemManager *self,
                                     const gchar *service,
                                     const gchar *graph,
                                     GPtrArray *events)
{
  if (g_strcmp0 (graph, PHOTOS_PICTURES_GRAPH) == 0)
    g_ptr_array_foreach (events, photos_item_manager_notifier_events_foreach, self);
}


static void
photos_item_manager_clear_active_item_load (PhotosItemManager *self)
{
  if (self->loader_cancellable != NULL)
    {
      g_cancellable_cancel (self->loader_cancellable);
      g_clear_object (&self->loader_cancellable);
    }
}


static gboolean
photos_item_manager_cursor_is_favorite (TrackerSparqlCursor *cursor)
{
  gboolean favorite;
  const gchar *rdf_type;

  favorite = tracker_sparql_cursor_get_boolean (cursor, PHOTOS_QUERY_COLUMNS_RESOURCE_FAVORITE);
  rdf_type = tracker_sparql_cursor_get_string (cursor, PHOTOS_QUERY_COLUMNS_RDF_TYPE, NULL);
  if (strstr (rdf_type, "nfo#DataContainer") != NULL)
    favorite = FALSE;

  return favorite;
}


static void
photos_item_manager_remove_timeout (PhotosItemManager *self)
{
  if (self->wait_for_changes_id != 0)
    {
      g_source_remove (self->wait_for_changes_id);
      self->wait_for_changes_id = 0;
    }
}


static void
photos_item_manager_wait_for_changes_timeout_cursor_next (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosItemManager *self = PHOTOS_ITEM_MANAGER (user_data);
  TrackerSparqlCursor *cursor = TRACKER_SPARQL_CURSOR (source_object);
  gboolean success;
  const gchar *id;
  const gchar *uri;
  guint wait_for_changes_size;

  {
    g_autoptr (GError) error = NULL;

    /* Note that tracker_sparql_cursor_next_finish can return FALSE even
     * without an error.
     */
    success = tracker_sparql_cursor_next_finish (cursor, res, &error);
    if (error != NULL)
      {
        g_warning ("Unable to fetch URN for URI: %s", error->message);
        goto out;
      }
  }

  if (!success)
    goto out;

  id = tracker_sparql_cursor_get_string (cursor, 0, NULL);
  uri = tracker_sparql_cursor_get_string (cursor, 1, NULL);
  if (id != NULL && id[0] != '\0' && uri != NULL && uri[0] != '\0')
    photos_item_manager_check_wait_for_changes (self, id, uri);

  photos_item_manager_remove_timeout (self);

  wait_for_changes_size = g_hash_table_size (self->wait_for_changes_table);
  if (wait_for_changes_size > 0)
    {
      self->wait_for_changes_id = g_timeout_add_seconds (WAIT_FOR_CHANGES_TIMEOUT,
                                                         photos_item_manager_wait_for_changes_timeout,
                                                         self);
    }

 out:
  g_object_unref (self);
}


static void
photos_item_manager_wait_for_changes_timeout_query_executed (GObject *source_object,
                                                             GAsyncResult *res,
                                                             gpointer user_data)
{
  PhotosItemManager *self = PHOTOS_ITEM_MANAGER (user_data);
  TrackerSparqlConnection *connection = TRACKER_SPARQL_CONNECTION (source_object);
  TrackerSparqlCursor *cursor = NULL; /* TODO: use g_autoptr */

  {
    g_autoptr (GError) error = NULL;

    cursor = tracker_sparql_connection_query_finish (connection, res, &error);
    if (error != NULL)
      {
        g_warning ("Unable to fetch URN for URI: %s", error->message);
        goto out;
      }
  }

  if (cursor == NULL)
    goto out;

  tracker_sparql_cursor_next_async (cursor,
                                    NULL,
                                    photos_item_manager_wait_for_changes_timeout_cursor_next,
                                    g_object_ref (self));

 out:
  g_clear_object (&cursor);
}


static gboolean
photos_item_manager_wait_for_changes_timeout (gpointer user_data)
{
  PhotosItemManager *self = PHOTOS_ITEM_MANAGER (user_data);
  GApplication *app;
  GHashTableIter iter;
  GList *tasks;
  const gchar *miner_files_name;
  const gchar *uri;

  if (G_UNLIKELY (self->queue == NULL))
    goto out;

  app = g_application_get_default ();
  miner_files_name = photos_application_get_miner_files_name (PHOTOS_APPLICATION (app));

  g_hash_table_iter_init (&iter, self->wait_for_changes_table);
  while (g_hash_table_iter_next (&iter, (gpointer *) &uri, (gpointer *) &tasks))
    {
      g_autoptr (PhotosQuery) query = NULL;
      g_autofree gchar *sparql = NULL;

      sparql = g_strdup_printf ("SELECT ?urn ?file WHERE {"
                                "  SERVICE <dbus:%s> {"
                                "    GRAPH tracker:Pictures {"
                                "      SELECT ?urn nie:isStoredAs (?urn) AS ?file WHERE {"
                                "        ?urn nie:isStoredAs '%s' "
                                "    }"
                                "  }"
                                "}",
                                miner_files_name,
                                uri);

      query = photos_query_new (NULL, sparql);
      photos_tracker_queue_select (self->queue,
                                   query,
                                   NULL,
                                   photos_item_manager_wait_for_changes_timeout_query_executed,
                                   g_object_ref (self),
                                   g_object_unref);
    }

 out:
  self->wait_for_changes_id = 0;
  return G_SOURCE_REMOVE;
}


static GObject *
photos_item_manager_get_active_object (PhotosBaseManager *mngr)
{
  PhotosItemManager *self = PHOTOS_ITEM_MANAGER (mngr);
  return self->active_object;
}


static gpointer
photos_item_manager_get_item (GListModel *list, guint position)
{
  PhotosItemManager *self = PHOTOS_ITEM_MANAGER (list);
  gpointer item;

  item = g_list_model_get_item (G_LIST_MODEL (self->item_mngr_chldrn[0]), position);
  return item;
}


static GType
photos_item_manager_get_item_type (GListModel *list)
{
  return PHOTOS_TYPE_BASE_ITEM;
}


static guint
photos_item_manager_get_n_items (GListModel *list)
{
  PhotosItemManager *self = PHOTOS_ITEM_MANAGER (list);
  guint n_items;

  n_items = g_list_model_get_n_items (G_LIST_MODEL (self->item_mngr_chldrn[0]));
  return n_items;
}


static GObject *
photos_item_manager_get_object_by_id (PhotosBaseManager *mngr, const gchar *id)
{
  PhotosItemManager *self = PHOTOS_ITEM_MANAGER (mngr);
  GObject *ret_val;

  ret_val = photos_base_manager_get_object_by_id (self->item_mngr_chldrn[0], id);
  return ret_val;
}


static gchar *
photos_item_manager_get_where (PhotosBaseManager *mngr, gint flags)
{
  PhotosItemManager *self = PHOTOS_ITEM_MANAGER (mngr);

  if (self->active_collection == NULL || (flags & PHOTOS_QUERY_FLAGS_SEARCH) != 0)
    return g_strdup ("");

  return photos_base_item_get_where (self->active_collection);
}


static void
photos_item_manager_item_load (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  g_autoptr (PhotosItemManager) self = PHOTOS_ITEM_MANAGER (user_data);
  g_autoptr (GeglNode) node = NULL;
  PhotosBaseItem *item = PHOTOS_BASE_ITEM (source_object);

  g_clear_object (&self->loader_cancellable);

  {
    g_autoptr (GError) error = NULL;

    node = photos_base_item_load_finish (item, res, &error);
    if (error != NULL)
      {
        g_autofree gchar *message = NULL;

        self->load_state = PHOTOS_LOAD_STATE_ERROR;

        if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
          {
            const gchar *domain_str;
            const gchar *name;

            domain_str = g_quark_to_string (error->domain);
            g_warning ("Unable to load item: (%s, %d) %s", domain_str, error->code, error->message);

            name = photos_base_item_get_name_with_fallback (item);
            message = g_strdup_printf (_("Oops! Unable to load “%s”"), name);
          }

        g_signal_emit (self, signals[LOAD_ERROR], 0, message, error);
        goto out;
      }
  }

  self->load_state = PHOTOS_LOAD_STATE_FINISHED;
  g_signal_emit (self, signals[LOAD_FINISHED], 0, item, node);

 out:
  return;
}


static void
photos_item_manager_items_changed (PhotosItemManager *self, guint position, guint removed, guint added)
{
  g_list_model_items_changed (G_LIST_MODEL (self), position, removed, added);
}


static void
photos_item_manager_remove_object_by_id (PhotosBaseManager *mngr, const gchar *id)
{
  PhotosItemManager *self = PHOTOS_ITEM_MANAGER (mngr);
  g_autoptr (PhotosBaseItem) item = NULL;
  guint i;

  g_hash_table_remove (self->collections, id);

  item = PHOTOS_BASE_ITEM (photos_base_manager_get_object_by_id (PHOTOS_BASE_MANAGER (self), id));
  if (item == NULL)
    return;

  g_signal_handlers_disconnect_by_func (item, photos_item_manager_info_updated, self);
  g_object_ref (item);

  for (i = 0; self->item_mngr_chldrn[i] != NULL; i++)
    photos_base_manager_remove_object_by_id (self->item_mngr_chldrn[i], id);

  g_signal_emit_by_name (self, "object-removed", G_OBJECT (item));
}


static void
photos_item_manager_update_fullscreen (PhotosItemManager *self)
{
  /* Should be called after priv->mode has been updated. */

  if (!photos_mode_controller_get_can_fullscreen (self) && self->fullscreen)
    photos_mode_controller_set_fullscreen (self, FALSE);

  g_signal_emit (self, signals[CAN_FULLSCREEN_CHANGED], 0);
}


static gboolean
photos_item_manager_set_window_mode_internal (PhotosItemManager *self,
                                              PhotosWindowMode mode,
                                              PhotosWindowMode *out_old_mode)
{
  PhotosWindowMode old_mode;
  gboolean ret_val = FALSE;

  old_mode = self->mode;

  if (old_mode == mode)
    goto out;

  g_queue_push_head (self->history, GINT_TO_POINTER (old_mode));
  self->mode = mode;

  if (out_old_mode != NULL)
    *out_old_mode = old_mode;

  ret_val = TRUE;

 out:
  return ret_val;
}


static gboolean
photos_item_manager_set_active_object (PhotosBaseManager *manager, GObject *object)
{
  PhotosItemManager *self = PHOTOS_ITEM_MANAGER (manager);
  PhotosWindowMode old_mode;
  gboolean active_collection_changed = FALSE;
  gboolean is_collection = FALSE;
  gboolean ret_val = FALSE;
  gboolean start_loading = FALSE;
  gboolean window_mode_changed = FALSE;

  g_return_val_if_fail (object != NULL, FALSE);
  g_return_val_if_fail (PHOTOS_IS_BASE_ITEM (object), FALSE);
  g_return_val_if_fail (self->mode != PHOTOS_WINDOW_MODE_EDIT, FALSE);
  g_return_val_if_fail (self->mode != PHOTOS_WINDOW_MODE_IMPORT, FALSE);

  is_collection = photos_base_item_is_collection (PHOTOS_BASE_ITEM (object));
  if (is_collection)
    g_return_val_if_fail (self->active_collection == NULL, FALSE);

  if (object == self->active_object)
    goto out;

  photos_item_manager_clear_active_item_load (self);

  if (is_collection)
    {
      window_mode_changed = photos_item_manager_set_window_mode_internal (self,
                                                                          PHOTOS_WINDOW_MODE_COLLECTION_VIEW,
                                                                          &old_mode);
      g_assert_true (window_mode_changed);

      g_assert_null (self->active_collection);
      self->active_collection = g_object_ref (PHOTOS_BASE_ITEM (object));
      self->load_state = PHOTOS_LOAD_STATE_NONE;
      active_collection_changed = TRUE;
    }
  else
    {
      window_mode_changed = photos_item_manager_set_window_mode_internal (self,
                                                                          PHOTOS_WINDOW_MODE_PREVIEW,
                                                                          &old_mode);
      photos_item_manager_update_fullscreen (self);
      self->load_state = PHOTOS_LOAD_STATE_STARTED;
      start_loading = TRUE;
    }

  g_set_object (&self->active_object, object);
  g_signal_emit_by_name (self, "active-changed", self->active_object);
  /* We have already eliminated the possibility of failure. */
  ret_val = TRUE;

  if (active_collection_changed)
    {
      g_signal_emit (self, signals[ACTIVE_COLLECTION_CHANGED], 0, self->active_collection);
      g_assert (self->active_object == (GObject *) self->active_collection);
    }

  if (start_loading)
    {
      GtkRecentManager *recent;
      const gchar *uri;

      recent = gtk_recent_manager_get_default ();
      uri = photos_base_item_get_uri (PHOTOS_BASE_ITEM (object));
      gtk_recent_manager_add_item (recent, uri);

      self->loader_cancellable = g_cancellable_new ();
      photos_base_item_load_async (PHOTOS_BASE_ITEM (object),
                                   self->loader_cancellable,
                                   photos_item_manager_item_load,
                                   g_object_ref (self));

      g_signal_emit (self, signals[LOAD_STARTED], 0, PHOTOS_BASE_ITEM (object));

      g_assert (self->active_object != (GObject *) self->active_collection);
    }

  if (window_mode_changed)
    g_signal_emit (self, signals[WINDOW_MODE_CHANGED], 0, self->mode, old_mode);

 out:
  return ret_val;
}


static gint
photos_item_manager_sort_func (gconstpointer a, gconstpointer b, gpointer user_data)
{
  PhotosBaseItem *item_a = PHOTOS_BASE_ITEM ((gpointer) a);
  PhotosBaseItem *item_b = PHOTOS_BASE_ITEM ((gpointer) b);
  gint ret_val;
  gint64 time_a;
  gint64 time_b;

  time_a = photos_base_item_get_date_created (item_a);
  time_b = photos_base_item_get_date_created (item_b);

  /* if creation time is missing, we use modification time */
  if (time_a < 0)
    time_a = photos_base_item_get_mtime (item_a);
  if (time_b < 0)
    time_b = photos_base_item_get_mtime (item_b);

  if (time_a > time_b)
    ret_val = -1;
  else if (time_a == time_b)
    ret_val = 0;
  else
    ret_val = 1;

  return ret_val;
}


static void
photos_item_manager_dispose (GObject *object)
{
  PhotosItemManager *self = PHOTOS_ITEM_MANAGER (object);

  photos_item_manager_remove_timeout (self);

  if (self->item_mngr_chldrn != NULL)
    {
      guint i;

      for (i = 0; self->item_mngr_chldrn[i] != NULL; i++)
        g_object_unref (self->item_mngr_chldrn[i]);

      g_free (self->item_mngr_chldrn);
      self->item_mngr_chldrn = NULL;
    }

  g_clear_pointer (&self->collections, g_hash_table_unref);
  g_clear_pointer (&self->hidden_items, g_hash_table_unref);
  g_clear_pointer (&self->wait_for_changes_table, g_hash_table_unref);
  g_clear_object (&self->active_object);
  g_clear_object (&self->loader_cancellable);
  g_clear_object (&self->active_collection);
  g_clear_object (&self->queue);
  g_clear_object (&self->notifier);

  G_OBJECT_CLASS (photos_item_manager_parent_class)->dispose (object);
}


static void
photos_item_manager_finalize (GObject *object)
{
  PhotosItemManager *self = PHOTOS_ITEM_MANAGER (object);

  g_queue_free (self->history);
  g_free (self->constrain_additions);

  G_OBJECT_CLASS (photos_item_manager_parent_class)->finalize (object);
}


static void
photos_item_manager_init (PhotosItemManager *self)
{
  GEnumClass *window_mode_class; /* TODO: use g_autoptr */
  guint i;

  self->collections = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
  self->hidden_items = g_hash_table_new_full (g_str_hash,
                                              g_str_equal,
                                              g_free,
                                              (GDestroyNotify) photos_item_manager_hidden_item_free);
  self->wait_for_changes_table = g_hash_table_new_full (g_str_hash,
                                                        g_str_equal,
                                                        g_free,
                                                        (GDestroyNotify) photos_utils_object_list_free_full);
  self->extension_point = g_io_extension_point_lookup (PHOTOS_BASE_ITEM_EXTENSION_POINT_NAME);
  self->history = g_queue_new ();

  window_mode_class = G_ENUM_CLASS (g_type_class_ref (PHOTOS_TYPE_WINDOW_MODE));

  self->item_mngr_chldrn = (PhotosBaseManager **) g_malloc0_n (window_mode_class->n_values + 1,
                                                               sizeof (PhotosBaseManager *));
  for (i = 0; i < window_mode_class->n_values; i++)
    self->item_mngr_chldrn[i] = photos_base_manager_new (photos_item_manager_sort_func, NULL);

  g_signal_connect_swapped (self->item_mngr_chldrn[0],
                            "items-changed",
                            G_CALLBACK (photos_item_manager_items_changed),
                            self);

  self->mode = PHOTOS_WINDOW_MODE_NONE;

  {
    g_autoptr (GError) error = NULL;

    self->queue = photos_tracker_queue_dup_singleton (NULL, &error);
    if (G_UNLIKELY (error != NULL))
      g_warning ("Unable to create PhotosTrackerQueue: %s", error->message);
  }

  self->fullscreen = FALSE;
  self->constrain_additions = (gboolean *) g_malloc0_n (window_mode_class->n_values, sizeof (gboolean));

  g_type_class_unref (window_mode_class);
}


static void
photos_item_manager_class_init (PhotosItemManagerClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  PhotosBaseManagerClass *base_manager_class = PHOTOS_BASE_MANAGER_CLASS (class);

  object_class->dispose = photos_item_manager_dispose;
  object_class->finalize = photos_item_manager_finalize;
  base_manager_class->add_object = photos_item_manager_add_object;
  base_manager_class->get_active_object = photos_item_manager_get_active_object;
  base_manager_class->get_where = photos_item_manager_get_where;
  base_manager_class->get_object_by_id = photos_item_manager_get_object_by_id;
  base_manager_class->set_active_object = photos_item_manager_set_active_object;
  base_manager_class->remove_object_by_id = photos_item_manager_remove_object_by_id;

  signals[ACTIVE_COLLECTION_CHANGED] = g_signal_new ("active-collection-changed",
                                                     G_TYPE_FROM_CLASS (class),
                                                     G_SIGNAL_RUN_LAST,
                                                     0,
                                                     NULL, /*accumulator */
                                                     NULL, /*accu_data */
                                                     g_cclosure_marshal_VOID__OBJECT,
                                                     G_TYPE_NONE,
                                                     1,
                                                     PHOTOS_TYPE_BASE_ITEM);

  signals[CAN_FULLSCREEN_CHANGED] = g_signal_new ("can-fullscreen-changed",
                                                  G_TYPE_FROM_CLASS (class),
                                                  G_SIGNAL_RUN_LAST,
                                                  0,
                                                  NULL, /*accumulator */
                                                  NULL, /*accu_data */
                                                  g_cclosure_marshal_VOID__VOID,
                                                  G_TYPE_NONE,
                                                  0);

  signals[FULLSCREEN_CHANGED] = g_signal_new ("fullscreen-changed",
                                              G_TYPE_FROM_CLASS (class),
                                              G_SIGNAL_RUN_LAST,
                                              0,
                                              NULL, /*accumulator */
                                              NULL, /* accu_data */
                                              g_cclosure_marshal_VOID__BOOLEAN,
                                              G_TYPE_NONE,
                                              1,
                                              G_TYPE_BOOLEAN);

  signals[LOAD_ERROR] = g_signal_new ("load-error",
                                      G_TYPE_FROM_CLASS (class),
                                      G_SIGNAL_RUN_LAST,
                                      0,
                                      NULL, /*accumulator */
                                      NULL, /*accu_data */
                                      _photos_marshal_VOID__STRING_ENUM,
                                      G_TYPE_NONE,
                                      2,
                                      G_TYPE_STRING,
                                      G_TYPE_ERROR);

  signals[LOAD_FINISHED] = g_signal_new ("load-finished",
                                         G_TYPE_FROM_CLASS (class),
                                         G_SIGNAL_RUN_LAST,
                                         0,
                                         NULL, /*accumulator */
                                         NULL, /*accu_data */
                                         g_cclosure_marshal_generic,
                                         G_TYPE_NONE,
                                         2,
                                         PHOTOS_TYPE_BASE_ITEM,
                                         GEGL_TYPE_NODE);

  signals[LOAD_STARTED] = g_signal_new ("load-started",
                                        G_TYPE_FROM_CLASS (class),
                                        G_SIGNAL_RUN_LAST,
                                        0,
                                        NULL, /*accumulator */
                                        NULL, /*accu_data */
                                        g_cclosure_marshal_VOID__OBJECT,
                                        G_TYPE_NONE,
                                        1,
                                        PHOTOS_TYPE_BASE_ITEM);

  signals[WINDOW_MODE_CHANGED] = g_signal_new ("window-mode-changed",
                                               G_TYPE_FROM_CLASS (class),
                                               G_SIGNAL_RUN_LAST,
                                               0,
                                               NULL, /*accumulator */
                                               NULL, /*accu_data */
                                               _photos_marshal_VOID__ENUM_ENUM,
                                               G_TYPE_NONE,
                                               2,
                                               PHOTOS_TYPE_WINDOW_MODE,
                                               PHOTOS_TYPE_WINDOW_MODE);
}


static void
photos_item_manager_list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item = photos_item_manager_get_item;
  iface->get_item_type = photos_item_manager_get_item_type;
  iface->get_n_items = photos_item_manager_get_n_items;
}


PhotosBaseManager *
photos_item_manager_new (void)
{
  return g_object_new (PHOTOS_TYPE_ITEM_MANAGER, NULL);
}


void
photos_item_manager_add_item (PhotosItemManager *self,
                              GType base_item_type,
                              TrackerSparqlCursor *cursor,
                              gboolean force)
{
  PhotosItemManagerHiddenItem *old_hidden_item;
  const gchar *id;

  g_return_if_fail (PHOTOS_IS_ITEM_MANAGER (self));
  g_return_if_fail (base_item_type == G_TYPE_NONE
                    || (base_item_type != PHOTOS_TYPE_BASE_ITEM
                        && g_type_is_a (base_item_type, PHOTOS_TYPE_BASE_ITEM)));
  g_return_if_fail (TRACKER_IS_SPARQL_CURSOR (cursor));

  id = tracker_sparql_cursor_get_string (cursor, PHOTOS_QUERY_COLUMNS_URN, NULL);
  g_return_if_fail (id != NULL && id[0] != '\0');

  old_hidden_item = (PhotosItemManagerHiddenItem *) g_hash_table_lookup (self->hidden_items, id);
  if (old_hidden_item != NULL)
    goto out;

  if (photos_item_manager_cursor_is_collection (cursor))
    {
      photos_item_manager_add_cursor_for_mode (self,
                                               base_item_type,
                                               cursor,
                                               PHOTOS_WINDOW_MODE_COLLECTIONS,
                                               force);
    }
  else
    {
      if (photos_item_manager_cursor_is_favorite (cursor))
        {
          photos_item_manager_add_cursor_for_mode (self,
                                                   base_item_type,
                                                   cursor,
                                                   PHOTOS_WINDOW_MODE_FAVORITES,
                                                   force);
        }

      photos_item_manager_add_cursor_for_mode (self,
                                               base_item_type,
                                               cursor,
                                               PHOTOS_WINDOW_MODE_OVERVIEW,
                                               force);
    }

 out:
  return;
}


void
photos_item_manager_add_item_for_mode (PhotosItemManager *self,
                                       GType base_item_type,
                                       PhotosWindowMode mode,
                                       TrackerSparqlCursor *cursor)
{
  PhotosItemManagerHiddenItem *old_hidden_item;
  const gchar *id;

  g_return_if_fail (base_item_type == G_TYPE_NONE
                    || (base_item_type != PHOTOS_TYPE_BASE_ITEM
                        && g_type_is_a (base_item_type, PHOTOS_TYPE_BASE_ITEM)));

  id = tracker_sparql_cursor_get_string (cursor, PHOTOS_QUERY_COLUMNS_URN, NULL);
  g_return_if_fail (id != NULL && id[0] != '\0');

  old_hidden_item = (PhotosItemManagerHiddenItem *) g_hash_table_lookup (self->hidden_items, id);
  if (old_hidden_item != NULL)
    goto out;

  photos_item_manager_add_cursor_for_mode (self, base_item_type, cursor, mode, FALSE);

 out:
  return;
}


void
photos_item_manager_clear (PhotosItemManager *self, PhotosWindowMode mode)
{
  PhotosBaseManager *item_mngr_chld;
  guint i;
  guint n_items;

  g_return_if_fail (PHOTOS_IS_ITEM_MANAGER (self));
  g_return_if_fail (mode != PHOTOS_WINDOW_MODE_NONE);
  g_return_if_fail (mode != PHOTOS_WINDOW_MODE_EDIT);
  g_return_if_fail (mode != PHOTOS_WINDOW_MODE_PREVIEW);

  item_mngr_chld = self->item_mngr_chldrn[mode];
  n_items = g_list_model_get_n_items (G_LIST_MODEL (item_mngr_chld));
  for (i = 0; i < n_items; i++)
    {
      g_autoptr (PhotosBaseItem) item = NULL;
      PhotosBaseItem *item1 = NULL;
      const gchar *id;
      guint j;

      item = PHOTOS_BASE_ITEM (g_list_model_get_object (G_LIST_MODEL (item_mngr_chld), i));
      id = photos_filterable_get_id (PHOTOS_FILTERABLE (item));

      for (j = 1; self->item_mngr_chldrn[j] != NULL; j++)
        {
          if (item_mngr_chld == self->item_mngr_chldrn[j])
            continue;

          item1 = PHOTOS_BASE_ITEM (photos_base_manager_get_object_by_id (self->item_mngr_chldrn[j], id));
          if (item1 != NULL)
            break;
        }

      if (item1 == NULL)
        {
          item1 = PHOTOS_BASE_ITEM (photos_base_manager_get_object_by_id (self->item_mngr_chldrn[0], id));
          g_assert_true (item == item1);

          g_signal_handlers_disconnect_by_func (item, photos_item_manager_info_updated, self);
          photos_base_manager_remove_object_by_id (self->item_mngr_chldrn[0], id);
        }
    }

  photos_base_manager_clear (item_mngr_chld);
}


PhotosBaseItem *
photos_item_manager_create_item (PhotosItemManager *self,
                                 GType base_item_type,
                                 TrackerSparqlCursor *cursor,
                                 gboolean create_thumbnails)
{
  PhotosBaseItem *item;
  PhotosBaseItem *ret_val = NULL;
  const gchar *id;

  g_return_val_if_fail (PHOTOS_IS_ITEM_MANAGER (self), NULL);
  g_return_val_if_fail (base_item_type == G_TYPE_NONE
                        || (base_item_type != PHOTOS_TYPE_BASE_ITEM
                            && g_type_is_a (base_item_type, PHOTOS_TYPE_BASE_ITEM)), NULL);
  g_return_val_if_fail (TRACKER_IS_SPARQL_CURSOR (cursor), NULL);

  id = tracker_sparql_cursor_get_string (cursor, PHOTOS_QUERY_COLUMNS_URN, NULL);
  item = PHOTOS_BASE_ITEM (photos_base_manager_get_object_by_id (PHOTOS_BASE_MANAGER (self), id));
  if (item != NULL)
    {
      ret_val = g_object_ref (item);
    }
  else
    {
      GType type;

      if (base_item_type == G_TYPE_NONE)
        {
          GIOExtension *extension;
          g_auto (GStrv) split_identifier = NULL;
          const gchar *extension_name = "local";
          g_autofree gchar *identifier = NULL;

          identifier = g_strdup (tracker_sparql_cursor_get_string (cursor,
                                                                   PHOTOS_QUERY_COLUMNS_IDENTIFIER,
                                                                   NULL));
          if (identifier != NULL)
            {
              split_identifier = g_strsplit (identifier, ":", 4);

              if (photos_item_manager_cursor_is_collection (cursor))
                {
                  /* Its a collection. */
                  extension_name = split_identifier[2];
                }
              else
                {
                  /* Its a normal photo item. */
                  if (g_strv_length (split_identifier) > 1)
                    extension_name = split_identifier[0];
                }
            }

          extension = g_io_extension_point_get_extension_by_name (self->extension_point, extension_name);
          if (G_UNLIKELY (extension == NULL))
            {
              g_warning ("Unable to find extension %s for identifier: %s", extension_name, identifier);
              goto out;
            }

          type = g_io_extension_get_type (extension);
        }
      else
        {
          type = base_item_type;
        }

      g_return_val_if_fail (type != G_TYPE_NONE, NULL);
      g_return_val_if_fail (type != PHOTOS_TYPE_BASE_ITEM && g_type_is_a (type, PHOTOS_TYPE_BASE_ITEM), NULL);

      ret_val = PHOTOS_BASE_ITEM (g_object_new (type,
                                                "cursor", cursor,
                                                "failed-thumbnailing", !create_thumbnails,
                                                NULL));
    }

 out:
  return ret_val;
}


PhotosBaseItem *
photos_item_manager_get_active_collection (PhotosItemManager *self)
{
  g_return_val_if_fail (PHOTOS_IS_ITEM_MANAGER (self), NULL);
  return self->active_collection;
}


GHashTable *
photos_item_manager_get_collections (PhotosItemManager *self)
{
  g_return_val_if_fail (PHOTOS_IS_ITEM_MANAGER (self), NULL);
  return self->collections;
}


PhotosBaseManager *
photos_item_manager_get_for_mode (PhotosItemManager *self, PhotosWindowMode mode)
{
  g_return_val_if_fail (PHOTOS_IS_ITEM_MANAGER (self), NULL);
  g_return_val_if_fail (mode != PHOTOS_WINDOW_MODE_NONE, NULL);
  g_return_val_if_fail (mode != PHOTOS_WINDOW_MODE_EDIT, NULL);
  g_return_val_if_fail (mode != PHOTOS_WINDOW_MODE_PREVIEW, NULL);

  return self->item_mngr_chldrn[mode];
}


PhotosLoadState
photos_item_manager_get_load_state (PhotosItemManager *self)
{
  g_return_val_if_fail (PHOTOS_IS_ITEM_MANAGER (self), PHOTOS_LOAD_STATE_NONE);
  return self->load_state;
}


void
photos_item_manager_hide_item (PhotosItemManager *self, PhotosBaseItem *item)
{
  PhotosItemManagerHiddenItem *hidden_item;
  PhotosItemManagerHiddenItem *old_hidden_item;
  const gchar *id;
  guint i;

  g_return_if_fail (PHOTOS_IS_ITEM_MANAGER (self));
  g_return_if_fail (PHOTOS_IS_BASE_ITEM (item));

  id = photos_filterable_get_id (PHOTOS_FILTERABLE (item));
  g_return_if_fail (id != NULL && id[0] != '\0');

  old_hidden_item = (PhotosItemManagerHiddenItem *) g_hash_table_lookup (self->hidden_items, id);
  g_return_if_fail (old_hidden_item == NULL);

  hidden_item = photos_item_manager_hidden_item_new (item);
  for (i = 0; self->item_mngr_chldrn[i] != NULL; i++)
    {
      PhotosBaseItem *item1;

      g_assert_cmpuint (i, <, hidden_item->n_modes);

      item1 = PHOTOS_BASE_ITEM (photos_base_manager_get_object_by_id (self->item_mngr_chldrn[i], id));
      if (item1 != NULL)
        {
          g_assert_true (item == item1);
          hidden_item->modes[i] = TRUE;
        }
    }

  g_hash_table_insert (self->hidden_items, g_strdup (id), hidden_item);
  photos_base_manager_remove_object_by_id (PHOTOS_BASE_MANAGER (self), id);
}


void
photos_item_manager_set_constraints_for_mode (PhotosItemManager *self, gboolean constrain, PhotosWindowMode mode)
{
  g_return_if_fail (PHOTOS_IS_ITEM_MANAGER (self));
  g_return_if_fail (mode != PHOTOS_WINDOW_MODE_NONE);
  g_return_if_fail (mode != PHOTOS_WINDOW_MODE_EDIT);
  g_return_if_fail (mode != PHOTOS_WINDOW_MODE_PREVIEW);

  self->constrain_additions[mode] = constrain;
}


void
photos_item_manager_set_notifier (PhotosItemManager *self, TrackerNotifier *notifier)
{
  g_return_if_fail (PHOTOS_IS_ITEM_MANAGER (self));
  g_return_if_fail (notifier == NULL || TRACKER_IS_NOTIFIER (notifier));

  if (self->notifier != NULL)
    {
      g_signal_handlers_disconnect_by_func (self->notifier, photos_item_manager_notifier_events, self);
      g_clear_object (&self->notifier);
    }

  if (notifier != NULL)
    {
      self->notifier = g_object_ref (notifier);
      g_signal_connect_swapped (self->notifier, "events", G_CALLBACK (photos_item_manager_notifier_events), self);
    }
}


void
photos_item_manager_unhide_item (PhotosItemManager *self, PhotosBaseItem *item)
{
  PhotosItemManagerHiddenItem *hidden_item;
  gboolean added_somewhere = FALSE;
  const gchar *id;
  guint i;

  g_return_if_fail (PHOTOS_IS_ITEM_MANAGER (self));
  g_return_if_fail (PHOTOS_IS_BASE_ITEM (item));

  id = photos_filterable_get_id (PHOTOS_FILTERABLE (item));
  g_return_if_fail (id != NULL && id[0] != '\0');

  hidden_item = (PhotosItemManagerHiddenItem *) g_hash_table_lookup (self->hidden_items, id);
  g_return_if_fail (hidden_item->item == item);

  for (i = 1; self->item_mngr_chldrn[i] != NULL; i++)
    {
      g_assert_cmpuint (i, <, hidden_item->n_modes);

      if (hidden_item->modes[i])
        added_somewhere = added_somewhere || photos_item_manager_try_to_add_item_for_mode (self, item, i);
    }

  g_hash_table_remove (self->hidden_items, id);

  if (added_somewhere)
    {
      photos_base_manager_add_object (self->item_mngr_chldrn[0], G_OBJECT (item));
      g_signal_connect_object (item, "info-updated", G_CALLBACK (photos_item_manager_info_updated), self, 0);
      g_signal_emit_by_name (self, "object-added", G_OBJECT (item));
    }
}


void
photos_item_manager_wait_for_file_async (PhotosItemManager *self,
                                         GFile *file,
                                         GCancellable *cancellable,
                                         GAsyncReadyCallback callback,
                                         gpointer user_data)
{
  GList *tasks;
  g_autoptr (GTask) task = NULL;
  g_autofree gchar *uri = NULL;

  g_return_if_fail (PHOTOS_IS_ITEM_MANAGER (self));
  g_return_if_fail (G_IS_FILE (file));
  g_return_if_fail (g_file_is_native (file));
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, photos_item_manager_wait_for_file_async);

  uri = g_file_get_uri (file);
  tasks = (GList *) g_hash_table_lookup (self->wait_for_changes_table, uri);
  tasks = g_list_copy_deep (tasks, (GCopyFunc) g_object_ref, NULL);
  tasks = g_list_prepend (tasks, g_object_ref (task));
  g_hash_table_insert (self->wait_for_changes_table, g_strdup (uri), tasks);

  photos_item_manager_remove_timeout (self);
  self->wait_for_changes_id = g_timeout_add_seconds (WAIT_FOR_CHANGES_TIMEOUT,
                                                     photos_item_manager_wait_for_changes_timeout,
                                                     self);

  photos_debug (PHOTOS_DEBUG_TRACKER, "Waiting for %s", uri);
}


gchar *
photos_item_manager_wait_for_file_finish (PhotosItemManager *self, GAsyncResult *res, GError **error)
{
  GTask *task;

  g_return_val_if_fail (PHOTOS_IS_ITEM_MANAGER (self), NULL);

  g_return_val_if_fail (g_task_is_valid (res, self), NULL);
  task = G_TASK (res);

  g_return_val_if_fail (g_task_get_source_tag (task) == photos_item_manager_wait_for_file_async, NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  return g_task_propagate_pointer (task, error);
}


void
photos_item_manager_wait_for_changes_async (PhotosItemManager *self,
                                            PhotosBaseItem *item,
                                            GCancellable *cancellable,
                                            GAsyncReadyCallback callback,
                                            gpointer user_data)
{
  GList *tasks;
  g_autoptr (GTask) task = NULL;
  const gchar *uri;

  g_return_if_fail (PHOTOS_IS_ITEM_MANAGER (self));
  g_return_if_fail (PHOTOS_IS_BASE_ITEM (item));
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, photos_item_manager_wait_for_changes_async);

  if (!PHOTOS_IS_LOCAL_ITEM (item) || photos_base_item_is_collection (item))
    {
      const gchar *id;

      id = photos_filterable_get_id (PHOTOS_FILTERABLE (item));
      g_task_return_pointer (task, g_strdup (id), g_free);
      goto out;
    }

  uri = photos_base_item_get_uri (item);
  tasks = (GList *) g_hash_table_lookup (self->wait_for_changes_table, uri);
  tasks = g_list_copy_deep (tasks, (GCopyFunc) g_object_ref, NULL);
  tasks = g_list_prepend (tasks, g_object_ref (task));
  g_hash_table_insert (self->wait_for_changes_table, g_strdup (uri), tasks);

  photos_item_manager_remove_timeout (self);
  self->wait_for_changes_id = g_timeout_add_seconds (WAIT_FOR_CHANGES_TIMEOUT,
                                                     photos_item_manager_wait_for_changes_timeout,
                                                     self);

  photos_debug (PHOTOS_DEBUG_TRACKER, "Waiting for %s", uri);

 out:
  return;
}


gchar *
photos_item_manager_wait_for_changes_finish (PhotosItemManager *self, GAsyncResult *res, GError **error)
{
  GTask *task = G_TASK (res);

  g_return_val_if_fail (PHOTOS_IS_ITEM_MANAGER (self), NULL);
  g_return_val_if_fail (g_task_is_valid (res, self), NULL);
  g_return_val_if_fail (g_task_get_source_tag (task) == photos_item_manager_wait_for_changes_async, NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  return g_task_propagate_pointer (task, error);
}


gboolean
photos_mode_controller_get_can_fullscreen (PhotosModeController *self)
{
  g_return_val_if_fail (PHOTOS_IS_MODE_CONTROLLER (self), FALSE);
  return self->mode == PHOTOS_WINDOW_MODE_PREVIEW;
}


gboolean
photos_mode_controller_get_fullscreen (PhotosModeController *self)
{
  g_return_val_if_fail (PHOTOS_IS_MODE_CONTROLLER (self), FALSE);
  return self->fullscreen;
}


PhotosWindowMode
photos_mode_controller_get_window_mode (PhotosModeController *self)
{
  g_return_val_if_fail (PHOTOS_IS_MODE_CONTROLLER (self), PHOTOS_WINDOW_MODE_NONE);
  return self->mode;
}


void
photos_mode_controller_go_back (PhotosModeController *self)
{
  GAction *selection_mode_action;
  GApplication *app;
  PhotosWindowMode old_mode;
  PhotosWindowMode tmp;
  gboolean active_changed = FALSE;
  gboolean active_collection_changed = FALSE;
  gboolean unset_selection_mode = FALSE;

  g_return_if_fail (PHOTOS_IS_MODE_CONTROLLER (self));
  g_return_if_fail (!g_queue_is_empty (self->history));

  app = g_application_get_default ();
  selection_mode_action = g_action_map_lookup_action (G_ACTION_MAP (app), "selection-mode");

  old_mode = (PhotosWindowMode) GPOINTER_TO_INT (g_queue_peek_head (self->history));
  g_return_if_fail (old_mode != PHOTOS_WINDOW_MODE_NONE);

  switch (self->mode)
    {
    case PHOTOS_WINDOW_MODE_EDIT:
      g_return_if_fail (self->load_state == PHOTOS_LOAD_STATE_FINISHED);
      g_return_if_fail (old_mode == PHOTOS_WINDOW_MODE_PREVIEW);
      break;

    case PHOTOS_WINDOW_MODE_COLLECTION_VIEW:
      g_return_if_fail (self->load_state == PHOTOS_LOAD_STATE_NONE);
      g_return_if_fail (PHOTOS_IS_BASE_ITEM (self->active_collection));
      g_return_if_fail (self->active_object == (GObject *) self->active_collection);
      g_return_if_fail (old_mode == PHOTOS_WINDOW_MODE_COLLECTIONS || old_mode == PHOTOS_WINDOW_MODE_SEARCH);
      break;

    case PHOTOS_WINDOW_MODE_COLLECTIONS:
    case PHOTOS_WINDOW_MODE_FAVORITES:
    case PHOTOS_WINDOW_MODE_OVERVIEW:
    case PHOTOS_WINDOW_MODE_SEARCH:
      g_return_if_fail (old_mode != PHOTOS_WINDOW_MODE_PREVIEW);
      break;

    case PHOTOS_WINDOW_MODE_IMPORT:
      {
        g_autoptr (GVariant) state = NULL;

        g_return_if_fail (old_mode == PHOTOS_WINDOW_MODE_COLLECTIONS
                          || old_mode == PHOTOS_WINDOW_MODE_FAVORITES
                          || old_mode == PHOTOS_WINDOW_MODE_OVERVIEW);

        state = g_action_get_state (selection_mode_action);
        g_return_if_fail (state != NULL);
        g_return_if_fail (g_variant_get_boolean (state));
        break;
      }

    case PHOTOS_WINDOW_MODE_PREVIEW:
      g_return_if_fail (PHOTOS_IS_BASE_ITEM (self->active_object));
      g_return_if_fail (self->active_object != (GObject *) self->active_collection);
      g_return_if_fail (old_mode != PHOTOS_WINDOW_MODE_PREVIEW);
      g_return_if_fail ((old_mode == PHOTOS_WINDOW_MODE_COLLECTION_VIEW && PHOTOS_IS_BASE_ITEM (self->active_collection))
                        || (old_mode != PHOTOS_WINDOW_MODE_COLLECTION_VIEW && self->active_collection == NULL));
      break;

    case PHOTOS_WINDOW_MODE_NONE:
    default:
      g_assert_not_reached ();
      break;
    }

  g_queue_pop_head (self->history);

  /* Swap the old and current modes */
  tmp = old_mode;
  old_mode = self->mode;
  self->mode = tmp;

  photos_item_manager_update_fullscreen (self);
  photos_item_manager_clear_active_item_load (self);

  switch (old_mode)
    {
    case PHOTOS_WINDOW_MODE_COLLECTION_VIEW:
      g_clear_object (&self->active_collection);
      g_clear_object (&self->active_object);
      active_changed = TRUE;
      active_collection_changed = TRUE;
      break;

    case PHOTOS_WINDOW_MODE_EDIT:
      break;

    case PHOTOS_WINDOW_MODE_IMPORT:
      unset_selection_mode = TRUE;
      break;

    case PHOTOS_WINDOW_MODE_PREVIEW:
      self->load_state = PHOTOS_LOAD_STATE_NONE;
      g_set_object (&self->active_object, G_OBJECT (self->active_collection));
      active_changed = TRUE;
      break;

    case PHOTOS_WINDOW_MODE_COLLECTIONS:
    case PHOTOS_WINDOW_MODE_FAVORITES:
    case PHOTOS_WINDOW_MODE_OVERVIEW:
    case PHOTOS_WINDOW_MODE_SEARCH:
      if (self->active_collection != NULL)
        {
          g_clear_object (&self->active_collection);
          active_collection_changed = TRUE;
        }

      g_clear_object (&self->active_object);
      active_changed = TRUE;

      self->load_state = PHOTOS_LOAD_STATE_NONE;
      break;

    case PHOTOS_WINDOW_MODE_NONE:
    default:
      g_assert_not_reached ();
      break;
    }

  if (unset_selection_mode)
    {
      GVariant *state;

      state = g_variant_new_boolean (FALSE);
      g_action_change_state (selection_mode_action, state);
    }

  if (active_changed)
    g_signal_emit_by_name (self, "active-changed", self->active_object);

  if (active_collection_changed)
    g_signal_emit (self, signals[ACTIVE_COLLECTION_CHANGED], 0, self->active_collection);

  g_signal_emit (self, signals[WINDOW_MODE_CHANGED], 0, self->mode, old_mode);
}


void
photos_mode_controller_toggle_fullscreen (PhotosModeController *self)
{
  g_return_if_fail (PHOTOS_IS_MODE_CONTROLLER (self));
  photos_mode_controller_set_fullscreen (self, !self->fullscreen);
}


void
photos_mode_controller_set_fullscreen (PhotosModeController *self, gboolean fullscreen)
{
  g_return_if_fail (PHOTOS_IS_MODE_CONTROLLER (self));

  if (self->fullscreen == fullscreen)
    return;

  self->fullscreen = fullscreen;
  g_signal_emit (self, signals[FULLSCREEN_CHANGED], 0, self->fullscreen);
}


void
photos_mode_controller_set_window_mode (PhotosModeController *self, PhotosWindowMode mode)
{
  GAction *selection_mode_action;
  GApplication *app;
  PhotosWindowMode old_mode;
  gboolean active_changed = FALSE;
  gboolean active_collection_changed = FALSE;
  gboolean set_selection_mode = FALSE;

  g_return_if_fail (PHOTOS_IS_MODE_CONTROLLER (self));
  g_return_if_fail (mode != PHOTOS_WINDOW_MODE_NONE);
  g_return_if_fail (mode != PHOTOS_WINDOW_MODE_COLLECTION_VIEW);
  g_return_if_fail (mode != PHOTOS_WINDOW_MODE_PREVIEW);

  app = g_application_get_default ();
  selection_mode_action = g_action_map_lookup_action (G_ACTION_MAP (app), "selection-mode");

  if (mode == PHOTOS_WINDOW_MODE_EDIT)
    {
      g_return_if_fail (self->load_state == PHOTOS_LOAD_STATE_FINISHED);
      g_return_if_fail (self->mode == PHOTOS_WINDOW_MODE_PREVIEW);
    }
  else if (mode == PHOTOS_WINDOW_MODE_IMPORT)
    {
      g_autoptr (GVariant) state = NULL;

      g_return_if_fail (self->mode == PHOTOS_WINDOW_MODE_COLLECTIONS
                        || self->mode == PHOTOS_WINDOW_MODE_FAVORITES
                        || self->mode == PHOTOS_WINDOW_MODE_OVERVIEW);

      state = g_action_get_state (selection_mode_action);
      g_return_if_fail (state != NULL);
      g_return_if_fail (!g_variant_get_boolean (state));
    }
  else
    {
      g_return_if_fail (self->mode != PHOTOS_WINDOW_MODE_EDIT);
      g_return_if_fail (self->mode != PHOTOS_WINDOW_MODE_PREVIEW);
    }

  if (!photos_item_manager_set_window_mode_internal (self, mode, &old_mode))
    return;

  photos_item_manager_update_fullscreen (self);
  photos_item_manager_clear_active_item_load (self);

  if (mode != PHOTOS_WINDOW_MODE_EDIT)
    {
      self->load_state = PHOTOS_LOAD_STATE_NONE;

      if (self->active_collection != NULL)
        {
          g_clear_object (&self->active_collection);
          active_collection_changed = TRUE;
        }

      g_clear_object (&self->active_object);
      active_changed = TRUE;
    }

  if (mode == PHOTOS_WINDOW_MODE_IMPORT)
    set_selection_mode = TRUE;

  if (set_selection_mode)
    {
      GVariant *state;

      state = g_variant_new_boolean (TRUE);
      g_action_change_state (selection_mode_action, state);
    }

  if (active_changed)
    g_signal_emit_by_name (self, "active-changed", self->active_object);

  if (active_collection_changed)
    g_signal_emit (self, signals[ACTIVE_COLLECTION_CHANGED], 0, self->active_collection);

  g_signal_emit (self, signals[WINDOW_MODE_CHANGED], 0, mode, old_mode);
}
