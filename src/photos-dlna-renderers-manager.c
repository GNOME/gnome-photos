/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2013 Intel Corporation. All rights reserved.
 * Copyright © 2013 – 2017 Red Hat, Inc.
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

#include "photos-debug.h"
#include "photos-dleyna-renderer-manager.h"
#include "photos-dlna-renderers-manager.h"
#include "photos-dlna-renderer.h"


struct _PhotosDlnaRenderersManager
{
  GObject parent_instance;
  DleynaRendererManager *proxy;
  GHashTable *renderers;
  GError *error;
};

enum
{
  RENDERER_FOUND,
  RENDERER_LOST,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };


G_DEFINE_TYPE (PhotosDlnaRenderersManager, photos_dlna_renderers_manager, G_TYPE_OBJECT);


static GObject *photos_dlna_renderers_manager_singleton = NULL;


static void
photos_dlna_renderers_manager_dispose (GObject *object)
{
  PhotosDlnaRenderersManager *self = PHOTOS_DLNA_RENDERERS_MANAGER (object);

  g_clear_object (&self->proxy);
  g_clear_pointer (&self->renderers, g_hash_table_unref);
  g_clear_error (&self->error);

  G_OBJECT_CLASS (photos_dlna_renderers_manager_parent_class)->dispose (object);
}


static void
photos_dlna_renderers_manager_renderer_new_cb (GObject      *source_object,
                                               GAsyncResult *res,
                                               gpointer      user_data)
{
  PhotosDlnaRenderersManager *self = PHOTOS_DLNA_RENDERERS_MANAGER (user_data);
  PhotosDlnaRenderer *renderer;
  const gchar *object_path;
  GError *error = NULL;

  renderer = photos_dlna_renderer_new_for_bus_finish (res, &error);
  if (error != NULL)
    {
      g_warning ("Unable to load renderer object: %s", error->message);
      g_propagate_error (&self->error, error);
      return;
    }

  object_path = photos_dlna_renderer_get_object_path (renderer);
  photos_debug (PHOTOS_DEBUG_DLNA,
                "%s '%s' %s %s", G_STRFUNC,
                photos_dlna_renderer_get_friendly_name (renderer),
                photos_dlna_renderer_get_udn (renderer),
                object_path);
  g_hash_table_insert (self->renderers, (gpointer) object_path, renderer);
  g_signal_emit (self, signals[RENDERER_FOUND], 0, renderer);
}


static void
photos_dlna_renderers_manager_renderer_found_cb (PhotosDlnaRenderersManager *self,
                                                 const gchar                *object_path,
                                                 gpointer                   *data)
{
  photos_dlna_renderer_new_for_bus (G_BUS_TYPE_SESSION,
                                    G_DBUS_PROXY_FLAGS_NONE,
                                    "com.intel.dleyna-renderer",
                                    object_path,
                                    NULL, /* GCancellable */
                                    photos_dlna_renderers_manager_renderer_new_cb,
                                    self);
}


static void
photos_dlna_renderers_manager_renderer_lost_cb (PhotosDlnaRenderersManager *self,
                                                const gchar                *object_path,
                                                gpointer                   *data)
{
  PhotosDlnaRenderer *renderer;

  renderer = PHOTOS_DLNA_RENDERER (g_hash_table_lookup (self->renderers, object_path));
  g_return_if_fail (renderer != NULL);

  g_hash_table_steal (self->renderers, object_path);
  photos_debug (PHOTOS_DEBUG_DLNA,
                "%s '%s' %s %s", G_STRFUNC,
                photos_dlna_renderer_get_friendly_name (renderer),
                photos_dlna_renderer_get_udn (renderer),
                object_path);
  g_signal_emit (self, signals[RENDERER_LOST], 0, renderer);
  g_object_unref (renderer);
}


static void
photos_dlna_renderers_manager_proxy_get_renderers_cb (GObject      *source_object,
                                                      GAsyncResult *res,
                                                      gpointer      user_data)
{
  PhotosDlnaRenderersManager *self = user_data;
  gchar **object_paths, **path;
  GError *error = NULL;

  dleyna_renderer_manager_call_get_renderers_finish (self->proxy, &object_paths, res, &error);
  if (error != NULL)
    {
      g_warning ("Unable to fetch the list of available renderers: %s", error->message);
      g_propagate_error (&self->error, error);
      return;
    }

  for (path = object_paths; *path != NULL; path++)
    photos_dlna_renderers_manager_renderer_found_cb (self, *path, NULL);

  g_strfreev (object_paths);
}


static void
photos_dlna_renderers_manager_proxy_new_cb (GObject      *source_object,
                                            GAsyncResult *res,
                                            gpointer      user_data)
{
  PhotosDlnaRenderersManager *self = user_data;
  GError *error = NULL;

  self->proxy = dleyna_renderer_manager_proxy_new_for_bus_finish (res, &error);
  if (error != NULL)
    {
      g_warning ("Unable to connect to the dLeynaRenderer.Manager DBus object: %s", error->message);
      g_propagate_error (&self->error, error);
      return;
    }

  photos_debug (PHOTOS_DEBUG_DLNA, "%s DLNA renderers manager initialized", G_STRFUNC);

  g_signal_connect_swapped (self->proxy, "found-renderer",
                            G_CALLBACK (photos_dlna_renderers_manager_renderer_found_cb), self);
  g_signal_connect_swapped (self->proxy, "lost-renderer",
                            G_CALLBACK (photos_dlna_renderers_manager_renderer_lost_cb), self);

  dleyna_renderer_manager_call_get_renderers (self->proxy, NULL,
                                              photos_dlna_renderers_manager_proxy_get_renderers_cb, self);
}


static GObject *
photos_dlna_renderers_manager_constructor (GType                  type,
                                           guint                  n_construct_params,
                                           GObjectConstructParam *construct_params)
{
  if (photos_dlna_renderers_manager_singleton == NULL)
    {
      photos_dlna_renderers_manager_singleton
        = G_OBJECT_CLASS (photos_dlna_renderers_manager_parent_class)->constructor (type,
                                                                                    n_construct_params,
                                                                                    construct_params);
      g_object_add_weak_pointer (photos_dlna_renderers_manager_singleton,
                                 (gpointer) &photos_dlna_renderers_manager_singleton);
      return photos_dlna_renderers_manager_singleton;
    }

  return g_object_ref (photos_dlna_renderers_manager_singleton);
}


static void
photos_dlna_renderers_manager_init (PhotosDlnaRenderersManager *self)
{
  dleyna_renderer_manager_proxy_new_for_bus (G_BUS_TYPE_SESSION,
                                             G_DBUS_PROXY_FLAGS_NONE,
                                             "com.intel.dleyna-renderer",
                                             "/com/intel/dLeynaRenderer",
                                             NULL, /* GCancellable */
                                             photos_dlna_renderers_manager_proxy_new_cb,
                                             self);
  self->renderers = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_object_unref);
}


static void
photos_dlna_renderers_manager_class_init (PhotosDlnaRenderersManagerClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->constructor = photos_dlna_renderers_manager_constructor;
  object_class->dispose = photos_dlna_renderers_manager_dispose;

  signals[RENDERER_FOUND] = g_signal_new ("renderer-found", G_TYPE_FROM_CLASS (class),
                                          G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                                          g_cclosure_marshal_VOID__OBJECT,
                                          G_TYPE_NONE, 1, PHOTOS_TYPE_DLNA_RENDERER);

  signals[RENDERER_LOST] = g_signal_new ("renderer-lost", G_TYPE_FROM_CLASS (class),
                                          G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                                          g_cclosure_marshal_VOID__OBJECT,
                                          G_TYPE_NONE, 1, PHOTOS_TYPE_DLNA_RENDERER);
}


PhotosDlnaRenderersManager *
photos_dlna_renderers_manager_dup_singleton (void)
{
  return g_object_new (PHOTOS_TYPE_DLNA_RENDERERS_MANAGER, NULL);
}


GList *
photos_dlna_renderers_manager_dup_renderers (PhotosDlnaRenderersManager *self)
{
  GList *renderers;

  renderers = g_hash_table_get_values (self->renderers);
  g_list_foreach (renderers, (GFunc) g_object_ref, NULL);

  return renderers;
}


gboolean
photos_dlna_renderers_manager_is_available (void)
{
  PhotosDlnaRenderersManager *self;

  if (photos_dlna_renderers_manager_singleton == NULL)
    return FALSE;

  self = PHOTOS_DLNA_RENDERERS_MANAGER (photos_dlna_renderers_manager_singleton);

  return self->error == NULL;
}
