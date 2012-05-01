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

#include <gio/gio.h>
#include <glib.h>
#include <tracker-sparql.h>

#include "photos-base-item.h"
#include "photos-query.h"
#include "photos-utils.h"


struct _PhotosBaseItemPrivate
{
  GdkPixbuf *icon;
  gboolean favorite;
  gchar *author;
  gchar *id;
  gchar *identifier;
  gchar *mime_type;
  gchar *name;
  gchar *rdf_type;
  gchar *resource_urn;
  gchar *type_description;
  gchar *uri;
  glong mtime;
};

enum
{
  PROP_0,
  PROP_CURSOR
};


G_DEFINE_TYPE (PhotosBaseItem, photos_base_item, G_TYPE_OBJECT);


static void
photos_base_item_check_effects_and_update_info (PhotosBaseItem *self)
{
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
  photos_base_item_update_icon_from_type (self);
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
  g_time_val_from_iso8601 (mtime, &timeval);
  priv->mtime = timeval.tv_sec;

  priv->mime_type = g_strdup (tracker_sparql_cursor_get_string (cursor, PHOTOS_QUERY_COLUMNS_MIME_TYPE, NULL));
  priv->rdf_type = g_strdup (tracker_sparql_cursor_get_string (cursor, PHOTOS_QUERY_COLUMNS_RDF_TYPE, NULL));

  PHOTOS_BASE_ITEM_GET_CLASS (self)->update_type_description (self);

  title = tracker_sparql_cursor_get_string (cursor, PHOTOS_QUERY_COLUMNS_TITLE, NULL);
  if (title == NULL || title[0] == '\0')
    {
      title = tracker_sparql_cursor_get_string (cursor, PHOTOS_QUERY_COLUMNS_FILENAME, NULL);
      title = photos_utils_filename_strip_extension (title);
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
photos_base_item_dispose (GObject *object)
{
  PhotosBaseItem *self = PHOTOS_BASE_ITEM (object);
  PhotosBaseItemPrivate *priv = self->priv;

  g_clear_object (&priv->icon);

  G_OBJECT_CLASS (photos_base_item_parent_class)->dispose (object);
}


static void
photos_base_item_finalize (GObject *object)
{
  PhotosBaseItem *self = PHOTOS_BASE_ITEM (object);
  PhotosBaseItemPrivate *priv = self->priv;

  g_free (priv->author);
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
photos_base_item_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
  PhotosBaseItem *self = PHOTOS_BASE_ITEM (object);

  switch (prop_id)
    {
    case PROP_CURSOR:
      photos_base_item_populate_from_cursor (self, TRACKER_SPARQL_CURSOR (g_value_get_object (value)));
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

  object_class->dispose = photos_base_item_dispose;
  object_class->finalize = photos_base_item_finalize;
  object_class->set_property = photos_base_item_set_property;
  class->update_type_description = photos_base_item_update_type_description;

  g_object_class_install_property (object_class,
                                   PROP_CURSOR,
                                   g_param_spec_object ("cursor",
                                                        "TrackerSparqlCursor object",
                                                        "A cursor to iterate over the results of a query",
                                                        TRACKER_SPARQL_TYPE_CURSOR,
                                                        G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE));

  g_type_class_add_private (class, sizeof (PhotosBaseItemPrivate));
}


const gchar *
photos_base_item_get_author (PhotosBaseItem *self)
{
  return self->priv->author;
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


glong
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
