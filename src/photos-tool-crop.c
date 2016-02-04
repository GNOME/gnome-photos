/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2015, 2016 Red Hat, Inc.
 * Copyright © 2015 Umang Jain
 * Copyright © 2011, 2012, 2013, 2014, 2015 Yorba Foundation
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
 *   + Shotwell
 */


#include "config.h"

#include <math.h>

#include <cairo.h>
#include <gdk/gdk.h>
#include <gio/gio.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "gegl-gtk-view.h"
#include "photos-icons.h"
#include "photos-tool.h"
#include "photos-tool-crop.h"
#include "photos-utils.h"


typedef enum
{
  PHOTOS_TOOL_CROP_LOCATION_NONE,
  PHOTOS_TOOL_CROP_LOCATION_BOTTOM_LEFT,
  PHOTOS_TOOL_CROP_LOCATION_BOTTOM_RIGHT,
  PHOTOS_TOOL_CROP_LOCATION_BOTTOM_SIDE,
  PHOTOS_TOOL_CROP_LOCATION_CENTER,
  PHOTOS_TOOL_CROP_LOCATION_LEFT_SIDE,
  PHOTOS_TOOL_CROP_LOCATION_RIGHT_SIDE,
  PHOTOS_TOOL_CROP_LOCATION_TOP_LEFT,
  PHOTOS_TOOL_CROP_LOCATION_TOP_RIGHT,
  PHOTOS_TOOL_CROP_LOCATION_TOP_SIDE
} PhotosToolCropLocation;

struct _PhotosToolCrop
{
  PhotosTool parent_instance;
  GAction *crop;
  GeglRectangle bbox_zoomed;
  GeglRectangle bbox_source;
  GtkListStore *model;
  GtkWidget *box;
  GtkWidget *combo_box;
  GtkWidget *lock_button;
  GtkWidget *reset_button;
  GtkWidget *revealer;
  GtkWidget *view;
  PhotosToolCropLocation location;
  cairo_surface_t *surface;
  gboolean activated;
  gboolean grabbed;
  gboolean reset;
  gdouble crop_aspect_ratio;
  gdouble crop_height;
  gdouble crop_width;
  gdouble crop_x;
  gdouble crop_y;
  gdouble event_x_last;
  gdouble event_y_last;
  gint combo_box_active;
  gulong size_allocate_id;
};

struct _PhotosToolCropClass
{
  PhotosToolClass parent_class;
};


G_DEFINE_TYPE_WITH_CODE (PhotosToolCrop, photos_tool_crop, PHOTOS_TYPE_TOOL,
                         photos_utils_ensure_extension_points ();
                         g_io_extension_point_implement (PHOTOS_TOOL_EXTENSION_POINT_NAME,
                                                         g_define_type_id,
                                                         "crop",
                                                         100));


typedef enum
{
  PHOTOS_TOOL_CROP_ASPECT_RATIO_ANY,
  PHOTOS_TOOL_CROP_ASPECT_RATIO_BASIS,
  PHOTOS_TOOL_CROP_ASPECT_RATIO_ORIGINAL,
  PHOTOS_TOOL_CROP_ASPECT_RATIO_SCREEN
} PhotosToolCropAspectRatioType;

typedef struct _PhotosToolCropConstraint PhotosToolCropConstraint;

struct _PhotosToolCropConstraint
{
  PhotosToolCropAspectRatioType aspect_ratio_type;
  const gchar *name;
  guint basis_height;
  guint basis_width;
};

enum
{
  CONSTRAINT_COLUMN_ASPECT_RATIO = 0,
  CONSTRAINT_COLUMN_NAME = 1,
  CONSTRAINT_COLUMN_BASIS_HEIGHT = 2,
  CONSTRAINT_COLUMN_BASIS_WIDTH = 3
};

/* "Free" is excluded from the GtkComboBox and represented by the
 *  GtkCheckButton. Adjust accordingly.
 */
static PhotosToolCropConstraint CONSTRAINTS[] =
{
  { PHOTOS_TOOL_CROP_ASPECT_RATIO_ANY, N_("Free"), 0, 0 },
  { PHOTOS_TOOL_CROP_ASPECT_RATIO_ORIGINAL, N_("Original Size"), 0, 0 },
  { PHOTOS_TOOL_CROP_ASPECT_RATIO_SCREEN, N_("Screen"), 0, 0 },
  { PHOTOS_TOOL_CROP_ASPECT_RATIO_BASIS, N_("Golden Cut"), 100000, 161803},
  { PHOTOS_TOOL_CROP_ASPECT_RATIO_BASIS, N_("Square"), 1, 1 },
  { PHOTOS_TOOL_CROP_ASPECT_RATIO_BASIS, N_("A3 (297 × 420 mm)"), 297, 420 },
  { PHOTOS_TOOL_CROP_ASPECT_RATIO_BASIS, N_("A4 (210 × 297 mm)"), 297, 210 }
};

static const gdouble CROP_MIN_SIZE = 16.0;
static const gdouble HANDLE_OFFSET = 3.0;
static const gdouble HANDLE_RADIUS = 8.0;


static gdouble
photos_tool_crop_calculate_aspect_ratio (PhotosToolCrop *self, guint constraint)
{
  gdouble ret_val = 1.0;

  switch (CONSTRAINTS[constraint].aspect_ratio_type)
    {
    case PHOTOS_TOOL_CROP_ASPECT_RATIO_ANY:
      ret_val = -1.0;
      break;

    case PHOTOS_TOOL_CROP_ASPECT_RATIO_BASIS:
      ret_val = (gdouble) CONSTRAINTS[constraint].basis_width / CONSTRAINTS[constraint].basis_height;
      break;

    case PHOTOS_TOOL_CROP_ASPECT_RATIO_ORIGINAL:
      ret_val = (gdouble) self->bbox_source.width / self->bbox_source.height;
      break;

    case PHOTOS_TOOL_CROP_ASPECT_RATIO_SCREEN:
      {
        GdkScreen *screen;
        gint height;
        gint width;

        screen = gdk_screen_get_default ();
        height = gdk_screen_get_height (screen);
        width = gdk_screen_get_width (screen);
        ret_val = (gdouble) width / height;
        break;
      }

    default:
      g_assert_not_reached ();
    }

  return ret_val;
}


static guint
photos_tool_crop_find_constraint (PhotosToolCrop *self, gdouble aspect_ratio)
{
  guint i;
  guint ret_val = 0; /* ANY */

  for (i = 0; i < G_N_ELEMENTS (CONSTRAINTS); i++)
    {
      gdouble constraint_aspect_ratio;

      constraint_aspect_ratio = photos_tool_crop_calculate_aspect_ratio (self, i);
      if (photos_utils_equal_double (aspect_ratio, constraint_aspect_ratio))
        {
          ret_val = i;
          break;
        }
    }

  return ret_val;
}


static void
photos_tool_crop_redraw_damaged_area (PhotosToolCrop *self)
{
  cairo_rectangle_int_t area;
  cairo_region_t *region;
  gdouble damage_offset = HANDLE_OFFSET + HANDLE_RADIUS;
  gdouble x;
  gdouble y;

  x = (gdouble) gegl_gtk_view_get_x (GEGL_GTK_VIEW (self->view));
  x = -x + self->crop_x - damage_offset;

  y = (gdouble) gegl_gtk_view_get_y (GEGL_GTK_VIEW (self->view));
  y = -y + self->crop_y - damage_offset;

  area.height = (gint) (self->crop_height + 2 * damage_offset + 0.5) + 2;
  area.width = (gint) (self->crop_width + 2 * damage_offset + 0.5) + 2;
  area.x = (gint) (x + 0.5) - 1;
  area.y = (gint) (y + 0.5) - 1;

  region = cairo_region_create_rectangle (&area);
  gtk_widget_queue_draw_region (self->view, region);
  cairo_region_destroy (region);
}


static void
photos_tool_crop_surface_create (PhotosToolCrop *self)
{
  GdkWindow *window;
  gfloat zoom;

  g_clear_pointer (&self->surface, (GDestroyNotify) cairo_surface_destroy);

  window = gtk_widget_get_window (self->view);
  zoom = gegl_gtk_view_get_zoom (GEGL_GTK_VIEW (self->view));
  self->bbox_zoomed.height = (gint) (zoom * self->bbox_source.height + 0.5);
  self->bbox_zoomed.width = (gint) (zoom * self->bbox_source.width + 0.5);
  self->surface = gdk_window_create_similar_surface (window,
                                                     CAIRO_CONTENT_COLOR_ALPHA,
                                                     self->bbox_zoomed.width,
                                                     self->bbox_zoomed.height);
}


static void
photos_tool_crop_surface_draw (PhotosToolCrop *self)
{
  cairo_t *cr;

  cr = cairo_create (self->surface);
  cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
  cairo_set_source_rgba (cr, 0.0, 0.0, 0.0, 0.5);
  cairo_paint (cr);

  cairo_set_source_rgba (cr, 0.0, 0.0, 0.0, 0.0);
  cairo_rectangle (cr, self->crop_x, self->crop_y, self->crop_width, self->crop_height);
  cairo_fill (cr);

  cairo_set_operator (cr, CAIRO_OPERATOR_OVER);

  cairo_set_source_rgba (cr, 0.25, 0.507, 0.828, 1.0);
  photos_utils_draw_rectangle_handles (cr,
                                       self->crop_x,
                                       self->crop_y,
                                       self->crop_width,
                                       self->crop_height,
                                       HANDLE_OFFSET,
                                       HANDLE_RADIUS);

  cairo_set_source_rgba (cr, 0.8, 0.8, 0.8, 1.0);
  cairo_set_line_width (cr, 0.5);
  photos_utils_draw_rectangle_thirds (cr, self->crop_x, self->crop_y, self->crop_width, self->crop_height);

  cairo_destroy (cr);
}


static void
photos_tool_crop_change_constraint (PhotosToolCrop *self)
{
  gdouble crop_center_x;
  gdouble crop_center_y;
  gdouble old_area;

  photos_tool_crop_redraw_damaged_area (self);

  crop_center_x = self->crop_x + self->crop_width / 2.0;
  crop_center_y = self->crop_y + self->crop_height / 2.0;
  old_area = self->crop_height * self->crop_width;

  self->crop_height = sqrt (old_area / self->crop_aspect_ratio);
  self->crop_width = sqrt (old_area * self->crop_aspect_ratio);
  self->crop_x = crop_center_x - self->crop_width / 2.0;
  self->crop_y = crop_center_y - self->crop_height / 2.0;

  photos_tool_crop_surface_draw (self);
  photos_tool_crop_redraw_damaged_area (self);
}


static gint
photos_tool_crop_get_active (PhotosToolCrop *self)
{
  gint active;

  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->lock_button)))
    {
      active = self->combo_box_active + 1;
      g_return_val_if_fail (active >= 1, -1);
    }
  else
    active = 0;

  return active;
}


static void
photos_tool_crop_init_crop (PhotosToolCrop *self)
{
  gdouble aspect_ratio;
  gint active;

  active = photos_tool_crop_get_active (self);
  g_return_if_fail (active >= 0);

  self->crop_aspect_ratio = photos_tool_crop_calculate_aspect_ratio (self, (guint) active);
  if (self->crop_aspect_ratio < 0.0)
    goto out;

  aspect_ratio = (gdouble) self->bbox_source.width / self->bbox_source.height;

  if (self->crop_aspect_ratio < aspect_ratio)
    {
      self->crop_height = 0.7 * self->bbox_zoomed.height;
      self->crop_width = self->crop_height * self->crop_aspect_ratio;
    }
  else
    {
      self->crop_width = 0.7 * self->bbox_zoomed.width;
      self->crop_height = self->crop_width / self->crop_aspect_ratio;
    }

  self->crop_x = ((gdouble) self->bbox_zoomed.width - self->crop_width) / 2.0;
  self->crop_y = ((gdouble) self->bbox_zoomed.height - self->crop_height) / 2.0;

 out:
  photos_tool_crop_surface_draw (self);
}


static void
photos_tool_crop_set_horiz_from_vert (PhotosToolCrop *self, gboolean centered, gboolean move_origin)
{
  gdouble crop_width_new;

  crop_width_new = self->crop_height * self->crop_aspect_ratio;

  if (move_origin)
    {
      gdouble x_offset;

      if (centered)
        x_offset = (crop_width_new - self->crop_width) / 2.0;
      else
        x_offset = crop_width_new - self->crop_width;

      self->crop_x -= x_offset;
    }

  self->crop_width = crop_width_new;
}


static void
photos_tool_crop_set_vert_from_horiz (PhotosToolCrop *self, gboolean centered, gboolean move_origin)
{
  gdouble crop_height_new;

  crop_height_new = self->crop_width / self->crop_aspect_ratio;

  if (move_origin)
    {
      gdouble y_offset;

      if (centered)
        y_offset = (crop_height_new - self->crop_height) / 2.0;
      else
        y_offset = crop_height_new - self->crop_height;

      self->crop_y -= y_offset;
    }

  self->crop_height = crop_height_new;
}


static void
photos_tool_crop_set_crop (PhotosToolCrop *self, gdouble event_x, gdouble event_y)
{
  PhotosToolCropLocation location;
  gboolean centered;
  gboolean changed = TRUE;
  gboolean move_origin;
  gdouble crop_center_x;
  gdouble crop_center_y;
  gdouble crop_height_old;
  gdouble crop_width_old;
  gdouble crop_x_old;
  gdouble crop_y_old;
  gdouble delta_x;
  gdouble delta_y;

  photos_tool_crop_redraw_damaged_area (self);

  crop_height_old = self->crop_height;
  crop_width_old = self->crop_width;
  crop_x_old = self->crop_x;
  crop_y_old = self->crop_y;

  crop_center_x = self->crop_x + self->crop_width / 2.0;
  crop_center_y = self->crop_y + self->crop_height / 2.0;

  delta_x = event_x - self->event_x_last;
  delta_y = event_y - self->event_y_last;

  self->event_x_last = event_x;
  self->event_y_last = event_y;

  switch (self->location)
    {
    case PHOTOS_TOOL_CROP_LOCATION_BOTTOM_SIDE:
    case PHOTOS_TOOL_CROP_LOCATION_LEFT_SIDE:
    case PHOTOS_TOOL_CROP_LOCATION_RIGHT_SIDE:
    case PHOTOS_TOOL_CROP_LOCATION_TOP_SIDE:
      location = self->location;
      centered = TRUE;
      move_origin = TRUE;
      break;

    case PHOTOS_TOOL_CROP_LOCATION_CENTER:
      location = self->location;
      break;

    case PHOTOS_TOOL_CROP_LOCATION_BOTTOM_LEFT:
      if (self->crop_aspect_ratio < 0.0)
        {
          self->crop_height = event_y - self->crop_y;
          self->crop_width -= event_x - self->crop_x;
          self->crop_x = event_x;
          location = PHOTOS_TOOL_CROP_LOCATION_NONE;
        }
      else
        {
          if (event_y < photos_utils_eval_radial_line (crop_center_x,
                                                       crop_center_y,
                                                       self->crop_x,
                                                       self->crop_y + self->crop_height,
                                                       event_x))
            {
              location = PHOTOS_TOOL_CROP_LOCATION_LEFT_SIDE;
              move_origin = FALSE;
            }
          else
            {
              location = PHOTOS_TOOL_CROP_LOCATION_BOTTOM_SIDE;
              move_origin = TRUE;
            }

          centered = FALSE;
        }
      break;

    case PHOTOS_TOOL_CROP_LOCATION_BOTTOM_RIGHT:
      if (self->crop_aspect_ratio < 0.0)
        {
          self->crop_height = event_y - self->crop_y;
          self->crop_width = event_x - self->crop_x;
          location = PHOTOS_TOOL_CROP_LOCATION_NONE;
        }
      else
        {
          if (event_y < photos_utils_eval_radial_line (crop_center_x,
                                                       crop_center_y,
                                                       self->crop_x + self->crop_width,
                                                       self->crop_y + self->crop_height,
                                                       event_x))
            location = PHOTOS_TOOL_CROP_LOCATION_RIGHT_SIDE;
          else
            location = PHOTOS_TOOL_CROP_LOCATION_BOTTOM_SIDE;

          centered = FALSE;
          move_origin = FALSE;
        }
      break;

    case PHOTOS_TOOL_CROP_LOCATION_TOP_LEFT:
      if (self->crop_aspect_ratio < 0.0)
        {
          self->crop_height -= event_y - self->crop_y;
          self->crop_width -= event_x - self->crop_x;
          self->crop_x = event_x;
          self->crop_y = event_y;
          location = PHOTOS_TOOL_CROP_LOCATION_NONE;
        }
      else
        {
          if (event_y < photos_utils_eval_radial_line (crop_center_x,
                                                       crop_center_y,
                                                       self->crop_x,
                                                       self->crop_y,
                                                       event_x))
            location = PHOTOS_TOOL_CROP_LOCATION_TOP_SIDE;
          else

            location = PHOTOS_TOOL_CROP_LOCATION_LEFT_SIDE;

          centered = FALSE;
          move_origin = TRUE;
        }
      break;

    case PHOTOS_TOOL_CROP_LOCATION_TOP_RIGHT:
      if (self->crop_aspect_ratio < 0.0)
        {
          self->crop_height -= event_y - self->crop_y;
          self->crop_width = event_x - self->crop_x;
          self->crop_y = event_y;
          location = PHOTOS_TOOL_CROP_LOCATION_NONE;
        }
      else
        {
          if (event_y < photos_utils_eval_radial_line (crop_center_x,
                                                       crop_center_y,
                                                       self->crop_x + self->crop_width,
                                                       self->crop_y,
                                                       event_x))
            {
              location = PHOTOS_TOOL_CROP_LOCATION_TOP_SIDE;
              move_origin = FALSE;
            }
          else
            {
              location = PHOTOS_TOOL_CROP_LOCATION_RIGHT_SIDE;
              move_origin = TRUE;
            }

          centered = FALSE;
        }
      break;

    case PHOTOS_TOOL_CROP_LOCATION_NONE:
    default:
      g_assert_not_reached ();
      break;
    }

  if (location == PHOTOS_TOOL_CROP_LOCATION_NONE)
    goto check_bounds;

  switch (location)
    {
    case PHOTOS_TOOL_CROP_LOCATION_BOTTOM_SIDE:
      self->crop_height = event_y - self->crop_y;
      if (self->crop_aspect_ratio > 0.0)
        photos_tool_crop_set_horiz_from_vert (self, centered, move_origin);
      break;

    case PHOTOS_TOOL_CROP_LOCATION_CENTER:
      {
        gboolean x_adj = FALSE;
        gboolean y_adj = FALSE;
        gdouble bbox_zoomed_height = (gdouble) self->bbox_zoomed.height;
        gdouble bbox_zoomed_width = (gdouble) self->bbox_zoomed.width;

        self->crop_x += delta_x;
        self->crop_y += delta_y;

        if (self->crop_x < 0.0)
          {
            self->crop_width += self->crop_x;
            self->crop_x = 0.0;
            x_adj = TRUE;
          }

        if (self->crop_y < 0.0)
          {
            self->crop_height += self->crop_y;
            self->crop_y = 0.0;
            y_adj = TRUE;
          }

        if (self->crop_x + self->crop_width > bbox_zoomed_width)
          {
            self->crop_width = bbox_zoomed_width - self->crop_x;
            x_adj = TRUE;
          }

        if (self->crop_y + self->crop_height > bbox_zoomed_height)
          {
            self->crop_height = bbox_zoomed_height - self->crop_y;
            y_adj = TRUE;
          }

        if (x_adj)
          {
            if (delta_x < 0.0)
              self->crop_width = crop_width_old;
            else
              {
                self->crop_x = bbox_zoomed_width - crop_width_old;
                self->crop_width = crop_width_old;
              }
          }

        if (y_adj)
          {
            if (delta_y < 0.0)
              self->crop_height = crop_height_old;
            else
              {
                self->crop_y = bbox_zoomed_height - crop_height_old;
                self->crop_height = crop_height_old;
              }
          }
      }

      break;

    case PHOTOS_TOOL_CROP_LOCATION_LEFT_SIDE:
      self->crop_width -= event_x - self->crop_x;
      self->crop_x = event_x;
      if (self->crop_aspect_ratio > 0.0)
        photos_tool_crop_set_vert_from_horiz (self, centered, move_origin);
      break;

    case PHOTOS_TOOL_CROP_LOCATION_RIGHT_SIDE:
      self->crop_width = event_x - self->crop_x;
      if (self->crop_aspect_ratio > 0.0)
        photos_tool_crop_set_vert_from_horiz (self, centered, move_origin);
      break;

    case PHOTOS_TOOL_CROP_LOCATION_TOP_SIDE:
      self->crop_height -= event_y - self->crop_y;
      self->crop_y = event_y;
      if (self->crop_aspect_ratio > 0.0)
        photos_tool_crop_set_horiz_from_vert (self, centered, move_origin);
      break;

    case PHOTOS_TOOL_CROP_LOCATION_NONE:
    case PHOTOS_TOOL_CROP_LOCATION_BOTTOM_LEFT:
    case PHOTOS_TOOL_CROP_LOCATION_BOTTOM_RIGHT:
    case PHOTOS_TOOL_CROP_LOCATION_TOP_LEFT:
    case PHOTOS_TOOL_CROP_LOCATION_TOP_RIGHT:
    default:
      g_assert_not_reached ();
      break;
    }

  if (location == PHOTOS_TOOL_CROP_LOCATION_CENTER)
    goto out;

 check_bounds:
  if (self->crop_height < CROP_MIN_SIZE
      || self->crop_width < CROP_MIN_SIZE
      || self->crop_x < 0.0
      || self->crop_x + self->crop_width > self->bbox_zoomed.width
      || self->crop_y < 0.0
      || self->crop_y + self->crop_height > self->bbox_zoomed.height)
    {
      self->crop_height = crop_height_old;
      self->crop_width = crop_width_old;
      self->crop_x = crop_x_old;
      self->crop_y = crop_y_old;
      changed = FALSE;
    }

 out:
  if (changed)
    {
      photos_tool_crop_surface_draw (self);
      photos_tool_crop_redraw_damaged_area (self);
    }
}


static void
photos_tool_crop_set_cursor (PhotosToolCrop *self)
{
  GdkCursor *cursor = NULL;
  GdkCursorType cursor_type;
  GdkDisplay *display;
  GdkWindow *window;

  window = gtk_widget_get_window (self->view);

  switch (self->location)
    {
    case PHOTOS_TOOL_CROP_LOCATION_NONE:
      goto set_cursor;

    case PHOTOS_TOOL_CROP_LOCATION_BOTTOM_LEFT:
      cursor_type = GDK_BOTTOM_LEFT_CORNER;
      break;

    case PHOTOS_TOOL_CROP_LOCATION_BOTTOM_RIGHT:
      cursor_type = GDK_BOTTOM_RIGHT_CORNER;
      break;

    case PHOTOS_TOOL_CROP_LOCATION_BOTTOM_SIDE:
      cursor_type = GDK_BOTTOM_SIDE;
      break;

    case PHOTOS_TOOL_CROP_LOCATION_CENTER:
      cursor_type = GDK_FLEUR;
      break;

    case PHOTOS_TOOL_CROP_LOCATION_LEFT_SIDE:
      cursor_type = GDK_LEFT_SIDE;
      break;

    case PHOTOS_TOOL_CROP_LOCATION_RIGHT_SIDE:
      cursor_type = GDK_RIGHT_SIDE;
      break;

    case PHOTOS_TOOL_CROP_LOCATION_TOP_LEFT:
      cursor_type = GDK_TOP_LEFT_CORNER;
      break;

    case PHOTOS_TOOL_CROP_LOCATION_TOP_RIGHT:
      cursor_type = GDK_TOP_RIGHT_CORNER;
      break;

    case PHOTOS_TOOL_CROP_LOCATION_TOP_SIDE:
      cursor_type = GDK_TOP_SIDE;
      break;

    default:
      g_assert_not_reached ();
      break;
    }

  display = gdk_window_get_display (window);
  cursor = gdk_cursor_new_for_display (display, cursor_type);

 set_cursor:
  gdk_window_set_cursor (window, cursor);
  g_clear_object (&cursor);
}


static void
photos_tool_crop_set_location (PhotosToolCrop *self, gdouble event_x, gdouble event_y)
{
  const gdouble edge_fuzz = 12.0;

  self->location = PHOTOS_TOOL_CROP_LOCATION_NONE;

  if (event_x > self->crop_x - edge_fuzz
      && event_y > self->crop_y - edge_fuzz
      && event_x < self->crop_x + self->crop_width + edge_fuzz
      && event_y < self->crop_y + self->crop_height + edge_fuzz)
    {
      if (event_x < self->crop_x + edge_fuzz && event_y < self->crop_y + edge_fuzz)
        self->location = PHOTOS_TOOL_CROP_LOCATION_TOP_LEFT;
      else if (event_x > self->crop_x + self->crop_width - edge_fuzz && event_y < self->crop_y + edge_fuzz)
        self->location = PHOTOS_TOOL_CROP_LOCATION_TOP_RIGHT;
      else if (event_x > self->crop_x + self->crop_width - edge_fuzz
               && event_y > self->crop_y + self->crop_height - edge_fuzz)
        self->location = PHOTOS_TOOL_CROP_LOCATION_BOTTOM_RIGHT;
      else if (event_x < self->crop_x + edge_fuzz && event_y > self->crop_y + self->crop_height - edge_fuzz)
        self->location = PHOTOS_TOOL_CROP_LOCATION_BOTTOM_LEFT;
      else if (event_y < self->crop_y + edge_fuzz)
        self->location = PHOTOS_TOOL_CROP_LOCATION_TOP_SIDE;
      else if (event_x > self->crop_x + self->crop_width - edge_fuzz)
        self->location = PHOTOS_TOOL_CROP_LOCATION_RIGHT_SIDE;
      else if (event_y > self->crop_y + self->crop_height - edge_fuzz)
        self->location = PHOTOS_TOOL_CROP_LOCATION_BOTTOM_SIDE;
      else if (event_x < self->crop_x + edge_fuzz)
        self->location = PHOTOS_TOOL_CROP_LOCATION_LEFT_SIDE;
      else
        self->location = PHOTOS_TOOL_CROP_LOCATION_CENTER;
    }
}


static void
photos_tool_crop_active_changed (PhotosToolCrop *self)
{
  gint active;

  g_return_if_fail (!self->reset);

  active = photos_tool_crop_get_active (self);
  g_return_if_fail (active >= 0);

  self->crop_aspect_ratio = photos_tool_crop_calculate_aspect_ratio (self, (guint) active);
  if (self->crop_aspect_ratio < 0.0)
    return;

  photos_tool_crop_change_constraint (self);
}


static void
photos_tool_crop_combo_box_changed (PhotosToolCrop *self)
{
  self->combo_box_active = gtk_combo_box_get_active (GTK_COMBO_BOX (self->combo_box));
  photos_tool_crop_active_changed (self);
}


static void
photos_tool_crop_set_active (PhotosToolCrop *self, gint active)
{
  g_signal_handlers_block_by_func (self->combo_box, photos_tool_crop_combo_box_changed, self);
  g_signal_handlers_block_by_func (self->lock_button, photos_tool_crop_active_changed, self);

  if (active == -1) /* reset */
    {
      self->combo_box_active = 0;
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->lock_button), TRUE);
    }
  else if (active == 0)
    {
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->lock_button), FALSE);
    }
  else if (active > 0)
    {
      self->combo_box_active = active;
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->lock_button), TRUE);
    }
  else
    {
      g_warn_if_reached ();
    }

  gtk_combo_box_set_active (GTK_COMBO_BOX (self->combo_box), self->combo_box_active);

  g_signal_handlers_unblock_by_func (self->combo_box, photos_tool_crop_combo_box_changed, self);
  g_signal_handlers_unblock_by_func (self->lock_button, photos_tool_crop_active_changed, self);
}


static void
photos_tool_crop_process (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosToolCrop *self = PHOTOS_TOOL_CROP (user_data);
  GError *error = NULL;
  PhotosBaseItem *item = PHOTOS_BASE_ITEM (source_object);
  gfloat zoom;
  guint active;

  photos_base_item_process_finish (item, res, &error);
  if (error != NULL)
    {
      g_warning ("Unable to process item: %s", error->message);
      g_error_free (error);
      goto out;
    }

  photos_tool_crop_surface_create (self);

  zoom = gegl_gtk_view_get_zoom (GEGL_GTK_VIEW (self->view));
  self->crop_height *= zoom;
  self->crop_width *= zoom;
  self->crop_x *= zoom;
  self->crop_y *= zoom;

  self->crop_aspect_ratio = self->crop_width / self->crop_height;
  active = photos_tool_crop_find_constraint (self, self->crop_aspect_ratio);
  photos_tool_crop_set_active (self, (gint) active);

  photos_tool_crop_surface_draw (self);

 out:
  gtk_widget_queue_draw (self->view);
  g_object_unref (self);
}


static void
photos_tool_crop_reset_clicked (PhotosToolCrop *self)
{
  self->reset = TRUE;
  photos_tool_crop_set_active (self, -1);
  g_signal_emit_by_name (self, "hide-requested");
}


static void
photos_tool_crop_size_allocate (PhotosToolCrop *self, GdkRectangle *allocation)
{
  gdouble crop_height_ratio;
  gdouble crop_width_ratio;
  gdouble crop_x_ratio;
  gdouble crop_y_ratio;

  crop_height_ratio = self->crop_height / (gdouble) self->bbox_zoomed.height;
  crop_width_ratio = self->crop_width / (gdouble) self->bbox_zoomed.width;
  crop_x_ratio = self->crop_x / (gdouble) self->bbox_zoomed.width;
  crop_y_ratio = self->crop_y / (gdouble) self->bbox_zoomed.height;

  photos_tool_crop_surface_create (self);

  self->crop_height = crop_height_ratio * (gdouble) self->bbox_zoomed.height;
  self->crop_width = crop_width_ratio * (gdouble) self->bbox_zoomed.width;
  self->crop_x = crop_x_ratio * (gdouble) self->bbox_zoomed.width;
  self->crop_y = crop_y_ratio * (gdouble) self->bbox_zoomed.height;

  photos_tool_crop_surface_draw (self);
}


static void
photos_tool_crop_activate (PhotosTool *tool, PhotosBaseItem *item, GeglGtkView *view)
{
  PhotosToolCrop *self = PHOTOS_TOOL_CROP (tool);
  gboolean got_bbox_source;
  gdouble height = -1.0;
  gdouble width = -1.0;
  gdouble x = -1.0;
  gdouble y = -1.0;

  got_bbox_source = photos_base_item_get_bbox_source (item, &self->bbox_source);
  g_return_if_fail (got_bbox_source);

  self->view = GTK_WIDGET (view);

  if (photos_base_item_operation_get (item,
                                      "gegl:crop",
                                      "height", &height,
                                      "width", &width,
                                      "x", &x,
                                      "y", &y,
                                      NULL))
    {
      g_return_if_fail (height >= 0.0);
      g_return_if_fail (width >= 0.0);
      g_return_if_fail (x >= 0.0);
      g_return_if_fail (y >= 0.0);

      /* These values are invalid until they are multiplied by the
       * view's zoom, which we won't know until we have finished
       * processing with the reset gegl:crop values.
       */
      self->crop_height = height;
      self->crop_width = width;
      self->crop_x = x;
      self->crop_y = y;

      photos_base_item_operation_add (item,
                                      "gegl:crop",
                                      "height", (gdouble) self->bbox_source.height,
                                      "width", (gdouble) self->bbox_source.width,
                                      "x", (gdouble) self->bbox_source.x,
                                      "y", (gdouble) self->bbox_source.y,
                                      NULL);
      photos_base_item_process_async (item, NULL, photos_tool_crop_process, g_object_ref (self));
    }
  else
    {
      photos_tool_crop_surface_create (self);
      photos_tool_crop_init_crop (self);
    }

  self->size_allocate_id = g_signal_connect_object (self->view,
                                                    "size-allocate",
                                                    G_CALLBACK (photos_tool_crop_size_allocate),
                                                    self,
                                                    G_CONNECT_SWAPPED);

  self->activated = TRUE;
  self->reset = FALSE;
}


static void
photos_tool_crop_deactivate (PhotosTool *tool)
{
  PhotosToolCrop *self = PHOTOS_TOOL_CROP (tool);

  g_return_if_fail (self->activated);

  if (!self->reset)
    {
      GVariantBuilder parameter;
      GVariantType *parameter_type;
      gfloat zoom;

      zoom = gegl_gtk_view_get_zoom (GEGL_GTK_VIEW (self->view));

      /* GEGL needs the parameters to be in device pixels. So, we
       * should multiply the extents of the crop rectangle and the
       * zoom by the scale factor to convert them. However, the scale
       * factor will cancel itself in the numerator and denominator,
       * so, in practice, the conversion is unnecessary.
       */
      parameter_type = g_variant_type_new ("a{sd}");
      g_variant_builder_init (&parameter, parameter_type);
      g_variant_builder_add (&parameter, "{sd}", "height", self->crop_height / zoom);
      g_variant_builder_add (&parameter, "{sd}", "width", self->crop_width / zoom);
      g_variant_builder_add (&parameter, "{sd}", "x", self->crop_x / zoom);
      g_variant_builder_add (&parameter, "{sd}", "y", self->crop_y / zoom);
      g_action_activate (self->crop, g_variant_builder_end (&parameter));

      g_variant_type_free (parameter_type);
    }

  if (self->size_allocate_id != 0)
    {
      g_signal_handler_disconnect (self->view, self->size_allocate_id);
      self->size_allocate_id = 0;
    }

  self->view = NULL;
  self->activated = FALSE;
}


static void
photos_tool_crop_draw (PhotosTool *tool, cairo_t *cr, GdkRectangle *rect)
{
  PhotosToolCrop *self = PHOTOS_TOOL_CROP (tool);
  gdouble x;
  gdouble y;

  g_return_if_fail (self->activated);
  g_return_if_fail (self->view != NULL);

  x = (gdouble) gegl_gtk_view_get_x (GEGL_GTK_VIEW (self->view));
  x = -x;

  y = (gdouble) gegl_gtk_view_get_y (GEGL_GTK_VIEW (self->view));
  y = -y;

  cairo_save (cr);
  cairo_set_source_surface (cr, self->surface, x, y);
  cairo_paint (cr);
  cairo_restore (cr);
}


static GtkWidget *
photos_tool_crop_get_widget (PhotosTool *tool)
{
  PhotosToolCrop *self = PHOTOS_TOOL_CROP (tool);
  return self->box;
}


static gboolean
photos_tool_crop_left_click_event (PhotosTool *tool, GdkEventButton *event)
{
  PhotosToolCrop *self = PHOTOS_TOOL_CROP (tool);
  gdouble x;
  gdouble y;

  g_return_val_if_fail (self->activated, GDK_EVENT_PROPAGATE);
  g_return_val_if_fail (self->view != NULL, GDK_EVENT_PROPAGATE);

  self->grabbed = TRUE;

  x = (gdouble) gegl_gtk_view_get_x (GEGL_GTK_VIEW (self->view));
  y = (gdouble) gegl_gtk_view_get_y (GEGL_GTK_VIEW (self->view));
  self->event_x_last = event->x + x;
  self->event_y_last = event->y + y;

  return GDK_EVENT_PROPAGATE;
}


static gboolean
photos_tool_crop_left_unclick_event (PhotosTool *tool, GdkEventButton *event)
{
  PhotosToolCrop *self = PHOTOS_TOOL_CROP (tool);

  g_return_val_if_fail (self->activated, GDK_EVENT_PROPAGATE);
  g_return_val_if_fail (self->view != NULL, GDK_EVENT_PROPAGATE);

  self->grabbed = FALSE;
  self->event_x_last = -1.0;
  self->event_y_last = -1.0;

  return GDK_EVENT_PROPAGATE;
}


static gboolean
photos_tool_crop_motion_event (PhotosTool *tool, GdkEventMotion *event)
{
  PhotosToolCrop *self = PHOTOS_TOOL_CROP (tool);
  gdouble event_x;
  gdouble event_y;
  gdouble x;
  gdouble y;

  g_return_val_if_fail (self->activated, GDK_EVENT_PROPAGATE);
  g_return_val_if_fail (self->view != NULL, GDK_EVENT_PROPAGATE);

  x = (gdouble) gegl_gtk_view_get_x (GEGL_GTK_VIEW (self->view));
  y = (gdouble) gegl_gtk_view_get_y (GEGL_GTK_VIEW (self->view));
  event_x = event->x + x;
  event_y = event->y + y;

  if (self->grabbed)
    {
      if (self->location != PHOTOS_TOOL_CROP_LOCATION_NONE)
        photos_tool_crop_set_crop (self, event_x, event_y);
    }
  else
    {
      photos_tool_crop_set_location (self, event_x, event_y);
      photos_tool_crop_set_cursor (self);
    }

  return GDK_EVENT_STOP;
}


static void
photos_tool_crop_dispose (GObject *object)
{
  PhotosToolCrop *self = PHOTOS_TOOL_CROP (object);

  g_clear_object (&self->model);
  g_clear_object (&self->box);
  g_clear_pointer (&self->surface, (GDestroyNotify) cairo_surface_destroy);

  G_OBJECT_CLASS (photos_tool_crop_parent_class)->dispose (object);
}


static void
photos_tool_crop_init (PhotosToolCrop *self)
{
  GApplication *app;
  GtkCellRenderer *renderer;
  guint i;

  app = g_application_get_default ();
  self->crop = g_action_map_lookup_action (G_ACTION_MAP (app), "crop-current");

  self->model = gtk_list_store_new (4, G_TYPE_INT, G_TYPE_STRING, G_TYPE_UINT, G_TYPE_UINT);

  for (i = 1; i < G_N_ELEMENTS (CONSTRAINTS); i++)
    {
      GtkTreeIter iter;

      gtk_list_store_append (self->model, &iter);
      gtk_list_store_set (self->model,
                          &iter,
                          CONSTRAINT_COLUMN_ASPECT_RATIO, CONSTRAINTS[i].aspect_ratio_type,
                          CONSTRAINT_COLUMN_NAME, CONSTRAINTS[i].name,
                          CONSTRAINT_COLUMN_BASIS_HEIGHT, CONSTRAINTS[i].basis_height,
                          CONSTRAINT_COLUMN_BASIS_WIDTH, CONSTRAINTS[i].basis_width,
                          -1);
    }

  /* We really need a GtkBox here. A GtkGrid won't work because it
   * doesn't expand the children to fill the full width of the
   * palette.
   */
  self->box = g_object_ref_sink (gtk_box_new (GTK_ORIENTATION_VERTICAL, 12));

  self->lock_button = gtk_check_button_new_with_label (_("Lock aspect ratio"));
  gtk_container_add (GTK_CONTAINER (self->box), self->lock_button);
  g_signal_connect_swapped (self->lock_button, "toggled", G_CALLBACK (photos_tool_crop_active_changed), self);

  self->revealer = gtk_revealer_new ();
  gtk_container_add (GTK_CONTAINER (self->box), self->revealer);
  gtk_revealer_set_transition_type (GTK_REVEALER (self->revealer), GTK_REVEALER_TRANSITION_TYPE_SLIDE_DOWN);
  g_object_bind_property (self->lock_button, "active", self->revealer, "reveal-child", G_BINDING_DEFAULT);

  self->combo_box = gtk_combo_box_new_with_model (GTK_TREE_MODEL (self->model));
  gtk_container_add (GTK_CONTAINER (self->revealer), self->combo_box);
  g_signal_connect_swapped (self->combo_box, "changed", G_CALLBACK (photos_tool_crop_combo_box_changed), self);

  photos_tool_crop_set_active (self, -1);

  self->reset_button = gtk_button_new_with_label (_("Reset"));
  gtk_widget_set_halign (self->reset_button, GTK_ALIGN_END);
  gtk_container_add (GTK_CONTAINER (self->box), self->reset_button);
  g_signal_connect_swapped (self->reset_button, "clicked", G_CALLBACK (photos_tool_crop_reset_clicked), self);

  renderer = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (self->combo_box), renderer, TRUE);
  gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (self->combo_box), renderer, "text", CONSTRAINT_COLUMN_NAME);

  self->event_x_last = -1.0;
  self->event_y_last = -1.0;
}


static void
photos_tool_crop_class_init (PhotosToolCropClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  PhotosToolClass *tool_class = PHOTOS_TOOL_CLASS (class);

  tool_class->icon_name = PHOTOS_ICON_IMAGE_CROP_SYMBOLIC;
  tool_class->name = _("Crop");

  object_class->dispose = photos_tool_crop_dispose;
  tool_class->activate = photos_tool_crop_activate;
  tool_class->deactivate = photos_tool_crop_deactivate;
  tool_class->draw = photos_tool_crop_draw;
  tool_class->get_widget = photos_tool_crop_get_widget;
  tool_class->left_click_event = photos_tool_crop_left_click_event;
  tool_class->left_unclick_event = photos_tool_crop_left_unclick_event;
  tool_class->motion_event = photos_tool_crop_motion_event;
}
