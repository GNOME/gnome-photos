/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2018 Red Hat, Inc.
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


#include "config.h"

#include "photos-model-button.h"


struct _PhotosModelButton
{
  GtkButton parent_instance;
};


G_DEFINE_TYPE (PhotosModelButton, photos_model_button, GTK_TYPE_BUTTON);


static void
photos_model_button_clicked (GtkButton *button)
{
  GtkWidget *popover;

  popover = gtk_widget_get_ancestor (GTK_WIDGET (button), GTK_TYPE_POPOVER);
  if (popover != NULL)
    gtk_popover_popdown (GTK_POPOVER (popover));

  if (GTK_BUTTON_CLASS (photos_model_button_parent_class)->clicked != NULL)
    GTK_BUTTON_CLASS (photos_model_button_parent_class)->clicked (button);
}


static void
photos_model_button_init (PhotosModelButton *self)
{
  gtk_button_set_relief (GTK_BUTTON (self), GTK_RELIEF_NONE);
}


static void
photos_model_button_class_init (PhotosModelButtonClass *class)
{
  GtkButtonClass *button_class = GTK_BUTTON_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  button_class->clicked = photos_model_button_clicked;

  gtk_widget_class_set_css_name (widget_class, "modelbutton");
}


GtkWidget *
photos_model_button_new (void)
{
  return g_object_new (PHOTOS_TYPE_MODEL_BUTTON, NULL);
}
