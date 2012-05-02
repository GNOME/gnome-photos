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

#include <glib.h>

#include "photos-offset-controller.h"


struct _PhotosOffsetControllerPrivate
{
  gint count;
  gint offset;
};

enum
{
  COUNT_CHANGED,
  OFFSET_CHANGED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };


G_DEFINE_TYPE (PhotosOffsetController, photos_offset_controller, G_TYPE_OBJECT);


enum
{
  OFFSET_STEP = 50
};


static GObject *
photos_offset_controller_constructor (GType                  type,
                                      guint                  n_construct_params,
                                      GObjectConstructParam *construct_params)
{
  static GObject *self = NULL;

  if (self == NULL)
    {
      self = G_OBJECT_CLASS (photos_offset_controller_parent_class)->constructor (type,
                                                                                  n_construct_params,
                                                                                  construct_params);
      g_object_add_weak_pointer (self, (gpointer) &self);
      return self;
    }

  return g_object_ref (self);
}


static void
photos_offset_controller_init (PhotosOffsetController *self)
{
  PhotosOffsetControllerPrivate *priv;

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                            PHOTOS_TYPE_OFFSET_CONTROLLER,
                                            PhotosOffsetControllerPrivate);
  priv = self->priv;
}


static void
photos_offset_controller_class_init (PhotosOffsetControllerClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->constructor = photos_offset_controller_constructor;

  signals[COUNT_CHANGED] = g_signal_new ("count-changed",
                                         G_TYPE_FROM_CLASS (class),
                                         G_SIGNAL_RUN_LAST,
                                         G_STRUCT_OFFSET (PhotosOffsetControllerClass,
                                                          count_changed),
                                         NULL, /*accumulator */
                                         NULL, /*accu_data */
                                         g_cclosure_marshal_VOID__INT,
                                         G_TYPE_NONE,
                                         1,
                                         G_TYPE_INT);

  signals[OFFSET_CHANGED] = g_signal_new ("offset-changed",
                                          G_TYPE_FROM_CLASS (class),
                                          G_SIGNAL_RUN_LAST,
                                          G_STRUCT_OFFSET (PhotosOffsetControllerClass,
                                                           offset_changed),
                                          NULL, /*accumulator */
                                          NULL, /*accu_data */
                                          g_cclosure_marshal_VOID__INT,
                                          G_TYPE_NONE,
                                          1,
                                          G_TYPE_INT);

  g_type_class_add_private (class, sizeof (PhotosOffsetControllerPrivate));
}


PhotosOffsetController *
photos_offset_controller_new (void)
{
  return g_object_new (PHOTOS_TYPE_OFFSET_CONTROLLER, NULL);
}


gint
photos_offset_controller_get_offset (PhotosOffsetController *self)
{
  return self->priv->offset;
}


gint
photos_offset_controller_get_remaining (PhotosOffsetController *self)
{
  PhotosOffsetControllerPrivate *priv = self->priv;
  return priv->count - (priv->offset + OFFSET_STEP);
}


gint
photos_offset_controller_get_step (PhotosOffsetController *self)
{
  return OFFSET_STEP;
}


void
photos_offset_controller_increase_offset (PhotosOffsetController *self)
{
  PhotosOffsetControllerPrivate *priv = self->priv;

  priv->offset += OFFSET_STEP;
  g_signal_emit (self, signals[OFFSET_CHANGED], 0, priv->offset);
}


void
photos_offset_controller_reset_offset (PhotosOffsetController *self)
{
  self->priv->offset = 0;
}
