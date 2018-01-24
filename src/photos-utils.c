/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2012 – 2017 Red Hat, Inc.
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

#include <string.h>

#include <gdk/gdk.h>
#include <glib.h>
#include <tracker-sparql.h>
#include <libgd/gd.h>

#include "photos-application.h"
#include "photos-enums.h"
#include "photos-error.h"
#include "photos-facebook-item.h"
#include "photos-flickr-item.h"
#include "photos-gegl.h"
#include "photos-google-item.h"
#include "photos-local-item.h"
#include "photos-media-server-item.h"
#include "photos-offset-collection-view-controller.h"
#include "photos-offset-collections-controller.h"
#include "photos-offset-favorites-controller.h"
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
#include "photos-tracker-overview-controller.h"
#include "photos-tracker-queue.h"
#include "photos-tracker-search-controller.h"
#include "photos-utils.h"


typedef struct _PhotosUtilsFileQueryInfoData PhotosUtilsFileQueryInfoData;

struct _PhotosUtilsFileQueryInfoData
{
  GFileQueryInfoFlags flags;
  gchar *attributes;
};

static const gdouble EPSILON = 1e-5;

enum
{
  THUMBNAIL_GENERATION = 0
};


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
                               const gchar *pipeline_uri,
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
                                                    pipeline_uri,
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
  GEnumClass *zoom_event_class = NULL; /* TODO: use g_autoptr */
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

  g_type_class_unref (zoom_event_class);
  g_return_val_if_fail (g_variant_is_floating (ret_val), ret_val);
  return ret_val;
}


static GIcon *
photos_utils_get_thumbnail_icon (PhotosBaseItem *item)
{
  g_autoptr (GFile) file = NULL;
  g_autoptr (GFile) thumb_file = NULL;
  g_autoptr (GFileInfo) info = NULL;
  GIcon *icon = NULL;
  const gchar *thumb_path;
  const gchar *uri;

  uri = photos_base_item_get_uri (item);
  if (uri == NULL || uri[0] == '\0')
    goto out;

  file = g_file_new_for_uri (uri);

  {
    g_autoptr (GError) error = NULL;

    info = photos_utils_file_query_info (file,
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
  GEnumClass *zoom_event_class = NULL;
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
  g_clear_pointer (&zoom_event_class, g_type_class_unref);
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
      g_type_ensure (PHOTOS_TYPE_FACEBOOK_ITEM);
      g_type_ensure (PHOTOS_TYPE_FLICKR_ITEM);
      g_type_ensure (PHOTOS_TYPE_GOOGLE_ITEM);
      g_type_ensure (PHOTOS_TYPE_LOCAL_ITEM);
      g_type_ensure (PHOTOS_TYPE_MEDIA_SERVER_ITEM);

      g_type_ensure (PHOTOS_TYPE_SHARE_POINT_EMAIL);
      g_type_ensure (PHOTOS_TYPE_SHARE_POINT_GOOGLE);

      g_type_ensure (PHOTOS_TYPE_TOOL_COLORS);
      g_type_ensure (PHOTOS_TYPE_TOOL_CROP);
      g_type_ensure (PHOTOS_TYPE_TOOL_ENHANCE);
      g_type_ensure (PHOTOS_TYPE_TOOL_FILTERS);

      g_type_ensure (PHOTOS_TYPE_TRACKER_COLLECTION_VIEW_CONTROLLER);
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


gboolean
photos_utils_equal_double (gdouble a, gdouble b)
{
  const gdouble diff = a - b;
  return diff > -EPSILON && diff < EPSILON;
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


GFileInfo *
photos_utils_file_query_info (GFile *file,
                              const gchar *attributes,
                              GFileQueryInfoFlags flags,
                              GCancellable *cancellable,
                              GError **error)
{
  GFileAttributeMatcher *matcher = NULL; /* TODO: use g_autoptr */
  g_autoptr (GFileInfo) info = NULL;
  GFileInfo *ret_val = NULL;

  g_return_val_if_fail (G_IS_FILE (file), NULL);
  g_return_val_if_fail (attributes != NULL && attributes[0] != '\0', NULL);
  g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  info = g_file_query_info (file, attributes, flags, cancellable, error);
  if (info == NULL)
    goto out;

  matcher = g_file_attribute_matcher_new (attributes);
  if (g_file_attribute_matcher_matches (matcher, G_FILE_ATTRIBUTE_THUMBNAIL_IS_VALID)
      || g_file_attribute_matcher_matches (matcher, G_FILE_ATTRIBUTE_THUMBNAIL_PATH)
      || g_file_attribute_matcher_matches (matcher, G_FILE_ATTRIBUTE_THUMBNAILING_FAILED))
    {
      g_autofree gchar *path = NULL;

      g_file_info_remove_attribute (info, G_FILE_ATTRIBUTE_THUMBNAIL_IS_VALID);
      g_file_info_remove_attribute (info, G_FILE_ATTRIBUTE_THUMBNAIL_PATH);
      g_file_info_remove_attribute (info, G_FILE_ATTRIBUTE_THUMBNAILING_FAILED);

      path = photos_utils_get_thumbnail_path_for_file (file);
      if (g_file_test (path, G_FILE_TEST_IS_REGULAR))
        {
          g_file_info_set_attribute_boolean (info, G_FILE_ATTRIBUTE_THUMBNAIL_IS_VALID, TRUE);
          g_file_info_set_attribute_byte_string (info, G_FILE_ATTRIBUTE_THUMBNAIL_PATH, path);
          g_file_info_set_attribute_boolean (info, G_FILE_ATTRIBUTE_THUMBNAILING_FAILED, FALSE);
        }
    }

  ret_val = g_object_ref (info);

 out:
  g_clear_pointer (&matcher, (GDestroyNotify) g_file_attribute_matcher_unref);
  return ret_val;
}


static void
photos_utils_file_query_info_data_free (PhotosUtilsFileQueryInfoData *data)
{
  g_free (data->attributes);
  g_slice_free (PhotosUtilsFileQueryInfoData, data);
}


static PhotosUtilsFileQueryInfoData *
photos_utils_file_query_info_data_new (const gchar *attributes, GFileQueryInfoFlags flags)
{
  PhotosUtilsFileQueryInfoData *data;

  data = g_slice_new0 (PhotosUtilsFileQueryInfoData);
  data->flags = flags;
  data->attributes = g_strdup (attributes);

  return data;
}


static void
photos_utils_file_query_info_in_thread_func (GTask *task,
                                             gpointer source_object,
                                             gpointer task_data,
                                             GCancellable *cancellable)
{
  GError *error;
  GFile *file = G_FILE (source_object);
  g_autoptr (GFileInfo) info = NULL;
  PhotosUtilsFileQueryInfoData *data = (PhotosUtilsFileQueryInfoData *) task_data;

  error = NULL;
  info = photos_utils_file_query_info (file, data->attributes, data->flags, cancellable, &error);
  if (error != NULL)
    {
      g_task_return_error (task, error);
      goto out;
    }

  g_task_return_pointer (task, g_object_ref (info), g_object_unref);

 out:
  return;
}


void
photos_utils_file_query_info_async (GFile *file,
                                    const gchar *attributes,
                                    GFileQueryInfoFlags flags,
                                    gint io_priority,
                                    GCancellable *cancellable,
                                    GAsyncReadyCallback callback,
                                    gpointer user_data)
{
  g_autoptr (GTask) task = NULL;
  PhotosUtilsFileQueryInfoData *data;
  const gchar *wildcard;

  g_return_if_fail (G_IS_FILE (file));
  g_return_if_fail (attributes != NULL && attributes[0] != '\0');
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  wildcard = strstr (attributes, "*");
  g_return_if_fail (wildcard == NULL);

  data = photos_utils_file_query_info_data_new (attributes, flags);

  task = g_task_new (file, cancellable, callback, user_data);
  g_task_set_priority (task, io_priority);
  g_task_set_source_tag (task, photos_utils_file_query_info_async);
  g_task_set_task_data (task, data, (GDestroyNotify) photos_utils_file_query_info_data_free);

  g_task_run_in_thread (task, photos_utils_file_query_info_in_thread_func);
}


GFileInfo *
photos_utils_file_query_info_finish (GFile *file, GAsyncResult *res, GError **error)
{
  GTask *task = G_TASK (res);

  g_return_val_if_fail (g_task_is_valid (res, file), NULL);
  g_return_val_if_fail (g_task_get_source_tag (task) == photos_utils_file_query_info_async, NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  return g_task_propagate_pointer (task, error);
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


gchar *
photos_utils_get_thumbnail_path_for_file (GFile *file)
{
  gchar *path;
  g_autofree gchar *uri = NULL;

  uri = g_file_get_uri (file);
  path = photos_utils_get_thumbnail_path_for_uri (uri);

  return path;
}


gchar *
photos_utils_get_thumbnail_path_for_uri (const gchar *uri)
{
  const gchar *cache_dir;
  g_autofree gchar *filename = NULL;
  g_autofree gchar *md5 = NULL;
  gchar *path;
  g_autofree gchar *thumbnails_subdir = NULL;
  gint size;

  md5 = g_compute_checksum_for_string (G_CHECKSUM_MD5, uri, -1);
  filename = g_strconcat (md5, ".png", NULL);

  cache_dir = g_get_user_cache_dir ();
  size = photos_utils_get_icon_size ();
  thumbnails_subdir = g_strdup_printf ("%d-%d", size, THUMBNAIL_GENERATION);

  path = g_build_filename (cache_dir,
                           PACKAGE_TARNAME,
                           "thumbnails",
                           thumbnails_subdir,
                           filename,
                           NULL);

  return path;
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

  sparql = g_strdup_printf ("INSERT OR REPLACE { <%s> nie:title \"%s\" }", urn, title);
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

  sparql = g_strdup_printf ("%s { <%s> nao:hasTag nao:predefined-tag-favorite }",
                            (is_favorite) ? "INSERT OR REPLACE" : "DELETE",
                            urn);
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
