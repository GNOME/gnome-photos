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

#include <gio/gio.h>
#include <glib.h>

#include "photos-local-item.h"


G_DEFINE_TYPE (PhotosLocalItem, photos_local_item, PHOTOS_TYPE_BASE_ITEM);


static void
photos_local_item_constructed (GObject *object)
{
  PhotosLocalItem *self = PHOTOS_LOCAL_ITEM (object);
  PhotosLocalItemPrivate *priv = self->priv;
  GAppInfo *default_app = NULL;
  const gchar *default_app_name;
  const gchar *mime_type;

  G_OBJECT_CLASS (photos_local_item_parent_class)->constructed (object);

  mime_type = photos_base_item_get_mime_type (PHOTOS_BASE_ITEM (self));
  if (mime_type == NULL)
    return;

  default_app = g_app_info_get_default_for_type (mime_type, TRUE);
  if (default_app == NULL)
    return;

  default_app_name = g_app_info_get_name (default_app);
  photos_base_item_set_default_app_name (PHOTOS_BASE_ITEM (self), default_app_name);

  g_object_unref (default_app);
}


static void
photos_local_item_init (PhotosLocalItem *self)
{
}


static void
photos_local_item_class_init (PhotosLocalItemClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->constructed= photos_local_item_constructed;
}


PhotosBaseItem *
photos_local_item_new (TrackerSparqlCursor *cursor)
{
  return g_object_new (PHOTOS_TYPE_LOCAL_ITEM,
                       "cursor", cursor,
                       "failed-thumbnailing", FALSE,
                       "tried-thumbnailing", FALSE,
                       NULL);
}
