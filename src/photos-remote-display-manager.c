/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2013 Intel Corporation. All rights reserved.
 * Copyright © 2014 – 2017 Red Hat, Inc.
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


#include "config.h"

#include <gio/gio.h>

#include "photos-dlna-renderers-manager.h"
#include "photos-filterable.h"
#include "photos-remote-display-manager.h"


struct _PhotosRemoteDisplayManager
{
  GObject parent_instance;
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


typedef struct _Share Share;

struct _Share
{
  PhotosRemoteDisplayManager *manager;
  PhotosDlnaRenderer *renderer;
  PhotosBaseItem *item;
};


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

  g_clear_object (&self->renderer);

  G_OBJECT_CLASS (photos_remote_display_manager_parent_class)->dispose (object);
}


static GObject *
photos_remote_display_manager_constructor (GType type,
                                           guint n_construct_params,
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
photos_remote_display_manager_init (PhotosRemoteDisplayManager *self)
{
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
}


static void
photos_remote_display_manager_share_cb (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  Share *share = user_data;
  g_autoptr (PhotosBaseItem) item = NULL;
  PhotosDlnaRenderer *renderer = PHOTOS_DLNA_RENDERER (source_object);

  {
    g_autoptr (GError) error = NULL;

    item = photos_dlna_renderer_share_finish (renderer, res, &error);
    if (error != NULL)
      {
        g_warning ("Unable to remotely display item '%s': %s",
                   share->item != NULL ? photos_filterable_get_id (PHOTOS_FILTERABLE (share->item)) : "(none)",
                   error->message);
        g_signal_emit (share->manager, signals[SHARE_ERROR], 0, share->renderer, share->item, error);
        goto out;
     }
  }

  g_signal_emit (share->manager, signals[SHARE_BEGAN], 0, share->renderer, share->item);

 out:
  photos_remote_display_manager_share_destroy (share);
}


static void
photos_remote_display_manager_unshare_all_cb (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosDlnaRenderer *renderer = PHOTOS_DLNA_RENDERER (source_object);
  PhotosRemoteDisplayManager *self = PHOTOS_REMOTE_DISPLAY_MANAGER (user_data);

  {
    g_autoptr (GError) error = NULL;

    photos_dlna_renderer_unshare_all_finish (renderer, res, &error);
    if (error != NULL)
      g_warning ("Error while unsharing: %s", error->message);
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
photos_remote_display_manager_set_renderer (PhotosRemoteDisplayManager *self, PhotosDlnaRenderer *renderer)
{
  g_clear_object (&self->renderer);

  if (renderer)
    self->renderer = g_object_ref (renderer);
}


PhotosDlnaRenderer *
photos_remote_display_manager_get_renderer (PhotosRemoteDisplayManager *self)
{
  return self->renderer;
}


void
photos_remote_display_manager_render (PhotosRemoteDisplayManager *self, PhotosBaseItem *item)
{
  Share *share;

  g_return_if_fail (self->renderer != NULL);

  share = photos_remote_display_manager_share_new (self, self->renderer, item);
  photos_dlna_renderer_share (share->renderer, share->item, NULL,
                              photos_remote_display_manager_share_cb, share);
}


void
photos_remote_display_manager_stop (PhotosRemoteDisplayManager *self)
{
  g_return_if_fail (self->renderer != NULL);

  photos_dlna_renderer_unshare_all (self->renderer, NULL,
                                    photos_remote_display_manager_unshare_all_cb, self);
}


gboolean
photos_remote_display_manager_is_active (PhotosRemoteDisplayManager *self)
{
  if (self->renderer == NULL)
    return FALSE;

  return photos_dlna_renderer_get_shared_count (self->renderer) > 0;
}
