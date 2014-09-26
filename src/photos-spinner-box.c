/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2012, 2013, 2014 Red Hat, Inc.
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
#include <glib/gi18n.h>

#include "photos-spinner-box.h"


struct _PhotosSpinnerBoxPrivate
{
  GtkWidget *spinner;
};


G_DEFINE_TYPE_WITH_PRIVATE (PhotosSpinnerBox, photos_spinner_box, GTK_TYPE_REVEALER);


static void
photos_spinner_box_constructed (GObject *object)
{
  PhotosSpinnerBox *self = PHOTOS_SPINNER_BOX (object);
  PhotosSpinnerBoxPrivate *priv = self->priv;

  G_OBJECT_CLASS (photos_spinner_box_parent_class)->constructed (object);

  priv->spinner = gtk_spinner_new ();
  gtk_widget_set_size_request (priv->spinner, 128, 128);
  gtk_widget_set_halign (priv->spinner, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (priv->spinner, GTK_ALIGN_CENTER);
  gtk_container_add (GTK_CONTAINER (self), priv->spinner);

  gtk_widget_show_all (GTK_WIDGET (self));
}


static void
photos_spinner_box_init (PhotosSpinnerBox *self)
{
  self->priv = photos_spinner_box_get_instance_private (self);
}


static void
photos_spinner_box_class_init (PhotosSpinnerBoxClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->constructed = photos_spinner_box_constructed;
}


GtkWidget *
photos_spinner_box_new (void)
{
  return g_object_new (PHOTOS_TYPE_SPINNER_BOX,
                       "halign", GTK_ALIGN_CENTER,
                       "transition-type", GTK_REVEALER_TRANSITION_TYPE_CROSSFADE,
                       "valign", GTK_ALIGN_CENTER,
                       NULL);
}


void
photos_spinner_box_start (PhotosSpinnerBox *self)
{
  gtk_revealer_set_reveal_child (GTK_REVEALER (self), TRUE);
  gtk_spinner_start (GTK_SPINNER (self->priv->spinner));
}


void
photos_spinner_box_stop (PhotosSpinnerBox *self)
{
  gtk_revealer_set_reveal_child (GTK_REVEALER (self), FALSE);
  gtk_spinner_stop (GTK_SPINNER (self->priv->spinner));
}
