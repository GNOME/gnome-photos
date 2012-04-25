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


#include "config.h"

#include "photos-organize-collection-dialog.h"


G_DEFINE_TYPE (PhotosOrganizeCollectionDialog, photos_organize_collection_dialog, GTK_TYPE_DIALOG);


static void
photos_organize_collection_dialog_response (GtkDialog *dialog, gint response_id)
{
  if (response_id != GTK_RESPONSE_ACCEPT)
    return;

  /* TODO: OrganizeCollectionView */
}


static void
photos_organize_collection_dialog_init (PhotosOrganizeCollectionDialog *self)
{
  PhotosOrganizeCollectionDialogPrivate *priv;
  GtkWidget *content_area;
  GtkWidget *sw;

  gtk_dialog_add_button (GTK_DIALOG (self), GTK_STOCK_ADD, GTK_RESPONSE_ACCEPT);
  gtk_dialog_add_button (GTK_DIALOG (self), GTK_STOCK_OK, GTK_RESPONSE_OK);
  gtk_dialog_set_default_response (GTK_DIALOG (self), GTK_RESPONSE_OK);

  content_area = gtk_dialog_get_content_area (GTK_DIALOG (self));
  sw = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw), GTK_SHADOW_IN);
  gtk_widget_set_margin_left (sw, 5);
  gtk_widget_set_margin_right (sw, 5);
  gtk_widget_set_margin_bottom (sw, 3);

  /* TODO: OrganizeCollectionView */

  gtk_container_add (GTK_CONTAINER (content_area), sw);
  gtk_widget_show_all (GTK_WIDGET (self));
}


static void
photos_organize_collection_dialog_class_init (PhotosOrganizeCollectionDialogClass *class)
{
  GtkDialogClass *dialog_class = GTK_DIALOG_CLASS (class);

  dialog_class->response = photos_organize_collection_dialog_response;
}


GtkWidget *
photos_organize_collection_dialog_new (GtkWindow *parent)
{
  g_return_val_if_fail (GTK_IS_WINDOW (parent), NULL);

  return g_object_new (PHOTOS_TYPE_ORGANIZE_COLLECTION_DIALOG,
                       "default-width", 400,
                       "default-height", 250,
                       "destroy-with-parent", TRUE,
                       "modal", TRUE,
                       "transient-for", parent,
                       NULL);
}
