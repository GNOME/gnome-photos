/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2012 Red Hat, Inc.
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

#include <glib.h>

#include "photos-item-manager.h"
#include "photos-selection-controller.h"


struct _PhotosSelectionControllerPrivate
{
  GList *selection;
  PhotosBaseManager *item_mngr;
  gboolean is_frozen;
  gboolean selection_mode;
};

enum
{
  SELECTION_CHANGED,
  SELECTION_MODE_CHANGED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };


G_DEFINE_TYPE_WITH_PRIVATE (PhotosSelectionController, photos_selection_controller, G_TYPE_OBJECT);


static void
photos_selection_controller_object_removed (PhotosBaseManager *manager, GObject *object, gpointer user_data)
{
  PhotosSelectionController *self = PHOTOS_SELECTION_CONTROLLER (user_data);
  PhotosSelectionControllerPrivate *priv = self->priv;
  GList *l;
  gboolean changed = FALSE;
  gchar *id;

  g_object_get (object, "id", &id, NULL);

  l = g_list_find_custom (priv->selection, (gconstpointer) id, (GCompareFunc) g_strcmp0);
  while (l != NULL)
    {
      changed = TRUE;
      g_free (l->data);
      priv->selection = g_list_delete_link (priv->selection, l);
      l = g_list_find_custom (priv->selection, (gconstpointer) id, (GCompareFunc) g_strcmp0);
    }

  g_free (id);

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
photos_selection_controller_dispose (GObject *object)
{
  PhotosSelectionController *self = PHOTOS_SELECTION_CONTROLLER (object);

  g_clear_object (&self->priv->item_mngr);

  G_OBJECT_CLASS (photos_selection_controller_parent_class)->dispose (object);
}


static void
photos_selection_controller_finalize (GObject *object)
{
  PhotosSelectionController *self = PHOTOS_SELECTION_CONTROLLER (object);
  PhotosSelectionControllerPrivate *priv = self->priv;

  if (priv->selection != NULL)
    g_list_free_full (priv->selection, g_free);

  G_OBJECT_CLASS (photos_selection_controller_parent_class)->finalize (object);
}


static void
photos_selection_controller_init (PhotosSelectionController *self)
{
  PhotosSelectionControllerPrivate *priv;

  self->priv = photos_selection_controller_get_instance_private (self);
  priv = self->priv;

  priv->item_mngr = photos_item_manager_new ();
  g_signal_connect (priv->item_mngr,
                    "object-removed",
                    G_CALLBACK (photos_selection_controller_object_removed),
                    self);
}


static void
photos_selection_controller_class_init (PhotosSelectionControllerClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->constructor = photos_selection_controller_constructor;
  object_class->dispose = photos_selection_controller_dispose;
  object_class->finalize = photos_selection_controller_finalize;

  signals[SELECTION_CHANGED] = g_signal_new ("selection-changed",
                                             G_TYPE_FROM_CLASS (class),
                                             G_SIGNAL_RUN_LAST,
                                             G_STRUCT_OFFSET (PhotosSelectionControllerClass,
                                                              selection_changed),
                                             NULL, /*accumulator */
                                             NULL, /*accu_data */
                                             g_cclosure_marshal_VOID__VOID,
                                             G_TYPE_NONE,
                                             0);

  signals[SELECTION_MODE_CHANGED] = g_signal_new ("selection-mode-changed",
                                                  G_TYPE_FROM_CLASS (class),
                                                  G_SIGNAL_RUN_LAST,
                                                  G_STRUCT_OFFSET (PhotosSelectionControllerClass,
                                                                   selection_mode_changed),
                                                  NULL, /*accumulator */
                                                  NULL, /*accu_data */
                                                  g_cclosure_marshal_VOID__BOOLEAN,
                                                  G_TYPE_NONE,
                                                  1,
                                                  G_TYPE_BOOLEAN);
}


PhotosSelectionController *
photos_selection_controller_new (void)
{
  return g_object_new (PHOTOS_TYPE_SELECTION_CONTROLLER, NULL);
}


void
photos_selection_controller_freeze_selection (PhotosSelectionController *self, gboolean freeze)
{
  self->priv->is_frozen = freeze;
}


GList *
photos_selection_controller_get_selection (PhotosSelectionController *self)
{
  return self->priv->selection;
}


gboolean
photos_selection_controller_get_selection_mode (PhotosSelectionController *self)
{
  return self->priv->selection_mode;
}


void
photos_selection_controller_set_selection (PhotosSelectionController *self, GList *selection)
{
  PhotosSelectionControllerPrivate *priv = self->priv;

  if (priv->is_frozen)
    return;

  g_list_free_full (priv->selection, g_free);
  priv->selection = NULL;

  if (selection != NULL)
    {
      GList *l;

      for (l = selection; l!= NULL; l = l->next)
        priv->selection = g_list_prepend (priv->selection, g_strdup (l->data));
    }

  g_signal_emit (self, signals[SELECTION_CHANGED], 0);
}


void
photos_selection_controller_set_selection_mode (PhotosSelectionController *self, gboolean mode)
{
  PhotosSelectionControllerPrivate *priv = self->priv;

  if (priv->selection_mode == mode)
    return;

  priv->selection_mode = mode;
  g_signal_emit (self, signals[SELECTION_MODE_CHANGED], 0, priv->selection_mode);
}
