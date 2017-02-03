/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2013 – 2017 Red Hat, Inc.
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


#include "config.h"

#include <tracker-sparql.h>

#include "photos-camera-cache.h"
#include "photos-query-builder.h"
#include "photos-search-context.h"
#include "photos-tracker-queue.h"
#include "photos-utils.h"


struct _PhotosCameraCache
{
  GObject parent_instance;
  GError *queue_error;
  GHashTable *cache;
  PhotosTrackerQueue *queue;
};


G_DEFINE_TYPE (PhotosCameraCache, photos_camera_cache, G_TYPE_OBJECT);


static void
photos_camera_cache_cursor_next (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  GTask *task = G_TASK (user_data);
  PhotosCameraCache *self;
  TrackerSparqlCursor *cursor = TRACKER_SPARQL_CURSOR (source_object);
  GError *error;
  gboolean success;
  const gchar *manufacturer;
  const gchar *model;
  gchar *camera;
  gpointer key;

  self = PHOTOS_CAMERA_CACHE (g_task_get_source_object (task));

  error = NULL;
  /* Note that tracker_sparql_cursor_next_finish can return FALSE even
   * without an error.
   */
  success = tracker_sparql_cursor_next_finish (cursor, res, &error);
  if (error != NULL)
    {
      g_task_return_error (task, error);
      goto out;
    }

  /* Note that the following SPARQL query:
   *   SELECT nfo:manufacturer (<(foo)>) nfo:model (<(foo)>) WHERE {}
   * ... will not return an empty cursor, but:
   *   (null), (null)
   */
  if (!success)
    {
      g_task_return_new_error (task, PHOTOS_ERROR, 0, "Cursor is empty — possibly wrong SPARQL query");
      goto out;
    }

  manufacturer = tracker_sparql_cursor_get_string (cursor, 0, NULL);
  model = tracker_sparql_cursor_get_string (cursor, 1, NULL);

  if (manufacturer == NULL && model == NULL)
    camera = NULL;
  else if (manufacturer == NULL || g_str_has_prefix (model, manufacturer))
    camera = g_strdup (model);
  else if (model == NULL)
    camera = g_strdup (manufacturer);
  else
    camera = g_strconcat (manufacturer, " ", model, NULL);

  key = g_task_get_task_data (task);
  g_hash_table_insert (self->cache, key, camera);

  g_task_return_pointer (task, g_strdup (camera), g_free);

 out:
  tracker_sparql_cursor_close (cursor);
  g_object_unref (task);
}


static void
photos_camera_cache_equipment_query_executed (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  GTask *task = G_TASK (user_data);
  TrackerSparqlConnection *connection = TRACKER_SPARQL_CONNECTION (source_object);
  TrackerSparqlCursor *cursor = NULL;
  GError *error;

  error = NULL;
  cursor = tracker_sparql_connection_query_finish (connection, res, &error);
  if (error != NULL)
    {
      g_task_return_error (task, error);
      goto out;
    }

  tracker_sparql_cursor_next_async (cursor, NULL, photos_camera_cache_cursor_next, g_object_ref (task));

 out:
  g_clear_object (&cursor);
}


static GObject *
photos_camera_cache_constructor (GType type, guint n_construct_params, GObjectConstructParam *construct_params)
{
  static GObject *self = NULL;

  if (self == NULL)
    {
      self = G_OBJECT_CLASS (photos_camera_cache_parent_class)->constructor (type,
                                                                             n_construct_params,
                                                                             construct_params);
      g_object_add_weak_pointer (self, (gpointer) &self);
      return self;
    }

  return g_object_ref (self);
}


static void
photos_camera_cache_dispose (GObject *object)
{
  PhotosCameraCache *self = PHOTOS_CAMERA_CACHE (object);

  g_clear_object (&self->queue);

  G_OBJECT_CLASS (photos_camera_cache_parent_class)->dispose (object);
}


static void
photos_camera_cache_finalize (GObject *object)
{
  PhotosCameraCache *self = PHOTOS_CAMERA_CACHE (object);

  g_clear_error (&self->queue_error);
  g_hash_table_unref (self->cache);

  G_OBJECT_CLASS (photos_camera_cache_parent_class)->finalize (object);
}


static void
photos_camera_cache_init (PhotosCameraCache *self)
{
  self->cache = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, g_free);
  self->queue = photos_tracker_queue_dup_singleton (NULL, &self->queue_error);
}


static void
photos_camera_cache_class_init (PhotosCameraCacheClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->constructor = photos_camera_cache_constructor;
  object_class->dispose = photos_camera_cache_dispose;
  object_class->finalize = photos_camera_cache_finalize;
}


PhotosCameraCache *
photos_camera_cache_dup_singleton (void)
{
  return g_object_new (PHOTOS_TYPE_CAMERA_CACHE, NULL);
}


void
photos_camera_cache_get_camera_async (PhotosCameraCache *self,
                                      GQuark id,
                                      GCancellable *cancellable,
                                      GAsyncReadyCallback callback,
                                      gpointer user_data)
{
  GApplication *app;
  GTask *task;
  PhotosQuery *query;
  PhotosSearchContextState *state;
  const gchar *camera;

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, photos_camera_cache_get_camera_async);
  g_task_set_task_data (task, GUINT_TO_POINTER (id), NULL);

  camera = g_hash_table_lookup (self->cache, GUINT_TO_POINTER (id));
  if (camera != NULL)
    {
      g_task_return_pointer (task, g_strdup (camera), g_free);
      goto out;
    }

  if (G_UNLIKELY (self->queue == NULL))
    {
      g_task_return_error (task, g_error_copy (self->queue_error));
      goto out;
    }

  app = g_application_get_default ();
  state = photos_search_context_get_state (PHOTOS_SEARCH_CONTEXT (app));

  query = photos_query_builder_equipment_query (state, id);
  photos_tracker_queue_select (self->queue,
                               query->sparql,
                               cancellable,
                               photos_camera_cache_equipment_query_executed,
                               g_object_ref (task),
                               g_object_unref);
  photos_query_free (query);

 out:
  g_object_unref (task);
}


gchar *
photos_camera_cache_get_camera_finish (PhotosCameraCache *self, GAsyncResult *res, GError **error)
{
  GTask *task = G_TASK (res);

  g_return_val_if_fail (g_task_is_valid (res, self), NULL);
  g_return_val_if_fail (g_task_get_source_tag (task) == photos_camera_cache_get_camera_async, NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  return g_task_propagate_pointer (task, error);
}
