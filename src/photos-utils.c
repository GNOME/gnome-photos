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

/* Based on code by the Documents team.
 */


#include "config.h"

#include <string.h>

#include <cairo.h>
#include <glib.h>
#include <libgnome-desktop/gnome-desktop-thumbnail.h>

#include "gd-main-view.h"
#include "photos-utils.h"


void
photos_utils_alpha_gtk_widget (GtkWidget *widget)
{
  GdkRGBA color = {0.0, 0.0, 0.0, 0.0};
  gtk_widget_override_background_color (widget, GTK_STATE_FLAG_NORMAL, &color);
}


GIcon *
photos_utils_create_symbolic_icon (const gchar *name, gint base_size)
{
  GIcon *icon;
  GIcon *ret_val = NULL;
  GdkPixbuf *pixbuf;
  GtkIconInfo *info;
  GtkIconTheme *theme;
  GtkStyleContext *style;
  GtkWidgetPath *path;
  cairo_surface_t *surface;
  cairo_t *cr;
  gchar *symbolic_name;
  const gint bg_min_size = 20;
  const gint emblem_min_size = 8;
  gint bg_size;
  gint emblem_size;
  gint total_size;

  total_size = base_size / 2;
  bg_size = MAX (total_size / 2, bg_min_size);
  emblem_size = MAX (bg_size - 8, emblem_min_size);

  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, total_size, total_size);
  cr = cairo_create (surface);

  style = gtk_style_context_new ();

  path = gtk_widget_path_new ();
  gtk_widget_path_append_type (path, GTK_TYPE_ICON_VIEW);
  gtk_style_context_set_path (style, path);
  gtk_widget_path_unref (path);

  gtk_style_context_add_class (style, "documents-icon-bg");

  gtk_render_background (style, cr, (total_size - bg_size) / 2, (total_size - bg_size) / 2, bg_size, bg_size);

  symbolic_name = g_strconcat (name, "-symbolic", NULL);
  icon = g_themed_icon_new_with_default_fallbacks (symbolic_name);
  g_free (symbolic_name);

  theme = gtk_icon_theme_get_default();
  info = gtk_icon_theme_lookup_by_gicon (theme, icon, emblem_size, GTK_ICON_LOOKUP_FORCE_SIZE);
  g_object_unref (icon);

  if (info == NULL)
    goto out;

  pixbuf = gtk_icon_info_load_symbolic_for_context (info, style, NULL, NULL);
  gtk_icon_info_free (info);

  if (pixbuf == NULL)
    goto out;

  gtk_render_icon (style, cr, pixbuf, (total_size - emblem_size) / 2,  (total_size - emblem_size) / 2);
  g_object_unref (pixbuf);

  ret_val = G_ICON (gdk_pixbuf_get_from_surface (surface, 0, 0, total_size, total_size));

 out:
  g_object_unref (style);
  cairo_surface_destroy (surface);
  cairo_destroy (cr);

  return ret_val;
}


static gboolean
photos_utils_create_thumbnail (GIOSchedulerJob *job, GCancellable *cancellable, gpointer user_data)
{
  GSimpleAsyncResult *result = user_data;
  GFile *file = G_FILE (g_async_result_get_source_object (G_ASYNC_RESULT (result)));
  GnomeDesktopThumbnailFactory *factory;
  GFileInfo *info;
  const gchar *attributes = G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE","G_FILE_ATTRIBUTE_TIME_MODIFIED;
  gchar *uri;
  GdkPixbuf *pixbuf;
  guint64 mtime;

  uri = g_file_get_uri (file);
  info = g_file_query_info (file, attributes, G_FILE_QUERY_INFO_NONE, NULL, NULL);

  /* we don't care about reporting errors here, just fail the
   * thumbnail.
   */
  if (info == NULL)
    {
      g_simple_async_result_set_op_res_gboolean (result, FALSE);
      goto out;
    }

  mtime = g_file_info_get_attribute_uint64 (info, G_FILE_ATTRIBUTE_TIME_MODIFIED);

  factory = gnome_desktop_thumbnail_factory_new (GNOME_DESKTOP_THUMBNAIL_SIZE_NORMAL);
  pixbuf = gnome_desktop_thumbnail_factory_generate_thumbnail (factory, uri, g_file_info_get_content_type (info));

  if (pixbuf != NULL)
    {
      gnome_desktop_thumbnail_factory_save_thumbnail (factory, pixbuf, uri, (time_t) mtime);
      g_simple_async_result_set_op_res_gboolean (result, TRUE);
    }
  else
    g_simple_async_result_set_op_res_gboolean (result, FALSE);

  g_object_unref (info);
  g_object_unref (file);
  g_object_unref (factory);
  g_clear_object (&pixbuf);

 out:
  g_simple_async_result_complete_in_idle (result);
  g_object_unref (result);

  return FALSE;
}


GdkPixbuf *
photos_utils_embed_image_in_frame (GdkPixbuf *source_image,
                                   const gchar *frame_image_path,
                                   GtkBorder *slice_width,
                                   GtkBorder *border_width)
{
  GError *error = NULL;
  GdkPixbuf *ret_val;
  GtkCssProvider *provider;
  GtkStyleContext *context;
  GtkWidgetPath *path;
  cairo_surface_t *surface;
  cairo_t *cr;
  gchar *css_str;
  int dest_height;
  int dest_width;
  int source_height;
  int source_width;

  source_width = gdk_pixbuf_get_width (source_image);
  source_height = gdk_pixbuf_get_height (source_image);

  dest_width = source_width +  border_width->left + border_width->right;
  dest_height = source_height + border_width->top + border_width->bottom;

  css_str = g_strdup_printf (".embedded-image { border-image: url(\"%s\") %d %d %d %d / %dpx %dpx %dpx %dpx }",
                             frame_image_path,
                             slice_width->top, slice_width->right, slice_width->bottom, slice_width->left,
                             border_width->top, border_width->right, border_width->bottom, border_width->left);
  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_data (provider, css_str, -1, &error);

  if (error != NULL)
    {
      g_warning ("Unable to create the thumbnail frame image: %s", error->message);
      g_error_free (error);
      g_free (css_str);

      return g_object_ref (source_image);
    }

  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, dest_width, dest_height);
  cr = cairo_create (surface);

  context = gtk_style_context_new ();
  path = gtk_widget_path_new ();
  gtk_widget_path_append_type (path, GTK_TYPE_ICON_VIEW);

  gtk_style_context_set_path (context, path);
  gtk_style_context_add_provider (context, GTK_STYLE_PROVIDER (provider), 600);

  gtk_style_context_save (context);
  gtk_style_context_add_class (context, "embedded-image");

  gtk_render_frame (context, cr,
                    0, 0,
                    dest_width, dest_height);

  gtk_style_context_restore (context);

  gtk_render_icon (context, cr,
                   source_image,
                   border_width->left, border_width->top);

  ret_val = gdk_pixbuf_get_from_surface (surface, 0, 0, dest_width, dest_height);

  cairo_surface_destroy (surface);
  cairo_destroy (cr);

  gtk_widget_path_unref (path);
  g_object_unref (provider);
  g_object_unref (context);
  g_free (css_str);

  return ret_val;
}


static gchar *
photos_utils_filename_get_extension_offset (const gchar *filename)
{
  gchar *end;
  gchar *end2;

  end = strrchr (filename, '.');

  if (end != NULL && end != filename)
    {
      if (g_strcmp0 (end, ".gz") == 0
          || g_strcmp0 (end, ".bz2") == 0
          || g_strcmp0 (end, ".sit") == 0
          || g_strcmp0 (end, ".Z") == 0)
        {
          end2 = end - 1;
          while (end2 > filename && *end2 != '.')
            end2--;
          if (end2 != filename)
            end = end2;
        }
  }

  return end;
}


gchar *
photos_utils_filename_strip_extension (const gchar *filename_with_extension)
{
  gchar *end;
  gchar *filename;

  if (filename_with_extension == NULL)
    return NULL;

  filename = g_strdup (filename_with_extension);
  end = photos_utils_filename_get_extension_offset (filename);

  if (end != NULL && end != filename)
    *end = '\0';

  return filename;
}


gint
photos_utils_get_icon_size (void)
{
  return 128;
}


GtkBorder *
photos_utils_get_thumbnail_frame_border (void)
{
  GtkBorder *slice;

  slice = gtk_border_new ();
  slice->top = 3;
  slice->right = 3;
  slice->bottom = 6;
  slice->left = 4;

  return slice;
}


GList *
photos_utils_get_urns_from_paths (GList *paths, GtkTreeModel *model)
{
  GList *l;
  GList *urns = NULL;

  for (l = paths; l != NULL; l = l->next)
    {
      GtkTreeIter iter;
      GtkTreePath *path = (GtkTreePath *) l->data;
      gchar *id;

      if (!gtk_tree_model_get_iter (model, &iter, path))
        continue;

      gtk_tree_model_get (model, &iter, GD_MAIN_COLUMN_ID, &id, -1);
      urns = g_list_prepend (urns, id);
    }

  return g_list_reverse (urns);
}


void
photos_utils_queue_thumbnail_job_for_file_async (GFile *file, GAsyncReadyCallback callback, gpointer user_data)
{
  GSimpleAsyncResult *result;

  result = g_simple_async_result_new (G_OBJECT (file),
                                      callback,
                                      user_data,
                                      photos_utils_queue_thumbnail_job_for_file_async);
  g_io_scheduler_push_job (photos_utils_create_thumbnail, result, NULL, G_PRIORITY_DEFAULT, NULL);
}


gboolean
photos_utils_queue_thumbnail_job_for_file_finish (GAsyncResult *res)
{
  GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (res);
  return g_simple_async_result_get_op_res_gboolean (simple);
}
