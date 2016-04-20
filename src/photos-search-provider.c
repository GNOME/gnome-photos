/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2014 – 2016 Red Hat, Inc.
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

#include "photos-fetch-ids-job.h"
#include "photos-fetch-metas-job.h"
#include "photos-search-context.h"
#include "photos-search-provider.h"
#include "photos-shell-search-provider2.h"


struct _PhotosSearchProvider
{
  GObject parent_instance;
  GCancellable *cancellable;
  GHashTable *cache;
  PhotosSearchContextState *state;
  ShellSearchProvider2 *skeleton;
};

struct _PhotosSearchProviderClass
{
  GObjectClass parent_class;
};

enum
{
  ACTIVATE_RESULT,
  LAUNCH_SEARCH,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static void photos_search_provider_search_context_iface_init (PhotosSearchContextInterface *iface);


G_DEFINE_TYPE_WITH_CODE (PhotosSearchProvider, photos_search_provider, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (PHOTOS_TYPE_SEARCH_CONTEXT,
                                                photos_search_provider_search_context_iface_init));


static gboolean
photos_search_provider_activate_result (PhotosSearchProvider *self,
                                        GDBusMethodInvocation *invocation,
                                        const gchar *identifier,
                                        const gchar *const *terms,
                                        guint timestamp)
{
  g_signal_emit (self, signals[ACTIVATE_RESULT], 0, identifier, terms, timestamp);
  shell_search_provider2_complete_activate_result (self->skeleton, invocation);
  return TRUE;
}


static void
photos_search_provider_fetch_ids_executed (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  GApplication *app;
  GDBusMethodInvocation *invocation = G_DBUS_METHOD_INVOCATION (user_data);
  PhotosFetchIdsJob *job = PHOTOS_FETCH_IDS_JOB (source_object);
  GError *error = NULL;
  GVariant *parameters;
  const gchar *const *ids;

  app = g_application_get_default ();
  g_application_release (app);

  ids = photos_fetch_ids_job_finish (job, res, &error);
  if (error != NULL)
    {
      g_dbus_method_invocation_take_error (invocation, error);
      goto out;
    }

  parameters = g_variant_new ("(^as)", ids);
  g_dbus_method_invocation_return_value (invocation, parameters);

 out:
  g_object_unref (invocation);
}


static PhotosSearchContextState *
photos_search_provider_get_state (PhotosSearchContext *context)
{
  PhotosSearchProvider *self = PHOTOS_SEARCH_PROVIDER (context);
  return self->state;
}


static void
photos_search_provider_return_metas_from_cache (PhotosSearchProvider *self,
                                                const gchar *const *identifiers,
                                                GDBusMethodInvocation *invocation)
{
  GApplication *app;
  GVariantBuilder builder;
  guint i;

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("aa{sv}"));

  for (i = 0; identifiers[i] != NULL; i++)
    {
      PhotosFetchMeta *meta;
      const gchar *id = identifiers[i];

      meta = (PhotosFetchMeta *) g_hash_table_lookup (self->cache, id);
      if (meta == NULL)
        continue;

      g_variant_builder_open (&builder, G_VARIANT_TYPE ("a{sv}"));
      g_variant_builder_add (&builder, "{sv}", "id", g_variant_new_string (meta->id));
      g_variant_builder_add (&builder, "{sv}", "name", g_variant_new_string (meta->title));
      if (meta->icon != NULL)
        g_variant_builder_add (&builder, "{sv}", "icon", g_icon_serialize (meta->icon));

      g_variant_builder_close (&builder);
    }

  app = g_application_get_default ();
  g_application_release (app);

  shell_search_provider2_complete_get_result_metas (self->skeleton, invocation, g_variant_builder_end (&builder));
}


static void
photos_search_provider_fetch_metas_executed (GList *metas, gpointer user_data)
{
  PhotosSearchProvider *self;
  GDBusMethodInvocation *invocation = G_DBUS_METHOD_INVOCATION (user_data);
  GList *l;
  const gchar *const *identifiers;

  self = PHOTOS_SEARCH_PROVIDER (g_object_get_data (G_OBJECT (invocation), "self"));
  identifiers = (const gchar *const *) g_object_get_data (G_OBJECT (invocation), "identifiers");

  for (l = metas; l != NULL; l = l->next)
    {
      PhotosFetchMeta *copy;
      PhotosFetchMeta *meta = (PhotosFetchMeta *) l->data;

      copy = photos_fetch_meta_copy (meta);
      g_hash_table_insert (self->cache, copy->id, copy);
    }

  photos_search_provider_return_metas_from_cache (self, identifiers, invocation);
}


static gboolean
photos_search_provider_get_initial_result_set (PhotosSearchProvider *self,
                                               GDBusMethodInvocation *invocation,
                                               const gchar *const *terms)
{
  GApplication *app;
  PhotosFetchIdsJob *job;

  app = g_application_get_default ();
  g_application_hold (app);

  g_cancellable_cancel (self->cancellable);
  g_cancellable_reset (self->cancellable);

  job = photos_fetch_ids_job_new (terms);
  photos_fetch_ids_job_run (job,
                            self->state,
                            self->cancellable,
                            photos_search_provider_fetch_ids_executed,
                            g_object_ref (invocation));
  g_object_unref (job);

  return TRUE;
}


static gboolean
photos_search_provider_get_result_metas (PhotosSearchProvider *self,
                                         GDBusMethodInvocation *invocation,
                                         const gchar *const *identifiers)
{
  GApplication *app;
  GPtrArray *to_fetch_arr;
  const gchar *const *to_fetch;
  guint i;
  guint n_identifiers;
  guint n_to_fetch;

  app = g_application_get_default ();
  g_application_hold (app);

  n_identifiers = g_strv_length ((gchar **) identifiers);
  to_fetch_arr = g_ptr_array_sized_new (n_identifiers + 1);

  for (i = 0; identifiers[i] != NULL; i++)
    {
      if (g_hash_table_lookup (self->cache, identifiers[i]) == NULL)
        g_ptr_array_add (to_fetch_arr, (gpointer) identifiers[i]);
    }

  g_ptr_array_add (to_fetch_arr, NULL);
  to_fetch = (const gchar * const *) to_fetch_arr->pdata;

  n_to_fetch = g_strv_length ((gchar **) to_fetch);
  if (n_to_fetch > 0)
    {
      PhotosFetchMetasJob *job;

      job = photos_fetch_metas_job_new (to_fetch);
      g_object_set_data_full (G_OBJECT (invocation),
                              "identifiers",
                              g_boxed_copy (G_TYPE_STRV, identifiers),
                              (GDestroyNotify) g_strfreev);
      g_object_set_data_full (G_OBJECT (invocation), "self", g_object_ref (self), g_object_unref);
      photos_fetch_metas_job_run (job,
                                  self->state,
                                  photos_search_provider_fetch_metas_executed,
                                  g_object_ref (invocation));
      g_object_unref (job);
    }
  else
    photos_search_provider_return_metas_from_cache (self, identifiers, invocation);

  g_ptr_array_unref (to_fetch_arr);

  return TRUE;
}


static gboolean
photos_search_provider_get_subsearch_result_set (PhotosSearchProvider *self,
                                                 GDBusMethodInvocation *invocation,
                                                 const gchar *const *previous_results,
                                                 const gchar *const *terms)
{
  GApplication *app;
  PhotosFetchIdsJob *job;

  app = g_application_get_default ();
  g_application_hold (app);

  g_cancellable_cancel (self->cancellable);
  g_cancellable_reset (self->cancellable);

  job = photos_fetch_ids_job_new (terms);
  photos_fetch_ids_job_run (job,
                            self->state,
                            self->cancellable,
                            photos_search_provider_fetch_ids_executed,
                            g_object_ref (invocation));
  g_object_unref (job);

  return TRUE;
}


static gboolean
photos_search_provider_launch_search (PhotosSearchProvider *self,
                                      GDBusMethodInvocation *invocation,
                                      const gchar *const *terms,
                                      guint timestamp)
{
  g_signal_emit (self, signals[LAUNCH_SEARCH], 0, terms, timestamp);
  shell_search_provider2_complete_launch_search (self->skeleton, invocation);
  return TRUE;
}


static void
photos_search_provider_dispose (GObject *object)
{
  PhotosSearchProvider *self = PHOTOS_SEARCH_PROVIDER (object);

  g_clear_object (&self->cancellable);
  g_clear_object (&self->skeleton);

  if (self->state != NULL)
    {
      photos_search_context_state_free (self->state);
      self->state = NULL;
    }

  G_OBJECT_CLASS (photos_search_provider_parent_class)->dispose (object);
}


static void
photos_search_provider_finalize (GObject *object)
{
  PhotosSearchProvider *self = PHOTOS_SEARCH_PROVIDER (object);

  g_hash_table_unref (self->cache);

  G_OBJECT_CLASS (photos_search_provider_parent_class)->finalize (object);
}


static void
photos_search_provider_init (PhotosSearchProvider *self)
{
  self->cancellable = g_cancellable_new ();
  self->cache = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, (GDestroyNotify) photos_fetch_meta_free);
  self->state = photos_search_context_state_new (PHOTOS_SEARCH_CONTEXT (self));
  self->skeleton = shell_search_provider2_skeleton_new ();

  g_signal_connect_swapped (self->skeleton,
                            "handle-activate-result",
                            G_CALLBACK (photos_search_provider_activate_result),
                            self);
  g_signal_connect_swapped (self->skeleton,
                            "handle-get-initial-result-set",
                            G_CALLBACK (photos_search_provider_get_initial_result_set),
                            self);
  g_signal_connect_swapped (self->skeleton,
                            "handle-get-subsearch-result-set",
                            G_CALLBACK (photos_search_provider_get_subsearch_result_set),
                            self);
  g_signal_connect_swapped (self->skeleton,
                            "handle-get-result-metas",
                            G_CALLBACK (photos_search_provider_get_result_metas),
                            self);
  g_signal_connect_swapped (self->skeleton,
                            "handle-launch-search",
                            G_CALLBACK (photos_search_provider_launch_search),
                            self);
}


static void
photos_search_provider_class_init (PhotosSearchProviderClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->dispose = photos_search_provider_dispose;
  object_class->finalize = photos_search_provider_finalize;

  signals[ACTIVATE_RESULT] = g_signal_new ("activate-result",
                                           G_TYPE_FROM_CLASS (class),
                                           G_SIGNAL_RUN_LAST,
                                           0,    /* class_offset */
                                           NULL, /* accumulator */
                                           NULL, /* accu_data */
                                           g_cclosure_marshal_generic,
                                           G_TYPE_NONE,
                                           3,
                                           G_TYPE_STRING,
                                           G_TYPE_STRV,
                                           G_TYPE_UINT);

  signals[LAUNCH_SEARCH] = g_signal_new ("launch-search",
                                         G_TYPE_FROM_CLASS (class),
                                         G_SIGNAL_RUN_LAST,
                                         0,    /* class_offset */
                                         NULL, /* accumulator */
                                         NULL, /* accu_data */
                                         g_cclosure_marshal_generic,
                                         G_TYPE_NONE,
                                         2,
                                         G_TYPE_STRV,
                                         G_TYPE_UINT);
}


static void
photos_search_provider_search_context_iface_init (PhotosSearchContextInterface *iface)
{
  iface->get_state = photos_search_provider_get_state;
}


PhotosSearchProvider *
photos_search_provider_new (void)
{
  return g_object_new (PHOTOS_TYPE_SEARCH_PROVIDER, NULL);
}


gboolean
photos_search_provider_dbus_export (PhotosSearchProvider *self,
                                    GDBusConnection *connection,
                                    const gchar *object_path,
                                    GError **error)
{
  return g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (self->skeleton),
                                           connection,
                                           object_path,
                                           error);
}


void
photos_search_provider_dbus_unexport (PhotosSearchProvider *self,
                                      GDBusConnection *connection,
                                      const gchar *object_path)
{
  if (g_dbus_interface_skeleton_has_connection (G_DBUS_INTERFACE_SKELETON (self->skeleton), connection))
    g_dbus_interface_skeleton_unexport_from_connection (G_DBUS_INTERFACE_SKELETON (self->skeleton), connection);
}
