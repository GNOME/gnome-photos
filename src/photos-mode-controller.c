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


#include "config.h"

#include <clutter-gtk/clutter-gtk.h>
#include <glib.h>
#include <glib/gi18n.h>

#include "photos-enums.h"
#include "photos-marshalers.h"
#include "photos-mode-controller.h"


struct _PhotosModeControllerPrivate
{
  PhotosWindowMode mode;
  gboolean fullscreen;
  gboolean can_fullscreen;
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
photos_mode_controller_init (PhotosModeController *self)
{
  PhotosModeControllerPrivate *priv;

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, PHOTOS_TYPE_MODE_CONTROLLER, PhotosModeControllerPrivate);
  priv = self->priv;

  priv->mode = PHOTOS_WINDOW_MODE_NONE;
  priv->fullscreen = FALSE;
  priv->can_fullscreen = FALSE;
}


static void
photos_mode_controller_class_init (PhotosModeControllerClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->constructor = photos_mode_controller_constructor;

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

  g_type_class_add_private (class, sizeof (PhotosModeControllerPrivate));
}


PhotosModeController *
photos_mode_controller_new (void)
{
  return g_object_new (PHOTOS_TYPE_MODE_CONTROLLER, NULL);
}


PhotosWindowMode
photos_mode_controller_get_can_fullscreen (PhotosModeController *self)
{
  return self->priv->can_fullscreen;
}


PhotosWindowMode
photos_mode_controller_get_fullscreen (PhotosModeController *self)
{
  return self->priv->fullscreen;
}


PhotosWindowMode
photos_mode_controller_get_window_mode (PhotosModeController *self)
{
  return self->priv->mode;
}


void
photos_mode_controller_toggle_fullscreen (PhotosModeController *self)
{
  photos_mode_controller_set_fullscreen (self, !self->priv->fullscreen);
}


void
photos_mode_controller_set_can_fullscreen (PhotosModeController *self, gboolean can_fullscreen)
{
  PhotosModeControllerPrivate *priv = self->priv;

  priv->can_fullscreen = can_fullscreen;
  if (!priv->can_fullscreen && priv->fullscreen)
    photos_mode_controller_set_fullscreen (self, FALSE);

  g_signal_emit (self, signals[CAN_FULLSCREEN_CHANGED], 0);
}


void
photos_mode_controller_set_fullscreen (PhotosModeController *self, gboolean fullscreen)
{
  PhotosModeControllerPrivate *priv = self->priv;

  if (priv->mode != PHOTOS_WINDOW_MODE_PREVIEW)
    return;

  if (priv->fullscreen == fullscreen)
    return;

  if (fullscreen && !priv->can_fullscreen)
    return;

  priv->fullscreen = fullscreen;
  g_signal_emit (self, signals[FULLSCREEN_CHANGED], 0, priv->fullscreen);
}


void
photos_mode_controller_set_window_mode (PhotosModeController *self, PhotosWindowMode mode)
{
  PhotosModeControllerPrivate *priv = self->priv;
  PhotosWindowMode old_mode;

  old_mode = priv->mode;

  if (old_mode == mode)
    return;

  if (mode == PHOTOS_WINDOW_MODE_OVERVIEW)
    photos_mode_controller_set_can_fullscreen (self, FALSE);

  priv->mode = mode;
  g_signal_emit (self, signals[WINDOW_MODE_CHANGED], 0, priv->mode, old_mode);
}
