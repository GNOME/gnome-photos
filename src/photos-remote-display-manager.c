/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2013 Intel Corporation. All rights reserved.
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

#include "photos-remote-display-manager.h"

#include <gio/gio.h>

#include "photos-dlna-renderers-manager.h"

typedef struct {
    PhotosRemoteDisplayManager *manager;
    PhotosDlnaRenderer *renderer;
    PhotosBaseItem *item;
} Share;

struct _PhotosRemoteDisplayManagerPrivate
{
  PhotosDlnaRenderersManager *renderers_mngr;
  PhotosDlnaRenderer *renderer;
};

enum
{
  SHARE_BEGAN,
  SHARE_ENDED,
  SHARE_ERROR,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static GObject *remote_display_manager_singleton = NULL;

G_DEFINE_TYPE (PhotosRemoteDisplayManager, photos_remote_display_manager, G_TYPE_OBJECT);


static Share *
photos_remote_display_manager_share_new (PhotosRemoteDisplayManager *manager,
                                         PhotosDlnaRenderer *renderer,
                                         PhotosBaseItem *item)
{
  Share *share;

  share = g_new (Share, 1);
  share->manager = manager;
  share->renderer = g_object_ref (renderer);
  share->item = g_object_ref (item);

  return share;
}


static void
photos_remote_display_manager_share_destroy (Share *share)
{
  g_object_unref (share->renderer);
  g_object_unref (share->item);
  g_free (share);
}

static void
photos_remote_display_manager_dispose (GObject *object)
{
  PhotosRemoteDisplayManager *self = PHOTOS_REMOTE_DISPLAY_MANAGER (object);
  PhotosRemoteDisplayManagerPrivate *priv = self->priv;

  g_clear_object (&priv->renderers_mngr);
  g_clear_object (&priv->renderer);

  G_OBJECT_CLASS (photos_remote_display_manager_parent_class)->dispose (object);
}


static GObject *
photos_remote_display_manager_constructor (GType                  type,
                                           guint                  n_construct_params,
                                           GObjectConstructParam *construct_params)
{
  if (remote_display_manager_singleton != NULL)
    return g_object_ref (remote_display_manager_singleton);

  remote_display_manager_singleton =
      G_OBJECT_CLASS (photos_remote_display_manager_parent_class)->constructor
          (type, n_construct_params, construct_params);

  g_object_add_weak_pointer (remote_display_manager_singleton, (gpointer) &remote_display_manager_singleton);

  return remote_display_manager_singleton;
}

static void
photos_remote_display_manager_renderer_lost_cb (PhotosRemoteDisplayManager *self,
                                                PhotosDlnaRenderer         *renderer,
                                                gpointer                    user_data)
{
  PhotosRemoteDisplayManagerPrivate *priv = self->priv;

  if (renderer == priv->renderer)
    photos_remote_display_manager_stop (self);
}

static void
photos_remote_display_manager_init (PhotosRemoteDisplayManager *self)
{
  PhotosRemoteDisplayManagerPrivate *priv;

  self->priv = priv = G_TYPE_INSTANCE_GET_PRIVATE (self, PHOTOS_TYPE_REMOTE_DISPLAY_MANAGER,
                                            PhotosRemoteDisplayManagerPrivate);

  /* Keep a connection to the renderers manager alive to keep the list of
   * renderers up-to-date */
  priv->renderers_mngr = photos_dlna_renderers_manager_dup_singleton ();

  g_signal_connect_object (priv->renderers_mngr, "renderer-lost",
                           G_CALLBACK (photos_remote_display_manager_renderer_lost_cb), self,
                           G_CONNECT_SWAPPED);
}

static void
photos_remote_display_manager_class_init (PhotosRemoteDisplayManagerClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->constructor = photos_remote_display_manager_constructor;
  object_class->dispose = photos_remote_display_manager_dispose;

  signals[SHARE_BEGAN] = g_signal_new ("share-began", G_TYPE_FROM_CLASS (class),
                                       G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL,
                                       G_TYPE_NONE, 2,
                                       PHOTOS_TYPE_DLNA_RENDERER,
                                       PHOTOS_TYPE_BASE_ITEM);

  signals[SHARE_ENDED] = g_signal_new ("share-ended", G_TYPE_FROM_CLASS (class),
                                       G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL,
                                       G_TYPE_NONE, 1,
                                       PHOTOS_TYPE_DLNA_RENDERER);

  signals[SHARE_ERROR] = g_signal_new ("share-error", G_TYPE_FROM_CLASS (class),
                                       G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL,
                                       G_TYPE_NONE, 3,
                                       PHOTOS_TYPE_DLNA_RENDERER,
                                       PHOTOS_TYPE_BASE_ITEM,
                                       G_TYPE_ERROR);

  g_type_class_add_private (class, sizeof (PhotosRemoteDisplayManagerPrivate));
}


static void
photos_remote_display_manager_share_cb (GObject      *source_object,
                                        GAsyncResult *res,
                                        gpointer      user_data)
{
  Share *share = user_data;
  PhotosRemoteDisplayManager *self = share->manager;
  PhotosRemoteDisplayManagerPrivate *priv = self->priv;
  PhotosDlnaRenderer *renderer = PHOTOS_DLNA_RENDERER (source_object);
  PhotosBaseItem *item;
  GError *error = NULL;

  item = photos_dlna_renderer_share_finish (priv->renderer, res, &error);
  g_object_unref (item); /* We already hold a ref to the item to be shared */

  if (error != NULL)
    {
      g_warning ("Unable to remotely display item '%s': %s",
                 item != NULL ? photos_base_item_get_id (item) : "(none)",
                 error->message);
      g_signal_emit (share->manager, signals[SHARE_ERROR], 0, share->renderer, share->item, error);
      g_error_free (error);
      goto out;
    }

  g_signal_emit (share->manager, signals[SHARE_BEGAN], 0, share->renderer, share->item);

out:
  photos_remote_display_manager_share_destroy (share);
}


static void
photos_remote_display_manager_unshare_all_cb (GObject      *source_object,
                                              GAsyncResult *res,
                                              gpointer      user_data)
{
  PhotosDlnaRenderer *renderer = PHOTOS_DLNA_RENDERER (source_object);
  PhotosRemoteDisplayManager *self = PHOTOS_REMOTE_DISPLAY_MANAGER (user_data);
  GError *error = NULL;

  photos_dlna_renderer_unshare_all_finish (renderer, res, &error);

  if (error != NULL)
    {
      g_warning ("Error while unsharing: %s", error->message);
      g_error_free (error);
    }

  /* Avoid firing ::share-ended if any other item has been shared between the
   * _unshare_all() call and this callback */
  if (!photos_remote_display_manager_is_active (self))
    g_signal_emit (self, signals[SHARE_ENDED], 0, renderer);
}


PhotosRemoteDisplayManager *
photos_remote_display_manager_dup_singleton (void)
{
  return g_object_new (PHOTOS_TYPE_REMOTE_DISPLAY_MANAGER, NULL);
}


void
photos_remote_display_manager_set_renderer (PhotosRemoteDisplayManager *self,
                                            PhotosDlnaRenderer         *renderer)
{
  PhotosRemoteDisplayManagerPrivate *priv = self->priv;

  g_clear_object (&priv->renderer);

  if (renderer)
    priv->renderer = g_object_ref (renderer);
}


PhotosDlnaRenderer *
photos_remote_display_manager_get_renderer (PhotosRemoteDisplayManager *self)
{
  PhotosRemoteDisplayManagerPrivate *priv = self->priv;

  return priv->renderer;
}


void
photos_remote_display_manager_render (PhotosRemoteDisplayManager *self,
                                      PhotosBaseItem             *item)
{
  PhotosRemoteDisplayManagerPrivate *priv = self->priv;
  Share *share;

  g_return_if_fail (priv->renderer != NULL);

  share = photos_remote_display_manager_share_new (self, priv->renderer, item);
  photos_dlna_renderer_share (share->renderer, share->item, NULL,
                              photos_remote_display_manager_share_cb, share);
}


void
photos_remote_display_manager_stop (PhotosRemoteDisplayManager *self)
{
  PhotosRemoteDisplayManagerPrivate *priv = self->priv;

  g_return_if_fail (priv->renderer != NULL);

  photos_dlna_renderer_unshare_all (priv->renderer, NULL,
                                    photos_remote_display_manager_unshare_all_cb, self);
}


gboolean
photos_remote_display_manager_is_active (PhotosRemoteDisplayManager *self)
{
  PhotosRemoteDisplayManagerPrivate *priv = self->priv;

  if (priv->renderer == NULL)
    return FALSE;

  return photos_dlna_renderer_get_shared_count (priv->renderer) > 0;
}
