/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2013 – 2021 Red Hat, Inc.
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
 *   + Eye of GNOME
 */


#include "config.h"

#include <stdio.h>

#include <babl/babl.h>
#include <glib.h>
#include <glib/gi18n.h>

#include "photos-gegl.h"
#include "photos-print-operation.h"
#include "photos-print-setup.h"


struct _PhotosPrintOperation
{
  GtkPrintOperation parent_instance;
  GeglNode *node;
  GtkUnit unit;
  PhotosBaseItem *item;
  gdouble left_margin;
  gdouble top_margin;
  gdouble scale_factor;
};

enum
{
  PROP_0,
  PROP_ITEM,
  PROP_NODE
};


G_DEFINE_TYPE (PhotosPrintOperation, photos_print_operation, GTK_TYPE_PRINT_OPERATION);


static void
photos_print_operation_custom_widget_apply (GtkPrintOperation *operation, GtkWidget *widget)
{
  PhotosPrintOperation *self = PHOTOS_PRINT_OPERATION (operation);

  photos_print_setup_get_options (PHOTOS_PRINT_SETUP (widget),
                                  &self->left_margin,
                                  &self->top_margin,
                                  &self->scale_factor,
                                  &self->unit);
}


static GtkWidget *
photos_print_operation_create_custom_widget (GtkPrintOperation *operation)
{
  PhotosPrintOperation *self = PHOTOS_PRINT_OPERATION (operation);
  GtkPageSetup *page_setup;
  GtkWidget *print_setup;

  page_setup = gtk_print_operation_get_default_page_setup (GTK_PRINT_OPERATION (self));
  print_setup = photos_print_setup_new (self->node, page_setup);
  return print_setup;
}


static void
photos_print_operation_draw_page (GtkPrintOperation *operation, GtkPrintContext *context, gint page_nr)
{
  PhotosPrintOperation *self = PHOTOS_PRINT_OPERATION (operation);
  GeglRectangle bbox;
  g_autoptr (GdkPixbuf) pixbuf = NULL;
  GtkPageSetup *page_setup;
  cairo_t *cr;
  gdouble dpi_x;
  gdouble dpi_y;
  gdouble page_height;
  gdouble page_width;
  gdouble scale_factor_n;
  gdouble x0;
  gdouble y0;

  scale_factor_n = self->scale_factor / 100.0;
  bbox = gegl_node_get_bounding_box (self->node);

  dpi_x = gtk_print_context_get_dpi_x (context);
  dpi_y = gtk_print_context_get_dpi_x (context);

  switch (self->unit)
    {
    case GTK_UNIT_INCH:
      x0 = self->left_margin * dpi_x;
      y0 = self->top_margin  * dpi_y;
      break;
    case GTK_UNIT_MM:
      x0 = self->left_margin * dpi_x / 25.4;
      y0 = self->top_margin  * dpi_y / 25.4;
      break;
    case GTK_UNIT_NONE:
    case GTK_UNIT_POINTS:
    default:
      g_assert_not_reached ();
    }

  cr = gtk_print_context_get_cairo_context (context);
  cairo_translate (cr, x0, y0);

  page_setup = gtk_print_context_get_page_setup (context);
  page_width =  gtk_page_setup_get_page_width (page_setup, GTK_UNIT_POINTS);
  page_height = gtk_page_setup_get_page_height (page_setup, GTK_UNIT_POINTS);

  /* This is both a workaround for a bug in cairo's PDF backend, and
   * a way to ensure we are not printing outside the page margins.
   */
  cairo_rectangle (cr,
                   0,
                   0,
                   MIN (bbox.width * scale_factor_n, page_width),
                   MIN (bbox.height * scale_factor_n, page_height));
  cairo_clip (cr);
  cairo_scale (cr, scale_factor_n, scale_factor_n);

  pixbuf = photos_gegl_create_pixbuf_from_node (self->node);
  if (pixbuf == NULL)
    goto out;

  gdk_cairo_set_source_pixbuf (cr, pixbuf, 0.0, 0.0);
  cairo_paint (cr);

 out:
  return;
}


static void
photos_print_operation_update_custom_widget (GtkPrintOperation *operation,
                                             GtkWidget *widget,
                                             GtkPageSetup *page_setup,
                                             GtkPrintSettings *settings)
{
  photos_print_setup_update (PHOTOS_PRINT_SETUP (widget), page_setup);
}


static void
photos_print_operation_constructed (GObject *object)
{
  PhotosPrintOperation *self = PHOTOS_PRINT_OPERATION (object);
  GeglRectangle bbox;
  g_autoptr (GtkPageSetup) page_setup = NULL;
  const gchar *name;

  G_OBJECT_CLASS (photos_print_operation_parent_class)->constructed (object);

  page_setup = gtk_page_setup_new ();
  gtk_print_operation_set_default_page_setup (GTK_PRINT_OPERATION (self), page_setup);

  bbox = gegl_node_get_bounding_box (self->node);
  if (bbox.height >= bbox.width)
    gtk_page_setup_set_orientation (page_setup, GTK_PAGE_ORIENTATION_PORTRAIT);
  else
    gtk_page_setup_set_orientation (page_setup, GTK_PAGE_ORIENTATION_LANDSCAPE);

  name = photos_base_item_get_name_with_fallback (self->item);
  gtk_print_operation_set_job_name (GTK_PRINT_OPERATION (self), name);
}


static void
photos_print_operation_dispose (GObject *object)
{
  PhotosPrintOperation *self = PHOTOS_PRINT_OPERATION (object);

  g_clear_object (&self->item);
  g_clear_object (&self->node);

  G_OBJECT_CLASS (photos_print_operation_parent_class)->dispose (object);
}


static void
photos_print_operation_finalize (GObject *object)
{
  G_OBJECT_CLASS (photos_print_operation_parent_class)->finalize (object);
}


static void
photos_print_operation_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
  PhotosPrintOperation *self = PHOTOS_PRINT_OPERATION (object);

  switch (prop_id)
    {
    case PROP_ITEM:
      self->item = PHOTOS_BASE_ITEM (g_value_dup_object (value));
      break;

    case PROP_NODE:
      self->node = GEGL_NODE (g_value_dup_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
photos_print_operation_init (PhotosPrintOperation *self)
{
  g_autoptr (GtkPrintSettings) settings = NULL;

  self->unit = GTK_UNIT_INCH;
  self->left_margin = 0.0;
  self->top_margin = 0.0;
  self->scale_factor = 100.0;

  settings = gtk_print_settings_new ();
  gtk_print_operation_set_print_settings (GTK_PRINT_OPERATION (self), settings);

  gtk_print_operation_set_custom_tab_label (GTK_PRINT_OPERATION (self), _("Image Settings"));
  gtk_print_operation_set_embed_page_setup (GTK_PRINT_OPERATION (self), TRUE);
  gtk_print_operation_set_n_pages (GTK_PRINT_OPERATION (self), 1);
}


static void
photos_print_operation_class_init (PhotosPrintOperationClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkPrintOperationClass *operation_class = GTK_PRINT_OPERATION_CLASS (class);

  object_class->constructed = photos_print_operation_constructed;
  object_class->dispose = photos_print_operation_dispose;
  object_class->finalize = photos_print_operation_finalize;
  object_class->set_property = photos_print_operation_set_property;
  operation_class->custom_widget_apply = photos_print_operation_custom_widget_apply;
  operation_class->create_custom_widget = photos_print_operation_create_custom_widget;
  operation_class->draw_page = photos_print_operation_draw_page;
  operation_class->update_custom_widget = photos_print_operation_update_custom_widget;

  g_object_class_install_property (object_class,
                                   PROP_ITEM,
                                   g_param_spec_object ("item",
                                                        "PhotosBaseItem object",
                                                        "The item to print",
                                                        PHOTOS_TYPE_BASE_ITEM,
                                                        G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE));

  g_object_class_install_property (object_class,
                                   PROP_NODE,
                                   g_param_spec_object ("node",
                                                        "GeglNode object",
                                                        "The node corresponding to the item",
                                                        GEGL_TYPE_NODE,
                                                        G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE));
}


GtkPrintOperation *
photos_print_operation_new (PhotosBaseItem *item, GeglNode *node)
{
  return g_object_new (PHOTOS_TYPE_PRINT_OPERATION, "item", item, "node", node, NULL);
}
