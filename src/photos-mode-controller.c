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

#include <glib.h>

#include "photos-enums.h"
#include "photos-marshalers.h"
#include "photos-mode-controller.h"


struct _PhotosModeController
{
  GObject parent_instance;
  GQueue *history;
  PhotosWindowMode mode;
  gboolean fullscreen;
};

struct _PhotosModeControllerClass
{
  GObjectClass parent_class;

  /* signals */
  void (*can_fullscreen_changed) (PhotosModeController *self);
  void (*fullscreen_changed)     (PhotosModeController *self, gboolean fullscreen);
  void (*window_mode_changed)    (PhotosModeController *self, PhotosWindowMode mode, PhotosWindowMode old_mode);
};

enum
{
  CAN_FULLSCREEN_CHANGED,
  FULLSCREEN_CHANGED,
  WINDOW_MODE_CHANGED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };


G_DEFINE_TYPE (PhotosModeController, photos_mode_controller, G_TYPE_OBJECT);


static void
photos_mode_controller_update_fullscreen (PhotosModeController *self)
{
  if (!photos_mode_controller_get_can_fullscreen (self) && self->fullscreen)
    photos_mode_controller_set_fullscreen (self, FALSE);

  g_signal_emit (self, signals[CAN_FULLSCREEN_CHANGED], 0);
}


static GObject *
photos_mode_controller_constructor (GType type, guint n_construct_params, GObjectConstructParam *construct_params)
{
  static GObject *self = NULL;

  if (self == NULL)
    {
      self = G_OBJECT_CLASS (photos_mode_controller_parent_class)->constructor (type,
                                                                                n_construct_params,
                                                                                construct_params);
      g_object_add_weak_pointer (self, (gpointer) &self);
      return self;
    }

  return g_object_ref (self);
}


static void
photos_mode_controller_finalize (GObject *object)
{
  PhotosModeController *self = PHOTOS_MODE_CONTROLLER (object);

  g_queue_free (self->history);

  G_OBJECT_CLASS (photos_mode_controller_parent_class)->finalize (object);
}


static void
photos_mode_controller_init (PhotosModeController *self)
{
  self->history = g_queue_new ();
  self->mode = PHOTOS_WINDOW_MODE_NONE;
  self->fullscreen = FALSE;
}


static void
photos_mode_controller_class_init (PhotosModeControllerClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->constructor = photos_mode_controller_constructor;
  object_class->finalize = photos_mode_controller_finalize;

  signals[CAN_FULLSCREEN_CHANGED] = g_signal_new ("can-fullscreen-changed",
                                                  G_TYPE_FROM_CLASS (class),
                                                  G_SIGNAL_RUN_LAST,
                                                  G_STRUCT_OFFSET (PhotosModeControllerClass,
                                                                   can_fullscreen_changed),
                                                  NULL, /*accumulator */
                                                  NULL, /*accu_data */
                                                  g_cclosure_marshal_VOID__VOID,
                                                  G_TYPE_NONE,
                                                  0);

  signals[FULLSCREEN_CHANGED] = g_signal_new ("fullscreen-changed",
                                              G_TYPE_FROM_CLASS (class),
                                              G_SIGNAL_RUN_LAST,
                                              G_STRUCT_OFFSET (PhotosModeControllerClass,
                                                               fullscreen_changed),
                                              NULL, /*accumulator */
                                              NULL, /* accu_data */
                                              g_cclosure_marshal_VOID__BOOLEAN,
                                              G_TYPE_NONE,
                                              1,
                                              G_TYPE_BOOLEAN);

  signals[WINDOW_MODE_CHANGED] = g_signal_new ("window-mode-changed",
                                               G_TYPE_FROM_CLASS (class),
                                               G_SIGNAL_RUN_LAST,
                                               G_STRUCT_OFFSET (PhotosModeControllerClass,
                                                                window_mode_changed),
                                               NULL, /*accumulator */
                                               NULL, /*accu_data */
                                               _photos_marshal_VOID__ENUM_ENUM,
                                               G_TYPE_NONE,
                                               2,
                                               PHOTOS_TYPE_WINDOW_MODE,
                                               PHOTOS_TYPE_WINDOW_MODE);
}


PhotosModeController *
photos_mode_controller_dup_singleton (void)
{
  return g_object_new (PHOTOS_TYPE_MODE_CONTROLLER, NULL);
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

  if (g_queue_is_empty (self->history))
    return;

  old_mode = (PhotosWindowMode) GPOINTER_TO_INT (g_queue_pop_head (self->history));

  /* Always go back to the overview when activated from the search
   * provider. It is easier to special case it here instead of all
   * over the code.
   */
  if (self->mode == PHOTOS_WINDOW_MODE_PREVIEW && old_mode == PHOTOS_WINDOW_MODE_NONE)
    old_mode = PHOTOS_WINDOW_MODE_OVERVIEW;

  if (old_mode == PHOTOS_WINDOW_MODE_NONE)
    return;

  /* Swap the old and current modes */
  tmp = old_mode;
  old_mode = self->mode;
  self->mode = tmp;

  photos_mode_controller_update_fullscreen (self);
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

  old_mode = self->mode;

  if (old_mode == mode)
    return;

  g_queue_push_head (self->history, GINT_TO_POINTER (old_mode));
  self->mode = mode;

  photos_mode_controller_update_fullscreen (self);
  g_signal_emit (self, signals[WINDOW_MODE_CHANGED], 0, self->mode, old_mode);
}
