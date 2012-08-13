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

#include <clutter/clutter.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "photos-spinner-box.h"


struct _PhotosSpinnerBoxPrivate
{
  guint delayed_move_id;
};


G_DEFINE_TYPE (PhotosSpinnerBox, photos_spinner_box, GTK_CLUTTER_TYPE_ACTOR);


static void
photos_spinner_box_clear_delay_id (PhotosSpinnerBox *self)
{
  PhotosSpinnerBoxPrivate *priv = self->priv;

  if (priv->delayed_move_id != 0)
    {
      g_source_remove (priv->delayed_move_id);
      priv->delayed_move_id = 0;
    }
}


static gboolean
photos_spinner_box_move_in_delayed_timeout (gpointer user_data)
{
  PhotosSpinnerBox *self = PHOTOS_SPINNER_BOX (user_data);

  self->priv->delayed_move_id = 0;
  photos_spinner_box_move_in (self);
  return G_SOURCE_REMOVE;
}


static void
photos_spinner_box_move_out_completed (PhotosSpinnerBox *self)
{
  ClutterActor *parent;

  parent = clutter_actor_get_parent (CLUTTER_ACTOR (self));
  if (parent == NULL)
    return;

  clutter_actor_set_child_below_sibling (parent, CLUTTER_ACTOR (self), NULL);
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
  PhotosSpinnerBoxPrivate *priv;
  GtkWidget *bin;
  GtkWidget *label;
  GtkWidget *spinner;
  GtkWidget *widget;
  gchar *text;

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, PHOTOS_TYPE_SPINNER_BOX, PhotosSpinnerBoxPrivate);
  priv = self->priv;

  clutter_actor_set_x_expand (CLUTTER_ACTOR (self), TRUE);
  clutter_actor_set_y_expand (CLUTTER_ACTOR (self), TRUE);

  widget = gtk_grid_new ();
  gtk_widget_set_halign (widget, GTK_ALIGN_CENTER);
  gtk_widget_set_hexpand (widget, TRUE);
  gtk_widget_set_valign (widget, GTK_ALIGN_CENTER);
  gtk_widget_set_vexpand (widget, TRUE);
  gtk_orientable_set_orientation (GTK_ORIENTABLE (widget), GTK_ORIENTATION_VERTICAL);
  gtk_grid_set_row_spacing (GTK_GRID (widget), 24);

  spinner = gtk_spinner_new ();
  gtk_widget_set_size_request (spinner, 128, 128);
  gtk_widget_set_halign (spinner, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (spinner, GTK_ALIGN_CENTER);
  gtk_spinner_start (GTK_SPINNER (spinner));
  gtk_container_add (GTK_CONTAINER (widget), spinner);

  label = gtk_label_new (NULL);

  gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
  text = g_strconcat ("<big><b>", _("Loading..."), "</b></big>", NULL);
  gtk_label_set_label (GTK_LABEL (label), text);
  g_free (text);

  gtk_widget_set_halign (label, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
  gtk_container_add (GTK_CONTAINER (widget), label);

  gtk_widget_show_all (widget);

  bin = gtk_clutter_actor_get_widget (GTK_CLUTTER_ACTOR (self));
  gtk_container_add (GTK_CONTAINER (bin), widget);
}


static void
photos_spinner_box_class_init (PhotosSpinnerBoxClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->dispose = photos_spinner_box_dispose;

  g_type_class_add_private (class, sizeof (PhotosSpinnerBoxPrivate));
}


ClutterActor *
photos_spinner_box_new (void)
{
  return g_object_new (PHOTOS_TYPE_SPINNER_BOX, NULL);
}


void
photos_spinner_box_move_in (PhotosSpinnerBox *self)
{
  ClutterActor *parent;

  photos_spinner_box_clear_delay_id (self);
  parent = clutter_actor_get_parent (CLUTTER_ACTOR (self));
  if (parent == NULL)
    return;

  clutter_actor_set_child_above_sibling (parent, CLUTTER_ACTOR (self), NULL);
  clutter_actor_animate (CLUTTER_ACTOR (self), CLUTTER_EASE_OUT_QUAD, 300, "opacity", 255, NULL);
}


void
photos_spinner_box_move_out (PhotosSpinnerBox *self)
{
  ClutterAnimation *animation;

  photos_spinner_box_clear_delay_id (self);
  animation = clutter_actor_animate (CLUTTER_ACTOR (self), CLUTTER_EASE_OUT_QUAD, 300, "opacity", 0, NULL);
  g_signal_connect_swapped (animation, "completed", G_CALLBACK (photos_spinner_box_move_out_completed), self);
}


void
photos_spinner_box_move_in_delayed (PhotosSpinnerBox *self, guint delay)
{
  photos_spinner_box_clear_delay_id (self);
  self->priv->delayed_move_id = g_timeout_add (delay, photos_spinner_box_move_in_delayed_timeout, self);
}
