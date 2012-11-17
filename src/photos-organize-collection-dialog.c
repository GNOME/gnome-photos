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

#include <glib/gi18n.h>

#include "photos-organize-collection-dialog.h"
#include "photos-organize-collection-view.h"


struct _PhotosOrganizeCollectionDialogPrivate
{
  GtkWidget *coll_view;
};


G_DEFINE_TYPE (PhotosOrganizeCollectionDialog, photos_organize_collection_dialog, GTK_TYPE_DIALOG);


static gboolean
photos_organize_collection_dialog_button_press_event (GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
  PhotosOrganizeCollectionDialog *self = PHOTOS_ORGANIZE_COLLECTION_DIALOG (widget);

  photos_organize_collection_view_confirmed_choice (PHOTOS_ORGANIZE_COLLECTION_VIEW (self->priv->coll_view));
  return FALSE;
}


static void
photos_organize_collection_dialog_response (GtkDialog *dialog, gint response_id)
{
  PhotosOrganizeCollectionDialog *self = PHOTOS_ORGANIZE_COLLECTION_DIALOG (dialog);

  if (response_id != GTK_RESPONSE_ACCEPT)
    return;

  photos_organize_collection_view_add_collection (PHOTOS_ORGANIZE_COLLECTION_VIEW (self->priv->coll_view));
}


static void
photos_organize_collection_dialog_init (PhotosOrganizeCollectionDialog *self)
{
  PhotosOrganizeCollectionDialogPrivate *priv;
  GtkWidget *content_area;
  GtkWidget *ok_button;
  GtkWidget *sw;

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                            PHOTOS_TYPE_ORGANIZE_COLLECTION_DIALOG,
                                            PhotosOrganizeCollectionDialogPrivate);
  priv = self->priv;

  ok_button = gtk_dialog_add_button (GTK_DIALOG (self), GTK_STOCK_ADD, GTK_RESPONSE_ACCEPT);
  gtk_dialog_add_button (GTK_DIALOG (self), GTK_STOCK_OK, GTK_RESPONSE_OK);
  gtk_dialog_set_default_response (GTK_DIALOG (self), GTK_RESPONSE_OK);

  content_area = gtk_dialog_get_content_area (GTK_DIALOG (self));
  sw = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw), GTK_SHADOW_IN);
  gtk_widget_set_margin_left (sw, 5);
  gtk_widget_set_margin_right (sw, 5);
  gtk_widget_set_margin_bottom (sw, 3);

  priv->coll_view = photos_organize_collection_view_new ();
  gtk_container_add (GTK_CONTAINER (sw), priv->coll_view);
  gtk_container_add (GTK_CONTAINER (content_area), sw);

  /* HACK:
   * - We want clicking on "OK" to add the typed-in collection if
   *   we're editing.
   * - Unfortunately, since we focus out of the editable entry in
   *   order to click the button, we'll get an editing-canceled signal
   *   on the renderer from GTK+. As this handler will run before
   *   focus-out, we here signal the view to ignore the next
   *   editing-canceled signal and add the collection in that case
   *   instead.
   */
  g_signal_connect (ok_button,
                    "button-press-event",
                    G_CALLBACK (photos_organize_collection_dialog_button_press_event),
                    self);

  gtk_widget_show_all (GTK_WIDGET (self));
}


static void
photos_organize_collection_dialog_class_init (PhotosOrganizeCollectionDialogClass *class)
{
  GtkDialogClass *dialog_class = GTK_DIALOG_CLASS (class);

  dialog_class->response = photos_organize_collection_dialog_response;

  g_type_class_add_private (class, sizeof (PhotosOrganizeCollectionDialogPrivate));
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
                       "title", _("Organize"),
                       "transient-for", parent,
                       NULL);
}
