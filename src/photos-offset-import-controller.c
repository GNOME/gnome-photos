/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2017 Red Hat, Inc.
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

#include "photos-base-manager.h"
#include "photos-filterable.h"
#include "photos-offset-import-controller.h"
#include "photos-query.h"
#include "photos-query-builder.h"
#include "photos-search-context.h"
#include "photos-source.h"


struct _PhotosOffsetImportController
{
  PhotosOffsetController parent_instance;
  PhotosBaseManager *src_mngr;
};


G_DEFINE_TYPE (PhotosOffsetImportController, photos_offset_import_controller, PHOTOS_TYPE_OFFSET_CONTROLLER);


static PhotosQuery *
photos_offset_import_controller_get_query (PhotosOffsetController *offset_cntrlr)
{
  PhotosOffsetImportController *self = PHOTOS_OFFSET_IMPORT_CONTROLLER (offset_cntrlr);
  GApplication *app;
  GMount *mount;
  g_autoptr (PhotosQuery) query = NULL;
  PhotosQuery *ret_val = NULL;
  PhotosSearchContextState *state;
  PhotosSource *source;
  const gchar *id;

  source = PHOTOS_SOURCE (photos_base_manager_get_active_object (self->src_mngr));
  g_return_val_if_fail (PHOTOS_IS_SOURCE (source), NULL);

  id = photos_filterable_get_id (PHOTOS_FILTERABLE (source));
  mount = photos_source_get_mount (source);
  g_return_val_if_fail (g_strcmp0 (id, PHOTOS_SOURCE_STOCK_ALL) == 0 || G_IS_MOUNT (mount), NULL);

  app = g_application_get_default ();
  state = photos_search_context_get_state (PHOTOS_SEARCH_CONTEXT (app));

  if (mount != NULL)
    query = photos_query_builder_count_query (state, PHOTOS_QUERY_FLAGS_IMPORT);
  else
    query = photos_query_new (state, "SELECT DISTINCT COUNT (?urn) WHERE { ?urn a rdfs:Resource FILTER (false) }");

  ret_val = g_steal_pointer (&query);
  return ret_val;
}


static GObject *
photos_offset_import_controller_constructor (GType type,
                                             guint n_construct_params,
                                             GObjectConstructParam *construct_params)
{
  static GObject *self = NULL;

  if (self == NULL)
    {
      self = G_OBJECT_CLASS (photos_offset_import_controller_parent_class)->constructor (type,
                                                                                         n_construct_params,
                                                                                         construct_params);
      g_object_add_weak_pointer (self, (gpointer) &self);
      return self;
    }

  return g_object_ref (self);
}


static void
photos_offset_import_controller_dispose (GObject *object)
{
  PhotosOffsetImportController *self = PHOTOS_OFFSET_IMPORT_CONTROLLER (object);

  g_clear_object (&self->src_mngr);

  G_OBJECT_CLASS (photos_offset_import_controller_parent_class)->dispose (object);
}


static void
photos_offset_import_controller_init (PhotosOffsetImportController *self)
{
  GApplication *app;
  PhotosSearchContextState *state;

  app = g_application_get_default ();
  state = photos_search_context_get_state (PHOTOS_SEARCH_CONTEXT (app));

  self->src_mngr = g_object_ref (state->src_mngr);
}


static void
photos_offset_import_controller_class_init (PhotosOffsetImportControllerClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  PhotosOffsetControllerClass *offset_controller_class = PHOTOS_OFFSET_CONTROLLER_CLASS (class);

  object_class->constructor = photos_offset_import_controller_constructor;
  object_class->dispose = photos_offset_import_controller_dispose;
  offset_controller_class->get_query = photos_offset_import_controller_get_query;
}


PhotosOffsetController *
photos_offset_import_controller_dup_singleton (void)
{
  return g_object_new (PHOTOS_TYPE_OFFSET_IMPORT_CONTROLLER, NULL);
}
