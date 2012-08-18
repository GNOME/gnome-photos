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

#include <clutter/clutter.h>
#include <glib.h>
#include <gtk/gtk.h>

#include "photos-error-box.h"


struct _PhotosErrorBoxPrivate
{
  GtkWidget *image;
  GtkWidget *primary_label;
  GtkWidget *secondary_label;
};


G_DEFINE_TYPE (PhotosErrorBox, photos_error_box, GTK_CLUTTER_TYPE_ACTOR);


static void
photos_error_box_move_out_completed (PhotosErrorBox *self)
{
  ClutterActor *parent;

  parent = clutter_actor_get_parent (CLUTTER_ACTOR (self));
  if (parent == NULL)
    return;

  clutter_actor_set_child_below_sibling (parent, CLUTTER_ACTOR (self), NULL);
}


static void
photos_error_box_dispose (GObject *object)
{
  PhotosErrorBox *self = PHOTOS_ERROR_BOX (object);
  PhotosErrorBoxPrivate *priv = self->priv;

  G_OBJECT_CLASS (photos_error_box_parent_class)->dispose (object);
}


static void
photos_error_box_init (PhotosErrorBox *self)
{
  PhotosErrorBoxPrivate *priv;
  GtkWidget *bin;
  GtkWidget *widget;

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, PHOTOS_TYPE_ERROR_BOX, PhotosErrorBoxPrivate);
  priv = self->priv;

  clutter_actor_set_x_expand (CLUTTER_ACTOR (self), TRUE);
  clutter_actor_set_y_expand (CLUTTER_ACTOR (self), TRUE);

  widget = gtk_grid_new ();
  gtk_widget_set_halign (widget, GTK_ALIGN_CENTER);
  gtk_widget_set_hexpand (widget, TRUE);
  gtk_widget_set_valign (widget, GTK_ALIGN_CENTER);
  gtk_widget_set_vexpand (widget, TRUE);
  gtk_orientable_set_orientation (GTK_ORIENTABLE (widget), GTK_ORIENTATION_VERTICAL);
  gtk_grid_set_row_spacing (GTK_GRID (widget), 12);

  priv->image = gtk_image_new_from_icon_name ("dialog-error", GTK_ICON_SIZE_INVALID);
  gtk_image_set_pixel_size (GTK_IMAGE (priv->image), 128);
  gtk_widget_set_halign (priv->image, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (priv->image, GTK_ALIGN_CENTER);
  gtk_container_add (GTK_CONTAINER (widget), priv->image);

  priv->primary_label = gtk_label_new (NULL);
  gtk_label_set_use_markup (GTK_LABEL (priv->primary_label), TRUE);
  gtk_widget_set_halign (priv->primary_label, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (priv->primary_label, GTK_ALIGN_CENTER);
  gtk_container_add (GTK_CONTAINER (widget), priv->primary_label);

  priv->secondary_label = gtk_label_new (NULL);
  gtk_label_set_use_markup (GTK_LABEL (priv->secondary_label), TRUE);
  gtk_widget_set_halign (priv->secondary_label, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (priv->secondary_label, GTK_ALIGN_CENTER);
  gtk_container_add (GTK_CONTAINER (widget), priv->secondary_label);

  gtk_widget_show_all (widget);

  bin = gtk_clutter_actor_get_widget (GTK_CLUTTER_ACTOR (self));
  gtk_container_add (GTK_CONTAINER (bin), widget);
}


static void
photos_error_box_class_init (PhotosErrorBoxClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->dispose = photos_error_box_dispose;

  g_type_class_add_private (class, sizeof (PhotosErrorBoxPrivate));
}


ClutterActor *
photos_error_box_new (void)
{
  return g_object_new (PHOTOS_TYPE_ERROR_BOX, NULL);
}


void
photos_error_box_move_in (PhotosErrorBox *self)
{
  ClutterActor *parent;

  parent = clutter_actor_get_parent (CLUTTER_ACTOR (self));
  if (parent == NULL)
    return;

  clutter_actor_set_child_above_sibling (parent, CLUTTER_ACTOR (self), NULL);
  clutter_actor_animate (CLUTTER_ACTOR (self), CLUTTER_EASE_OUT_QUAD, 300, "opacity", 255, NULL);
}


void
photos_error_box_move_out (PhotosErrorBox *self)
{
  ClutterAnimation *animation;

  animation = clutter_actor_animate (CLUTTER_ACTOR (self), CLUTTER_EASE_OUT_QUAD, 300, "opacity", 0, NULL);
  g_signal_connect_swapped (animation, "completed", G_CALLBACK (photos_error_box_move_out_completed), self);
}


void
photos_error_box_update (PhotosErrorBox *self, const gchar *primary, const gchar *secondary)
{
  PhotosErrorBoxPrivate *priv = self->priv;
  gchar *markup;

  if (primary != NULL)
    {
      markup = g_markup_printf_escaped ("<big><b>%s</b></big>", primary);
      gtk_label_set_markup (GTK_LABEL (priv->primary_label), markup);
      g_free (markup);
    }

  if (secondary != NULL)
    {
      markup = g_markup_escape_text (secondary, -1);
      gtk_label_set_markup (GTK_LABEL (priv->secondary_label), markup);
      g_free (markup);
    }
}
