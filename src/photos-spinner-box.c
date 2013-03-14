/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2012, 2013 Red Hat, Inc.
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
  guint delayed_show_id;
};


G_DEFINE_TYPE (PhotosSpinnerBox, photos_spinner_box, GTK_TYPE_GRID);


static void
photos_spinner_box_clear_delay_id (PhotosSpinnerBox *self)
{
  PhotosSpinnerBoxPrivate *priv = self->priv;

  if (priv->delayed_show_id != 0)
    {
      g_source_remove (priv->delayed_show_id);
      priv->delayed_show_id = 0;
    }
}


static gboolean
photos_spinner_box_start_delayed_timeout (gpointer user_data)
{
  PhotosSpinnerBox *self = PHOTOS_SPINNER_BOX (user_data);

  self->priv->delayed_show_id = 0;
  photos_spinner_box_start (self);
  return G_SOURCE_REMOVE;
}


static void
photos_spinner_box_constructed (GObject *object)
{
  PhotosSpinnerBox *self = PHOTOS_SPINNER_BOX (object);
  PhotosSpinnerBoxPrivate *priv = self->priv;
  GtkWidget *label;
  gchar *text;

  G_OBJECT_CLASS (photos_spinner_box_parent_class)->constructed (object);

  gtk_widget_set_halign (GTK_WIDGET (self), GTK_ALIGN_CENTER);
  gtk_widget_set_hexpand (GTK_WIDGET (self), TRUE);
  gtk_widget_set_valign (GTK_WIDGET (self), GTK_ALIGN_CENTER);
  gtk_widget_set_vexpand (GTK_WIDGET (self), TRUE);
  gtk_orientable_set_orientation (GTK_ORIENTABLE (self), GTK_ORIENTATION_VERTICAL);
  gtk_grid_set_row_spacing (GTK_GRID (self), 24);

  priv->spinner = gtk_spinner_new ();
  gtk_widget_set_size_request (priv->spinner, 128, 128);
  gtk_widget_set_halign (priv->spinner, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (priv->spinner, GTK_ALIGN_CENTER);
  gtk_container_add (GTK_CONTAINER (self), priv->spinner);

  label = gtk_label_new (NULL);

  gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
  text = g_strconcat ("<big><b>", _("Loading..."), "</b></big>", NULL);
  gtk_label_set_label (GTK_LABEL (label), text);
  g_free (text);

  gtk_widget_set_halign (label, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
  gtk_container_add (GTK_CONTAINER (self), label);

  gtk_widget_show_all (GTK_WIDGET (self));
}


static void
photos_spinner_box_dispose (GObject *object)
{
  PhotosSpinnerBox *self = PHOTOS_SPINNER_BOX (object);

  photos_spinner_box_clear_delay_id (self);

  G_OBJECT_CLASS (photos_spinner_box_parent_class)->dispose (object);
}


static void
photos_spinner_box_init (PhotosSpinnerBox *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, PHOTOS_TYPE_SPINNER_BOX, PhotosSpinnerBoxPrivate);
}


static void
photos_spinner_box_class_init (PhotosSpinnerBoxClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->constructed = photos_spinner_box_constructed;
  object_class->dispose = photos_spinner_box_dispose;

  g_type_class_add_private (class, sizeof (PhotosSpinnerBoxPrivate));
}


GtkWidget *
photos_spinner_box_new (void)
{
  return g_object_new (PHOTOS_TYPE_SPINNER_BOX, NULL);
}


void
photos_spinner_box_start (PhotosSpinnerBox *self)
{
  photos_spinner_box_clear_delay_id (self);
  gtk_spinner_start (GTK_SPINNER (self->priv->spinner));
}


void
photos_spinner_box_stop (PhotosSpinnerBox *self)
{
  photos_spinner_box_clear_delay_id (self);
  gtk_spinner_stop (GTK_SPINNER (self->priv->spinner));
}


void
photos_spinner_box_start_delayed (PhotosSpinnerBox *self, guint delay)
{
  photos_spinner_box_clear_delay_id (self);
  self->priv->delayed_show_id = g_timeout_add (delay, photos_spinner_box_start_delayed_timeout, self);
}
