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

/* Based on code from:
 *   + Documents
 */


#include "config.h"

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "photos-empty-results-box.h"


G_DEFINE_TYPE (PhotosEmptyResultsBox, photos_empty_results_box, GTK_CLUTTER_TYPE_ACTOR);


static void
photos_empty_results_box_constructed (GObject *object)
{
  PhotosEmptyResultsBox *self = PHOTOS_EMPTY_RESULTS_BOX (object);
  GtkStyleContext *context;
  GtkWidget *bin;
  GtkWidget *image;
  GtkWidget *labels_grid;
  GtkWidget *title_label;
  GtkWidget *widget;
  gchar *label;

  G_OBJECT_CLASS (photos_empty_results_box_parent_class)->constructed (object);

  widget = gtk_grid_new ();
  gtk_widget_set_halign (widget, GTK_ALIGN_CENTER);
  gtk_widget_set_hexpand (widget, TRUE);
  gtk_widget_set_valign (widget, GTK_ALIGN_CENTER);
  gtk_widget_set_vexpand (widget, TRUE);
  gtk_orientable_set_orientation (GTK_ORIENTABLE (widget), GTK_ORIENTATION_HORIZONTAL);
  gtk_grid_set_column_spacing (GTK_GRID (widget), 12);
  context = gtk_widget_get_style_context (widget);
  gtk_style_context_add_class (context, "dim-label");

  image = gtk_image_new_from_icon_name ("emblem-photos-symbolic", GTK_ICON_SIZE_INVALID);
  gtk_image_set_pixel_size (GTK_IMAGE (image), 64);
  gtk_container_add (GTK_CONTAINER (widget), image);

  labels_grid = gtk_grid_new ();
  gtk_orientable_set_orientation (GTK_ORIENTABLE (labels_grid), GTK_ORIENTATION_VERTICAL);
  gtk_grid_set_row_spacing (GTK_GRID (labels_grid), 12);
  gtk_container_add (GTK_CONTAINER (widget), labels_grid);

  label = g_strconcat ("<b><span size=\"large\">", _("No Photos Found"), "</span></b>", NULL);
  title_label = gtk_label_new (label);
  gtk_widget_set_halign (title_label, GTK_ALIGN_START);
  gtk_widget_set_vexpand (title_label, TRUE);
  gtk_label_set_use_markup (GTK_LABEL (title_label), TRUE);
  gtk_container_add (GTK_CONTAINER (labels_grid), title_label);
  g_free (label);

  /* TODO: Check PhotosSourceManager for online sources */
  gtk_widget_set_valign (title_label, GTK_ALIGN_CENTER);

  gtk_widget_show_all (widget);

  bin = gtk_clutter_actor_get_widget (GTK_CLUTTER_ACTOR (self));
  gtk_container_add (GTK_CONTAINER (bin), widget);
}


static void
photos_empty_results_box_init (PhotosEmptyResultsBox *self)
{
  clutter_actor_set_opacity (CLUTTER_ACTOR (self), 255);
  clutter_actor_set_x_expand (CLUTTER_ACTOR (self), TRUE);
  clutter_actor_set_y_expand (CLUTTER_ACTOR (self), TRUE);
}


static void
photos_empty_results_box_class_init (PhotosEmptyResultsBoxClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->constructed = photos_empty_results_box_constructed;
}


ClutterActor *
photos_empty_results_box_new (void)
{
  return g_object_new (PHOTOS_TYPE_EMPTY_RESULTS_BOX, NULL);
}
