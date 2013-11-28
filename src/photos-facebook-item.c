/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2013 Álvaro Peña
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

#include <gfbgraph/gfbgraph.h>
#include <gfbgraph/gfbgraph-goa-authorizer.h>
#include <gio/gio.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <goa/goa.h>
#include <libgnome-desktop/gnome-desktop-thumbnail.h>

#include "photos-facebook-item.h"
#include "photos-source.h"
#include "photos-source-manager.h"
#include "photos-utils.h"


struct _PhotosFacebookItemPrivate
{
  PhotosBaseManager *src_mngr;
};


G_DEFINE_TYPE_WITH_PRIVATE (PhotosFacebookItem, photos_facebook_item, PHOTOS_TYPE_BASE_ITEM);


static GFBGraphPhoto *
photos_facebook_get_gfbgraph_photo (PhotosBaseItem *item, GCancellable *cancellable, GError **error)
{
  PhotosFacebookItemPrivate *priv = PHOTOS_FACEBOOK_ITEM (item)->priv;
  PhotosSource *source;
  const gchar *identifier, *resource_urn;
  GFBGraphGoaAuthorizer *authorizer;
  GFBGraphPhoto *photo;

  resource_urn = photos_base_item_get_resource_urn (item);
  source = PHOTOS_SOURCE (photos_base_manager_get_object_by_id (priv->src_mngr, resource_urn));
  authorizer = gfbgraph_goa_authorizer_new (photos_source_get_goa_object (source));
  identifier = photos_base_item_get_identifier (item) + strlen("facebook:");

  gfbgraph_authorizer_refresh_authorization (GFBGRAPH_AUTHORIZER (authorizer), cancellable, error);

  photo = gfbgraph_photo_new_from_id (GFBGRAPH_AUTHORIZER (authorizer), identifier, error);

  return photo;
}


static gboolean
photos_facebook_item_create_thumbnail (PhotosBaseItem *item, GCancellable *cancellable, GError **error)
{
  GFBGraphPhoto *photo;
  const GFBGraphPhotoImage *thumbnail_image;
  gchar *local_path, *local_dir;
  GFile *local_file, *remote_file;
  gboolean ret_val = FALSE;

  photo = photos_facebook_get_gfbgraph_photo (item, cancellable, error);
  if (photo == NULL)
    {
      g_error ("Can't get the photo from the Facebook Graph\n");
    }
  else
    {
    thumbnail_image = gfbgraph_photo_get_image_near_width (photo, photos_utils_get_icon_size ());
    if (thumbnail_image == NULL)
      {
        g_error ("Can't get a photo size to create the thumbnail.\n");
      }
    else
      {
        remote_file = g_file_new_for_uri (thumbnail_image->source);

        local_path = gnome_desktop_thumbnail_path_for_uri (photos_base_item_get_uri (item),
                                                           GNOME_DESKTOP_THUMBNAIL_SIZE_NORMAL);
        local_file = g_file_new_for_path (local_path);
        local_dir = g_path_get_dirname (local_path);
        g_mkdir_with_parents (local_dir, 0700);

        g_debug ("Downloading %s from Facebook to %s",
                 thumbnail_image->source, local_path);
        if (g_file_copy (remote_file,
                         local_file,
                         G_FILE_COPY_ALL_METADATA | G_FILE_COPY_OVERWRITE,
                         cancellable,
                         NULL,
                         NULL,
                         error))
          ret_val = TRUE;

        g_free (local_path);
        g_free (local_dir);
        g_clear_object (&local_file);
        g_clear_object (&remote_file);
      }
    }

  return ret_val;
}


static gchar *
photos_facebook_item_download (PhotosBaseItem *item, GCancellable *cancellable, GError **error)
{
  GFBGraphPhoto *photo;
  const GFBGraphPhotoImage *higher_image;
  const gchar *identifier;
  gchar *local_dir, *filename, *local_filename;
  GFile *local_file, *remote_file;

  photo = photos_facebook_get_gfbgraph_photo (item, cancellable, error);
  if (photo == NULL)
    {
      g_error ("Can't get the photo from the Facebook Graph\n");
    }
  else
    {
      higher_image = gfbgraph_photo_get_image_hires (photo);
      if (higher_image == NULL)
        {
          g_error ("Cant' get the higher photo size from Facebook.\n");
        }
      else
        {
          remote_file = g_file_new_for_uri (higher_image->source);

          /* Local path */
          local_dir = g_build_filename (g_get_user_cache_dir (), PACKAGE_TARNAME, "facebook", NULL);
          g_mkdir_with_parents (local_dir, 0700);

          /* Local filename */
          identifier = photos_base_item_get_identifier (item) + strlen("facebook:photos:");
          filename = g_strdup_printf ("%s.jpeg", identifier);
          local_filename = g_build_filename (local_dir, filename, NULL);

          local_file = g_file_new_for_path (local_filename);

          g_debug ("Downloading %s from Facebook to %s", higher_image->source, local_filename);
          if (!g_file_copy (remote_file,
                            local_file,
                            G_FILE_COPY_ALL_METADATA | G_FILE_COPY_OVERWRITE,
                            cancellable,
                            NULL,
                            NULL,
                            error))
            g_warning ("Failed downloading %s from Facebook to %s\n", higher_image->source, local_filename);

          g_free (filename);
          g_free (local_dir);
          g_clear_object (&local_file);
          g_clear_object (&remote_file);
        }

      g_clear_object (&photo);
    }

  return local_filename;
}


static const gchar *
photos_facebook_item_get_source_name (PhotosBaseItem *item)
{
  return _("Facebook");
}


/* TODO */
static void
photos_facebook_item_open (PhotosBaseItem *item, GdkScreen *screen, guint32 timestamp)
{
  GError *error;
  const gchar *facebook_uri;

  facebook_uri = photos_base_item_get_uri (item);

  error = NULL;
  gtk_show_uri (screen, facebook_uri, timestamp, &error);
  if (error != NULL)
    {
      g_warning ("Unable to show URI %s: %s", facebook_uri, error->message);
      g_error_free (error);
    }
}


static void
photos_facebook_item_constructed (GObject *object)
{
  PhotosFacebookItem *self = PHOTOS_FACEBOOK_ITEM (object);

  G_OBJECT_CLASS (photos_facebook_item_parent_class)->constructed (object);

  photos_base_item_set_default_app_name (PHOTOS_BASE_ITEM (self), _("Facebook"));
}


static void
photos_facebook_item_dispose (GObject *object)
{
  PhotosFacebookItem *self = PHOTOS_FACEBOOK_ITEM (object);

  g_clear_object (&self->priv->src_mngr);

  G_OBJECT_CLASS (photos_facebook_item_parent_class)->dispose (object);
}


static void
photos_facebook_item_init (PhotosFacebookItem *self)
{
  PhotosFacebookItemPrivate *priv;

  self->priv = photos_facebook_item_get_instance_private (self);
  priv = self->priv;

  priv->src_mngr = photos_source_manager_dup_singleton ();
}


static void
photos_facebook_item_class_init (PhotosFacebookItemClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  PhotosBaseItemClass *base_item_class = PHOTOS_BASE_ITEM_CLASS (class);

  object_class->constructed= photos_facebook_item_constructed;
  object_class->dispose = photos_facebook_item_dispose;
  base_item_class->create_thumbnail = photos_facebook_item_create_thumbnail;
  base_item_class->download = photos_facebook_item_download;
  base_item_class->get_source_name = photos_facebook_item_get_source_name;
  base_item_class->open = photos_facebook_item_open;
}


PhotosBaseItem *
photos_facebook_item_new (TrackerSparqlCursor *cursor)
{
  return g_object_new (PHOTOS_TYPE_FACEBOOK_ITEM,
                       "cursor", cursor,
                       "failed-thumbnailing", FALSE,
                       NULL);
}
