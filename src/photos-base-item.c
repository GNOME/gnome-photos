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

#include <string.h>

#include <gio/gio.h>
#include <glib.h>
#include <tracker-sparql.h>

#include "photos-base-item.h"
#include "photos-query.h"
#include "photos-utils.h"


struct _PhotosBaseItemPrivate
{
  GdkPixbuf *icon;
  GdkPixbuf *pristine_icon;
  TrackerSparqlCursor *cursor;
  gboolean collection;
  gboolean failed_thumbnailing;
  gboolean favorite;
  gboolean thumbnailed;
  gboolean tried_thumbnailing;
  const gchar *thumb_path;
  gchar *author;
  gchar *default_app_name;
  gchar *id;
  gchar *identifier;
  gchar *mime_type;
  gchar *name;
  gchar *rdf_type;
  gchar *resource_urn;
  gchar *type_description;
  gchar *uri;
  gint64 mtime;
};

enum
{
  PROP_0,
  PROP_CURSOR,
  PROP_FAILED_THUMBNAILING,
  PROP_ID,
  PROP_TRIED_THUMBNAILING
};

enum
{
  INFO_UPDATED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };


G_DEFINE_TYPE (PhotosBaseItem, photos_base_item, G_TYPE_OBJECT);


static GIcon *
photos_base_item_create_symbolic_emblem (const gchar *name)
{
  GIcon *pix;
  gint size;

  size = photos_utils_get_icon_size ();
  pix = photos_utils_create_symbolic_icon (name, size);
  if (pix == NULL)
    pix = g_themed_icon_new (name);

  return pix;
}


static void
photos_base_item_check_effects_and_update_info (PhotosBaseItem *self)
{
  PhotosBaseItemPrivate *priv = self->priv;
  GIcon *pix;
  GList *emblem_icons = NULL;
  GdkPixbuf *icon;

  icon = g_object_ref (priv->icon);
  priv->pristine_icon = g_object_ref (icon);

  if (priv->favorite)
    {
      pix = photos_base_item_create_symbolic_emblem ("emblem-favorite");
      emblem_icons = g_list_prepend (emblem_icons, pix);
    }

  if (g_list_length (emblem_icons) > 0)
    {
      GIcon *emblemed_icon;
      GList *l;
      GtkIconInfo *icon_info;
      GtkIconTheme *theme;
      gint height;
      gint size;
      gint width;

      emblem_icons = g_list_reverse (emblem_icons);
      emblemed_icon = g_emblemed_icon_new (G_ICON (priv->icon), NULL);
      for (l = emblem_icons; l != NULL; l = g_list_next (l))
        {
          GEmblem *emblem;
          GIcon *emblem_icon = G_ICON (l->data);

          emblem = g_emblem_new (emblem_icon);
          g_emblemed_icon_add_emblem (G_EMBLEMED_ICON (emblemed_icon), emblem);
          g_object_unref (emblem);
        }

      theme = gtk_icon_theme_get_default ();

      width = gdk_pixbuf_get_width (priv->icon);
      height = gdk_pixbuf_get_height (priv->icon);
      size = (width > height) ? width : height;

      icon_info = gtk_icon_theme_lookup_by_gicon (theme, emblemed_icon, size, GTK_ICON_LOOKUP_FORCE_SIZE);

      if (icon_info != NULL)
        {
          GError *error = NULL;
          GdkPixbuf *tmp;

          tmp = gtk_icon_info_load_icon (icon_info, &error);
          if (error != NULL)
            {
              g_warning ("Unable to render the emblem: %s", error->message);
              g_error_free (error);
            }
          else
            {
              g_object_unref (icon);
              icon = tmp;
            }

          gtk_icon_info_free (icon_info);
        }
    }

  g_object_unref (priv->icon);

  if (priv->thumbnailed)
    {
      GtkBorder *slice;

      slice = photos_utils_get_thumbnail_frame_border ();
      priv->icon = photos_utils_embed_image_in_frame (icon,
                                                      PACKAGE_ICONS_DIR "/thumbnail-frame.png",
                                                      slice,
                                                      slice);
      gtk_border_free (slice);
    }
  else
    priv->icon = g_object_ref (icon);

  g_signal_emit (self, signals[INFO_UPDATED], 0);

  g_object_unref (icon);
  g_list_free_full (emblem_icons, g_object_unref);
}


static GdkPixbuf *
photos_base_item_default_load (PhotosBaseItem *self, GCancellable *cancellable, GError **error)
{
  PhotosBaseItemPrivate *priv = self->priv;
  GdkPixbuf *pixbuf = NULL;
  GFile *file = NULL;
  GFileInputStream *stream = NULL;

  file = g_file_new_for_uri (priv->uri);
  stream = g_file_read (file, cancellable, error);
  if (stream == NULL)
    goto out;

  pixbuf = gdk_pixbuf_new_from_stream (G_INPUT_STREAM (stream), cancellable, error);

 out:
  g_clear_object (&stream);
  g_clear_object (&file);
  return pixbuf;
}


static void
photos_base_item_default_set_favorite (PhotosBaseItem *self, gboolean favorite)
{
  photos_utils_set_favorite (self->priv->id, favorite);
}


static void
photos_base_item_refresh_thumb_path_pixbuf (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosBaseItem *self = PHOTOS_BASE_ITEM (user_data);
  PhotosBaseItemPrivate *priv = self->priv;
  GError *error = NULL;
  GInputStream *stream = G_INPUT_STREAM (source_object);

  priv->icon = gdk_pixbuf_new_from_stream_finish (res, &error);
  if (error != NULL)
    {
      priv->failed_thumbnailing = TRUE;
      g_error_free (error);
      goto out;
    }

  priv->thumbnailed = TRUE;
  photos_base_item_check_effects_and_update_info (self);

 out:
  g_input_stream_close_async (stream, G_PRIORITY_DEFAULT, NULL, NULL, NULL);
  g_object_unref (self);
}


static void
photos_base_item_refresh_thumb_path_read (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosBaseItem *self = PHOTOS_BASE_ITEM (user_data);
  PhotosBaseItemPrivate *priv = self->priv;
  GError *error = NULL;
  GFile *file = G_FILE (source_object);
  GFileInputStream *stream;
  gint size;

  stream = g_file_read_finish (file, res, &error);
  if (error != NULL)
    {
      priv->failed_thumbnailing = TRUE;
      g_error_free (error);
      goto out;
    }

  size = photos_utils_get_icon_size ();
  gdk_pixbuf_new_from_stream_at_scale_async (G_INPUT_STREAM (stream),
                                             size,
                                             size,
                                             TRUE,
                                             NULL,
                                             photos_base_item_refresh_thumb_path_pixbuf,
                                             g_object_ref (self));
  g_object_unref (stream);

 out:
  g_object_unref (self);
}


static void
photos_base_item_refresh_thumb_path (PhotosBaseItem *self)
{
  PhotosBaseItemPrivate *priv = self->priv;
  GFile *thumb_file;

  thumb_file = g_file_new_for_path (priv->thumb_path);
  g_file_read_async (thumb_file,
                     G_PRIORITY_DEFAULT,
                     NULL,
                     photos_base_item_refresh_thumb_path_read,
                     g_object_ref (self));
  g_object_unref (thumb_file);
}


static void
photos_base_item_thumbnail_path_info (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosBaseItem *self = PHOTOS_BASE_ITEM (user_data);
  PhotosBaseItemPrivate *priv = self->priv;
  GError *error = NULL;
  GFile *file = G_FILE (source_object);
  GFileInfo *info;

  info = g_file_query_info_finish (file, res, &error);
  if (error != NULL)
    {
      g_warning ("Unable to query info for file at %s: %s", priv->uri, error->message);
      priv->failed_thumbnailing = TRUE;
      g_error_free (error);
      goto out;
    }

  priv->thumb_path = g_file_info_get_attribute_byte_string (info, G_FILE_ATTRIBUTE_THUMBNAIL_PATH);
  if (priv->thumb_path != NULL)
    photos_base_item_refresh_thumb_path (self);
  else
    priv->failed_thumbnailing = TRUE;

 out:
  g_object_unref (self);
}


static void
photos_base_item_queue_thumbnail_job (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosBaseItem *self = PHOTOS_BASE_ITEM (user_data);
  PhotosBaseItemPrivate *priv = self->priv;
  GFile *file = G_FILE (source_object);
  gboolean thumbnailed;

  thumbnailed = photos_utils_queue_thumbnail_job_for_file_finish (res);
  if (!thumbnailed)
    {
      priv->failed_thumbnailing = TRUE;
      goto out;
    }

  g_file_query_info_async (file,
                           G_FILE_ATTRIBUTE_THUMBNAIL_PATH,
                           G_FILE_QUERY_INFO_NONE,
                           G_PRIORITY_DEFAULT,
                           NULL,
                           photos_base_item_thumbnail_path_info,
                           g_object_ref (self));

 out:
  g_object_unref (self);
}


static void
photos_base_item_file_query_info (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosBaseItem *self = PHOTOS_BASE_ITEM (user_data);
  PhotosBaseItemPrivate *priv = self->priv;
  GError *error = NULL;
  GFile *file = G_FILE (source_object);
  GFileInfo *info;
  gboolean have_new_icon = FALSE;

  info = g_file_query_info_finish (file, res, &error);
  if (error != NULL)
    {
      g_warning ("Unable to query info for file at %s: %s", priv->uri, error->message);
      priv->failed_thumbnailing = TRUE;
      g_error_free (error);
      goto out;
    }

  priv->thumb_path = g_file_info_get_attribute_byte_string (info, G_FILE_ATTRIBUTE_THUMBNAIL_PATH);
  if (priv->thumb_path != NULL)
    photos_base_item_refresh_thumb_path (self);
  else
    {
      priv->thumbnailed = FALSE;
      photos_utils_queue_thumbnail_job_for_file_async (file,
                                                       photos_base_item_queue_thumbnail_job,
                                                       g_object_ref (self));
    }

 out:
  g_object_unref (self);
}


static void
photos_base_item_load_in_thread_func (GSimpleAsyncResult *simple, GObject *object, GCancellable *cancellable)
{
  PhotosBaseItem *self = PHOTOS_BASE_ITEM (object);
  GdkPixbuf *pixbuf;
  GError *error = NULL;

  pixbuf = PHOTOS_BASE_ITEM_GET_CLASS (self)->load (self, cancellable, &error);
  if (error != NULL)
    g_simple_async_result_take_error (simple, error);

  g_simple_async_result_set_op_res_gpointer (simple, (gpointer) pixbuf, g_object_unref);
}


static void
photos_base_item_update_icon_from_type (PhotosBaseItem *self)
{
  PhotosBaseItemPrivate *priv = self->priv;
  GIcon *icon = NULL;
  GtkIconInfo *info;
  GtkIconTheme *theme;

  if (priv->mime_type != NULL)
    icon = g_content_type_get_icon (priv->mime_type);

  /* TODO: Get icon from RDF type */

  theme = gtk_icon_theme_get_default ();
  info = gtk_icon_theme_lookup_by_gicon (theme,
                                         icon,
                                         128,
                                         GTK_ICON_LOOKUP_FORCE_SIZE | GTK_ICON_LOOKUP_GENERIC_FALLBACK);
  if (info != NULL)
    {
      priv->icon = gtk_icon_info_load_icon (info, NULL);
      /* TODO: use a GError */
    }

  photos_base_item_check_effects_and_update_info (self);
}


static void
photos_base_item_refresh_icon (PhotosBaseItem *self)
{
  PhotosBaseItemPrivate *priv = self->priv;
  GFile *file;

  if (priv->thumb_path != NULL)
    {
      photos_base_item_refresh_thumb_path (self);
      return;
    }

  photos_base_item_update_icon_from_type (self);

  if (priv->failed_thumbnailing)
    return;

  if (!priv->tried_thumbnailing)
    priv->tried_thumbnailing = TRUE;

  file = g_file_new_for_uri (priv->uri);
  g_file_query_info_async (file,
                           G_FILE_ATTRIBUTE_THUMBNAIL_PATH,
                           G_FILE_QUERY_INFO_NONE,
                           G_PRIORITY_DEFAULT,
                           NULL,
                           photos_base_item_file_query_info,
                           g_object_ref (self));
  g_object_unref (file);
}


static void
photos_base_item_update_info_from_type (PhotosBaseItem *self)
{
  PhotosBaseItemPrivate *priv = self->priv;

  if (strstr (priv->rdf_type, "nfo#DataContainer") != NULL)
    priv->collection = TRUE;

  PHOTOS_BASE_ITEM_GET_CLASS (self)->update_type_description (self);
}


static void
photos_base_item_populate_from_cursor (PhotosBaseItem *self, TrackerSparqlCursor *cursor)
{
  PhotosBaseItemPrivate *priv = self->priv;
  GTimeVal timeval;
  const gchar *mtime;
  const gchar *title;
  const gchar *uri;

  uri = tracker_sparql_cursor_get_string (cursor, PHOTOS_QUERY_COLUMNS_URI, NULL);
  priv->uri = g_strdup ((uri == NULL) ? "" : uri);

  priv->id = g_strdup (tracker_sparql_cursor_get_string (cursor, PHOTOS_QUERY_COLUMNS_URN, NULL));
  priv->identifier = g_strdup (tracker_sparql_cursor_get_string (cursor, PHOTOS_QUERY_COLUMNS_IDENTIFIER, NULL));
  priv->author = g_strdup (tracker_sparql_cursor_get_string (cursor, PHOTOS_QUERY_COLUMNS_AUTHOR, NULL));
  priv->resource_urn = g_strdup (tracker_sparql_cursor_get_string (cursor, PHOTOS_QUERY_COLUMNS_RESOURCE_URN, NULL));
  priv->favorite = tracker_sparql_cursor_get_boolean (cursor, PHOTOS_QUERY_COLUMNS_RESOURCE_FAVORITE);

  mtime = tracker_sparql_cursor_get_string (cursor, PHOTOS_QUERY_COLUMNS_MTIME, NULL);
  if (mtime != NULL)
    {
      g_time_val_from_iso8601 (mtime, &timeval);
      priv->mtime = (gint64) timeval.tv_sec;
    }
  else
    priv->mtime = g_get_real_time () / 1000000;

  priv->mime_type = g_strdup (tracker_sparql_cursor_get_string (cursor, PHOTOS_QUERY_COLUMNS_MIME_TYPE, NULL));
  priv->rdf_type = g_strdup (tracker_sparql_cursor_get_string (cursor, PHOTOS_QUERY_COLUMNS_RDF_TYPE, NULL));
  photos_base_item_update_info_from_type (self);

  title = tracker_sparql_cursor_get_string (cursor, PHOTOS_QUERY_COLUMNS_TITLE, NULL);
  if (title == NULL || title[0] == '\0')
    {
      const gchar *filename;

      filename = tracker_sparql_cursor_get_string (cursor, PHOTOS_QUERY_COLUMNS_FILENAME, NULL);
      if (filename != NULL)
        title = photos_utils_filename_strip_extension (filename);
      else
        title = "";
    }
  priv->name = g_strdup (title);

  photos_base_item_refresh_icon (self);
}


static void
photos_base_item_update_type_description (PhotosBaseItem *self)
{
  PhotosBaseItemPrivate *priv = self->priv;

  if (priv->mime_type == NULL)
    return;

  priv->type_description = g_content_type_get_description (priv->mime_type);
}


static void
photos_base_item_constructed (GObject *object)
{
  PhotosBaseItem *self = PHOTOS_BASE_ITEM (object);
  PhotosBaseItemPrivate *priv = self->priv;

  G_OBJECT_CLASS (photos_base_item_parent_class)->constructed (object);

  photos_base_item_populate_from_cursor (self, priv->cursor);
  g_clear_object (&priv->cursor); /* We will not need it any more */
}


static void
photos_base_item_dispose (GObject *object)
{
  PhotosBaseItem *self = PHOTOS_BASE_ITEM (object);
  PhotosBaseItemPrivate *priv = self->priv;

  g_clear_object (&priv->icon);
  g_clear_object (&priv->pristine_icon);
  g_clear_object (&priv->cursor);

  G_OBJECT_CLASS (photos_base_item_parent_class)->dispose (object);
}


static void
photos_base_item_finalize (GObject *object)
{
  PhotosBaseItem *self = PHOTOS_BASE_ITEM (object);
  PhotosBaseItemPrivate *priv = self->priv;

  g_free (priv->author);
  g_free (priv->default_app_name);
  g_free (priv->id);
  g_free (priv->identifier);
  g_free (priv->mime_type);
  g_free (priv->name);
  g_free (priv->rdf_type);
  g_free (priv->resource_urn);
  g_free (priv->type_description);
  g_free (priv->uri);

  G_OBJECT_CLASS (photos_base_item_parent_class)->finalize (object);
}


static void
photos_base_item_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
  PhotosBaseItem *self = PHOTOS_BASE_ITEM (object);

  switch (prop_id)
    {
    case PROP_ID:
      g_value_set_string (value, self->priv->id);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
photos_base_item_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
  PhotosBaseItem *self = PHOTOS_BASE_ITEM (object);
  PhotosBaseItemPrivate *priv = self->priv;

  switch (prop_id)
    {
    case PROP_CURSOR:
      priv->cursor = TRACKER_SPARQL_CURSOR (g_value_dup_object (value));
      break;

    case PROP_FAILED_THUMBNAILING:
      priv->failed_thumbnailing = g_value_get_boolean (value);
      break;

    case PROP_TRIED_THUMBNAILING:
      priv->tried_thumbnailing = g_value_get_boolean (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
photos_base_item_init (PhotosBaseItem *self)
{
  PhotosBaseItemPrivate *priv;

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, PHOTOS_TYPE_BASE_ITEM, PhotosBaseItemPrivate);
  priv = self->priv;
}


static void
photos_base_item_class_init (PhotosBaseItemClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->constructed= photos_base_item_constructed;
  object_class->dispose = photos_base_item_dispose;
  object_class->finalize = photos_base_item_finalize;
  object_class->get_property = photos_base_item_get_property;
  object_class->set_property = photos_base_item_set_property;
  class->load = photos_base_item_default_load;
  class->set_favorite = photos_base_item_default_set_favorite;
  class->update_type_description = photos_base_item_update_type_description;

  g_object_class_install_property (object_class,
                                   PROP_CURSOR,
                                   g_param_spec_object ("cursor",
                                                        "TrackerSparqlCursor object",
                                                        "A cursor to iterate over the results of a query",
                                                        TRACKER_SPARQL_TYPE_CURSOR,
                                                        G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE));

  g_object_class_install_property (object_class,
                                   PROP_FAILED_THUMBNAILING,
                                   g_param_spec_boolean ("failed-thumbnailing",
                                                         "Thumbnailing failed",
                                                         "Failed to create a thumbnail",
                                                         FALSE,
                                                         G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE));

  g_object_class_install_property (object_class,
                                   PROP_ID,
                                   g_param_spec_string ("id",
                                                        "Uniform Resource Name",
                                                        "An unique ID associated with this item",
                                                        "",
                                                        G_PARAM_READABLE));

  g_object_class_install_property (object_class,
                                   PROP_TRIED_THUMBNAILING,
                                   g_param_spec_boolean ("tried-thumbnailing",
                                                         "Thumbnailing attempted",
                                                         "An attempt was made to create a thumbnail",
                                                         FALSE,
                                                         G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE));

  signals[INFO_UPDATED] = g_signal_new ("info-updated",
                                        G_TYPE_FROM_CLASS (class),
                                        G_SIGNAL_RUN_LAST,
                                        G_STRUCT_OFFSET (PhotosBaseItemClass,
                                                         info_updated),
                                        NULL, /* accumulator */
                                        NULL, /* accu_data */
                                        g_cclosure_marshal_VOID__VOID,
                                        G_TYPE_NONE,
                                        0);

  g_type_class_add_private (class, sizeof (PhotosBaseItemPrivate));
}


gboolean
photos_base_item_can_trash (PhotosBaseItem *self)
{
  return self->priv->collection;
}


const gchar *
photos_base_item_get_author (PhotosBaseItem *self)
{
  return self->priv->author;
}


const gchar *
photos_base_item_get_default_app_name (PhotosBaseItem *self)
{
  return self->priv->default_app_name;
}


GdkPixbuf *
photos_base_item_get_icon (PhotosBaseItem *self)
{
  return self->priv->icon;
}


const gchar *
photos_base_item_get_id (PhotosBaseItem *self)
{
  return self->priv->id;
}


const gchar *
photos_base_item_get_mime_type (PhotosBaseItem *self)
{
  return self->priv->mime_type;
}


gint64
photos_base_item_get_mtime (PhotosBaseItem *self)
{
  return self->priv->mtime;
}


const gchar *
photos_base_item_get_name (PhotosBaseItem *self)
{
  return self->priv->name;
}


const gchar *
photos_base_item_get_uri (PhotosBaseItem *self)
{
  return self->priv->uri;
}


gchar *
photos_base_item_get_where (PhotosBaseItem *self)
{
  PhotosBaseItemPrivate *priv = self->priv;
  gchar *ret_val;

  if (priv->collection)
    ret_val = g_strconcat ("{ ?urn nie:isPartOf <", priv->id, "> }", NULL);
  else
    ret_val = g_strdup ("");

  return ret_val;
}


gboolean
photos_base_item_is_collection (PhotosBaseItem *self)
{
  return self->priv->collection;
}


gboolean
photos_base_item_is_favorite (PhotosBaseItem *self)
{
  return self->priv->favorite;
}


void
photos_base_item_load_async (PhotosBaseItem *self,
                             GCancellable *cancellable,
                             GAsyncReadyCallback callback,
                             gpointer user_data)
{
  GSimpleAsyncResult *simple;

  g_return_if_fail (PHOTOS_IS_BASE_ITEM (self));

  simple = g_simple_async_result_new (G_OBJECT (self),
                                      callback,
                                      user_data,
                                      photos_base_item_load_async);
  g_simple_async_result_set_check_cancellable (simple, cancellable);

  g_simple_async_result_run_in_thread (simple,
                                       photos_base_item_load_in_thread_func,
                                       G_PRIORITY_DEFAULT,
                                       cancellable);
  g_object_unref (simple);
}


GdkPixbuf *
photos_base_item_load_finish (PhotosBaseItem *self, GAsyncResult *res, GError **error)
{
  GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (res);
  GdkPixbuf *ret_val = NULL;

  g_return_val_if_fail (g_simple_async_result_is_valid (res, G_OBJECT (self), photos_base_item_load_async), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  if (g_simple_async_result_propagate_error (simple, error))
    goto out;

  ret_val = GDK_PIXBUF (g_simple_async_result_get_op_res_gpointer (simple));
  g_object_ref (ret_val);

 out:
  return ret_val;
}


void
photos_base_item_set_default_app_name (PhotosBaseItem *self, const gchar *default_app_name)
{
  PhotosBaseItemPrivate *priv = self->priv;

  g_free (priv->default_app_name);
  priv->default_app_name = g_strdup (default_app_name);
}


void
photos_base_item_set_favorite (PhotosBaseItem *self, gboolean favorite)
{
  PHOTOS_BASE_ITEM_GET_CLASS (self)->set_favorite (self, favorite);
}
