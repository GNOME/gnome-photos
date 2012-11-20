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

#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gprintf.h>

#include "photos-load-more-button.h"
#include "photos-offset-controller.h"


struct _PhotosLoadMoreButtonPrivate
{
  GtkWidget *label;
  GtkWidget *spinner;
  PhotosOffsetController *offset_cntrlr;
  gboolean block;
  gulong offset_cntrlr_id;
};


G_DEFINE_TYPE (PhotosLoadMoreButton, photos_load_more_button, GTK_TYPE_BUTTON);


static void
photos_load_more_button_count_changed (PhotosOffsetController *offset_cntrlr, gint count, gpointer user_data)
{
  PhotosLoadMoreButton *self = PHOTOS_LOAD_MORE_BUTTON (user_data);
  PhotosLoadMoreButtonPrivate *priv = self->priv;
  gboolean visible;
  gint remaining;

  remaining = photos_offset_controller_get_remaining (offset_cntrlr);
  visible = !(remaining <= 0 || priv->block);
  gtk_widget_set_visible (GTK_WIDGET (self), visible);

  if (!visible)
    {
      gtk_label_set_label (GTK_LABEL (priv->label), _("Load More"));
      gtk_spinner_stop (GTK_SPINNER (priv->spinner));
      gtk_widget_hide (priv->spinner);
    }
}


static void
photos_load_more_button_clicked (GtkButton *button)
{
  PhotosLoadMoreButton *self = PHOTOS_LOAD_MORE_BUTTON (button);
  PhotosLoadMoreButtonPrivate *priv = self->priv;

  gtk_label_set_label (GTK_LABEL (priv->label), _("Loading..."));
  gtk_widget_show (priv->spinner);
  gtk_spinner_start (GTK_SPINNER (priv->spinner));

  photos_offset_controller_increase_offset (self->priv->offset_cntrlr);
}


static void
photos_load_more_button_dispose (GObject *object)
{
  PhotosLoadMoreButton *self = PHOTOS_LOAD_MORE_BUTTON (object);
  PhotosLoadMoreButtonPrivate *priv = self->priv;

  if (priv->offset_cntrlr_id != 0)
    {
      g_signal_handler_disconnect (priv->offset_cntrlr, priv->offset_cntrlr_id);
      priv->offset_cntrlr_id = 0;
    }

  g_clear_object (&self->priv->offset_cntrlr);

  G_OBJECT_CLASS (photos_load_more_button_parent_class)->dispose (object);
}


static void
photos_load_more_button_init (PhotosLoadMoreButton *self)
{
  PhotosLoadMoreButtonPrivate *priv;
  GtkStyleContext *context;
  GtkWidget *child;
  gint count;

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, PHOTOS_TYPE_LOAD_MORE_BUTTON, PhotosLoadMoreButtonPrivate);
  priv = self->priv;

  gtk_widget_set_no_show_all (GTK_WIDGET (self), TRUE);
  context = gtk_widget_get_style_context (GTK_WIDGET (self));
  gtk_style_context_add_class (context, "documents-load-more");

  child = gtk_grid_new ();
  gtk_widget_set_halign (child, GTK_ALIGN_CENTER);
  gtk_widget_set_hexpand (child, TRUE);
  gtk_widget_set_visible (child, TRUE);
  gtk_grid_set_column_spacing (GTK_GRID (child), 10);
  gtk_container_add (GTK_CONTAINER (self), child);

  priv->spinner = gtk_spinner_new ();
  gtk_widget_set_halign (priv->spinner, GTK_ALIGN_CENTER);
  gtk_widget_set_no_show_all (priv->spinner, TRUE);
  gtk_widget_set_size_request (priv->spinner, 16, 16);
  gtk_container_add (GTK_CONTAINER (child), priv->spinner);

  priv->label = gtk_label_new (_("Load More"));
  gtk_widget_set_visible (priv->label, TRUE);
  gtk_container_add (GTK_CONTAINER (child), priv->label);

  priv->offset_cntrlr = photos_offset_controller_new ();
  priv->offset_cntrlr_id = g_signal_connect (priv->offset_cntrlr,
                                            "count-changed",
                                            G_CALLBACK (photos_load_more_button_count_changed),
                                            self);

  count = photos_offset_controller_get_count (priv->offset_cntrlr);
  photos_load_more_button_count_changed (priv->offset_cntrlr, count, self);
}


static void
photos_load_more_button_class_init (PhotosLoadMoreButtonClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkButtonClass *button_class = GTK_BUTTON_CLASS (class);

  object_class->dispose = photos_load_more_button_dispose;
  button_class->clicked = photos_load_more_button_clicked;

  g_type_class_add_private (class, sizeof (PhotosLoadMoreButtonPrivate));
}


GtkWidget *
photos_load_more_button_new (void)
{
  return g_object_new (PHOTOS_TYPE_LOAD_MORE_BUTTON, NULL);
}


void
photos_load_more_button_set_block (PhotosLoadMoreButton *self, gboolean block)
{
  PhotosLoadMoreButtonPrivate *priv = self->priv;
  gint count;

  if (priv->block == block)
    return;

  priv->block = block;

  count = photos_offset_controller_get_count (priv->offset_cntrlr);
  photos_load_more_button_count_changed (priv->offset_cntrlr, count, self);
}
