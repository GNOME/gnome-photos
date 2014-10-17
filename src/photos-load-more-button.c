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
#include <glib/gprintf.h>

#include "photos-enums.h"
#include "photos-load-more-button.h"
#include "photos-offset-collections-controller.h"
#include "photos-offset-favorites-controller.h"
#include "photos-offset-overview-controller.h"
#include "photos-offset-search-controller.h"


struct _PhotosLoadMoreButtonPrivate
{
  GtkWidget *label;
  GtkWidget *spinner;
  PhotosOffsetController *offset_cntrlr;
  PhotosWindowMode mode;
  gboolean block;
};

enum
{
  PROP_0,
  PROP_MODE
};


G_DEFINE_TYPE_WITH_PRIVATE (PhotosLoadMoreButton, photos_load_more_button, GTK_TYPE_BUTTON);


static void
photos_load_more_button_count_changed (PhotosLoadMoreButton *self)
{
  PhotosLoadMoreButtonPrivate *priv = self->priv;
  gboolean visible;
  gint remaining;

  remaining = photos_offset_controller_get_remaining (priv->offset_cntrlr);
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

  gtk_label_set_label (GTK_LABEL (priv->label), _("Loading…"));
  gtk_widget_show (priv->spinner);
  gtk_spinner_start (GTK_SPINNER (priv->spinner));

  photos_offset_controller_increase_offset (self->priv->offset_cntrlr);
}


static void
photos_load_more_button_constructed (GObject *object)
{
  PhotosLoadMoreButton *self = PHOTOS_LOAD_MORE_BUTTON (object);
  PhotosLoadMoreButtonPrivate *priv = self->priv;

  G_OBJECT_CLASS (photos_load_more_button_parent_class)->constructed (object);

  switch (priv->mode)
    {
    case PHOTOS_WINDOW_MODE_COLLECTIONS:
      priv->offset_cntrlr = photos_offset_collections_controller_dup_singleton ();
      break;

    case PHOTOS_WINDOW_MODE_FAVORITES:
      priv->offset_cntrlr = photos_offset_favorites_controller_dup_singleton ();
      break;

    case PHOTOS_WINDOW_MODE_OVERVIEW:
      priv->offset_cntrlr = photos_offset_overview_controller_dup_singleton ();
      break;

    case PHOTOS_WINDOW_MODE_SEARCH:
      priv->offset_cntrlr = photos_offset_search_controller_dup_singleton ();
      break;

    default:
      g_assert_not_reached ();
      break;
    }

  g_signal_connect_object (priv->offset_cntrlr,
                           "count-changed",
                           G_CALLBACK (photos_load_more_button_count_changed),
                           self,
                           G_CONNECT_SWAPPED);

  photos_load_more_button_count_changed (self);
}


static void
photos_load_more_button_dispose (GObject *object)
{
  PhotosLoadMoreButton *self = PHOTOS_LOAD_MORE_BUTTON (object);
  PhotosLoadMoreButtonPrivate *priv = self->priv;

  g_clear_object (&priv->offset_cntrlr);

  G_OBJECT_CLASS (photos_load_more_button_parent_class)->dispose (object);
}


static void
photos_load_more_button_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
  PhotosLoadMoreButton *self = PHOTOS_LOAD_MORE_BUTTON (object);

  switch (prop_id)
    {
    case PROP_MODE:
      self->priv->mode = (PhotosWindowMode) g_value_get_enum (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
photos_load_more_button_init (PhotosLoadMoreButton *self)
{
  PhotosLoadMoreButtonPrivate *priv;
  GtkStyleContext *context;
  GtkWidget *child;

  self->priv = photos_load_more_button_get_instance_private (self);
  priv = self->priv;

  gtk_widget_set_no_show_all (GTK_WIDGET (self), TRUE);
  context = gtk_widget_get_style_context (GTK_WIDGET (self));
  gtk_style_context_add_class (context, "photos-load-more");

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
}


static void
photos_load_more_button_class_init (PhotosLoadMoreButtonClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkButtonClass *button_class = GTK_BUTTON_CLASS (class);

  object_class->constructed = photos_load_more_button_constructed;
  object_class->dispose = photos_load_more_button_dispose;
  object_class->set_property = photos_load_more_button_set_property;
  button_class->clicked = photos_load_more_button_clicked;

  g_object_class_install_property (object_class,
                                   PROP_MODE,
                                   g_param_spec_enum ("mode",
                                                      "PhotosWindowMode enum",
                                                      "The mode for which the widget is a load button",
                                                      PHOTOS_TYPE_WINDOW_MODE,
                                                      PHOTOS_WINDOW_MODE_NONE,
                                                      G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE));
}


GtkWidget *
photos_load_more_button_new (PhotosWindowMode mode)
{
  return g_object_new (PHOTOS_TYPE_LOAD_MORE_BUTTON, "mode", mode, NULL);
}


void
photos_load_more_button_set_block (PhotosLoadMoreButton *self, gboolean block)
{
  PhotosLoadMoreButtonPrivate *priv = self->priv;

  if (priv->block == block)
    return;

  priv->block = block;
  photos_load_more_button_count_changed (self);
}
