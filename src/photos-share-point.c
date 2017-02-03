/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2016 – 2017 Red Hat, Inc.
 * Copyright © 2016 Umang Jain
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

#include "photos-filterable.h"
#include "photos-share-point.h"


G_DEFINE_ABSTRACT_TYPE_WITH_CODE (PhotosSharePoint, photos_share_point, G_TYPE_OBJECT,
                                  G_IMPLEMENT_INTERFACE (PHOTOS_TYPE_FILTERABLE, NULL));


static void
photos_share_point_init (PhotosSharePoint *self)
{
}


static void
photos_share_point_class_init (PhotosSharePointClass *class)
{
}


GIcon *
photos_share_point_get_icon (PhotosSharePoint *self)
{
  g_return_val_if_fail (PHOTOS_IS_SHARE_POINT (self), NULL);
  return PHOTOS_SHARE_POINT_GET_CLASS (self)->get_icon (self);
}


const gchar *
photos_share_point_get_name (PhotosSharePoint *self)
{
  g_return_val_if_fail (PHOTOS_IS_SHARE_POINT (self), NULL);
  return PHOTOS_SHARE_POINT_GET_CLASS (self)->get_name (self);
}


gchar *
photos_share_point_parse_error (PhotosSharePoint *self, GError *error)
{
  g_return_val_if_fail (PHOTOS_IS_SHARE_POINT (self), NULL);
  g_return_val_if_fail (error != NULL, NULL);

  return PHOTOS_SHARE_POINT_GET_CLASS (self)->parse_error (self, error);
}


void
photos_share_point_share_async (PhotosSharePoint *self,
                                PhotosBaseItem *item,
                                GCancellable *cancellable,
                                GAsyncReadyCallback callback,
                                gpointer user_data)
{
  g_return_if_fail (PHOTOS_IS_SHARE_POINT (self));
  g_return_if_fail (PHOTOS_IS_BASE_ITEM (item));

  PHOTOS_SHARE_POINT_GET_CLASS (self)->share_async (self, item, cancellable, callback, user_data);
}


gboolean
photos_share_point_share_finish (PhotosSharePoint *self, GAsyncResult *res, GError **error)
{
  g_return_val_if_fail (PHOTOS_IS_SHARE_POINT (self), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (res), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  return PHOTOS_SHARE_POINT_GET_CLASS (self)->share_finish (self, res, error);
}
