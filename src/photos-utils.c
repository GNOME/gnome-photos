/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2012 – 2021 Red Hat, Inc.
 * Copyright © 2009 Yorba Foundation
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

/* Based on code from:
 *   + Documents
 *   + Eye of GNOME
 *   + Shotwell
 */


#include "config.h"

#include <math.h>

#include <gdk/gdk.h>
#include <glib.h>
#include <tracker-sparql.h>
#include <libgd/gd.h>

#include "photos-application.h"
#include "photos-device-item.h"
#include "photos-enums.h"
#include "photos-error.h"
#include "photos-gegl.h"
#include "photos-google-item.h"
#include "photos-local-item.h"
#include "photos-media-server-item.h"
#include "photos-offset-collection-view-controller.h"
#include "photos-offset-collections-controller.h"
#include "photos-offset-favorites-controller.h"
#include "photos-offset-import-controller.h"
#include "photos-offset-overview-controller.h"
#include "photos-offset-search-controller.h"
#include "photos-query.h"
#include "photos-share-point.h"
#include "photos-share-point-email.h"
#include "photos-share-point-google.h"
#include "photos-share-point-online.h"
#include "photos-source.h"
#include "photos-thumbnail-factory.h"
#include "photos-tool.h"
#include "photos-tool-colors.h"
#include "photos-tool-crop.h"
#include "photos-tool-enhance.h"
#include "photos-tool-filters.h"
#include "photos-tracker-collection-view-controller.h"
#include "photos-tracker-collections-controller.h"
#include "photos-tracker-favorites-controller.h"
#include "photos-tracker-import-controller.h"
#include "photos-tracker-overview-controller.h"
#include "photos-tracker-queue.h"
#include "photos-tracker-search-controller.h"
#include "photos-utils.h"


GdkPixbuf *
photos_utils_center_pixbuf (GdkPixbuf *pixbuf, gint size)
{
  GdkPixbuf *ret_val;
  gint height;
  gint pixbuf_size;
  gint width;

  height = gdk_pixbuf_get_height (pixbuf);
  width = gdk_pixbuf_get_width (pixbuf);

  pixbuf_size = MAX (height, width);
  if (pixbuf_size >= size)
    {
      ret_val = g_object_ref (pixbuf);
      goto out;
    }

  ret_val = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, size, size);
  gdk_pixbuf_fill (ret_val, 0x00000000);
  gdk_pixbuf_copy_area (pixbuf, 0, 0, width, height, ret_val, (size - width) / 2, (size - height) / 2);

 out:
  return ret_val;
}


gchar *
photos_utils_convert_path_to_uri (const gchar *path)
{
  g_autoptr (GFile) file = NULL;
  gchar *uri = NULL;

  if (path == NULL)
    {
      uri = g_strdup ("");
      goto out;
    }

  file = g_file_new_for_path (path);
  uri = g_file_get_uri (file);

 out:
  g_return_val_if_fail (uri != NULL, NULL);
  return uri;
}


GStrv
photos_utils_convert_paths_to_uris (const gchar *const *paths)
{
  GStrv uris = NULL;
  guint i;
  guint n_paths;

  if (paths == NULL)
    goto out;

  n_paths = g_strv_length ((GStrv) paths);
  uris = (GStrv) g_malloc0_n (n_paths + 1, sizeof (gchar *));

  for (i = 0; paths[i] != NULL; i++)
    {
      g_autofree gchar *uri = NULL;

      uri = photos_utils_convert_path_to_uri (paths[i]);
      uris[i] = g_steal_pointer (&uri);
    }

 out:
  return uris;
}


GIcon *
photos_utils_create_collection_icon (gint base_size, GList *pixbufs)
{
  cairo_surface_t *surface; /* TODO: use g_autoptr */
  cairo_t *cr; /* TODO: use g_autoptr */
  GdkPixbuf *pix;
  GIcon *ret_val;
  GList *l;
  g_autoptr (GtkStyleContext) context = NULL;
  g_autoptr (GtkWidgetPath) path = NULL;
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

  return ret_val;
}


GdkPixbuf *
photos_utils_create_placeholder_icon_for_scale (const gchar *name, gint size, gint scale)
{
  GApplication *app;
  g_autoptr (GdkPixbuf) centered_pixbuf = NULL;
  g_autoptr (GdkPixbuf) pixbuf = NULL;
  GdkPixbuf *ret_val = NULL;
  g_autoptr (GIcon) icon = NULL;
  GList *windows;
  g_autoptr (GtkIconInfo) info = NULL;
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

  {
    g_autoptr (GError) error = NULL;

    pixbuf = gtk_icon_info_load_symbolic_for_context (info, context, NULL, &error);
    if (error != NULL)
      {
        g_warning ("Unable to load icon '%s': %s", name, error->message);
        goto out;
      }
  }

  size_scaled = size * scale;
  centered_pixbuf = photos_utils_center_pixbuf (pixbuf, size_scaled);

  ret_val = centered_pixbuf;
  centered_pixbuf = NULL;

 out:
  return ret_val;
}


GIcon *
photos_utils_create_symbolic_icon_for_scale (const gchar *name, gint base_size, gint scale)
{
  g_autoptr (GIcon) icon = NULL;
  GIcon *ret_val = NULL;
  g_autoptr (GdkPixbuf) pixbuf = NULL;
  g_autoptr (GtkIconInfo) info = NULL;
  GtkIconTheme *theme;
  g_autoptr (GtkStyleContext) style = NULL;
  g_autoptr (GtkWidgetPath) path = NULL;
  cairo_surface_t *icon_surface = NULL; /* TODO: use g_autoptr */
  cairo_surface_t *surface; /* TODO: use g_autoptr */
  cairo_t *cr; /* TODO: use g_autoptr */
  g_autofree gchar *symbolic_name = NULL;
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

  gtk_style_context_add_class (style, "photos-icon-bg");

  gtk_render_background (style, cr, total_size - bg_size, total_size - bg_size, bg_size, bg_size);

  symbolic_name = g_strconcat (name, "-symbolic", NULL);
  icon = g_themed_icon_new_with_default_fallbacks (symbolic_name);

  theme = gtk_icon_theme_get_default();
  info = gtk_icon_theme_lookup_by_gicon_for_scale (theme, icon, emblem_size, scale, GTK_ICON_LOOKUP_FORCE_SIZE);
  if (info == NULL)
    goto out;

  pixbuf = gtk_icon_info_load_symbolic_for_context (info, style, NULL, NULL);
  if (pixbuf == NULL)
    goto out;

  icon_surface = gdk_cairo_surface_create_from_pixbuf (pixbuf, scale, NULL);

  emblem_pos = total_size - emblem_size - emblem_margin;
  gtk_render_icon_surface (style, cr, icon_surface, emblem_pos, emblem_pos);

  ret_val = G_ICON (gdk_pixbuf_get_from_surface (surface, 0, 0, total_size_scaled, total_size_scaled));

 out:
  cairo_surface_destroy (icon_surface);
  cairo_surface_destroy (surface);
  cairo_destroy (cr);

  return ret_val;
}


gboolean
photos_utils_create_thumbnail (GFile *file,
                               const gchar *mime_type,
                               gint64 mtime,
                               GQuark orientation,
                               gint64 original_height,
                               gint64 original_width,
                               const gchar *const *pipeline_uris,
                               const gchar *thumbnail_path,
                               GCancellable *cancellable,
                               GError **error)
{
  g_autoptr (PhotosThumbnailFactory) factory = NULL;
  gboolean ret_val = FALSE;

  factory = photos_thumbnail_factory_dup_singleton (NULL, NULL);
  if (!photos_thumbnail_factory_generate_thumbnail (factory,
                                                    file,
                                                    mime_type,
                                                    orientation,
                                                    original_height,
                                                    original_width,
                                                    pipeline_uris,
                                                    thumbnail_path,
                                                    cancellable,
                                                    error))
    goto out;

  ret_val = TRUE;

 out:
  return ret_val;
}


GVariant *
photos_utils_create_zoom_target_value (gdouble delta, PhotosZoomEvent event)
{
  g_autoptr (GEnumClass) zoom_event_class = NULL;
  GEnumValue *event_value;
  GVariant *delta_value;
  GVariant *event_nick_value;
  GVariant *ret_val = NULL;
  g_auto (GVariantBuilder) builder = G_VARIANT_BUILDER_INIT (G_VARIANT_TYPE_VARDICT);
  const gchar *event_nick = "none";

  g_return_val_if_fail (delta >= 0.0, NULL);
  g_return_val_if_fail (event != PHOTOS_ZOOM_EVENT_NONE, NULL);

  delta_value = g_variant_new_double (delta);
  g_variant_builder_add (&builder, "{sv}", "delta", delta_value);

  zoom_event_class = G_ENUM_CLASS (g_type_class_ref (PHOTOS_TYPE_ZOOM_EVENT));

  event_value = g_enum_get_value (zoom_event_class, (gint) event);
  if (event_value != NULL)
    event_nick = event_value->value_nick;

  event_nick_value = g_variant_new_string (event_nick);
  g_variant_builder_add (&builder, "{sv}", "event", event_nick_value);

  ret_val = g_variant_builder_end (&builder);

  g_return_val_if_fail (g_variant_is_floating (ret_val), ret_val);
  return ret_val;
}


static GIcon *
photos_utils_get_thumbnail_icon (PhotosBaseItem *item)
{
  g_autoptr (GFile) thumb_file = NULL;
  g_autoptr (GFileInfo) info = NULL;
  GIcon *icon = NULL;
  const gchar *thumb_path;
  const gchar *uri;

  uri = photos_base_item_get_uri (item);
  if (uri == NULL || uri[0] == '\0')
    goto out;

  {
    g_autoptr (GError) error = NULL;

    info = photos_base_item_query_info (item,
                                        G_FILE_ATTRIBUTE_THUMBNAIL_PATH,
                                        G_FILE_QUERY_INFO_NONE,
                                        NULL,
                                        &error);
    if (error != NULL)
      {
        g_warning ("Unable to fetch thumbnail path for %s: %s", uri, error->message);
        goto out;
      }
  }

  thumb_path = g_file_info_get_attribute_byte_string (info, G_FILE_ATTRIBUTE_THUMBNAIL_PATH);
  if (thumb_path == NULL)
    goto out;

  thumb_file = g_file_new_for_path (thumb_path);
  icon = g_file_icon_new (thumb_file);

 out:
  return icon;
}


GIcon *
photos_utils_get_icon_from_item (PhotosBaseItem *item)
{
  GIcon *icon = NULL;
  gboolean is_remote = FALSE;
  const gchar *identifier;
  const gchar *mime_type;

  identifier = photos_base_item_get_identifier (item);
  if (identifier != NULL)
    {
      if (g_str_has_prefix (identifier, "facebook:") ||
          g_str_has_prefix (identifier, "flickr:") ||
          g_str_has_prefix (identifier, "google:"))
        is_remote = TRUE;
    }

  if (!is_remote)
    icon = photos_utils_get_thumbnail_icon (item);

  if (icon != NULL)
    goto out;

  mime_type = photos_base_item_get_mime_type (item);
  if (mime_type != NULL)
    icon = g_content_type_get_icon (mime_type);

  if (icon != NULL)
    goto out;

  if (photos_base_item_is_collection (item))
    {
      gint size;

      size = photos_utils_get_icon_size ();
      icon = photos_utils_create_collection_icon (size, NULL);
    }

  if (icon != NULL)
    goto out;

  icon = g_themed_icon_new ("image-x-generic");

 out:
  return icon;
}


gdouble
photos_utils_get_zoom_delta (GVariant *dictionary)
{
  gdouble delta;
  gdouble ret_val = -1.0;

  g_return_val_if_fail (dictionary != NULL, -1.0);
  g_return_val_if_fail (g_variant_is_of_type (dictionary, G_VARIANT_TYPE_VARDICT), -1.0);

  if (!g_variant_lookup (dictionary, "delta", "d", &delta))
    goto out;

  ret_val = delta;

 out:
  g_return_val_if_fail (ret_val >= 0.0, -1.0);
  return ret_val;
}


PhotosZoomEvent
photos_utils_get_zoom_event (GVariant *dictionary)
{
  g_autoptr (GEnumClass) zoom_event_class = NULL;
  GEnumValue *event_value;
  PhotosZoomEvent ret_val = PHOTOS_ZOOM_EVENT_NONE;
  const gchar *event_str;

  g_return_val_if_fail (dictionary != NULL, PHOTOS_ZOOM_EVENT_NONE);
  g_return_val_if_fail (g_variant_is_of_type (dictionary, G_VARIANT_TYPE_VARDICT), PHOTOS_ZOOM_EVENT_NONE);

  if (!g_variant_lookup (dictionary, "event", "&s", &event_str))
    goto out;

  zoom_event_class = G_ENUM_CLASS (g_type_class_ref (PHOTOS_TYPE_ZOOM_EVENT));

  event_value = g_enum_get_value_by_nick (zoom_event_class, event_str);
  if (event_value == NULL)
    event_value = g_enum_get_value_by_name (zoom_event_class, event_str);
  if (event_value == NULL)
    goto out;

  ret_val = (PhotosZoomEvent) event_value->value;

 out:
  g_return_val_if_fail (ret_val != PHOTOS_ZOOM_EVENT_NONE, PHOTOS_ZOOM_EVENT_NONE);
  return ret_val;
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
   * Lo-Dpi.
   *
   * Sometimes, a pixbuf can be slightly smaller than size. eg.,
   * server-generated thumbnails for remote tems. Scaling them up
   * won't cause any discernible loss of quality and will make our
   * letterboxed grid look nicer. 75% of 'scale' has been chosen as
   * the arbitrary definition of 'slightly smaller'.
   *
   * Therefore, if a pixbuf lies between (3 * size / 4, size * scale)
   * we scale it up to size * scale, so that it doesn't look smaller.
   * Similarly, if a pixbuf is smaller than size, then we increase its
   * dimensions by the scale factor.
   */

  if (pixbuf_size == scaled_size)
    {
      ret_val = g_object_ref (pixbuf);
    }
  else if (pixbuf_size > 3 * size / 4)
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
  photos_gegl_ensure_builtins ();

  if (g_once_init_enter (&once_init_value))
    {
      g_type_ensure (PHOTOS_TYPE_DEVICE_ITEM);
      //g_type_ensure (PHOTOS_TYPE_GOOGLE_ITEM);
      g_type_ensure (PHOTOS_TYPE_LOCAL_ITEM);
      g_type_ensure (PHOTOS_TYPE_MEDIA_SERVER_ITEM);

      g_type_ensure (PHOTOS_TYPE_SHARE_POINT_EMAIL);
      //g_type_ensure (PHOTOS_TYPE_SHARE_POINT_GOOGLE);

      g_type_ensure (PHOTOS_TYPE_TOOL_COLORS);
      g_type_ensure (PHOTOS_TYPE_TOOL_CROP);
      g_type_ensure (PHOTOS_TYPE_TOOL_ENHANCE);
      g_type_ensure (PHOTOS_TYPE_TOOL_FILTERS);

      g_type_ensure (PHOTOS_TYPE_TRACKER_COLLECTION_VIEW_CONTROLLER);
      g_type_ensure (PHOTOS_TYPE_TRACKER_COLLECTIONS_CONTROLLER);
      g_type_ensure (PHOTOS_TYPE_TRACKER_FAVORITES_CONTROLLER);
      g_type_ensure (PHOTOS_TYPE_TRACKER_IMPORT_CONTROLLER);
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

      extension_point = g_io_extension_point_register (PHOTOS_SHARE_POINT_EXTENSION_POINT_NAME);
      g_io_extension_point_set_required_type (extension_point, PHOTOS_TYPE_SHARE_POINT);

      extension_point = g_io_extension_point_register (PHOTOS_SHARE_POINT_ONLINE_EXTENSION_POINT_NAME);
      g_io_extension_point_set_required_type (extension_point, PHOTOS_TYPE_SHARE_POINT_ONLINE);

      extension_point = g_io_extension_point_register (PHOTOS_TOOL_EXTENSION_POINT_NAME);
      g_io_extension_point_set_required_type (extension_point, PHOTOS_TYPE_TOOL);

      extension_point = g_io_extension_point_register (PHOTOS_TRACKER_CONTROLLER_EXTENSION_POINT_NAME);
      g_io_extension_point_set_required_type (extension_point, PHOTOS_TYPE_TRACKER_CONTROLLER);

      g_once_init_leave (&once_init_value, 1);
    }
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


gboolean
photos_utils_file_copy_as_thumbnail (GFile *source,
                                     GFile *destination,
                                     const gchar *original_uri,
                                     gint64 original_height,
                                     gint64 original_width,
                                     GCancellable *cancellable,
                                     GError **error)
{
  g_autoptr (GFileInputStream) istream = NULL;
  g_autoptr (GFileOutputStream) ostream = NULL;
  g_autoptr (GdkPixbuf) pixbuf = NULL;
  gboolean ret_val = FALSE;
  const gchar *prgname;
  g_autofree gchar *original_height_str = NULL;
  g_autofree gchar *original_width_str = NULL;

  g_return_val_if_fail (G_IS_FILE (source), FALSE);
  g_return_val_if_fail (G_IS_FILE (destination), FALSE);
  g_return_val_if_fail (original_uri != NULL && original_uri[0] != '\0', FALSE);
  g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  istream = g_file_read (source, cancellable, error);
  if (istream == NULL)
    goto out;

  pixbuf = gdk_pixbuf_new_from_stream (G_INPUT_STREAM (istream), cancellable, error);
  if (pixbuf == NULL)
    goto out;

  ostream = g_file_replace (destination,
                            NULL,
                            FALSE,
                            G_FILE_CREATE_PRIVATE | G_FILE_CREATE_REPLACE_DESTINATION,
                            cancellable,
                            error);
  if (ostream == NULL)
    goto out;

  original_height_str = g_strdup_printf ("%" G_GINT64_FORMAT, original_height);
  original_width_str = g_strdup_printf ("%" G_GINT64_FORMAT, original_width);
  prgname = g_get_prgname ();
  if (!gdk_pixbuf_save_to_stream (pixbuf,
                                  G_OUTPUT_STREAM (ostream),
                                  "png",
                                  cancellable,
                                  error,
                                  "tEXt::Software", prgname,
                                  "tEXt::Thumb::URI", original_uri,
                                  "tEXt::Thumb::Image::Height", original_height_str,
                                  "tEXt::Thumb::Image::Width", original_width_str,
                                  NULL))
    {
      goto out;
    }

  ret_val = TRUE;

 out:
  return ret_val;
}


void
photos_utils_get_controller (PhotosWindowMode mode,
                             PhotosOffsetController **out_offset_cntrlr,
                             PhotosTrackerController **out_trk_cntrlr)
{
  g_autoptr (PhotosOffsetController) offset_cntrlr = NULL;
  g_autoptr (PhotosTrackerController) trk_cntrlr = NULL;

  g_return_if_fail (mode != PHOTOS_WINDOW_MODE_NONE);
  g_return_if_fail (mode != PHOTOS_WINDOW_MODE_EDIT);
  g_return_if_fail (mode != PHOTOS_WINDOW_MODE_PREVIEW);

  switch (mode)
    {
    case PHOTOS_WINDOW_MODE_COLLECTION_VIEW:
      offset_cntrlr = photos_offset_collection_view_controller_dup_singleton ();
      trk_cntrlr = photos_tracker_collection_view_controller_dup_singleton ();
      break;

    case PHOTOS_WINDOW_MODE_COLLECTIONS:
      offset_cntrlr = photos_offset_collections_controller_dup_singleton ();
      trk_cntrlr = photos_tracker_collections_controller_dup_singleton ();
      break;

    case PHOTOS_WINDOW_MODE_FAVORITES:
      offset_cntrlr = photos_offset_favorites_controller_dup_singleton ();
      trk_cntrlr = photos_tracker_favorites_controller_dup_singleton ();
      break;

    case PHOTOS_WINDOW_MODE_IMPORT:
      offset_cntrlr = photos_offset_import_controller_dup_singleton ();
      trk_cntrlr = photos_tracker_import_controller_dup_singleton ();
      break;

    case PHOTOS_WINDOW_MODE_OVERVIEW:
      offset_cntrlr = photos_offset_overview_controller_dup_singleton ();
      trk_cntrlr = photos_tracker_overview_controller_dup_singleton ();
      break;

    case PHOTOS_WINDOW_MODE_SEARCH:
      offset_cntrlr = photos_offset_search_controller_dup_singleton ();
      trk_cntrlr = photos_tracker_search_controller_dup_singleton ();
      break;

    case PHOTOS_WINDOW_MODE_NONE:
    case PHOTOS_WINDOW_MODE_EDIT:
    case PHOTOS_WINDOW_MODE_PREVIEW:
    default:
      g_assert_not_reached ();
      break;
    }

  if (out_offset_cntrlr != NULL)
    g_set_object (out_offset_cntrlr, offset_cntrlr);

  if (out_trk_cntrlr != NULL)
    g_set_object (out_trk_cntrlr, trk_cntrlr);
}


gdouble
photos_utils_get_double_from_sparql_cursor_with_default (TrackerSparqlCursor *cursor,
                                                         PhotosQueryColumns column,
                                                         gdouble default_value)
{
  TrackerSparqlValueType value_type;
  gdouble ret_val = default_value;

  value_type = tracker_sparql_cursor_get_value_type (cursor, column);
  if (value_type == TRACKER_SPARQL_VALUE_TYPE_UNBOUND)
    goto out;

  ret_val = tracker_sparql_cursor_get_double (cursor, column);

 out:
  return ret_val;
}


gchar *
photos_utils_get_extension_from_mime_type (const gchar *mime_type)
{
  g_autoptr (GSList) formats = NULL;
  GSList *l;
  gchar *ret_val = NULL;

  formats = gdk_pixbuf_get_formats ();

  for (l = formats; l != NULL; l = l->next)
    {
      GdkPixbufFormat *format = (GdkPixbufFormat*) l->data;
      g_auto (GStrv) supported_mime_types = NULL;
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

      if (ret_val != NULL)
        break;
    }

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


gint64
photos_utils_get_integer_from_sparql_cursor_with_default (TrackerSparqlCursor *cursor,
                                                          PhotosQueryColumns column,
                                                          gint64 default_value)
{
  TrackerSparqlValueType value_type;
  gint64 ret_val = default_value;

  value_type = tracker_sparql_cursor_get_value_type (cursor, column);
  if (value_type == TRACKER_SPARQL_VALUE_TYPE_UNBOUND)
    goto out;

  ret_val = tracker_sparql_cursor_get_integer (cursor, column);

 out:
  return ret_val;
}


gint64
photos_utils_get_mtime_from_sparql_cursor (TrackerSparqlCursor *cursor)
{
  const gchar *mtime_str;
  gint64 mtime = -1;

  mtime_str = tracker_sparql_cursor_get_string (cursor, PHOTOS_QUERY_COLUMNS_MTIME, NULL);
  if (mtime_str != NULL)
    {
      g_autoptr (GDateTime) date_modified = NULL;

      date_modified = g_date_time_new_from_iso8601 (mtime_str, NULL);
      if (date_modified != NULL)
        mtime = g_date_time_to_unix (date_modified);
    }

  if (mtime == -1)
    mtime = g_get_real_time () / 1000000;

  return mtime;
}


gchar *
photos_utils_get_pixbuf_common_suffix (GdkPixbufFormat *format)
{
  g_auto (GStrv) extensions = NULL;
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

  return result;
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


gboolean
photos_utils_get_selection_mode (void)
{
  GAction *action;
  GApplication *app;
  g_autoptr (GVariant) state = NULL;
  gboolean selection_mode;

  app = g_application_get_default ();
  action = g_action_map_lookup_action (G_ACTION_MAP (app), "selection-mode");

  state = g_action_get_state (action);
  g_return_val_if_fail (state != NULL, FALSE);

  selection_mode = g_variant_get_boolean (state);

  return selection_mode;
}


GList *
photos_utils_get_urns_from_items (GList *items)
{
  GList *l;
  GList *urns = NULL;

  for (l = items; l != NULL; l = l->next)
    {
      GdMainBoxItem *box_item = GD_MAIN_BOX_ITEM (l->data);
      const gchar *id;

      id = gd_main_box_item_get_id (box_item);
      urns = g_list_prepend (urns, g_strdup (id));
    }

  return g_list_reverse (urns);
}


const gchar *
photos_utils_get_version (void)
{
  const gchar *ret_val = NULL;

#ifdef PACKAGE_COMMIT_ID
  ret_val = PACKAGE_COMMIT_ID;
#else
  ret_val = PACKAGE_VERSION;
#endif

  return ret_val;
}


gboolean
photos_utils_is_flatpak (void)
{
  gboolean ret_val;

  ret_val = g_file_test ("/.flatpak-info", G_FILE_TEST_EXISTS);
  return ret_val;
}


void
photos_utils_launch_online_accounts (const gchar *account_id)
{
  GApplication *app;
  g_autoptr (GDBusActionGroup) control_center = NULL;
  GDBusConnection *connection;
  GVariant *parameters;
  g_auto (GVariantBuilder) panel_parameters = G_VARIANT_BUILDER_INIT (G_VARIANT_TYPE ("av"));

  app = g_application_get_default ();
  connection = g_application_get_dbus_connection (app);
  control_center = g_dbus_action_group_get (connection, "org.gnome.ControlCenter", "/org/gnome/ControlCenter");

  if (account_id != NULL && account_id[0] != '\0')
    {
      GVariant *account_id_variant;

      account_id_variant = g_variant_new_string (account_id);
      g_variant_builder_add (&panel_parameters, "v", account_id_variant);
    }

  parameters = g_variant_new ("(s@av)", "online-accounts", g_variant_builder_end (&panel_parameters));
  g_action_group_activate_action (G_ACTION_GROUP (control_center), "launch-panel", parameters);
}


void
photos_utils_list_box_header_func (GtkListBoxRow *row, GtkListBoxRow *before, gpointer user_data)
{
  GtkWidget *header;

  if (before == NULL)
    {
      gtk_list_box_row_set_header (row, NULL);
      return;
    }

  header = gtk_list_box_row_get_header (row);
  if (header == NULL)
    {
      header = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
      gtk_widget_show (header);
      gtk_list_box_row_set_header (row, header);
    }
}


GAppLaunchContext *
photos_utils_new_app_launch_context_from_widget (GtkWidget *widget)
{
  GAppLaunchContext *ret_val = NULL;
  g_autoptr (GdkAppLaunchContext) ctx = NULL;
  GdkDisplay *display = NULL;
  GdkScreen *screen = NULL;

  if (widget != NULL)
    {
      screen = gtk_widget_get_screen (widget);
      display = gdk_screen_get_display (screen);
    }

  if (display == NULL)
    display = gdk_display_get_default ();

  ctx = gdk_display_get_app_launch_context (display);
  if (screen != NULL)
    gdk_app_launch_context_set_screen (ctx, screen);

  ret_val = G_APP_LAUNCH_CONTEXT (g_steal_pointer (&ctx));
  return ret_val;
}


void
photos_utils_object_list_free_full (GList *objects)
{
  g_list_free_full (objects, g_object_unref);
}


gchar *
photos_utils_print_zoom_action_detailed_name (const gchar *action_name, gdouble delta, PhotosZoomEvent event)
{
  g_autoptr (GVariant) target_value = NULL;
  gchar *ret_val = NULL;

  g_return_val_if_fail (action_name != NULL && action_name[0] != '\0', NULL);
  g_return_val_if_fail (delta >= 0.0, NULL);
  g_return_val_if_fail (event != PHOTOS_ZOOM_EVENT_NONE, NULL);

  target_value = photos_utils_create_zoom_target_value (delta, event);
  target_value = g_variant_ref_sink (target_value);

  ret_val = g_action_print_detailed_name (action_name, target_value);

  return ret_val;
}


static gboolean
photos_utils_adjustment_can_scroll (GtkAdjustment *adjustment)
{
  gdouble lower;
  gdouble page_size;
  gdouble upper;

  g_return_val_if_fail (GTK_IS_ADJUSTMENT (adjustment), FALSE);

  lower = gtk_adjustment_get_lower (adjustment);
  page_size = gtk_adjustment_get_page_size (adjustment);
  upper = gtk_adjustment_get_upper (adjustment);

  return upper - lower > page_size;
}


gboolean
photos_utils_scrolled_window_can_scroll (GtkScrolledWindow *scrolled_window)
{
  GtkAdjustment *adjustment;
  gboolean ret_val = TRUE;

  g_return_val_if_fail (GTK_IS_SCROLLED_WINDOW (scrolled_window), FALSE);

  adjustment = gtk_scrolled_window_get_hadjustment (scrolled_window);
  if (photos_utils_adjustment_can_scroll (adjustment))
    goto out;

  adjustment = gtk_scrolled_window_get_vadjustment (scrolled_window);
  if (photos_utils_adjustment_can_scroll (adjustment))
    goto out;

  ret_val = FALSE;

 out:
  return ret_val;
}


static void
photos_utils_adjustment_scroll (GtkAdjustment *adjustment, gdouble delta)
{
  gdouble value;

  g_return_if_fail (GTK_IS_ADJUSTMENT (adjustment));

  value = gtk_adjustment_get_value (adjustment);
  value += delta;
  gtk_adjustment_set_value (adjustment, value);
}


void
photos_utils_scrolled_window_scroll (GtkScrolledWindow *scrolled_window, gdouble delta_x, gdouble delta_y)
{
  GtkAdjustment *adjustment;

  g_return_if_fail (GTK_IS_SCROLLED_WINDOW (scrolled_window));
  g_return_if_fail (photos_utils_scrolled_window_can_scroll (scrolled_window));

  adjustment = gtk_scrolled_window_get_hadjustment (scrolled_window);
  photos_utils_adjustment_scroll (adjustment, delta_x);

  adjustment = gtk_scrolled_window_get_vadjustment (scrolled_window);
  photos_utils_adjustment_scroll (adjustment, delta_y);
}


static void
photos_utils_update_executed (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  TrackerSparqlConnection *connection = TRACKER_SPARQL_CONNECTION (source_object);
  const gchar *urn = (gchar *) user_data;

  {
    g_autoptr (GError) error = NULL;

    tracker_sparql_connection_update_finish (connection, res, &error);
    if (error != NULL)
      g_warning ("Unable to update %s: %s", urn, error->message);
  }
}


void
photos_utils_set_edited_name (const gchar *urn, const gchar *title)
{
  g_autoptr (PhotosQuery) query = NULL;
  g_autoptr (PhotosTrackerQueue) queue = NULL;
  g_autofree gchar *sparql = NULL;

  sparql = g_strdup_printf ("WITH tracker:Pictures "
                            "DELETE { <%s> nie:title ?title } "
                            "INSERT { "
                            "  <%s> a nmm:Photo ; nie:title \"%s\" . "
                            "}"
                            "WHERE { <%s> nie:title ?title }", urn, urn, title, urn);

  query = photos_query_new (NULL, sparql);

  {
    g_autoptr (GError) error = NULL;

    queue = photos_tracker_queue_dup_singleton (NULL, &error);
    if (G_UNLIKELY (error != NULL))
      {
        g_warning ("Unable to set edited name %s: %s", urn, error->message);
        goto out;
      }
  }

  photos_tracker_queue_update (queue, query, NULL, photos_utils_update_executed, g_strdup (urn), g_free);

 out:
  return;
}


void
photos_utils_set_favorite (const gchar *urn, gboolean is_favorite)
{
  g_autoptr (PhotosQuery) query = NULL;
  g_autoptr (PhotosTrackerQueue) queue = NULL;
  g_autofree gchar *sparql = NULL;

  if (is_favorite)
    {
      sparql = g_strdup_printf ("INSERT DATA { "
                                "  GRAPH tracker:Pictures {"
                                "    <%s> a nmm:Photo ; nao:hasTag nao:predefined-tag-favorite ."
                                "  } "
                                "}", urn);
    }
  else
    {
      sparql = g_strdup_printf ("DELETE DATA {"
                                "  GRAPH tracker:Pictures {"
                                "    <%s> nao:hasTag nao:predefined-tag-favorite "
                                "  } "
                                "}", urn);
    }

  query = photos_query_new (NULL, sparql);

  {
    g_autoptr (GError) error = NULL;

    queue = photos_tracker_queue_dup_singleton (NULL, &error);
    if (G_UNLIKELY (error != NULL))
      {
        g_warning ("Unable to set favorite %s: %s", urn, error->message);
        goto out;
      }
  }

  photos_tracker_queue_update (queue, query, NULL, photos_utils_update_executed, g_strdup (urn), g_free);

 out:
  return;
}


gboolean
photos_utils_set_string (gchar **string_ptr, const gchar *new_string)
{
  gboolean ret_val = FALSE;

  g_return_val_if_fail (string_ptr != NULL, FALSE);

  if (*string_ptr == new_string)
    goto out;

  if (g_strcmp0 (*string_ptr, new_string) == 0)
    goto out;

  g_free (*string_ptr);
  *string_ptr = g_strdup (new_string);

  ret_val = TRUE;

 out:
  return ret_val;
}


gboolean
photos_utils_take_string (gchar **string_ptr, gchar *new_string)
{
  gboolean ret_val = FALSE;

  g_return_val_if_fail (string_ptr != NULL, FALSE);

  if (*string_ptr == new_string)
    {
      new_string = NULL;
      goto out;
    }

  if (g_strcmp0 (*string_ptr, new_string) == 0)
    goto out;

  g_free (*string_ptr);
  *string_ptr = new_string;
  new_string = NULL;

  ret_val = TRUE;

 out:
  g_free (new_string);
  return ret_val;
}
