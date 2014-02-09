/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2014 Red Hat, Inc.
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

#include <string.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <tracker-sparql.h>

#include "photos-create-collection-icon-job.h"
#include "photos-fetch-metas-job.h"
#include "photos-query.h"
#include "photos-single-item-job.h"
#include "photos-utils.h"


struct _PhotosFetchMetasJobPrivate
{
  GList *metas;
  PhotosFetchMetasJobCallback callback;
  gchar **ids;
  gpointer user_data;
  guint active_jobs;
};

enum
{
  PROP_0,
  PROP_IDS
};


G_DEFINE_TYPE_WITH_PRIVATE (PhotosFetchMetasJob, photos_fetch_metas_job, G_TYPE_OBJECT);


static PhotosFetchMeta *
photos_fetch_meta_new (GIcon *icon, const gchar *id, const gchar *title)
{
  PhotosFetchMeta *meta;

  meta = g_slice_new0 (PhotosFetchMeta);

  if (icon != NULL)
    meta->icon = g_object_ref (icon);

  meta->id = g_strdup (id);
  meta->title = g_strdup (title);
  return meta;
}


static void
photos_fetch_metas_job_emit_callback (PhotosFetchMetasJob *self)
{
  PhotosFetchMetasJobPrivate *priv = self->priv;

  if (priv->callback == NULL)
    return;

  (*priv->callback) (priv->metas, priv->user_data);
}


static void
photos_fetch_metas_job_collector (PhotosFetchMetasJob *self)
{
  PhotosFetchMetasJobPrivate *priv = self->priv;

  priv->active_jobs--;
  if (priv->active_jobs == 0)
    photos_fetch_metas_job_emit_callback (self);
}


static void
photos_fetch_metas_job_create_collection_icon_executed (GIcon *icon, gpointer user_data)
{
  GTask *task = G_TASK (user_data);
  PhotosFetchMetasJob *self;
  PhotosFetchMetasJobPrivate *priv;
  PhotosFetchMeta *meta;

  self = PHOTOS_FETCH_METAS_JOB (g_task_get_source_object (task));
  priv = self->priv;

  meta = (PhotosFetchMeta *) g_task_get_task_data (task);

  if (icon != NULL)
    {
      g_clear_object (&meta->icon);
      meta->icon = g_object_ref (icon);
    }

  priv->metas = g_list_prepend (priv->metas, meta);
  photos_fetch_metas_job_collector (self);
  g_object_unref (task);
}


static void
photos_fetch_metas_job_create_collection_pixbuf (PhotosFetchMetasJob *self, PhotosFetchMeta *meta)
{
  GTask *task;
  PhotosCreateCollectionIconJob *job;

  job = photos_create_collection_icon_job_new (meta->id);
  task = g_task_new (self, NULL, NULL, NULL);
  g_task_set_task_data (task, meta, NULL);
  photos_create_collection_icon_job_run (job,
                                         photos_fetch_metas_job_create_collection_icon_executed,
                                         g_object_ref (task));
  g_object_unref (task);
}


static void
photos_fetch_metas_job_executed (TrackerSparqlCursor *cursor, gpointer user_data)
{
  PhotosFetchMetasJob *self = PHOTOS_FETCH_METAS_JOB (user_data);
  PhotosFetchMetasJobPrivate *priv = self->priv;
  GIcon *icon = NULL;
  PhotosFetchMeta *meta;
  gboolean is_collection;
  const gchar *id;
  const gchar *rdf_type;
  const gchar *title;

  id = tracker_sparql_cursor_get_string (cursor, PHOTOS_QUERY_COLUMNS_URN, NULL);
  title = tracker_sparql_cursor_get_string (cursor, PHOTOS_QUERY_COLUMNS_TITLE, NULL);
  rdf_type = tracker_sparql_cursor_get_string (cursor, PHOTOS_QUERY_COLUMNS_RDF_TYPE, NULL);

  is_collection = (strstr (rdf_type, "nfo#DataContainer") != NULL);

  if (!is_collection)
    icon = photos_utils_get_icon_from_cursor (cursor);

  if (title == NULL || title[0] == '\0')
    title = _("Untitled Photo");

  meta = photos_fetch_meta_new (icon, id, title);

  if (is_collection)
    photos_fetch_metas_job_create_collection_pixbuf (self, meta);
  else
    {
      priv->metas = g_list_prepend (priv->metas, meta);
      photos_fetch_metas_job_collector (self);
    }

  g_clear_object (&icon);
  g_object_unref (self);
}


static void
photos_fetch_metas_job_finalize (GObject *object)
{
  PhotosFetchMetasJob *self = PHOTOS_FETCH_METAS_JOB (object);
  PhotosFetchMetasJobPrivate *priv = self->priv;

  g_list_free_full (priv->metas, (GDestroyNotify) photos_fetch_meta_free);
  g_strfreev (self->priv->ids);

  G_OBJECT_CLASS (photos_fetch_metas_job_parent_class)->finalize (object);
}


static void
photos_fetch_metas_job_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
  PhotosFetchMetasJob *self = PHOTOS_FETCH_METAS_JOB (object);

  switch (prop_id)
    {
    case PROP_IDS:
      self->priv->ids = g_value_dup_boxed (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
photos_fetch_metas_job_init (PhotosFetchMetasJob *self)
{
  self->priv = photos_fetch_metas_job_get_instance_private (self);
}


static void
photos_fetch_metas_job_class_init (PhotosFetchMetasJobClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->finalize = photos_fetch_metas_job_finalize;
  object_class->set_property = photos_fetch_metas_job_set_property;

  g_object_class_install_property (object_class,
                                   PROP_IDS,
                                   g_param_spec_boxed ("ids",
                                                       "Identifiers",
                                                       "Identifiers of items whose metadata is needed",
                                                       G_TYPE_STRV,
                                                       G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE));
}


PhotosFetchMeta *
photos_fetch_meta_copy (PhotosFetchMeta *meta)
{
  PhotosFetchMeta *copy;

  copy = g_slice_new0 (PhotosFetchMeta);

  if (meta->icon != NULL)
    copy->icon = g_object_ref (meta->icon);

  copy->id = g_strdup (meta->id);
  copy->title = g_strdup (meta->title);
  return copy;
}


void
photos_fetch_meta_free (PhotosFetchMeta *meta)
{
  g_clear_object (&meta->icon);
  g_free (meta->id);
  g_free (meta->title);
  g_slice_free (PhotosFetchMeta, meta);
}


PhotosFetchMetasJob *
photos_fetch_metas_job_new (const gchar *const *ids)
{
  return g_object_new (PHOTOS_TYPE_FETCH_METAS_JOB, "ids", ids, NULL);
}


void
photos_fetch_metas_job_run (PhotosFetchMetasJob *self,
                            PhotosSearchContextState *state,
                            PhotosFetchMetasJobCallback callback,
                            gpointer user_data)
{
  PhotosFetchMetasJobPrivate *priv = self->priv;
  guint i;

  priv->callback = callback;
  priv->user_data = user_data;
  priv->active_jobs = g_strv_length (priv->ids);

  for (i = 0; priv->ids[i] != NULL; i++)
    {
      PhotosSingleItemJob *job;
      const gchar *id = priv->ids[i];

      job = photos_single_item_job_new (id);
      photos_single_item_job_run (job,
                                  state,
                                  PHOTOS_QUERY_FLAGS_UNFILTERED,
                                  photos_fetch_metas_job_executed,
                                  g_object_ref (self));
      g_object_unref (job);
    }
}
