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

#include "photos-base-manager.h"


struct _PhotosBaseManagerPrivate
{
};

enum
{
  ACTIVE_CHANGED,
  ITEM_ADDED,
  ITEM_REMOVED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };


G_DEFINE_TYPE (PhotosBaseManager, photos_base_manager, G_TYPE_OBJECT);


static void
photos_base_manager_init (PhotosBaseManager *self)
{
  PhotosBaseManagerPrivate *priv;

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, PHOTOS_TYPE_BASE_MANAGER, PhotosBaseManagerPrivate);
  priv = self->priv;
}


static void
photos_base_manager_class_init (PhotosBaseManagerClass *class)
{
  signals[ACTIVE_CHANGED] = g_signal_new ("active-changed",
                                          G_TYPE_FROM_CLASS (class),
                                          G_SIGNAL_RUN_LAST,
                                          G_STRUCT_OFFSET (PhotosBaseManagerClass,
                                                           active_changed),
                                          NULL, /*accumulator */
                                          NULL, /*accu_data */
                                          g_cclosure_marshal_VOID__OBJECT,
                                          G_TYPE_NONE,
                                          1,
                                          G_TYPE_OBJECT);

  signals[ITEM_ADDED] = g_signal_new ("item-added",
                                      G_TYPE_FROM_CLASS (class),
                                      G_SIGNAL_RUN_LAST,
                                      G_STRUCT_OFFSET (PhotosBaseManagerClass,
                                                       item_added),
                                      NULL, /*accumulator */
                                      NULL, /* accu_data */
                                      g_cclosure_marshal_VOID__OBJECT,
                                      G_TYPE_NONE,
                                      1,
                                      G_TYPE_OBJECT);

  signals[ITEM_REMOVED] = g_signal_new ("item-removed",
                                        G_TYPE_FROM_CLASS (class),
                                        G_SIGNAL_RUN_LAST,
                                        G_STRUCT_OFFSET (PhotosBaseManagerClass,
                                                         item_removed),
                                        NULL, /*accumulator */
                                        NULL, /*accu_data */
                                        g_cclosure_marshal_VOID__OBJECT,
                                        G_TYPE_NONE,
                                        1,
                                        G_TYPE_OBJECT);

  g_type_class_add_private (class, sizeof (PhotosBaseManagerPrivate));
}


PhotosBaseManager *
photos_base_manager_new (void)
{
  return g_object_new (PHOTOS_TYPE_BASE_MANAGER, NULL);
}
