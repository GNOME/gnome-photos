/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2006 – 2007 The Free Software Foundation
 * Copyright © 2013 – 2017 Red Hat, Inc.
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
 *   + Eye of GNOME
 */

#include "config.h"

#ifdef HAVE__NL_MEASUREMENT_MEASUREMENT
#include <langinfo.h>
#endif

#include <glib/gi18n.h>
#include <glib/gprintf.h>
#include <gtk/gtkunixprint.h>

#include "photos-gegl.h"
#include "photos-print-setup.h"
#include "photos-print-preview.h"


struct _PhotosPrintSetup
{
  GtkGrid parent_instance;
  GeglNode *node;
  GtkPageSetup *page_setup;
  GtkWidget *left;
  GtkWidget *right;
  GtkWidget *top;
  GtkWidget *bottom;
  GtkWidget *center;
  GtkWidget *width;
  GtkWidget *height;
  GtkWidget *scaling;
  GtkWidget *preview;
  GtkWidget *unit;
  GtkUnit current_unit;
};

struct _PhotosPrintSetupClass
{
  GtkGridClass parent_class;
};

enum
{
  PROP_0,
  PROP_NODE,
  PROP_PAGE_SETUP
};


G_DEFINE_TYPE (PhotosPrintSetup, photos_print_setup, GTK_TYPE_GRID);


enum
{
  CENTER_NONE,
  CENTER_HORIZONTAL,
  CENTER_VERTICAL,
  CENTER_BOTH
};

enum
{
  CHANGE_HORIZ,
  CHANGE_VERT
};

enum
{
  UNIT_INCH,
  UNIT_MM
};

#define FACTOR_INCH_TO_MM 25.4
#define FACTOR_INCH_TO_PIXEL 72.
#define FACTOR_MM_TO_INCH 0.03937007874015748
#define FACTOR_MM_TO_PIXEL 2.834645669

static void photos_print_setup_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec);

static void on_left_value_changed   (GtkSpinButton *spinbutton, gpointer user_data);
static void on_right_value_changed  (GtkSpinButton *spinbutton, gpointer user_data);
static void on_top_value_changed    (GtkSpinButton *spinbutton, gpointer user_data);
static void on_bottom_value_changed (GtkSpinButton *spinbutton, gpointer user_data);

static void on_width_value_changed  (GtkSpinButton *spinbutton, gpointer user_data);
static void on_height_value_changed (GtkSpinButton *spinbutton, gpointer user_data);


static void
photos_print_setup_block_handlers (PhotosPrintSetup *self)
{
  g_signal_handlers_block_by_func (self->left, on_left_value_changed, self);
  g_signal_handlers_block_by_func (self->right, on_right_value_changed, self);
  g_signal_handlers_block_by_func (self->width, on_width_value_changed, self);
  g_signal_handlers_block_by_func (self->top, on_top_value_changed, self);
  g_signal_handlers_block_by_func (self->bottom, on_bottom_value_changed, self);
  g_signal_handlers_block_by_func (self->height, on_height_value_changed, self);
}


static void
photos_print_setup_unblock_handlers (PhotosPrintSetup *self)
{
  g_signal_handlers_unblock_by_func (self->left, on_left_value_changed, self);
  g_signal_handlers_unblock_by_func (self->right, on_right_value_changed, self);
  g_signal_handlers_unblock_by_func (self->width, on_width_value_changed, self);
  g_signal_handlers_unblock_by_func (self->top, on_top_value_changed, self);
  g_signal_handlers_unblock_by_func (self->bottom, on_bottom_value_changed, self);
  g_signal_handlers_unblock_by_func (self->height, on_height_value_changed, self);
}


static gdouble
get_scale_to_px_factor (PhotosPrintSetup *self)
{
  gdouble factor = 0.;

  switch (self->current_unit)
    {
    case GTK_UNIT_MM:
      factor = FACTOR_MM_TO_PIXEL;
      break;

    case GTK_UNIT_INCH:
      factor = FACTOR_INCH_TO_PIXEL;
      break;

    case GTK_UNIT_NONE:
    case GTK_UNIT_POINTS:
    default:
      g_assert_not_reached ();
    }

  return factor;
}


static gdouble
photos_print_setup_get_max_percentage (PhotosPrintSetup *self)
{
  GeglRectangle bbox;
  gdouble height;
  gdouble page_height;
  gdouble page_width;
  gdouble width;
  gdouble perc;

  page_width = gtk_page_setup_get_page_width (self->page_setup, GTK_UNIT_INCH);
  page_height = gtk_page_setup_get_page_height (self->page_setup, GTK_UNIT_INCH);
  bbox = gegl_node_get_bounding_box (self->node);

  width  = (gdouble) bbox.width / FACTOR_INCH_TO_PIXEL;
  height = (gdouble) bbox.height / FACTOR_INCH_TO_PIXEL;

  if (page_width > width && page_height > height)
    perc = 1.0;
  else
    perc = MIN (page_width / width, page_height / height);

  return perc;
}


static void
photos_print_setup_center (gdouble page_width, gdouble width, GtkSpinButton *s_left, GtkSpinButton *s_right)
{
  gdouble left;
  gdouble right;

  left = (page_width - width) / 2;
  right = page_width - left - width;
  gtk_spin_button_set_value (s_left, left);
  gtk_spin_button_set_value (s_right, right);
}


static void
on_center_changed (GtkComboBox *combobox, gpointer user_data)
{
  PhotosPrintSetup *self = PHOTOS_PRINT_SETUP (user_data);
  gint active;

  active = gtk_combo_box_get_active (combobox);

  switch (active)
    {
    case CENTER_HORIZONTAL:
      photos_print_setup_center (gtk_page_setup_get_page_width (self->page_setup, self->current_unit),
                                 gtk_spin_button_get_value (GTK_SPIN_BUTTON (self->width)),
                                 GTK_SPIN_BUTTON (self->left),
                                 GTK_SPIN_BUTTON (self->right));
      break;

    case CENTER_VERTICAL:
      photos_print_setup_center (gtk_page_setup_get_page_height (self->page_setup, self->current_unit),
                                 gtk_spin_button_get_value (GTK_SPIN_BUTTON (self->height)),
                                 GTK_SPIN_BUTTON (self->top),
                                 GTK_SPIN_BUTTON (self->bottom));
      break;

    case CENTER_BOTH:
      photos_print_setup_center (gtk_page_setup_get_page_width (self->page_setup, self->current_unit),
                                 gtk_spin_button_get_value (GTK_SPIN_BUTTON (self->width)),
                                 GTK_SPIN_BUTTON (self->left),
                                 GTK_SPIN_BUTTON (self->right));
      photos_print_setup_center (gtk_page_setup_get_page_height (self->page_setup, self->current_unit),
                                 gtk_spin_button_get_value (GTK_SPIN_BUTTON (self->height)),
                                 GTK_SPIN_BUTTON (self->top),
                                 GTK_SPIN_BUTTON (self->bottom));
      break;

    case CENTER_NONE:
    default:
      break;
    }

  gtk_combo_box_set_active (combobox, active);
}


static void
update_image_pos_ranges (PhotosPrintSetup *self,
			 gdouble page_width,
			 gdouble page_height,
			 gdouble width,
			 gdouble height)
{
  gtk_spin_button_set_range (GTK_SPIN_BUTTON (self->left), 0, page_width - width);
  gtk_spin_button_set_range (GTK_SPIN_BUTTON (self->right), 0, page_width - width);
  gtk_spin_button_set_range (GTK_SPIN_BUTTON (self->top), 0, page_height - height);
  gtk_spin_button_set_range (GTK_SPIN_BUTTON (self->bottom), 0, page_height - height);
}


static void
on_scale_changed (GtkRange *range, gpointer user_data)
{
  PhotosPrintSetup *self = PHOTOS_PRINT_SETUP (user_data);
  GeglRectangle bbox;
  gdouble height;
  gdouble scale;
  gdouble width;
  gdouble left, right, top, bottom;
  gdouble page_width, page_height;
  gdouble factor;

  gtk_combo_box_set_active (GTK_COMBO_BOX (self->center), CENTER_NONE);

  bbox = gegl_node_get_bounding_box (self->node);
  factor = get_scale_to_px_factor (self);

  width = (gdouble) bbox.width / factor;
  height = (gdouble) bbox.height / factor;

  left = gtk_spin_button_get_value (GTK_SPIN_BUTTON (self->left));
  top = gtk_spin_button_get_value (GTK_SPIN_BUTTON (self->top));

  scale = CLAMP (0.01 * gtk_range_get_value (range), 0, photos_print_setup_get_max_percentage (self));

  photos_print_preview_set_scale (PHOTOS_PRINT_PREVIEW (self->preview), scale);

  width  *= scale;
  height *= scale;

  page_width = gtk_page_setup_get_page_width (self->page_setup, self->current_unit);
  page_height = gtk_page_setup_get_page_height (self->page_setup, self->current_unit);

  update_image_pos_ranges (self, page_width, page_height, width, height);

  right = page_width - left - width;
  bottom = page_height - top - height;

  gtk_spin_button_set_value (GTK_SPIN_BUTTON (self->width), width);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (self->height), height);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (self->right), right);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (self->bottom), bottom);
}


static gchar *
on_scale_format_value (GtkScale *scale, gdouble value)
{
  return g_strdup_printf ("%i%%", (gint) value);
}


static void
photos_print_setup_position_values_changed (PhotosPrintSetup *self,
                                            GtkWidget *w_changed,
                                            GtkWidget *w_to_update,
                                            GtkWidget *w_size,
                                            gdouble total_size,
                                            gint change)
{
  gdouble changed, to_update, size;
  gdouble pos;

  size = gtk_spin_button_get_value (GTK_SPIN_BUTTON (w_size));
  changed = gtk_spin_button_get_value (GTK_SPIN_BUTTON (w_changed));

  to_update = total_size - changed - size;
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (w_to_update), to_update);
  gtk_combo_box_set_active (GTK_COMBO_BOX (self->center), CENTER_NONE);

  switch (change)
    {
    case CHANGE_HORIZ:
      pos = gtk_spin_button_get_value (GTK_SPIN_BUTTON (self->left));
      if (self->current_unit == GTK_UNIT_MM)
        pos *= FACTOR_MM_TO_INCH;
      photos_print_preview_set_image_position (PHOTOS_PRINT_PREVIEW (self->preview), pos, -1);
      break;

    case CHANGE_VERT:
      pos = gtk_spin_button_get_value (GTK_SPIN_BUTTON (self->top));
      if (self->current_unit == GTK_UNIT_MM)
        pos *= FACTOR_MM_TO_INCH;
      photos_print_preview_set_image_position (PHOTOS_PRINT_PREVIEW (self->preview), -1, pos);
      break;

    default:
      g_assert_not_reached ();
      break;
    }
}


static void
on_left_value_changed (GtkSpinButton *spinbutton, gpointer user_data)
{
  PhotosPrintSetup *self = PHOTOS_PRINT_SETUP (user_data);

  photos_print_setup_position_values_changed (self,
                                              self->left,
                                              self->right,
                                              self->width,
                                              gtk_page_setup_get_page_width (self->page_setup,
                                                                             self->current_unit),
                                              CHANGE_HORIZ);
}


static void
on_right_value_changed (GtkSpinButton *spinbutton, gpointer user_data)
{
  PhotosPrintSetup *self = PHOTOS_PRINT_SETUP (user_data);

  photos_print_setup_position_values_changed (self,
                                              self->right,
                                              self->left,
                                              self->width,
                                              gtk_page_setup_get_page_width (self->page_setup,
                                                                             self->current_unit),
                                              CHANGE_HORIZ);
}


static void
on_top_value_changed (GtkSpinButton *spinbutton,
		      gpointer       user_data)
{
  PhotosPrintSetup *self = PHOTOS_PRINT_SETUP (user_data);

  photos_print_setup_position_values_changed (self,
                                              self->top,
                                              self->bottom,
                                              self->height,
                                              gtk_page_setup_get_page_height (self->page_setup,
                                                                              self->current_unit),
                                              CHANGE_VERT);
}


static void
on_bottom_value_changed (GtkSpinButton *spinbutton, gpointer user_data)
{
  PhotosPrintSetup *self = PHOTOS_PRINT_SETUP (user_data);

  photos_print_setup_position_values_changed (self,
                                              self->bottom,
                                              self->top,
                                              self->height,
                                              gtk_page_setup_get_page_height (self->page_setup,
                                                                              self->current_unit),
                                              CHANGE_VERT);
}


static void
photos_print_setup_size_changed (PhotosPrintSetup *self,
                                 GtkWidget *w_size_x,
                                 GtkWidget *w_size_y,
                                 GtkWidget *w_margin_x_1,
                                 GtkWidget *w_margin_x_2,
                                 GtkWidget *w_margin_y_1,
                                 GtkWidget *w_margin_y_2,
                                 gdouble page_size_x,
                                 gdouble page_size_y,
                                 gint change)
{
  GeglRectangle bbox;
  gdouble margin_x_1, margin_x_2;
  gdouble margin_y_1, margin_y_2;
  gdouble orig_size_x = -1, orig_size_y = -1, scale;
  gdouble size_x, size_y;
  gdouble factor;

  size_x = gtk_spin_button_get_value (GTK_SPIN_BUTTON (w_size_x));
  margin_x_1 = gtk_spin_button_get_value (GTK_SPIN_BUTTON (w_margin_x_1));
  margin_y_1 = gtk_spin_button_get_value (GTK_SPIN_BUTTON (w_margin_y_1));

  bbox = gegl_node_get_bounding_box (self->node);
  factor = get_scale_to_px_factor (self);

  switch (change)
    {
    case CHANGE_HORIZ:
      orig_size_x = (gdouble) bbox.width / factor;
      orig_size_y = (gdouble) bbox.height / factor;
      break;

    case CHANGE_VERT:
      orig_size_y = (gdouble) bbox.width / factor;
      orig_size_x = (gdouble) bbox.height / factor;
      break;

    default:
      g_assert_not_reached ();
      break;
    }

  scale = CLAMP (size_x / orig_size_x, 0, 1);

  size_y = scale * orig_size_y;

  margin_x_2 = page_size_x - margin_x_1 - size_x;
  margin_y_2 = page_size_y - margin_y_1 - size_y;

  photos_print_preview_set_scale (PHOTOS_PRINT_PREVIEW (self->preview), scale);

  switch (change)
    {
    case CHANGE_HORIZ:
      update_image_pos_ranges (self, page_size_x, page_size_y, size_x, size_y);
      break;

    case CHANGE_VERT:
      update_image_pos_ranges (self, page_size_y, page_size_x, size_y, size_x);
      break;

    default:
      g_assert_not_reached ();
      break;
    }

  gtk_range_set_value (GTK_RANGE (self->scaling), 100*scale);

  gtk_spin_button_set_value (GTK_SPIN_BUTTON (w_margin_x_2), margin_x_2);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (w_size_y), size_y);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (w_margin_y_2), margin_y_2);

  gtk_combo_box_set_active (GTK_COMBO_BOX (self->center), CENTER_NONE);
}

static void
on_width_value_changed (GtkSpinButton *spinbutton, gpointer user_data)
{
  PhotosPrintSetup *self = PHOTOS_PRINT_SETUP (user_data);

  photos_print_setup_size_changed (self,
                                   self->width,
                                   self->height,
                                   self->left,
                                   self->right,
                                   self->top,
                                   self->bottom,
                                   gtk_page_setup_get_page_width (self->page_setup, self->current_unit),
                                   gtk_page_setup_get_page_height (self->page_setup, self->current_unit),
                                   CHANGE_HORIZ);
}


static void
on_height_value_changed (GtkSpinButton *spinbutton, gpointer user_data)
{
  PhotosPrintSetup *self = PHOTOS_PRINT_SETUP (user_data);

  photos_print_setup_size_changed (self,
                                   self->height,
                                   self->width,
                                   self->top,
                                   self->bottom,
                                   self->left,
                                   self->right,
                                   gtk_page_setup_get_page_height (self->page_setup, self->current_unit),
                                   gtk_page_setup_get_page_width (self->page_setup, self->current_unit),
                                   CHANGE_VERT);
}


static void
change_unit (GtkSpinButton *spinbutton, gdouble factor, gint digits, gdouble step, gdouble page)
{
  gdouble value;
  gdouble range;

  gtk_spin_button_get_range (spinbutton, NULL, &range);
  range *= factor;

  value = gtk_spin_button_get_value (spinbutton);
  value *= factor;

  gtk_spin_button_set_range (spinbutton, 0, range);
  gtk_spin_button_set_value (spinbutton, value);
  gtk_spin_button_set_digits (spinbutton, digits);
  gtk_spin_button_set_increments  (spinbutton, step, page);
}


static void
photos_print_setup_set_scale_unit (PhotosPrintSetup *self, GtkUnit unit)
{
  gdouble factor;
  gdouble step, page;
  gint digits;

  if (G_UNLIKELY (self->current_unit == unit))
    return;

  switch (unit)
    {
    case GTK_UNIT_MM:
      factor = FACTOR_INCH_TO_MM;
      digits = 0;
      step = 1;
      page = 10;
      break;

    case GTK_UNIT_INCH:
      factor = FACTOR_MM_TO_INCH;
      digits = 2;
      step = 0.01;
      page = 0.1;
      break;

    case GTK_UNIT_NONE:
    case GTK_UNIT_POINTS:
    default:
      g_assert_not_reached ();
    }

  photos_print_setup_block_handlers (self);

  change_unit (GTK_SPIN_BUTTON (self->width), factor, digits, step, page);
  change_unit (GTK_SPIN_BUTTON (self->height), factor, digits, step, page);
  change_unit (GTK_SPIN_BUTTON (self->left), factor, digits, step, page);
  change_unit (GTK_SPIN_BUTTON (self->right), factor, digits, step, page);
  change_unit (GTK_SPIN_BUTTON (self->top), factor, digits, step, page);
  change_unit (GTK_SPIN_BUTTON (self->bottom), factor, digits, step, page);

  photos_print_setup_unblock_handlers (self);

  self->current_unit = unit;
}


static void
on_unit_changed (GtkComboBox *combobox, gpointer user_data)
{
  GtkUnit unit = GTK_UNIT_INCH;

  switch (gtk_combo_box_get_active (combobox))
    {
    case UNIT_INCH:
      unit = GTK_UNIT_INCH;
      break;

    case UNIT_MM:
      unit = GTK_UNIT_MM;
      break;

    default:
      g_assert_not_reached ();
    }

  photos_print_setup_set_scale_unit (PHOTOS_PRINT_SETUP (user_data), unit);
}


static void
on_preview_pixbuf_moved (PhotosPrintPreview *preview, gpointer user_data)
{
  PhotosPrintSetup *self = PHOTOS_PRINT_SETUP (user_data);
  gdouble x;
  gdouble y;

  photos_print_preview_get_image_position (preview, &x, &y);

  if (self->current_unit == GTK_UNIT_MM)
    {
      x *= FACTOR_INCH_TO_MM;
      y *= FACTOR_INCH_TO_MM;
    }

  gtk_spin_button_set_value (GTK_SPIN_BUTTON (self->left), x);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (self->top), y);
}


static gboolean
on_preview_image_scrolled (GtkWidget *widget, GdkEventScroll *event, gpointer user_data)
{
  PhotosPrintSetup *self = PHOTOS_PRINT_SETUP (user_data);
  PhotosPrintPreview *preview = PHOTOS_PRINT_PREVIEW (widget);
  gfloat scale;

  scale = photos_print_preview_get_scale (preview);

  if (!photos_print_preview_point_in_image_area (preview, event->x, event->y))
    return FALSE;

  switch (event->direction)
    {
    case GDK_SCROLL_UP:
      /* scale up */
      scale *= 1.1;
      break;

    case GDK_SCROLL_DOWN:
      /* scale down */
      scale *= 0.9;
      break;

    case GDK_SCROLL_LEFT:
    case GDK_SCROLL_RIGHT:
    case GDK_SCROLL_SMOOTH:
    default:
      return FALSE;
      break;
    }

  gtk_range_set_value (GTK_RANGE (self->scaling), 100*scale);

  return TRUE;
}


static gboolean
on_preview_image_key_pressed (GtkWidget *widget, GdkEventKey *event, gpointer user_data)
{
  PhotosPrintSetup *self = PHOTOS_PRINT_SETUP (user_data);
  PhotosPrintPreview *preview = PHOTOS_PRINT_PREVIEW (widget);
  gfloat scale;

  scale = photos_print_preview_get_scale (preview);

  switch (event->keyval)
    {
    case GDK_KEY_KP_Add:
    case GDK_KEY_plus:
      /* scale up */
      scale *= 1.1;
      break;

    case GDK_KEY_KP_Subtract:
    case GDK_KEY_minus:
      /* scale down */
      scale *= 0.9;
      break;

    default:
      return FALSE;
      break;
    }

  gtk_range_set_value (GTK_RANGE (self->scaling), 100 * scale);

  return TRUE;
}


/* Function taken from gtkprintunixdialog.c */
static GtkWidget *
photos_print_setup_wrap_in_frame (const gchar *label, GtkWidget *child)
{
  GtkWidget *frame;
  GtkWidget *label_widget;
  gchar *bold_text;

  label_widget = gtk_label_new ("");
  gtk_label_set_xalign (GTK_LABEL (label_widget), 0.0);
  gtk_widget_show (label_widget);

  bold_text = g_markup_printf_escaped ("<b>%s</b>", label);
  gtk_label_set_markup (GTK_LABEL (label_widget), bold_text);
  g_free (bold_text);

  frame = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_box_pack_start (GTK_BOX (frame), label_widget, FALSE, FALSE, 0);

  gtk_widget_set_margin_start (child, 12);
  gtk_widget_set_halign (child, GTK_ALIGN_FILL);
  gtk_widget_set_valign (child, GTK_ALIGN_FILL);

  gtk_box_pack_start (GTK_BOX (frame), child, FALSE, FALSE, 0);

  gtk_widget_show (frame);

  return frame;
}


static GtkWidget *
grid_attach_spin_button_with_label (GtkWidget *grid, const gchar* text_label, gint left, gint top)
{
  GtkWidget *label;
  GtkWidget *spin_button;

  label = gtk_label_new_with_mnemonic (text_label);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);
  spin_button = gtk_spin_button_new_with_range (0, 100, 0.01);
  gtk_spin_button_set_digits (GTK_SPIN_BUTTON (spin_button), 2);
  gtk_entry_set_width_chars (GTK_ENTRY (spin_button), 6);
  gtk_grid_attach (GTK_GRID (grid), label, left, top, 1, 1);
  gtk_grid_attach_next_to (GTK_GRID (grid), spin_button, label, GTK_POS_RIGHT, 1, 1);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), spin_button);

  return spin_button;
}


static void
photos_print_setup_set_initial_values (PhotosPrintSetup *self)
{
  GeglRectangle bbox;
  gdouble page_height;
  gdouble page_width;
  gdouble factor;
  gdouble height;
  gdouble max_perc;
  gdouble width;

  factor = get_scale_to_px_factor (self);

  bbox = gegl_node_get_bounding_box (self->node);
  width = (gdouble) bbox.width/factor;
  height = (gdouble) bbox.height/factor;

  max_perc = photos_print_setup_get_max_percentage (self);

  width *= max_perc;
  height *= max_perc;

  gtk_range_set_range (GTK_RANGE (self->scaling), 1, 100 * max_perc);
  gtk_range_set_increments (GTK_RANGE (self->scaling), max_perc, 10 * max_perc);
  gtk_range_set_value (GTK_RANGE (self->scaling), 100 * max_perc);

  photos_print_preview_set_scale (PHOTOS_PRINT_PREVIEW (self->preview), max_perc);
  gtk_spin_button_set_range (GTK_SPIN_BUTTON (self->width), 0, width);
  gtk_spin_button_set_range (GTK_SPIN_BUTTON (self->height), 0, height);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (self->width), width);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (self->height), height);

  gtk_combo_box_set_active (GTK_COMBO_BOX (self->center), CENTER_BOTH);

  photos_print_setup_center (gtk_page_setup_get_page_width (self->page_setup, self->current_unit),
                             gtk_spin_button_get_value (GTK_SPIN_BUTTON (self->width)),
                             GTK_SPIN_BUTTON (self->left), GTK_SPIN_BUTTON (self->right));
  photos_print_setup_center (gtk_page_setup_get_page_height (self->page_setup, self->current_unit),
                             gtk_spin_button_get_value (GTK_SPIN_BUTTON (self->height)),
                             GTK_SPIN_BUTTON (self->top), GTK_SPIN_BUTTON (self->bottom));

  page_width = gtk_page_setup_get_page_width (self->page_setup, self->current_unit);
  page_height = gtk_page_setup_get_page_height (self->page_setup, self->current_unit);

  update_image_pos_ranges (self, page_width, page_height, width, height);
}


static void
photos_print_setup_constructed (GObject *object)
{
  PhotosPrintSetup *self = PHOTOS_PRINT_SETUP (object);

  G_OBJECT_CLASS (photos_print_setup_parent_class)->constructed (object);

  photos_print_setup_set_initial_values (self);
  photos_print_preview_set_from_page_setup (PHOTOS_PRINT_PREVIEW (self->preview), self->page_setup);

  g_signal_connect (self->left, "value-changed", G_CALLBACK (on_left_value_changed), self);
  g_signal_connect (self->right, "value-changed", G_CALLBACK (on_right_value_changed), self);
  g_signal_connect (self->top, "value-changed", G_CALLBACK (on_top_value_changed), self);
  g_signal_connect (self->bottom, "value-changed", G_CALLBACK (on_bottom_value_changed), self);
  g_signal_connect (self->width, "value-changed", G_CALLBACK (on_width_value_changed), self);
  g_signal_connect (self->height, "value-changed", G_CALLBACK (on_height_value_changed), self);
  g_signal_connect (self->scaling, "value-changed", G_CALLBACK (on_scale_changed), self);
  g_signal_connect (self->scaling, "format-value", G_CALLBACK (on_scale_format_value), NULL);
  g_signal_connect (self->preview, "pixbuf-moved", G_CALLBACK (on_preview_pixbuf_moved), self);
  g_signal_connect (self->preview, "scroll-event", G_CALLBACK (on_preview_image_scrolled), self);
  g_signal_connect (self->preview, "key-press-event", G_CALLBACK (on_preview_image_key_pressed), self);
}


static void
photos_print_setup_dispose (GObject *object)
{
  PhotosPrintSetup *self = PHOTOS_PRINT_SETUP (object);

  g_clear_object (&self->node);
  g_clear_object (&self->page_setup);

  G_OBJECT_CLASS (photos_print_setup_parent_class)->dispose (object);
}


static void
photos_print_setup_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
  PhotosPrintSetup *self = PHOTOS_PRINT_SETUP (object);

  switch (prop_id)
    {
    case PROP_NODE:
      {
        GdkPixbuf *pixbuf;

        self->node = GEGL_NODE (g_value_dup_object (value));
        pixbuf = photos_gegl_create_pixbuf_from_node (self->node);
        if (pixbuf != NULL)
          {
            g_object_set (self->preview, "pixbuf", pixbuf, NULL);
            g_object_unref (pixbuf);
          }
      }
      break;

    case PROP_PAGE_SETUP:
      self->page_setup = GTK_PAGE_SETUP (g_value_dup_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}


static void
photos_print_setup_init (PhotosPrintSetup *self)
{
  GtkWidget *frame;
  GtkWidget *grid;
  GtkWidget *label;
  GtkWidget *hscale;
  GtkWidget *combobox;

#ifdef HAVE__NL_MEASUREMENT_MEASUREMENT
  gchar *locale_scale = NULL;
#endif

  gtk_container_set_border_width (GTK_CONTAINER (self), 12);
  gtk_orientable_set_orientation (GTK_ORIENTABLE (self), GTK_ORIENTATION_VERTICAL);
  gtk_grid_set_column_spacing (GTK_GRID (self), 18);
  gtk_grid_set_row_spacing (GTK_GRID (self), 18);

  grid = gtk_grid_new ();
  gtk_grid_set_row_spacing (GTK_GRID (grid), 6);
  gtk_grid_set_column_spacing (GTK_GRID (grid), 12);
  frame = photos_print_setup_wrap_in_frame (_("Position"), grid);
  gtk_grid_attach (GTK_GRID (self), frame, 0, 0, 1, 1);

  self->left = grid_attach_spin_button_with_label (grid, _("_Left:"), 0, 0);
  self->right = grid_attach_spin_button_with_label (grid,_("_Right:"), 0, 1);
  self->top = grid_attach_spin_button_with_label (grid, _("_Top:"), 2, 0);
  self->bottom = grid_attach_spin_button_with_label (grid, _("_Bottom:"), 2, 1);

  label = gtk_label_new_with_mnemonic (_("C_enter:"));
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);

  combobox = gtk_combo_box_text_new ();
  gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT (combobox), CENTER_NONE, _("None"));
  gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT (combobox), CENTER_HORIZONTAL, _("Horizontal"));
  gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT (combobox), CENTER_VERTICAL, _("Vertical"));
  gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT (combobox), CENTER_BOTH, _("Both"));
  gtk_combo_box_set_active (GTK_COMBO_BOX (combobox), CENTER_NONE);
  /* Attach combobox below right margin spinbutton and span until end */
  gtk_grid_attach_next_to (GTK_GRID (grid), combobox, self->right, GTK_POS_BOTTOM, 3, 1);
  /* Attach the label to the left of the combobox */
  gtk_grid_attach_next_to (GTK_GRID (grid), label, combobox, GTK_POS_LEFT, 1, 1);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), combobox);
  self->center = combobox;
  g_signal_connect (G_OBJECT (combobox), "changed", G_CALLBACK (on_center_changed), self);

  grid = gtk_grid_new ();
  gtk_grid_set_row_spacing (GTK_GRID (grid), 6);
  gtk_grid_set_column_spacing (GTK_GRID (grid), 12);
  frame = photos_print_setup_wrap_in_frame (_("Size"), grid);
  gtk_grid_attach (GTK_GRID (self), frame, 0, 1, 1, 1);

  self->width = grid_attach_spin_button_with_label (grid, _("_Width:"), 0, 0);
  self->height = grid_attach_spin_button_with_label (grid, _("_Height:"), 2, 0);

  label = gtk_label_new_with_mnemonic (_("_Scaling:"));
  hscale = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL, 1, 100, 1);
  gtk_scale_set_value_pos (GTK_SCALE (hscale), GTK_POS_RIGHT);
  gtk_range_set_value (GTK_RANGE (hscale), 100);
  gtk_grid_attach_next_to (GTK_GRID (grid), hscale, self->width, GTK_POS_BOTTOM, 3, 1);
  gtk_grid_attach_next_to (GTK_GRID (grid), label, hscale, GTK_POS_LEFT, 1, 1);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), hscale);
  self->scaling = hscale;

  label = gtk_label_new_with_mnemonic (_("_Unit:"));
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);

  combobox = gtk_combo_box_text_new ();
  gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT (combobox), UNIT_MM, _("Millimeters"));
  gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT (combobox), UNIT_INCH, _("Inches"));

#ifdef HAVE__NL_MEASUREMENT_MEASUREMENT
  locale_scale = nl_langinfo (_NL_MEASUREMENT_MEASUREMENT);
  if (locale_scale && locale_scale[0] == 2)
    {
      gtk_combo_box_set_active (GTK_COMBO_BOX (combobox), UNIT_INCH);
      photos_print_setup_set_scale_unit (self, GTK_UNIT_INCH);
    }
  else
#endif
    {
      gtk_combo_box_set_active (GTK_COMBO_BOX (combobox), UNIT_MM);
      photos_print_setup_set_scale_unit (self, GTK_UNIT_MM);
    }

  gtk_grid_attach_next_to (GTK_GRID (grid), combobox, hscale, GTK_POS_BOTTOM, 3, 1);
  gtk_grid_attach_next_to (GTK_GRID (grid), label, combobox, GTK_POS_LEFT, 1, 1);

  gtk_label_set_mnemonic_widget (GTK_LABEL (label), combobox);
  self->unit = combobox;
  g_signal_connect (G_OBJECT (combobox), "changed", G_CALLBACK (on_unit_changed), self);

  self->preview = photos_print_preview_new ();

  /* FIXME: This shouldn't be set by hand */
  gtk_widget_set_size_request (self->preview, 250, 250);

  frame = photos_print_setup_wrap_in_frame (_("Preview"), self->preview);
  /* The preview widget needs to span the whole grid height */
  gtk_grid_attach (GTK_GRID (self), frame, 1, 0, 1, 2);

  gtk_widget_show_all (GTK_WIDGET (self));
}


static void
photos_print_setup_class_init (PhotosPrintSetupClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->constructed = photos_print_setup_constructed;
  object_class->dispose = photos_print_setup_dispose;
  object_class->set_property = photos_print_setup_set_property;

  g_object_class_install_property (object_class,
                                   PROP_NODE,
                                   g_param_spec_object ("node",
                                                        "GeglNode object",
                                                        "The node corresponding to the item whose printing "
                                                        "properties will be set up",
                                                        GEGL_TYPE_NODE,
                                                        G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE));

  g_object_class_install_property (object_class,
                                   PROP_PAGE_SETUP,
                                   g_param_spec_object ("page-setup",
                                                        "GtkPageSetup object",
                                                        "The information for the page where the item will be "
                                                        "printed",
                                                        GTK_TYPE_PAGE_SETUP,
                                                        G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE));
}


GtkWidget *
photos_print_setup_new (GeglNode *node, GtkPageSetup *page_setup)
{
  return g_object_new (PHOTOS_TYPE_PRINT_SETUP, "node", node, "page-setup", page_setup, NULL);
}


void
photos_print_setup_get_options (PhotosPrintSetup *self,
                                gdouble *left,
                                gdouble *top,
                                gdouble *scale,
                                GtkUnit *unit)
{
  *left = gtk_spin_button_get_value (GTK_SPIN_BUTTON (self->left));
  *top = gtk_spin_button_get_value (GTK_SPIN_BUTTON (self->top));
  *scale = gtk_range_get_value (GTK_RANGE (self->scaling));
  *unit = self->current_unit;
}


void
photos_print_setup_update (PhotosPrintSetup *self, GtkPageSetup *page_setup)
{
  gdouble pos_x;
  gdouble pos_y;

  self->page_setup = gtk_page_setup_copy (page_setup);

  photos_print_setup_set_initial_values (PHOTOS_PRINT_SETUP (self));

  photos_print_preview_set_from_page_setup (PHOTOS_PRINT_PREVIEW (self->preview), self->page_setup);

  pos_x = gtk_spin_button_get_value (GTK_SPIN_BUTTON (self->left));
  pos_y = gtk_spin_button_get_value (GTK_SPIN_BUTTON (self->top));
  if (self->current_unit == GTK_UNIT_MM)
    {
      pos_x *= FACTOR_MM_TO_INCH;
      pos_y *= FACTOR_MM_TO_INCH;
    }
  photos_print_preview_set_image_position (PHOTOS_PRINT_PREVIEW (self->preview), pos_x, pos_y);
}
