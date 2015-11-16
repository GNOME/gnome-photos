/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2012, 2013, 2014, 2015 Red Hat, Inc.
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

#include <gio/gio.h>
#include <glib.h>
#include <tracker-sparql.h>

#include "photos-enums.h"
#include "photos-filterable.h"
#include "photos-item-manager.h"
#include "photos-local-item.h"
#include "photos-marshalers.h"
#include "photos-query.h"
#include "photos-search-context.h"
#include "photos-single-item-job.h"
#include "photos-tracker-change-event.h"
#include "photos-tracker-change-monitor.h"
#include "photos-utils.h"


struct _PhotosItemManager
{
  PhotosBaseManager parent_instance;
  GCancellable *loader_cancellable;
  GHashTable *collections;
  GIOExtensionPoint *extension_point;
  GQueue *collection_path;
  GQueue *history;
  PhotosBaseItem *active_collection;
  PhotosTrackerChangeMonitor *monitor;
  PhotosWindowMode mode;
  gboolean fullscreen;
};

struct _PhotosItemManagerClass
{
  PhotosBaseManagerClass parent_class;

  /* signals */
  void (*active_collection_changed) (PhotosItemManager *self, PhotosBaseItem *collection);
  void (*can_fullscreen_changed)    (PhotosItemManager *self);
  void (*fullscreen_changed)        (PhotosItemManager *self, gboolean fullscreen);
  void (*load_finished)             (PhotosItemManager *self, PhotosBaseItem *item, GeglNode *node);
  void (*load_started)              (PhotosItemManager *self, PhotosBaseItem *item, GCancellable *cancellable);
  void (*window_mode_changed)       (PhotosItemManager *self, PhotosWindowMode mode, PhotosWindowMode old_mode);
};

enum
{
  ACTIVE_COLLECTION_CHANGED,
  CAN_FULLSCREEN_CHANGED,
  FULLSCREEN_CHANGED,
  LOAD_FINISHED,
  LOAD_STARTED,
  WINDOW_MODE_CHANGED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };


G_DEFINE_TYPE (PhotosItemManager, photos_item_manager, PHOTOS_TYPE_BASE_MANAGER);


static void
photos_item_manager_add_object (PhotosBaseManager *mngr, GObject *object)
{
  PhotosItemManager *self = PHOTOS_ITEM_MANAGER (mngr);
  PhotosBaseItem *item;
  const gchar *id;
  gpointer *old_collection;

  g_return_if_fail (PHOTOS_IS_BASE_ITEM (object));

  item = PHOTOS_BASE_ITEM (object);

  if (!photos_base_item_is_collection (item))
    goto end;

  id = photos_filterable_get_id (PHOTOS_FILTERABLE (item));
  if (id == NULL)
    goto end;

  old_collection = g_hash_table_lookup (self->collections, id);
  if (old_collection != NULL)
    goto end;

  g_hash_table_insert (self->collections, g_strdup (id), g_object_ref (item));

 end:
  PHOTOS_BASE_MANAGER_CLASS (photos_item_manager_parent_class)->add_object (mngr, object);
}


static void
photos_item_manager_item_created_executed (TrackerSparqlCursor *cursor, gpointer user_data)
{
  PhotosItemManager *self = PHOTOS_ITEM_MANAGER (user_data);

  if (cursor == NULL)
    goto out;

  photos_item_manager_add_item (self, cursor);

 out:
  g_object_unref (self);
}


static void
photos_item_manager_item_created (PhotosItemManager *self, const gchar *urn)
{
  GApplication *app;
  PhotosSearchContextState *state;
  PhotosSingleItemJob *job;

  app = g_application_get_default ();
  state = photos_search_context_get_state (PHOTOS_SEARCH_CONTEXT (app));

  job = photos_single_item_job_new (urn);
  photos_single_item_job_run (job,
                              state,
                              PHOTOS_QUERY_FLAGS_NONE,
                              photos_item_manager_item_created_executed,
                              g_object_ref (self));
  g_object_unref (job);
}


static void
photos_item_manager_changes_pending_foreach (gpointer key, gpointer value, gpointer user_data)
{
  PhotosItemManager *self = PHOTOS_ITEM_MANAGER (user_data);
  PhotosTrackerChangeEvent *change_event = (PhotosTrackerChangeEvent *) value;
  PhotosTrackerChangeEventType change_type;
  const gchar *change_urn;

  change_type = photos_tracker_change_event_get_type (change_event);
  change_urn = photos_tracker_change_event_get_urn (change_event);

  if (change_type == PHOTOS_TRACKER_CHANGE_EVENT_CHANGED)
    {
      GObject *object;

      object = photos_base_manager_get_object_by_id (PHOTOS_BASE_MANAGER (self), change_urn);
      if (object != NULL)
        photos_base_item_refresh (PHOTOS_BASE_ITEM (object));
    }
  else if (change_type == PHOTOS_TRACKER_CHANGE_EVENT_CREATED)
    {
      photos_item_manager_item_created (self, change_urn);
    }
  else if (change_type == PHOTOS_TRACKER_CHANGE_EVENT_DELETED)
    {
      GObject *object;

      object = photos_base_manager_get_object_by_id (PHOTOS_BASE_MANAGER (self), change_urn);
      if (object != NULL)
        {
          photos_base_item_destroy (PHOTOS_BASE_ITEM (object));
          photos_base_manager_remove_object_by_id (PHOTOS_BASE_MANAGER (self), change_urn);
        }
    }
}


static void
photos_item_manager_changes_pending (PhotosItemManager *self, GHashTable *changes)
{
  g_hash_table_foreach (changes, photos_item_manager_changes_pending_foreach, self);
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


static void
photos_item_manager_collection_path_free_foreach (gpointer data, gpointer user_data)
{
  g_clear_object (&data);
}


static void
photos_item_manager_collection_path_free (PhotosItemManager *self)
{
  g_queue_foreach (self->collection_path, photos_item_manager_collection_path_free_foreach, NULL);
  g_queue_free (self->collection_path);
}


static gchar *
photos_item_manager_get_where (PhotosBaseManager *mngr, gint flags)
{
  PhotosItemManager *self = PHOTOS_ITEM_MANAGER (mngr);

  if (self->active_collection == NULL)
    return g_strdup ("");

  return photos_base_item_get_where (self->active_collection);
}


static void
photos_item_manager_item_load (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosItemManager *self = PHOTOS_ITEM_MANAGER (user_data);
  GError *error;
  GeglNode *node = NULL;
  PhotosBaseItem *item = PHOTOS_BASE_ITEM (source_object);

  g_clear_object (&self->loader_cancellable);

  error = NULL;
  node = photos_base_item_load_finish (item, res, &error);
  if (error != NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Unable to load the item: %s", error->message);
      g_error_free (error);
    }

  g_signal_emit (self, signals[LOAD_FINISHED], 0, item, node);

  g_clear_object (&node);
  g_object_unref (self);
}


static void
photos_item_manager_remove_object_by_id (PhotosBaseManager *mngr, const gchar *id)
{
  PhotosItemManager *self = PHOTOS_ITEM_MANAGER (mngr);
  gpointer *collection;

  if (id == NULL)
    goto end;

  collection = g_hash_table_lookup (self->collections, id);
  if (collection == NULL)
    goto end;

  g_hash_table_remove (self->collections, id);

 end:
  PHOTOS_BASE_MANAGER_CLASS (photos_item_manager_parent_class)->remove_object_by_id (mngr, id);
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
  GObject *active_item;
  PhotosWindowMode old_mode;
  gboolean active_collection_changed = FALSE;
  gboolean ret_val = FALSE;
  gboolean start_loading = FALSE;
  gboolean window_mode_changed = FALSE;

  g_return_val_if_fail (object != NULL, FALSE);
  g_return_val_if_fail (PHOTOS_IS_BASE_ITEM (object), FALSE);

  active_item = photos_base_manager_get_active_object (manager);
  if (object == active_item)
    goto out;

  photos_item_manager_clear_active_item_load (self);

  if (photos_base_item_is_collection (PHOTOS_BASE_ITEM (object)))
    {
      g_queue_push_head (self->collection_path,
                         (self->active_collection != NULL) ? g_object_ref (self->active_collection) : NULL);

      g_clear_object (&self->active_collection);
      self->active_collection = g_object_ref (object);
      active_collection_changed = TRUE;
    }
  else
    {
      window_mode_changed = photos_item_manager_set_window_mode_internal (self,
                                                                          PHOTOS_WINDOW_MODE_PREVIEW,
                                                                          &old_mode);
      photos_item_manager_update_fullscreen (self);
      start_loading = TRUE;
    }

  ret_val = PHOTOS_BASE_MANAGER_CLASS (photos_item_manager_parent_class)->set_active_object (manager, object);
  /* We have already eliminated the possibility of failure. */
  g_assert (ret_val == TRUE);

  active_item = photos_base_manager_get_active_object (manager);
  g_assert (active_item == object);

  if (active_collection_changed)
    {
      g_signal_emit (self, signals[ACTIVE_COLLECTION_CHANGED], 0, self->active_collection);
      g_assert (active_item == (GObject *) self->active_collection);
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

      if (window_mode_changed)
        g_signal_emit (self, signals[WINDOW_MODE_CHANGED], 0, PHOTOS_WINDOW_MODE_PREVIEW, old_mode);

      g_assert (active_item != (GObject *) self->active_collection);
    }

 out:
  return ret_val;
}


static void
photos_item_manager_dispose (GObject *object)
{
  PhotosItemManager *self = PHOTOS_ITEM_MANAGER (object);

  if (self->collection_path != NULL)
    {
      photos_item_manager_collection_path_free (self);
      self->collection_path = NULL;
    }

  g_clear_pointer (&self->collections, (GDestroyNotify) g_hash_table_unref);
  g_clear_object (&self->loader_cancellable);
  g_clear_object (&self->active_collection);
  g_clear_object (&self->monitor);

  G_OBJECT_CLASS (photos_item_manager_parent_class)->dispose (object);
}


static void
photos_item_manager_finalize (GObject *object)
{
  PhotosItemManager *self = PHOTOS_ITEM_MANAGER (object);

  g_queue_free (self->history);

  G_OBJECT_CLASS (photos_item_manager_parent_class)->finalize (object);
}


static void
photos_item_manager_init (PhotosItemManager *self)
{
  self->collections = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
  self->extension_point = g_io_extension_point_lookup (PHOTOS_BASE_ITEM_EXTENSION_POINT_NAME);
  self->collection_path = g_queue_new ();
  self->history = g_queue_new ();
  self->mode = PHOTOS_WINDOW_MODE_NONE;

  self->monitor = photos_tracker_change_monitor_dup_singleton (NULL, NULL);
  if (G_LIKELY (self->monitor != NULL))
    g_signal_connect_swapped (self->monitor,
                              "changes-pending",
                              G_CALLBACK (photos_item_manager_changes_pending),
                              self);

  self->fullscreen = FALSE;
}


static void
photos_item_manager_class_init (PhotosItemManagerClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  PhotosBaseManagerClass *base_manager_class = PHOTOS_BASE_MANAGER_CLASS (class);

  object_class->dispose = photos_item_manager_dispose;
  object_class->finalize = photos_item_manager_finalize;
  base_manager_class->add_object = photos_item_manager_add_object;
  base_manager_class->get_where = photos_item_manager_get_where;
  base_manager_class->set_active_object = photos_item_manager_set_active_object;
  base_manager_class->remove_object_by_id = photos_item_manager_remove_object_by_id;

  signals[ACTIVE_COLLECTION_CHANGED] = g_signal_new ("active-collection-changed",
                                                     G_TYPE_FROM_CLASS (class),
                                                     G_SIGNAL_RUN_LAST,
                                                     G_STRUCT_OFFSET (PhotosItemManagerClass,
                                                                      active_collection_changed),
                                                     NULL, /*accumulator */
                                                     NULL, /*accu_data */
                                                     g_cclosure_marshal_VOID__OBJECT,
                                                     G_TYPE_NONE,
                                                     1,
                                                     PHOTOS_TYPE_BASE_ITEM);

  signals[CAN_FULLSCREEN_CHANGED] = g_signal_new ("can-fullscreen-changed",
                                                  G_TYPE_FROM_CLASS (class),
                                                  G_SIGNAL_RUN_LAST,
                                                  G_STRUCT_OFFSET (PhotosItemManagerClass,
                                                                   can_fullscreen_changed),
                                                  NULL, /*accumulator */
                                                  NULL, /*accu_data */
                                                  g_cclosure_marshal_VOID__VOID,
                                                  G_TYPE_NONE,
                                                  0);

  signals[FULLSCREEN_CHANGED] = g_signal_new ("fullscreen-changed",
                                              G_TYPE_FROM_CLASS (class),
                                              G_SIGNAL_RUN_LAST,
                                              G_STRUCT_OFFSET (PhotosItemManagerClass,
                                                               fullscreen_changed),
                                              NULL, /*accumulator */
                                              NULL, /* accu_data */
                                              g_cclosure_marshal_VOID__BOOLEAN,
                                              G_TYPE_NONE,
                                              1,
                                              G_TYPE_BOOLEAN);

  signals[LOAD_FINISHED] = g_signal_new ("load-finished",
                                         G_TYPE_FROM_CLASS (class),
                                         G_SIGNAL_RUN_LAST,
                                         G_STRUCT_OFFSET (PhotosItemManagerClass,
                                                          load_finished),
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
                                        G_STRUCT_OFFSET (PhotosItemManagerClass,
                                                         load_started),
                                        NULL, /*accumulator */
                                        NULL, /*accu_data */
                                        g_cclosure_marshal_VOID__OBJECT,
                                        G_TYPE_NONE,
                                        1,
                                        PHOTOS_TYPE_BASE_ITEM);

  signals[WINDOW_MODE_CHANGED] = g_signal_new ("window-mode-changed",
                                               G_TYPE_FROM_CLASS (class),
                                               G_SIGNAL_RUN_LAST,
                                               G_STRUCT_OFFSET (PhotosItemManagerClass,
                                                                window_mode_changed),
                                               NULL, /*accumulator */
                                               NULL, /*accu_data */
                                               _photos_marshal_VOID__ENUM_ENUM,
                                               G_TYPE_NONE,
                                               2,
                                               PHOTOS_TYPE_WINDOW_MODE,
                                               PHOTOS_TYPE_WINDOW_MODE);
}


PhotosBaseManager *
photos_item_manager_new (void)
{
  return g_object_new (PHOTOS_TYPE_ITEM_MANAGER, NULL);
}


void
photos_item_manager_activate_previous_collection (PhotosItemManager *self)
{
  gpointer *collection;

  photos_item_manager_clear_active_item_load (self);

  collection = g_queue_pop_head (self->collection_path);
  g_assert (collection == NULL || PHOTOS_IS_BASE_ITEM (collection));

  g_clear_object (&self->active_collection);

  if (collection != NULL)
    g_object_ref (collection);
  self->active_collection = PHOTOS_BASE_ITEM (collection);

  PHOTOS_BASE_MANAGER_CLASS (photos_item_manager_parent_class)
    ->set_active_object (PHOTOS_BASE_MANAGER (self), (GObject *) collection);

  g_signal_emit (self, signals[ACTIVE_COLLECTION_CHANGED], 0, self->active_collection);

  g_clear_object (&collection);
}


void
photos_item_manager_add_item (PhotosItemManager *self, TrackerSparqlCursor *cursor)
{
  PhotosBaseItem *item = NULL;
  GObject *object;
  const gchar *id;

  id = tracker_sparql_cursor_get_string (cursor, PHOTOS_QUERY_COLUMNS_URN, NULL);
  object = photos_base_manager_get_object_by_id (PHOTOS_BASE_MANAGER (self), id);
  if (object != NULL)
    {
      g_signal_emit_by_name (self, "object-added", object);
      goto out;
    }

  item = photos_item_manager_create_item (self, cursor);
  photos_base_manager_add_object (PHOTOS_BASE_MANAGER (self), G_OBJECT (item));

 out:
  g_clear_object (&item);
}


PhotosBaseItem *
photos_item_manager_create_item (PhotosItemManager *self, TrackerSparqlCursor *cursor)
{
  PhotosBaseItem *ret_val = NULL;
  GIOExtension *extension;
  GType type;
  const gchar *extension_name = "local";
  gchar *identifier = NULL;
  gchar **split_identifier = NULL;

  identifier = g_strdup (tracker_sparql_cursor_get_string (cursor, PHOTOS_QUERY_COLUMNS_IDENTIFIER, NULL));
  if (identifier == NULL)
    goto final;

  split_identifier = g_strsplit (identifier, ":", 4);

  if (g_str_has_prefix (identifier, "photos:collection:"))
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

 final:
  extension = g_io_extension_point_get_extension_by_name (self->extension_point, extension_name);
  if (G_UNLIKELY (extension == NULL))
    {
      g_warning ("Unable to find extension %s for identifier: %s", extension_name, identifier);
      goto out;
    }

  type = g_io_extension_get_type (extension);
  ret_val = PHOTOS_BASE_ITEM (g_object_new (type,
                                            "cursor", cursor,
                                            "failed-thumbnailing", FALSE,
                                            NULL));

 out:
  g_strfreev (split_identifier);
  g_free (identifier);
  return ret_val;
}


PhotosBaseItem *
photos_item_manager_get_active_collection (PhotosItemManager *self)
{
  return self->active_collection;
}


GHashTable *
photos_item_manager_get_collections (PhotosItemManager *self)
{
  return self->collections;
}


gboolean
photos_mode_controller_get_can_fullscreen (PhotosModeController *self)
{
  return self->mode == PHOTOS_WINDOW_MODE_PREVIEW;
}


gboolean
photos_mode_controller_get_fullscreen (PhotosModeController *self)
{
  return self->fullscreen;
}


PhotosWindowMode
photos_mode_controller_get_window_mode (PhotosModeController *self)
{
  return self->mode;
}


void
photos_mode_controller_go_back (PhotosModeController *self)
{
  PhotosWindowMode old_mode;
  PhotosWindowMode tmp;

  g_return_if_fail (!g_queue_is_empty (self->history));

  old_mode = (PhotosWindowMode) GPOINTER_TO_INT (g_queue_peek_head (self->history));

  /* Always go back to the overview when activated from the search
   * provider. It is easier to special case it here instead of all
   * over the code.
   */
  if (self->mode == PHOTOS_WINDOW_MODE_PREVIEW && old_mode == PHOTOS_WINDOW_MODE_NONE)
    old_mode = PHOTOS_WINDOW_MODE_OVERVIEW;

  g_return_if_fail (old_mode != PHOTOS_WINDOW_MODE_NONE);
  g_return_if_fail (old_mode != PHOTOS_WINDOW_MODE_PREVIEW
                    || (self->mode == PHOTOS_WINDOW_MODE_EDIT && old_mode == PHOTOS_WINDOW_MODE_PREVIEW));

  g_queue_pop_head (self->history);

  /* Swap the old and current modes */
  tmp = old_mode;
  old_mode = self->mode;
  self->mode = tmp;

  photos_item_manager_update_fullscreen (self);
  photos_item_manager_clear_active_item_load (self);

  if (old_mode == PHOTOS_WINDOW_MODE_PREVIEW)
    {
      PHOTOS_BASE_MANAGER_CLASS (photos_item_manager_parent_class)
        ->set_active_object (PHOTOS_BASE_MANAGER (self), (GObject *) self->active_collection);
    }
  else if (old_mode != PHOTOS_WINDOW_MODE_EDIT)
    {
      photos_item_manager_collection_path_free (self);
      self->collection_path = g_queue_new ();

      g_clear_object (&self->active_collection);

      PHOTOS_BASE_MANAGER_CLASS (photos_item_manager_parent_class)
        ->set_active_object (PHOTOS_BASE_MANAGER (self), NULL);

      g_signal_emit (self, signals[ACTIVE_COLLECTION_CHANGED], 0, self->active_collection);
    }

  g_signal_emit (self, signals[WINDOW_MODE_CHANGED], 0, self->mode, old_mode);
}


void
photos_mode_controller_toggle_fullscreen (PhotosModeController *self)
{
  photos_mode_controller_set_fullscreen (self, !self->fullscreen);
}


void
photos_mode_controller_set_fullscreen (PhotosModeController *self, gboolean fullscreen)
{
  if (self->fullscreen == fullscreen)
    return;

  self->fullscreen = fullscreen;
  g_signal_emit (self, signals[FULLSCREEN_CHANGED], 0, self->fullscreen);
}


void
photos_mode_controller_set_window_mode (PhotosModeController *self, PhotosWindowMode mode)
{
  PhotosWindowMode old_mode;
  gboolean active_collection_changed = FALSE;

  g_return_if_fail (mode != PHOTOS_WINDOW_MODE_NONE);
  g_return_if_fail (mode != PHOTOS_WINDOW_MODE_PREVIEW);
  g_return_if_fail (self->mode != PHOTOS_WINDOW_MODE_PREVIEW
                    || (self->mode == PHOTOS_WINDOW_MODE_PREVIEW && mode == PHOTOS_WINDOW_MODE_EDIT));

  if (!photos_item_manager_set_window_mode_internal (self, mode, &old_mode))
    return;

  photos_item_manager_update_fullscreen (self);
  photos_item_manager_clear_active_item_load (self);

  if (mode != PHOTOS_WINDOW_MODE_EDIT)
    {
      photos_item_manager_collection_path_free (self);
      self->collection_path = g_queue_new ();

      if (self->active_collection != NULL)
        {
          g_clear_object (&self->active_collection);
          active_collection_changed = TRUE;
        }

      PHOTOS_BASE_MANAGER_CLASS (photos_item_manager_parent_class)
        ->set_active_object (PHOTOS_BASE_MANAGER (self), NULL);

      if (active_collection_changed)
        g_signal_emit (self, signals[ACTIVE_COLLECTION_CHANGED], 0, self->active_collection);
    }

  g_signal_emit (self, signals[WINDOW_MODE_CHANGED], 0, mode, old_mode);
}
