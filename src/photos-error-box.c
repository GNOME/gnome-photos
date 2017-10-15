/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2012 – 2017 Red Hat, Inc.
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

#include <glib.h>

#include "photos-error-box.h"


struct _PhotosErrorBox
{
  GtkGrid parent_instance;
  GtkWidget *image;
  GtkWidget *primary_label;
  GtkWidget *secondary_label;
};


G_DEFINE_TYPE (PhotosErrorBox, photos_error_box, GTK_TYPE_GRID);


static void
photos_error_box_constructed (GObject *object)
{
  PhotosErrorBox *self = PHOTOS_ERROR_BOX (object);

  G_OBJECT_CLASS (photos_error_box_parent_class)->constructed (object);

  gtk_widget_set_halign (GTK_WIDGET (self), GTK_ALIGN_CENTER);
  gtk_widget_set_hexpand (GTK_WIDGET (self), TRUE);
  gtk_widget_set_valign (GTK_WIDGET (self), GTK_ALIGN_CENTER);
  gtk_widget_set_vexpand (GTK_WIDGET (self), TRUE);
  gtk_orientable_set_orientation (GTK_ORIENTABLE (self), GTK_ORIENTATION_VERTICAL);
  gtk_grid_set_row_spacing (GTK_GRID (self), 12);

  self->image = gtk_image_new_from_icon_name ("dialog-error", GTK_ICON_SIZE_INVALID);
  gtk_image_set_pixel_size (GTK_IMAGE (self->image), 128);
  gtk_widget_set_halign (self->image, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (self->image, GTK_ALIGN_CENTER);
  gtk_container_add (GTK_CONTAINER (self), self->image);

  self->primary_label = gtk_label_new (NULL);
  gtk_label_set_use_markup (GTK_LABEL (self->primary_label), TRUE);
  gtk_widget_set_halign (self->primary_label, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (self->primary_label, GTK_ALIGN_CENTER);
  gtk_container_add (GTK_CONTAINER (self), self->primary_label);

  self->secondary_label = gtk_label_new (NULL);
  gtk_label_set_use_markup (GTK_LABEL (self->secondary_label), TRUE);
  gtk_label_set_line_wrap (GTK_LABEL (self->secondary_label), TRUE);
  gtk_widget_set_halign (self->secondary_label, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (self->secondary_label, GTK_ALIGN_CENTER);
  gtk_container_add (GTK_CONTAINER (self), self->secondary_label);

  gtk_widget_show_all (GTK_WIDGET (self));
}


static void
photos_error_box_init (PhotosErrorBox *self)
{
}


static void
photos_error_box_class_init (PhotosErrorBoxClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->constructed = photos_error_box_constructed;
}


GtkWidget *
photos_error_box_new (void)
{
  return g_object_new (PHOTOS_TYPE_ERROR_BOX, NULL);
}


void
photos_error_box_update (PhotosErrorBox *self, const gchar *primary, const gchar *secondary)
{
  if (primary != NULL)
    {
      g_autofree gchar *markup = NULL;

      markup = g_markup_printf_escaped ("<big><b>%s</b></big>", primary);
      gtk_label_set_markup (GTK_LABEL (self->primary_label), markup);
    }

  if (secondary != NULL)
    {
      g_autofree gchar *markup = NULL;

      markup = g_markup_escape_text (secondary, -1);
      gtk_label_set_markup (GTK_LABEL (self->secondary_label), markup);
    }
}
