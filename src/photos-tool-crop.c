/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2015 – 2017 Red Hat, Inc.
 * Copyright © 2015 – 2017 Umang Jain
 * Copyright © 2011 – 2015 Yorba Foundation
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

#include "photos-icons.h"
#include "photos-image-view.h"
#include "photos-tool.h"
#include "photos-tool-crop.h"
#include "photos-utils.h"


typedef enum
{
  PHOTOS_TOOL_CROP_ASPECT_RATIO_NONE,
  PHOTOS_TOOL_CROP_ASPECT_RATIO_ANY,
  PHOTOS_TOOL_CROP_ASPECT_RATIO_BASIS,
  PHOTOS_TOOL_CROP_ASPECT_RATIO_ORIGINAL
} PhotosToolCropAspectRatioType;

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

typedef struct _PhotosToolCropConstraint PhotosToolCropConstraint;

struct _PhotosToolCropConstraint
{
  PhotosToolCropAspectRatioType aspect_ratio_type;
  gboolean orientable;
  const gchar *name;
  guint basis_height;
  guint basis_width;
};

struct _PhotosToolCrop
{
  PhotosTool parent_instance;
  GAction *crop;
  GCancellable *cancellable;
  GeglRectangle bbox_zoomed;
  GeglRectangle bbox_source;
  GtkWidget *grid;
  GtkWidget *landscape_button;
  GtkWidget *list_box;
  GtkWidget *lock_button;
  GtkWidget *orientation_revealer;
  GtkWidget *portrait_button;
  GtkWidget *reset_button;
  GtkWidget *ratio_revealer;
  GtkWidget *view;
  PhotosToolCropConstraint *constraints;
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
  gint list_box_active;
  gulong size_allocate_id;
};


G_DEFINE_TYPE_WITH_CODE (PhotosToolCrop, photos_tool_crop, PHOTOS_TYPE_TOOL,
                         photos_utils_ensure_extension_points ();
                         g_io_extension_point_implement (PHOTOS_TOOL_EXTENSION_POINT_NAME,
                                                         g_define_type_id,
                                                         "crop",
                                                         100));


static const gdouble CROP_MIN_SIZE = 16.0;
static const gdouble HANDLE_OFFSET = 3.0;
static const gdouble HANDLE_RADIUS = 8.0;


static gdouble
photos_tool_crop_calculate_aspect_ratio (PhotosToolCrop *self, guint constraint)
{
  gdouble ret_val = 1.0;

  switch (self->constraints[constraint].aspect_ratio_type)
    {
    case PHOTOS_TOOL_CROP_ASPECT_RATIO_ANY:
      ret_val = -1.0;
      break;

    case PHOTOS_TOOL_CROP_ASPECT_RATIO_BASIS:
      ret_val = (gdouble) self->constraints[constraint].basis_width / self->constraints[constraint].basis_height;
      break;

    case PHOTOS_TOOL_CROP_ASPECT_RATIO_ORIGINAL:
      ret_val = (gdouble) self->bbox_source.width / self->bbox_source.height;
      if (ret_val < 1.0)
        ret_val = 1.0 / ret_val;
      break;

    case PHOTOS_TOOL_CROP_ASPECT_RATIO_NONE:
    default:
      g_assert_not_reached ();
      break;
    }

  return ret_val;
}


static gdouble
photos_tool_crop_calculate_aspect_ratio_with_orientation (PhotosToolCrop *self, guint constraint)
{
  gdouble aspect_ratio;

  aspect_ratio = photos_tool_crop_calculate_aspect_ratio (self, constraint);

  if (self->constraints[constraint].orientable)
    {
      if ((aspect_ratio < 1.0 && gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->landscape_button)))
          || (aspect_ratio > 1.0 && gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->portrait_button))))
        aspect_ratio = 1.0 / aspect_ratio;
    }

  return aspect_ratio;
}


static void
photos_tool_crop_create_constraints (PhotosToolCrop *self)
{
  /* "Free" is excluded from the GtkListBox and represented by the
   *  GtkCheckButton. Adjust accordingly.
   */
  PhotosToolCropConstraint constraints[] =
    {
      { PHOTOS_TOOL_CROP_ASPECT_RATIO_ANY, FALSE, N_("Free"), 0, 0 },
      { PHOTOS_TOOL_CROP_ASPECT_RATIO_ORIGINAL, TRUE, N_("Original"), 0, 0 },
      { PHOTOS_TOOL_CROP_ASPECT_RATIO_BASIS, FALSE, N_("1×1 (Square)"), 1, 1 },
      { PHOTOS_TOOL_CROP_ASPECT_RATIO_BASIS, TRUE, N_("10×8 / 5×4"), 4, 5 },
      { PHOTOS_TOOL_CROP_ASPECT_RATIO_BASIS, TRUE, N_("4×3 / 8×6 (1024×768)"), 3, 4 },
      { PHOTOS_TOOL_CROP_ASPECT_RATIO_BASIS, TRUE, N_("7×5"), 5, 7 },
      { PHOTOS_TOOL_CROP_ASPECT_RATIO_BASIS, TRUE, N_("3×2 / 6×4"), 2, 3 },
      { PHOTOS_TOOL_CROP_ASPECT_RATIO_BASIS, TRUE, N_("16×10 (1280×800)"), 10, 16 },
      { PHOTOS_TOOL_CROP_ASPECT_RATIO_BASIS, TRUE, N_("16×9 (1920×1080)"), 9, 16 },
      { PHOTOS_TOOL_CROP_ASPECT_RATIO_NONE, FALSE, NULL, 0, 0 }
    };
  guint i;

  self->constraints = g_malloc0_n (G_N_ELEMENTS (constraints), sizeof (PhotosToolCropConstraint));

  for (i = 0; i < G_N_ELEMENTS (constraints); i++)
    self->constraints[i] = constraints[i];
}


static guint
photos_tool_crop_find_constraint (PhotosToolCrop *self, gdouble aspect_ratio)
{
  guint i;
  guint ret_val = 0; /* ANY */

  for (i = 0; self->constraints[i].aspect_ratio_type != PHOTOS_TOOL_CROP_ASPECT_RATIO_NONE; i++)
    {
      gdouble constraint_aspect_ratio;

      constraint_aspect_ratio = photos_tool_crop_calculate_aspect_ratio (self, i);
      if (photos_utils_equal_double (aspect_ratio, constraint_aspect_ratio))
        {
          ret_val = i;
          break;
        }

      constraint_aspect_ratio = 1.0 / constraint_aspect_ratio;
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

  x = photos_image_view_get_x (PHOTOS_IMAGE_VIEW (self->view));
  x = -x + self->crop_x - damage_offset;

  y = photos_image_view_get_y (PHOTOS_IMAGE_VIEW (self->view));
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
  gdouble zoom;

  g_clear_pointer (&self->surface, (GDestroyNotify) cairo_surface_destroy);

  window = gtk_widget_get_window (self->view);
  zoom = photos_image_view_get_zoom (PHOTOS_IMAGE_VIEW (self->view));
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

  if (self->crop_height > self->bbox_zoomed.height)
    {
      self->crop_height = self->bbox_zoomed.height;
      self->crop_width = self->crop_height * self->crop_aspect_ratio;
    }

  if (self->crop_width > self->bbox_zoomed.width)
    {
      self->crop_width = self->bbox_zoomed.width;
      self->crop_height = self->crop_width / self->crop_aspect_ratio;
    }

  self->crop_x = crop_center_x - self->crop_width / 2.0;
  self->crop_x = CLAMP (self->crop_x,
                        0.0,
                        self->bbox_zoomed.width - self->crop_width);

  self->crop_y = crop_center_y - self->crop_height / 2.0;
  self->crop_y = CLAMP (self->crop_y,
                        0.0,
                        self->bbox_zoomed.height - self->crop_height);

  photos_tool_crop_surface_draw (self);
  photos_tool_crop_redraw_damaged_area (self);
}


static gint
photos_tool_crop_get_active (PhotosToolCrop *self)
{
  gint active;

  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->lock_button)))
    {
      active = self->list_box_active + 1;
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

  self->crop_aspect_ratio = photos_tool_crop_calculate_aspect_ratio_with_orientation (self, (guint) active);
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

  gtk_revealer_set_reveal_child (GTK_REVEALER (self->orientation_revealer), self->constraints[active].orientable);

  self->crop_aspect_ratio = photos_tool_crop_calculate_aspect_ratio_with_orientation (self, (guint) active);
  if (self->crop_aspect_ratio < 0.0)
    return;

  photos_tool_crop_change_constraint (self);
}


static void
photos_tool_crop_list_box_update (PhotosToolCrop *self, GtkListBoxRow *active_row)
{
  GtkListBoxRow *row;
  gint i;

  for (i = 0; (row = gtk_list_box_get_row_at_index (GTK_LIST_BOX (self->list_box), i)) != NULL; i++)
    {
      GtkWidget *image;

      image = GTK_WIDGET (g_object_get_data (G_OBJECT (row), "image"));
      gtk_widget_set_visible (image, row == active_row);
    }
}


static void
photos_tool_crop_list_box_row_activated (PhotosToolCrop *self, GtkListBoxRow *row)
{
  photos_tool_crop_list_box_update (self, row);

  self->list_box_active = gtk_list_box_row_get_index (GTK_LIST_BOX_ROW (row));
  photos_tool_crop_active_changed (self);
}


static void
photos_tool_crop_landscape_button_toggled (PhotosToolCrop *self)
{
  if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->landscape_button)))
    goto out;

  photos_tool_crop_active_changed (self);

 out:
  return;
}


static void
photos_tool_crop_portrait_button_toggled (PhotosToolCrop *self)
{
  if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->portrait_button)))
    goto out;

  photos_tool_crop_active_changed (self);

 out:
  return;
}


static void
photos_tool_crop_set_active (PhotosToolCrop *self, gint active, GtkToggleButton *orientation_button)
{
  GtkListBoxRow *row;

  g_return_if_fail ((active == -1 && orientation_button == NULL)
                    || (active != -1 && GTK_IS_TOGGLE_BUTTON (orientation_button)));

  g_signal_handlers_block_by_func (self->landscape_button, photos_tool_crop_landscape_button_toggled, self);
  g_signal_handlers_block_by_func (self->list_box, photos_tool_crop_list_box_row_activated, self);
  g_signal_handlers_block_by_func (self->lock_button, photos_tool_crop_active_changed, self);
  g_signal_handlers_block_by_func (self->portrait_button, photos_tool_crop_portrait_button_toggled, self);

  if (active == -1) /* reset */
    {
      self->list_box_active = 0;
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->lock_button), TRUE);
      active = 1;
      orientation_button = GTK_TOGGLE_BUTTON (self->landscape_button);
    }
  else if (active == 0)
    {
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->lock_button), FALSE);
    }
  else if (active > 0)
    {
      self->list_box_active = active - 1;
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->lock_button), TRUE);
    }
  else
    {
      g_warn_if_reached ();
    }

  row = gtk_list_box_get_row_at_index (GTK_LIST_BOX (self->list_box), self->list_box_active);
  photos_tool_crop_list_box_update (self, row);

  gtk_revealer_set_reveal_child (GTK_REVEALER (self->orientation_revealer), self->constraints[active].orientable);
  gtk_toggle_button_set_active (orientation_button, TRUE);

  g_signal_handlers_unblock_by_func (self->landscape_button, photos_tool_crop_landscape_button_toggled, self);
  g_signal_handlers_unblock_by_func (self->list_box, photos_tool_crop_list_box_row_activated, self);
  g_signal_handlers_unblock_by_func (self->lock_button, photos_tool_crop_active_changed, self);
  g_signal_handlers_unblock_by_func (self->portrait_button, photos_tool_crop_portrait_button_toggled, self);
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
photos_tool_crop_process (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosToolCrop *self;
  GError *error = NULL;
  GtkWidget *orientation_button;
  PhotosBaseItem *item = PHOTOS_BASE_ITEM (source_object);
  gdouble zoom;
  guint active;

  photos_base_item_operation_remove_finish (item, res, &error);
  if (error != NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Unable to process item: %s", error->message);
      g_error_free (error);
      return;
    }

  self = PHOTOS_TOOL_CROP (user_data);

  photos_tool_crop_surface_create (self);

  zoom = photos_image_view_get_zoom (PHOTOS_IMAGE_VIEW (self->view));
  self->crop_height *= zoom;
  self->crop_width *= zoom;
  self->crop_x *= zoom;
  self->crop_y *= zoom;

  self->crop_aspect_ratio = self->crop_width / self->crop_height;
  active = photos_tool_crop_find_constraint (self, self->crop_aspect_ratio);
  orientation_button = self->crop_aspect_ratio > 1.0 ? self->landscape_button : self->portrait_button;
  photos_tool_crop_set_active (self, (gint) active, GTK_TOGGLE_BUTTON (orientation_button));

  photos_tool_crop_surface_draw (self);
  gtk_widget_queue_draw (self->view);

  self->size_allocate_id = g_signal_connect_object (self->view,
                                                    "size-allocate",
                                                    G_CALLBACK (photos_tool_crop_size_allocate),
                                                    self,
                                                    G_CONNECT_SWAPPED);

  self->activated = TRUE;
  g_signal_emit_by_name (self, "activated");
}


static void
photos_tool_crop_reset_clicked (PhotosToolCrop *self)
{
  self->reset = TRUE;
  photos_tool_crop_set_active (self, -1, NULL);
  gtk_widget_queue_draw (self->view);
  g_signal_emit_by_name (self, "hide-requested");
}


static void
photos_tool_crop_update_original_orientable (PhotosToolCrop *self)
{
  guint i;

  for (i = 0; self->constraints[i].aspect_ratio_type != 0; i++)
    {
      if (self->constraints[i].aspect_ratio_type == PHOTOS_TOOL_CROP_ASPECT_RATIO_ORIGINAL)
        self->constraints[i].orientable = self->bbox_source.height != self->bbox_source.width;
    }
}


static void
photos_tool_crop_activate (PhotosTool *tool, PhotosBaseItem *item, PhotosImageView *view)
{
  PhotosToolCrop *self = PHOTOS_TOOL_CROP (tool);
  gboolean got_bbox_source;
  gdouble height = -1.0;
  gdouble width = -1.0;
  gdouble x = -1.0;
  gdouble y = -1.0;

  got_bbox_source = photos_base_item_get_bbox_source (item, &self->bbox_source);
  g_return_if_fail (got_bbox_source);

  self->reset = FALSE;
  self->view = GTK_WIDGET (view);
  photos_tool_crop_update_original_orientable (self);

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

      photos_base_item_operation_remove_async (item,
                                               "gegl:crop",
                                               self->cancellable,
                                               photos_tool_crop_process,
                                               self);
    }
  else
    {
      GtkWidget *orientation_button;
      gdouble aspect_ratio;

      photos_tool_crop_surface_create (self);
      photos_tool_crop_init_crop (self);

      aspect_ratio = (gdouble) self->bbox_source.width / (gdouble) self->bbox_source.height;
      orientation_button = aspect_ratio > 1.0 ? self->landscape_button : self->portrait_button;
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (orientation_button), TRUE);

      gtk_widget_queue_draw (self->view);

      self->size_allocate_id = g_signal_connect_object (self->view,
                                                        "size-allocate",
                                                        G_CALLBACK (photos_tool_crop_size_allocate),
                                                        self,
                                                        G_CONNECT_SWAPPED);

      self->activated = TRUE;
      g_signal_emit_by_name (self, "activated");
    }
}


static void
photos_tool_crop_deactivate (PhotosTool *tool)
{
  PhotosToolCrop *self = PHOTOS_TOOL_CROP (tool);

  g_return_if_fail (self->activated);

  if (!self->reset)
    {
      GVariantBuilder parameter;
      gdouble zoom;

      zoom = photos_image_view_get_zoom (PHOTOS_IMAGE_VIEW (self->view));

      /* GEGL needs the parameters to be in device pixels. So, we
       * should multiply the extents of the crop rectangle and the
       * zoom by the scale factor to convert them. However, the scale
       * factor will cancel itself in the numerator and denominator,
       * so, in practice, the conversion is unnecessary.
       */
      g_variant_builder_init (&parameter, G_VARIANT_TYPE ("a{sd}"));
      g_variant_builder_add (&parameter, "{sd}", "height", self->crop_height / zoom);
      g_variant_builder_add (&parameter, "{sd}", "width", self->crop_width / zoom);
      g_variant_builder_add (&parameter, "{sd}", "x", self->crop_x / zoom);
      g_variant_builder_add (&parameter, "{sd}", "y", self->crop_y / zoom);
      g_action_activate (self->crop, g_variant_builder_end (&parameter));
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

  x = photos_image_view_get_x (PHOTOS_IMAGE_VIEW (self->view));
  x = -x;

  y = photos_image_view_get_y (PHOTOS_IMAGE_VIEW (self->view));
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
  return self->grid;
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

  x = photos_image_view_get_x (PHOTOS_IMAGE_VIEW (self->view));
  y = photos_image_view_get_y (PHOTOS_IMAGE_VIEW (self->view));
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

  x = photos_image_view_get_x (PHOTOS_IMAGE_VIEW (self->view));
  y = photos_image_view_get_y (PHOTOS_IMAGE_VIEW (self->view));
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
photos_tool_crop_finalize (GObject *object)
{
  PhotosToolCrop *self = PHOTOS_TOOL_CROP (object);

  g_free (self->constraints);

  G_OBJECT_CLASS (photos_tool_crop_parent_class)->finalize (object);
}


static void
photos_tool_crop_dispose (GObject *object)
{
  PhotosToolCrop *self = PHOTOS_TOOL_CROP (object);

  if (self->cancellable != NULL)
    {
      g_cancellable_cancel (self->cancellable);
      g_clear_object (&self->cancellable);
    }

  g_clear_object (&self->grid);
  g_clear_pointer (&self->surface, (GDestroyNotify) cairo_surface_destroy);

  G_OBJECT_CLASS (photos_tool_crop_parent_class)->dispose (object);
}


static void
photos_tool_crop_init (PhotosToolCrop *self)
{
  GApplication *app;
  GtkSizeGroup *orientation_size_group = NULL;
  GtkStyleContext *context;
  GtkWidget *orientation_box;
  GtkWidget *ratio_grid;
  guint i;

  app = g_application_get_default ();
  self->crop = g_action_map_lookup_action (G_ACTION_MAP (app), "crop-current");

  self->cancellable = g_cancellable_new ();

  self->grid = g_object_ref_sink (gtk_grid_new ());
  gtk_orientable_set_orientation (GTK_ORIENTABLE (self->grid), GTK_ORIENTATION_VERTICAL);
  gtk_grid_set_row_spacing (GTK_GRID (self->grid), 12);

  self->lock_button = gtk_check_button_new_with_label (_("Lock aspect ratio"));
  gtk_container_add (GTK_CONTAINER (self->grid), self->lock_button);
  g_signal_connect_swapped (self->lock_button, "toggled", G_CALLBACK (photos_tool_crop_active_changed), self);

  self->ratio_revealer = gtk_revealer_new ();
  gtk_container_add (GTK_CONTAINER (self->grid), self->ratio_revealer);
  gtk_revealer_set_transition_type (GTK_REVEALER (self->ratio_revealer), GTK_REVEALER_TRANSITION_TYPE_SLIDE_DOWN);
  g_object_bind_property (self->lock_button, "active", self->ratio_revealer, "reveal-child", G_BINDING_DEFAULT);

  ratio_grid = gtk_grid_new ();
  gtk_orientable_set_orientation (GTK_ORIENTABLE (ratio_grid), GTK_ORIENTATION_VERTICAL);
  gtk_grid_set_row_spacing (GTK_GRID (ratio_grid), 12);
  gtk_container_add (GTK_CONTAINER (self->ratio_revealer), ratio_grid);

  self->list_box = gtk_list_box_new ();
  gtk_widget_set_hexpand (self->list_box, TRUE);
  gtk_list_box_set_selection_mode (GTK_LIST_BOX (self->list_box), GTK_SELECTION_NONE);
  gtk_container_add (GTK_CONTAINER (ratio_grid), self->list_box);
  g_signal_connect_swapped (self->list_box,
                            "row-activated",
                            G_CALLBACK (photos_tool_crop_list_box_row_activated),
                            self);

  photos_tool_crop_create_constraints (self);

  for (i = 1; self->constraints[i].aspect_ratio_type != PHOTOS_TOOL_CROP_ASPECT_RATIO_NONE; i++)
    {
      GtkWidget *grid;
      GtkWidget *image;
      GtkWidget *label;
      GtkWidget *row;

      row = gtk_list_box_row_new ();
      gtk_container_add (GTK_CONTAINER (self->list_box), row);

      grid = gtk_grid_new ();
      gtk_widget_set_hexpand (grid, FALSE);
      gtk_widget_set_margin_bottom (grid, 3);
      gtk_widget_set_margin_top (grid, 3);
      gtk_orientable_set_orientation (GTK_ORIENTABLE (grid), GTK_ORIENTATION_HORIZONTAL);
      gtk_grid_set_column_spacing (GTK_GRID (grid), 6);
      gtk_container_add (GTK_CONTAINER (row), grid);

      label = gtk_label_new (_(self->constraints[i].name));
      gtk_container_add (GTK_CONTAINER (grid), label);

      image = gtk_image_new_from_icon_name (PHOTOS_ICON_OBJECT_SELECT_SYMBOLIC, GTK_ICON_SIZE_INVALID);
      gtk_widget_set_no_show_all (image, TRUE);
      gtk_image_set_pixel_size (GTK_IMAGE (image), 16);
      gtk_container_add (GTK_CONTAINER (grid), image);

      g_object_set_data (G_OBJECT (row), "image", image);
    }

  self->orientation_revealer = gtk_revealer_new ();
  gtk_container_add (GTK_CONTAINER (ratio_grid), self->orientation_revealer);
  gtk_revealer_set_transition_type (GTK_REVEALER (self->orientation_revealer),
                                    GTK_REVEALER_TRANSITION_TYPE_SLIDE_DOWN);

  orientation_box = gtk_grid_new ();
  gtk_orientable_set_orientation (GTK_ORIENTABLE (orientation_box), GTK_ORIENTATION_HORIZONTAL);
  gtk_container_add (GTK_CONTAINER (self->orientation_revealer), orientation_box);
  gtk_widget_set_hexpand (orientation_box, TRUE);
  context = gtk_widget_get_style_context (orientation_box);
  gtk_style_context_add_class (context, "linked");

  orientation_size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

  self->landscape_button = gtk_radio_button_new (NULL);
  gtk_widget_set_hexpand (self->landscape_button, TRUE);
  gtk_button_set_label (GTK_BUTTON (self->landscape_button), _("Landscape"));
  gtk_toggle_button_set_mode (GTK_TOGGLE_BUTTON (self->landscape_button), FALSE);
  gtk_container_add (GTK_CONTAINER (orientation_box), self->landscape_button);
  gtk_size_group_add_widget (orientation_size_group, self->landscape_button);
  g_signal_connect_swapped (self->landscape_button,
                            "toggled",
                            G_CALLBACK (photos_tool_crop_landscape_button_toggled),
                            self);

  self->portrait_button = gtk_radio_button_new_from_widget (GTK_RADIO_BUTTON (self->landscape_button));
  gtk_widget_set_hexpand (self->portrait_button, TRUE);
  gtk_button_set_label (GTK_BUTTON (self->portrait_button), _("Portrait"));
  gtk_toggle_button_set_mode (GTK_TOGGLE_BUTTON (self->portrait_button), FALSE);
  gtk_container_add (GTK_CONTAINER (orientation_box), self->portrait_button);
  gtk_size_group_add_widget (orientation_size_group, self->portrait_button);
  g_signal_connect_swapped (self->portrait_button,
                            "toggled",
                            G_CALLBACK (photos_tool_crop_portrait_button_toggled),
                            self);

  photos_tool_crop_set_active (self, -1, NULL);

  self->reset_button = gtk_button_new_with_label (_("Reset"));
  gtk_widget_set_halign (self->reset_button, GTK_ALIGN_END);
  gtk_widget_set_hexpand (self->reset_button, TRUE);
  gtk_container_add (GTK_CONTAINER (self->grid), self->reset_button);
  g_signal_connect_swapped (self->reset_button, "clicked", G_CALLBACK (photos_tool_crop_reset_clicked), self);

  self->event_x_last = -1.0;
  self->event_y_last = -1.0;

  g_object_unref (orientation_size_group);
}


static void
photos_tool_crop_class_init (PhotosToolCropClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  PhotosToolClass *tool_class = PHOTOS_TOOL_CLASS (class);

  tool_class->icon_name = PHOTOS_ICON_IMAGE_CROP_SYMBOLIC;
  tool_class->name = _("Crop");

  object_class->dispose = photos_tool_crop_dispose;
  object_class->finalize = photos_tool_crop_finalize;
  tool_class->activate = photos_tool_crop_activate;
  tool_class->deactivate = photos_tool_crop_deactivate;
  tool_class->draw = photos_tool_crop_draw;
  tool_class->get_widget = photos_tool_crop_get_widget;
  tool_class->left_click_event = photos_tool_crop_left_click_event;
  tool_class->left_unclick_event = photos_tool_crop_left_unclick_event;
  tool_class->motion_event = photos_tool_crop_motion_event;
}
