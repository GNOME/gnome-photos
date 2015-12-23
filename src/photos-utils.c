/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2012, 2013, 2014, 2015 Red Hat, Inc.
 * Copyright © 2009 Yorba Foundation
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
 *   + Eye of GNOME
 *   + Shotwell
 *   + Totem
 */


#include "config.h"

#include <string.h>

#include <glib.h>
#include <libgnome-desktop/gnome-desktop-thumbnail.h>
#include <tracker-sparql.h>
#include <libgd/gd.h>

#include "photos-application.h"
#include "photos-facebook-item.h"
#include "photos-flickr-item.h"
#include "photos-google-item.h"
#include "photos-local-item.h"
#include "photos-media-server-item.h"
#include "photos-operation-insta-curve.h"
#include "photos-operation-insta-filter.h"
#include "photos-operation-jpg-guess-sizes.h"
#include "photos-query.h"
#include "photos-source.h"
#include "photos-tool.h"
#include "photos-tool-colors.h"
#include "photos-tool-crop.h"
#include "photos-tool-enhance.h"
#include "photos-tool-filters.h"
#include "photos-tracker-collections-controller.h"
#include "photos-tracker-controller.h"
#include "photos-tracker-favorites-controller.h"
#include "photos-tracker-overview-controller.h"
#include "photos-tracker-queue.h"
#include "photos-tracker-search-controller.h"
#include "photos-utils.h"


static const gdouble EPSILON = 1e-5;


static void
photos_utils_put_pixel (guchar *p)
{
  p[0] = 46;
  p[1] = 52;
  p[2] = 54;
  p[3] = 0xff;
}


void
photos_utils_border_pixbuf (GdkPixbuf *pixbuf)
{
  gint height;
  gint width;
  gint rowstride;
  gint x;
  gint y;
  guchar *pixels;

  height = gdk_pixbuf_get_height (pixbuf);
  width = gdk_pixbuf_get_width (pixbuf);

  pixels = gdk_pixbuf_get_pixels (pixbuf);
  rowstride = gdk_pixbuf_get_rowstride (pixbuf);

  /* top */
  for (x = 0; x < width; x++)
    photos_utils_put_pixel (pixels + x * 4);

  /* bottom */
  for (x = 0; x < width; x++)
    photos_utils_put_pixel (pixels + (height - 1) * rowstride + x * 4);

  /* left */
  for (y = 1; y < height - 1; y++)
    photos_utils_put_pixel (pixels + y * rowstride);

  /* right */
  for (y = 1; y < height - 1; y++)
    photos_utils_put_pixel (pixels + y * rowstride + (width - 1) * 4);
}


GdkPixbuf *
photos_utils_center_pixbuf (GdkPixbuf *pixbuf, gint size)
{
  GdkPixbuf *ret_val;
  gint height;
  gint width;

  ret_val = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, size, size);
  gdk_pixbuf_fill (ret_val, 0x00000000);

  height = gdk_pixbuf_get_height (pixbuf);
  width = gdk_pixbuf_get_width (pixbuf);
  gdk_pixbuf_copy_area (pixbuf, 0, 0, width, height, ret_val, (size - width) / 2, (size - height) / 2);

  return ret_val;
}


gchar *
photos_utils_convert_path_to_uri (const gchar *path)
{
  GFile *file;
  gchar *uri;

  if (path == NULL)
    return g_strdup ("");

  file = g_file_new_for_path (path);
  uri = g_file_get_uri (file);
  g_object_unref (file);

  return uri;
}


GeglBuffer *
photos_utils_create_buffer_from_node (GeglNode *node)
{
  GeglBuffer *buffer = NULL;
  GeglNode *buffer_sink;
  GeglNode *graph;

  graph = gegl_node_get_parent (node);
  buffer_sink = gegl_node_new_child (graph,
                                     "operation", "gegl:buffer-sink",
                                     "buffer", &buffer,
                                     NULL);
  gegl_node_link (node, buffer_sink);
  gegl_node_process (buffer_sink);
  g_object_unref (buffer_sink);

  return buffer;
}


GIcon *
photos_utils_create_collection_icon (gint base_size, GList *pixbufs)
{
  cairo_surface_t *surface;
  cairo_t *cr;
  GdkPixbuf *pix;
  GIcon *ret_val;
  GList *l;
  GtkStyleContext *context;
  GtkWidgetPath *path;
  gint cur_x;
  gint cur_y;
  gint padding;
  gint pix_height;
  gint pix_width;
  gint scale_size;
  gint tile_size;
  guint idx;
  guint n_grid;
  guint n_pixbufs;
  guint n_tiles;

  n_pixbufs = g_list_length (pixbufs);
  if (n_pixbufs < 3)
    {
      n_grid = 1;
      n_tiles = 1;
    }
  else
    {
      n_grid = 2;
      n_tiles = 4;
    }

  padding = MAX (base_size / 10, 4);
  tile_size = (base_size - ((n_grid + 1) * padding)) / n_grid;

  context = gtk_style_context_new ();
  gtk_style_context_add_class (context, "photos-collection-icon");

  path = gtk_widget_path_new ();
  gtk_widget_path_append_type (path, GTK_TYPE_ICON_VIEW);
  gtk_style_context_set_path (context, path);
  gtk_widget_path_unref (path);

  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, base_size, base_size);
  cr = cairo_create (surface);

  gtk_render_background (context, cr, 0, 0, base_size, base_size);

  l = pixbufs;
  idx = 0;
  cur_x = padding;
  cur_y = padding;

  while (l != NULL && idx < n_tiles)
    {
      pix = l->data;
      pix_width = gdk_pixbuf_get_width (pix);
      pix_height = gdk_pixbuf_get_height (pix);

      scale_size = MIN (pix_width, pix_height);

      cairo_save (cr);

      cairo_translate (cr, cur_x, cur_y);

      cairo_rectangle (cr, 0, 0,
                       tile_size, tile_size);
      cairo_clip (cr);

      cairo_scale (cr, (gdouble) tile_size / (gdouble) scale_size, (gdouble) tile_size / (gdouble) scale_size);
      gdk_cairo_set_source_pixbuf (cr, pix, 0, 0);

      cairo_paint (cr);
      cairo_restore (cr);

      idx++;
      l = l->next;

      if ((idx % n_grid) == 0)
        {
          cur_x = padding;
          cur_y += tile_size + padding;
        }
      else
        {
          cur_x += tile_size + padding;
        }
    }

  ret_val = G_ICON (gdk_pixbuf_get_from_surface (surface, 0, 0, base_size, base_size));

  cairo_surface_destroy (surface);
  cairo_destroy (cr);
  g_object_unref (context);

  return ret_val;
}


GeglNode *
photos_utils_create_orientation_node (GeglNode *parent, GQuark orientation)
{
  GeglNode *ret_val = NULL;
  double degrees = 1.0;

  if (orientation == PHOTOS_ORIENTATION_TOP)
    goto out;

  if (orientation == PHOTOS_ORIENTATION_BOTTOM)
    degrees = -180.0;
  else if (orientation == PHOTOS_ORIENTATION_LEFT)
    degrees = -270.0;
  else if (orientation == PHOTOS_ORIENTATION_RIGHT)
    degrees = -90.0;

  if (degrees < 0.0)
    ret_val = gegl_node_new_child (parent, "operation", "gegl:rotate-on-center", "degrees", degrees, NULL);

 out:
  if (ret_val == NULL)
    ret_val = gegl_node_new_child (parent, "operation", "gegl:nop", NULL);

  return ret_val;
}


GdkPixbuf *
photos_utils_create_placeholder_icon_for_scale (const gchar *name, gint size, gint scale)
{
  GApplication *app;
  GdkPixbuf *centered_pixbuf = NULL;
  GdkPixbuf *pixbuf = NULL;
  GdkPixbuf *ret_val = NULL;
  GError *error;
  GIcon *icon = NULL;
  GList *windows;
  GtkIconInfo *info = NULL;
  GtkIconTheme *theme;
  GtkStyleContext *context;
  gint size_scaled;

  app = g_application_get_default ();
  windows = gtk_application_get_windows (GTK_APPLICATION (app));
  if (windows == NULL)
    goto out;

  icon = g_themed_icon_new (name);
  theme = gtk_icon_theme_get_default ();
  info = gtk_icon_theme_lookup_by_gicon_for_scale (theme,
                                                   icon,
                                                   16,
                                                   scale,
                                                   GTK_ICON_LOOKUP_FORCE_SIZE | GTK_ICON_LOOKUP_FORCE_SYMBOLIC);
  if (info == NULL)
    goto out;

  context = gtk_widget_get_style_context (GTK_WIDGET (windows->data));

  error = NULL;
  pixbuf = gtk_icon_info_load_symbolic_for_context (info, context, NULL, &error);
  if (error != NULL)
    {
      g_warning ("Unable to load icon '%s': %s", name, error->message);
      g_error_free (error);
      goto out;
    }

  size_scaled = size * scale;
  centered_pixbuf = photos_utils_center_pixbuf (pixbuf, size_scaled);
  photos_utils_border_pixbuf (centered_pixbuf);

  ret_val = centered_pixbuf;
  centered_pixbuf = NULL;

 out:
  g_clear_object (&centered_pixbuf);
  g_clear_object (&pixbuf);
  g_clear_object (&info);
  g_clear_object (&icon);
  return ret_val;
}


GdkPixbuf *
photos_utils_create_pixbuf_from_node (GeglNode *node)
{
  GdkPixbuf *pixbuf = NULL;
  GeglNode *graph;
  GeglNode *save_pixbuf;

  graph = gegl_node_get_parent (node);
  save_pixbuf = gegl_node_new_child (graph,
                                     "operation", "gegl:save-pixbuf",
                                     "pixbuf", &pixbuf,
                                     NULL);
  gegl_node_link_many (node, save_pixbuf, NULL);
  gegl_node_process (save_pixbuf);
  g_object_unref (save_pixbuf);

  return pixbuf;
}


GIcon *
photos_utils_create_symbolic_icon_for_scale (const gchar *name, gint base_size, gint scale)
{
  GIcon *icon;
  GIcon *ret_val = NULL;
  GdkPixbuf *pixbuf;
  GtkIconInfo *info;
  GtkIconTheme *theme;
  GtkStyleContext *style;
  GtkWidgetPath *path;
  cairo_surface_t *icon_surface = NULL;
  cairo_surface_t *surface;
  cairo_t *cr;
  gchar *symbolic_name;
  const gint bg_size = 24;
  const gint emblem_margin = 4;
  gint emblem_pos;
  gint emblem_size;
  gint total_size;
  gint total_size_scaled;

  total_size = base_size / 2;
  total_size_scaled = total_size * scale;
  emblem_size = bg_size - emblem_margin * 2;

  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, total_size_scaled, total_size_scaled);
  cairo_surface_set_device_scale (surface, (gdouble) scale, (gdouble) scale);
  cr = cairo_create (surface);

  style = gtk_style_context_new ();

  path = gtk_widget_path_new ();
  gtk_widget_path_append_type (path, GTK_TYPE_ICON_VIEW);
  gtk_style_context_set_path (style, path);
  gtk_widget_path_unref (path);

  gtk_style_context_add_class (style, "photos-icon-bg");

  gtk_render_background (style, cr, total_size - bg_size, total_size - bg_size, bg_size, bg_size);

  symbolic_name = g_strconcat (name, "-symbolic", NULL);
  icon = g_themed_icon_new_with_default_fallbacks (symbolic_name);
  g_free (symbolic_name);

  theme = gtk_icon_theme_get_default();
  info = gtk_icon_theme_lookup_by_gicon_for_scale (theme, icon, emblem_size, scale, GTK_ICON_LOOKUP_FORCE_SIZE);
  g_object_unref (icon);

  if (info == NULL)
    goto out;

  pixbuf = gtk_icon_info_load_symbolic_for_context (info, style, NULL, NULL);
  g_object_unref (info);

  if (pixbuf == NULL)
    goto out;

  icon_surface = gdk_cairo_surface_create_from_pixbuf (pixbuf, scale, NULL);
  g_object_unref (pixbuf);

  emblem_pos = total_size - emblem_size - emblem_margin;
  gtk_render_icon_surface (style, cr, icon_surface, emblem_pos, emblem_pos);

  ret_val = G_ICON (gdk_pixbuf_get_from_surface (surface, 0, 0, total_size_scaled, total_size_scaled));

 out:
  g_object_unref (style);
  cairo_surface_destroy (icon_surface);
  cairo_surface_destroy (surface);
  cairo_destroy (cr);

  return ret_val;
}


gboolean
photos_utils_create_thumbnail (GFile *file, GCancellable *cancellable, GError **error)
{
  GnomeDesktopThumbnailFactory *factory = NULL;
  GFileInfo *info = NULL;
  const gchar *attributes = G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE","G_FILE_ATTRIBUTE_TIME_MODIFIED;
  gboolean ret_val = FALSE;
  gchar *uri = NULL;
  GdkPixbuf *pixbuf = NULL;
  guint64 mtime;

  uri = g_file_get_uri (file);
  info = g_file_query_info (file, attributes, G_FILE_QUERY_INFO_NONE, cancellable, error);
  if (info == NULL)
    goto out;

  mtime = g_file_info_get_attribute_uint64 (info, G_FILE_ATTRIBUTE_TIME_MODIFIED);

  factory = gnome_desktop_thumbnail_factory_new (GNOME_DESKTOP_THUMBNAIL_SIZE_LARGE);
  pixbuf = gnome_desktop_thumbnail_factory_generate_thumbnail (factory, uri, g_file_info_get_content_type (info));
  if (pixbuf == NULL)
    {
      /* FIXME: use proper #defines and enumerated types */
      g_set_error (error,
                   g_quark_from_static_string ("gnome-desktop-error"),
                   0,
                   "GnomeDesktopThumbnailFactory failed");
      goto out;
    }

  gnome_desktop_thumbnail_factory_save_thumbnail (factory, pixbuf, uri, (time_t) mtime);
  ret_val = TRUE;

 out:
  g_clear_object (&pixbuf);
  g_clear_object (&factory);
  g_clear_object (&info);
  g_free (uri);
  return ret_val;
}


static GIcon *
photos_utils_get_thumbnail_icon (const gchar *uri)
{
  GError *error;
  GFile *file = NULL;
  GFile *thumb_file = NULL;
  GFileInfo *info = NULL;
  GIcon *icon = NULL;
  const gchar *thumb_path;

  file = g_file_new_for_uri (uri);

  error = NULL;
  info = g_file_query_info (file, G_FILE_ATTRIBUTE_THUMBNAIL_PATH, G_FILE_QUERY_INFO_NONE, NULL, &error);
  if (error != NULL)
    {
      g_warning ("Unable to fetch thumbnail path for %s: %s", uri, error->message);
      g_error_free (error);
      goto out;
    }

  thumb_path = g_file_info_get_attribute_byte_string (info, G_FILE_ATTRIBUTE_THUMBNAIL_PATH);
  thumb_file = g_file_new_for_path (thumb_path);
  icon = g_file_icon_new (thumb_file);

 out:
  g_clear_object (&thumb_file);
  g_clear_object (&info);
  g_clear_object (&file);
  return icon;
}


GIcon *
photos_utils_get_icon_from_cursor (TrackerSparqlCursor *cursor)
{
  GIcon *icon = NULL;
  gboolean is_remote = FALSE;
  const gchar *identifier;
  const gchar *mime_type;
  const gchar *rdf_type;

  identifier = tracker_sparql_cursor_get_string (cursor, PHOTOS_QUERY_COLUMNS_IDENTIFIER, NULL);
  if (identifier != NULL)
    {
      if (g_str_has_prefix (identifier, "facebook:") ||
          g_str_has_prefix (identifier, "flickr:") ||
          g_str_has_prefix (identifier, "google:"))
        is_remote = TRUE;
    }

  if (!is_remote)
    {
      const gchar *uri;

      uri = tracker_sparql_cursor_get_string (cursor, PHOTOS_QUERY_COLUMNS_URI, NULL);
      if (uri != NULL)
        icon = photos_utils_get_thumbnail_icon (uri);
    }

  if (icon != NULL)
    goto out;

  mime_type = tracker_sparql_cursor_get_string (cursor, PHOTOS_QUERY_COLUMNS_MIME_TYPE, NULL);
  if (mime_type != NULL)
    icon = g_content_type_get_icon (mime_type);

  if (icon != NULL)
    goto out;

  rdf_type = tracker_sparql_cursor_get_string (cursor, PHOTOS_QUERY_COLUMNS_RDF_TYPE, NULL);
  if (mime_type != NULL)
    icon = photos_utils_icon_from_rdf_type (rdf_type);

  if (icon != NULL)
    goto out;

  icon = g_themed_icon_new ("image-x-generic");

 out:
  return icon;
}


GdkPixbuf *
photos_utils_downscale_pixbuf_for_scale (GdkPixbuf *pixbuf, gint size, gint scale)
{
  GdkPixbuf *ret_val;
  gint height;
  gint pixbuf_size;
  gint scaled_size;
  gint width;

  height = gdk_pixbuf_get_height (pixbuf);
  width = gdk_pixbuf_get_width (pixbuf);
  pixbuf_size = MAX (height, width);

  scaled_size = size * scale;

  /* On Hi-Dpi displays, a pixbuf should never appear smaller than on
   * Lo-Dpi. Therefore, if a pixbuf lies between (size, size * scale)
   * we scale it up to size * scale, so that it doesn't look smaller.
   * Similarly, if a pixbuf is smaller than size, then we increase its
   * dimensions by the scale factor.
   */

  if (pixbuf_size == scaled_size)
    {
      ret_val = g_object_ref (pixbuf);
    }
  else if (pixbuf_size > size)
    {
      if (height == width)
        {
          height = scaled_size;
          width = scaled_size;
        }
      else if (height > width)
        {
          width = (gint) (0.5 + (gdouble) (width * scaled_size) / (gdouble) height);
          height = scaled_size;
        }
      else
        {
          height = (gint) (0.5 + (gdouble) (height * scaled_size) / (gdouble) width);
          width = scaled_size;
        }

      height = MAX (height, 1);
      width = MAX (width, 1);
      ret_val = gdk_pixbuf_scale_simple (pixbuf, width, height, GDK_INTERP_BILINEAR);
    }
  else /* pixbuf_size <= size */
    {
      if (scale == 1)
        {
          ret_val = g_object_ref (pixbuf);
        }
      else
        {
          height *= scale;
          width *= scale;

          height = MAX (height, 1);
          width = MAX (width, 1);
          ret_val = gdk_pixbuf_scale_simple (pixbuf, width, height, GDK_INTERP_BILINEAR);
        }
    }

  return ret_val;
}


void
photos_utils_draw_rectangle_handles (cairo_t *cr,
                                     gdouble x,
                                     gdouble y,
                                     gdouble width,
                                     gdouble height,
                                     gdouble offset,
                                     gdouble radius)
{
  cairo_save (cr);

  cairo_new_sub_path (cr);
  cairo_arc (cr, x - offset, y - offset, radius, 0.0, 2.0 * M_PI);
  cairo_fill (cr);

  cairo_new_sub_path (cr);
  cairo_arc (cr, x + width + offset, y - offset, radius, 0.0, 2.0 * M_PI);
  cairo_fill (cr);

  cairo_new_sub_path (cr);
  cairo_arc (cr, x + width + offset, y + height + offset, radius, 0.0, 2.0 * M_PI);
  cairo_fill (cr);

  cairo_new_sub_path (cr);
  cairo_arc (cr, x - offset, y + height + offset, radius, 0.0, 2.0 * M_PI);
  cairo_fill (cr);

  cairo_restore (cr);
}


void
photos_utils_draw_rectangle_thirds (cairo_t *cr, gdouble x, gdouble y, gdouble width, gdouble height)
{
  const gdouble one_third_x = width / 3.0;
  const gdouble one_third_y = height / 3.0;

  cairo_save (cr);

  cairo_move_to (cr, x + one_third_x, y);
  cairo_line_to (cr, x + one_third_x, y + height);
  cairo_stroke (cr);

  cairo_move_to (cr, x + 2.0 * one_third_x, y);
  cairo_line_to (cr, x + 2.0 * one_third_x, y + height);
  cairo_stroke (cr);

  cairo_move_to (cr, x, y + one_third_y);
  cairo_line_to (cr, x + width, y + one_third_y);
  cairo_stroke (cr);

  cairo_move_to (cr, x, y + 2.0 * one_third_y);
  cairo_line_to (cr, x + width, y + 2.0 * one_third_y);
  cairo_stroke (cr);

  cairo_restore (cr);
}


void
photos_utils_ensure_builtins (void)
{
  static gsize once_init_value = 0;

  photos_utils_ensure_extension_points ();

  if (g_once_init_enter (&once_init_value))
    {
      g_type_ensure (PHOTOS_TYPE_FACEBOOK_ITEM);
      g_type_ensure (PHOTOS_TYPE_FLICKR_ITEM);
      g_type_ensure (PHOTOS_TYPE_GOOGLE_ITEM);
      g_type_ensure (PHOTOS_TYPE_LOCAL_ITEM);
      g_type_ensure (PHOTOS_TYPE_MEDIA_SERVER_ITEM);

      g_type_ensure (PHOTOS_TYPE_OPERATION_INSTA_CURVE);
      g_type_ensure (PHOTOS_TYPE_OPERATION_INSTA_FILTER);
      g_type_ensure (PHOTOS_TYPE_OPERATION_JPG_GUESS_SIZES);

      g_type_ensure (PHOTOS_TYPE_TOOL_COLORS);
      g_type_ensure (PHOTOS_TYPE_TOOL_CROP);
      g_type_ensure (PHOTOS_TYPE_TOOL_ENHANCE);
      g_type_ensure (PHOTOS_TYPE_TOOL_FILTERS);

      g_type_ensure (PHOTOS_TYPE_TRACKER_COLLECTIONS_CONTROLLER);
      g_type_ensure (PHOTOS_TYPE_TRACKER_FAVORITES_CONTROLLER);
      g_type_ensure (PHOTOS_TYPE_TRACKER_OVERVIEW_CONTROLLER);
      g_type_ensure (PHOTOS_TYPE_TRACKER_SEARCH_CONTROLLER);

      g_once_init_leave (&once_init_value, 1);
    }
}


void
photos_utils_ensure_extension_points (void)
{
  static gsize once_init_value = 0;

  if (g_once_init_enter (&once_init_value))
    {
      GIOExtensionPoint *extension_point;

      extension_point = g_io_extension_point_register (PHOTOS_BASE_ITEM_EXTENSION_POINT_NAME);
      g_io_extension_point_set_required_type (extension_point, PHOTOS_TYPE_BASE_ITEM);

      extension_point = g_io_extension_point_register (PHOTOS_TOOL_EXTENSION_POINT_NAME);
      g_io_extension_point_set_required_type (extension_point, PHOTOS_TYPE_TOOL);

      extension_point = g_io_extension_point_register (PHOTOS_TRACKER_CONTROLLER_EXTENSION_POINT_NAME);
      g_io_extension_point_set_required_type (extension_point, PHOTOS_TYPE_TRACKER_CONTROLLER);

      g_once_init_leave (&once_init_value, 1);
    }
}


gboolean
photos_utils_equal_double (gdouble a, gdouble b)
{
  const gdouble diff = a - b;
  return diff > -EPSILON && diff < EPSILON;
}


GQuark
photos_utils_error_quark (void)
{
  return g_quark_from_static_string ("gnome-photos-error-quark");
}


gdouble
photos_utils_eval_radial_line (gdouble crop_center_x,
                               gdouble crop_center_y,
                               gdouble corner_x,
                               gdouble corner_y,
                               gdouble event_x)
{
  gdouble decision_intercept;
  gdouble decision_slope;
  gdouble projected_y;

  decision_slope = (corner_y - crop_center_y) / (corner_x - crop_center_x);
  decision_intercept = corner_y - (decision_slope * corner_x);
  projected_y = decision_slope * event_x + decision_intercept;

  return projected_y;
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


GQuark
photos_utils_flash_off_quark (void)
{
  return g_quark_from_static_string ("http://www.tracker-project.org/temp/nmm#flash-off");
}


GQuark
photos_utils_flash_on_quark (void)
{
  return g_quark_from_static_string ("http://www.tracker-project.org/temp/nmm#flash-on");
}


gchar *
photos_utils_get_extension_from_mime_type (const gchar *mime_type)
{
  GSList *formats;
  GSList *l;
  gchar *ret_val = NULL;

  formats = gdk_pixbuf_get_formats ();

  for (l = formats; l != NULL; l = l->next)
    {
      GdkPixbufFormat *format = (GdkPixbufFormat*) l->data;
      gchar **supported_mime_types;
      guint i;

      supported_mime_types = gdk_pixbuf_format_get_mime_types (format);
      for (i = 0; supported_mime_types[i] != NULL; i++)
        {
          if (g_strcmp0 (mime_type, supported_mime_types[i]) == 0)
            {
              ret_val = photos_utils_get_pixbuf_common_suffix (format);
              break;
            }
        }

      g_strfreev (supported_mime_types);
      if (ret_val != NULL)
        break;
    }

  g_slist_free (formats);
  return ret_val;
}


gint
photos_utils_get_icon_size (void)
{
  GApplication *app;
  gint scale;
  gint size;

  app = g_application_get_default ();
  scale = photos_application_get_scale_factor (PHOTOS_APPLICATION (app));
  size = photos_utils_get_icon_size_unscaled ();
  return scale * size;
}


gint
photos_utils_get_icon_size_unscaled (void)
{
  return 256;
}


gchar *
photos_utils_get_pixbuf_common_suffix (GdkPixbufFormat *format)
{
  gchar **extensions;
  gchar *result = NULL;
  gint i;

  if (format == NULL)
    return NULL;

  extensions = gdk_pixbuf_format_get_extensions (format);
  if (extensions[0] == NULL)
    return NULL;

  /* try to find 3-char suffix first, use the last occurence */
  for (i = 0; extensions [i] != NULL; i++)
    {
      if (strlen (extensions[i]) <= 3)
        {
          g_free (result);
          result = g_ascii_strdown (extensions[i], -1);
        }
    }

  /* otherwise take the first one */
  if (result == NULL)
    result = g_ascii_strdown (extensions[0], -1);

  g_strfreev (extensions);

  return result;
}


gchar *
photos_utils_get_pixbuf_type_from_mime_type (const gchar *mime_type)
{
  GSList *formats;
  GSList *l;
  gchar *ret_val = NULL;

  formats = gdk_pixbuf_get_formats ();

  for (l = formats; l != NULL; l = l->next)
    {
      GdkPixbufFormat *format = (GdkPixbufFormat*) l->data;
      gchar **supported_mime_types;
      guint i;

      supported_mime_types = gdk_pixbuf_format_get_mime_types (format);
      for (i = 0; supported_mime_types[i] != NULL; i++)
        {
          if (g_strcmp0 (mime_type, supported_mime_types[i]) == 0)
            {
              ret_val = gdk_pixbuf_format_get_name (format);
              break;
            }
        }

      g_strfreev (supported_mime_types);
      if (ret_val != NULL)
        break;
    }

  g_slist_free (formats);
  return ret_val;
}


const gchar *
photos_utils_get_provider_name (PhotosBaseManager *src_mngr, PhotosBaseItem *item)
{
  PhotosSource *source;
  const gchar *name;
  const gchar *resource_urn;

  resource_urn = photos_base_item_get_resource_urn (item);
  source = PHOTOS_SOURCE (photos_base_manager_get_object_by_id (src_mngr, resource_urn));
  name = photos_source_get_name (source);
  return name;
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


GIcon *
photos_utils_icon_from_rdf_type (const gchar *type)
{
  GIcon *ret_val = NULL;
  gint size;

  size = photos_utils_get_icon_size ();
  if (strstr (type, "nfo#DataContainer") != NULL)
    ret_val = photos_utils_create_collection_icon (size, NULL);

  return ret_val;
}


gboolean
photos_utils_make_directory_with_parents (GFile *file, GCancellable *cancellable, GError **error)
{
  GError *local_error = NULL;
  gboolean ret_val;

  ret_val = g_file_make_directory_with_parents (file, cancellable, &local_error);
  if (local_error != NULL)
    {
      if (g_error_matches (local_error, G_IO_ERROR, G_IO_ERROR_EXISTS))
        {
          g_clear_error (&local_error);
          ret_val = TRUE;
        }
    }

  if (local_error != NULL)
    g_propagate_error (error, local_error);

  return ret_val;
}


GQuark
photos_utils_orientation_bottom_quark (void)
{
  return g_quark_from_static_string ("http://www.semanticdesktop.org/ontologies/2007/03/22/nfo#orientation-bottom");
}


GQuark
photos_utils_orientation_left_quark (void)
{
  return g_quark_from_static_string ("http://www.semanticdesktop.org/ontologies/2007/03/22/nfo#orientation-left");
}


GQuark
photos_utils_orientation_right_quark (void)
{
  return g_quark_from_static_string ("http://www.semanticdesktop.org/ontologies/2007/03/22/nfo#orientation-right");
}


GQuark
photos_utils_orientation_top_quark (void)
{
  return g_quark_from_static_string ("http://www.semanticdesktop.org/ontologies/2007/03/22/nfo#orientation-top");
}


static void
photos_utils_update_executed (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  TrackerSparqlConnection *connection = TRACKER_SPARQL_CONNECTION (source_object);
  const gchar *urn = (gchar *) user_data;
  GError *error;

  error = NULL;
  tracker_sparql_connection_update_finish (connection, res, &error);
  if (error != NULL)
    {
      g_warning ("Unable to update %s: %s", urn, error->message);
      g_error_free (error);
    }
}


void
photos_utils_set_edited_name (const gchar *urn, const gchar *title)
{
  GError *error;
  PhotosTrackerQueue *queue = NULL;
  gchar *sparql = NULL;

  sparql = g_strdup_printf ("INSERT OR REPLACE { <%s> nie:title \"%s\" }", urn, title);

  error = NULL;
  queue = photos_tracker_queue_dup_singleton (NULL, &error);
  if (G_UNLIKELY (error != NULL))
    {
      g_warning ("Unable to set edited name %s: %s", urn, error->message);
      g_error_free (error);
      goto out;
    }

  photos_tracker_queue_update (queue, sparql, NULL, photos_utils_update_executed, g_strdup (urn), g_free);

 out:
  g_clear_object (&queue);
  g_free (sparql);
}


void
photos_utils_set_favorite (const gchar *urn, gboolean is_favorite)
{
  GError *error;
  PhotosTrackerQueue *queue = NULL;
  gchar *sparql;

  sparql = g_strdup_printf ("%s { <%s> nao:hasTag nao:predefined-tag-favorite }",
                            (is_favorite) ? "INSERT OR REPLACE" : "DELETE",
                            urn);

  error = NULL;
  queue = photos_tracker_queue_dup_singleton (NULL, &error);
  if (G_UNLIKELY (error != NULL))
    {
      g_warning ("Unable to set favorite %s: %s", urn, error->message);
      g_error_free (error);
      goto out;
    }

  photos_tracker_queue_update (queue, sparql, NULL, photos_utils_update_executed, g_strdup (urn), g_free);

 out:
  g_clear_object (&queue);
}
