/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2012 – 2017 Red Hat, Inc.
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

#include <dazzle.h>
#include <gio/gio.h>
#include <glib.h>

#include "photos-base-manager.h"
#include "photos-filterable.h"
#include "photos-search-context.h"
#include "photos-selection-controller.h"


struct _PhotosSelectionController
{
  GObject parent_instance;
  GList *selection;
  PhotosBaseManager *item_mngr;
  gboolean is_frozen;
};

enum
{
  SELECTION_CHANGED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };


G_DEFINE_TYPE (PhotosSelectionController, photos_selection_controller, G_TYPE_OBJECT);
DZL_DEFINE_COUNTER (instances,
                    "PhotosSelectionController",
                    "Instances",
                    "Number of PhotosSelectionController instances")


static void
photos_selection_controller_object_removed (PhotosBaseManager *manager, GObject *object, gpointer user_data)
{
  PhotosSelectionController *self = PHOTOS_SELECTION_CONTROLLER (user_data);
  GList *l;
  gboolean changed = FALSE;
  const gchar *id;

  id = photos_filterable_get_id (PHOTOS_FILTERABLE (object));
  l = g_list_find_custom (self->selection, (gconstpointer) id, (GCompareFunc) g_strcmp0);
  while (l != NULL)
    {
      changed = TRUE;
      g_free (l->data);
      self->selection = g_list_delete_link (self->selection, l);
      l = g_list_find_custom (self->selection, (gconstpointer) id, (GCompareFunc) g_strcmp0);
    }

  if (changed)
    g_signal_emit (self, signals[SELECTION_CHANGED], 0);
}


static GObject *
photos_selection_controller_constructor (GType                  type,
                                         guint                  n_construct_params,
                                         GObjectConstructParam *construct_params)
{
  static GObject *self = NULL;

  if (self == NULL)
    {
      self = G_OBJECT_CLASS (photos_selection_controller_parent_class)->constructor (type,
                                                                                     n_construct_params,
                                                                                     construct_params);
      g_object_add_weak_pointer (self, (gpointer) &self);
      return self;
    }

  return g_object_ref (self);
}


static void
photos_selection_controller_finalize (GObject *object)
{
  PhotosSelectionController *self = PHOTOS_SELECTION_CONTROLLER (object);

  if (self->selection != NULL)
    g_list_free_full (self->selection, g_free);

  if (self->item_mngr != NULL)
    g_object_remove_weak_pointer (G_OBJECT (self->item_mngr), (gpointer *) &self->item_mngr);

  G_OBJECT_CLASS (photos_selection_controller_parent_class)->finalize (object);

  DZL_COUNTER_DEC (instances);
}


static void
photos_selection_controller_init (PhotosSelectionController *self)
{
  GApplication *app;
  PhotosSearchContextState *state;

  DZL_COUNTER_INC (instances);

  app = g_application_get_default ();
  state = photos_search_context_get_state (PHOTOS_SEARCH_CONTEXT (app));

  self->item_mngr = state->item_mngr;
  g_object_add_weak_pointer (G_OBJECT (self->item_mngr), (gpointer *) &self->item_mngr);
  g_signal_connect_object (self->item_mngr,
                           "object-removed",
                           G_CALLBACK (photos_selection_controller_object_removed),
                           self,
                           0);
}


static void
photos_selection_controller_class_init (PhotosSelectionControllerClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->constructor = photos_selection_controller_constructor;
  object_class->finalize = photos_selection_controller_finalize;

  signals[SELECTION_CHANGED] = g_signal_new ("selection-changed",
                                             G_TYPE_FROM_CLASS (class),
                                             G_SIGNAL_RUN_LAST,
                                             0,
                                             NULL, /*accumulator */
                                             NULL, /*accu_data */
                                             g_cclosure_marshal_VOID__VOID,
                                             G_TYPE_NONE,
                                             0);
}


PhotosSelectionController *
photos_selection_controller_dup_singleton (void)
{
  return g_object_new (PHOTOS_TYPE_SELECTION_CONTROLLER, NULL);
}


void
photos_selection_controller_freeze_selection (PhotosSelectionController *self, gboolean freeze)
{
  self->is_frozen = freeze;
}


GList *
photos_selection_controller_get_selection (PhotosSelectionController *self)
{
  return self->selection;
}


void
photos_selection_controller_set_selection (PhotosSelectionController *self, GList *selection)
{
  if (self->is_frozen)
    return;

  if (self->selection == NULL && selection == NULL)
    return;

  g_list_free_full (self->selection, g_free);
  self->selection = NULL;

  self->selection = g_list_copy_deep (selection, (GCopyFunc) g_strdup, NULL);
  g_signal_emit (self, signals[SELECTION_CHANGED], 0);
}
