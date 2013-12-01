/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2014 Red Hat, Inc.
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

#include <gio/gio.h>

#include "photos-base-view.h"
#include "photos-dropdown.h"
#include "photos-search-context.h"


struct _PhotosDropdownPrivate
{
  GtkWidget *grid;
  GtkWidget *match_view;
  GtkWidget *source_view;
  GtkWidget *type_view;
  PhotosBaseManager *srch_mtch_mngr;
  PhotosBaseManager *srch_typ_mngr;
  PhotosBaseManager *src_mngr;
};

enum
{
  ITEM_ACTIVATED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };


G_DEFINE_TYPE_WITH_PRIVATE (PhotosDropdown, photos_dropdown, GTK_TYPE_REVEALER);


static void
photos_dropdown_item_activated (PhotosDropdown *self)
{
  g_signal_emit (self, signals[ITEM_ACTIVATED], 0);
}


static void
photos_dropdown_dispose (GObject *object)
{
  PhotosDropdown *self = PHOTOS_DROPDOWN (object);
  PhotosDropdownPrivate *priv = self->priv;

  g_clear_object (&priv->srch_mtch_mngr);
  g_clear_object (&priv->srch_typ_mngr);
  g_clear_object (&priv->src_mngr);

  G_OBJECT_CLASS (photos_dropdown_parent_class)->dispose (object);
}


static void
photos_dropdown_init (PhotosDropdown *self)
{
  PhotosDropdownPrivate *priv;
  GApplication *app;
  GtkStyleContext *context;
  GtkWidget *frame;
  PhotosSearchContextState *state;

  self->priv = photos_dropdown_get_instance_private (self);
  priv = self->priv;

  app = g_application_get_default ();
  state = photos_search_context_get_state (PHOTOS_SEARCH_CONTEXT (app));

  priv->srch_mtch_mngr = g_object_ref (state->srch_mtch_mngr);
  priv->srch_typ_mngr = g_object_ref (state->srch_typ_mngr);
  priv->src_mngr = g_object_ref (state->src_mngr);

  priv->match_view = photos_base_view_new (priv->srch_mtch_mngr);
  priv->source_view = photos_base_view_new (priv->src_mngr);
  priv->type_view = photos_base_view_new (priv->srch_typ_mngr);

  g_signal_connect_swapped (priv->match_view, "item-activated", G_CALLBACK (photos_dropdown_item_activated), self);
  g_signal_connect_swapped (priv->source_view, "item-activated", G_CALLBACK (photos_dropdown_item_activated), self);
  g_signal_connect_swapped (priv->type_view, "item-activated", G_CALLBACK (photos_dropdown_item_activated), self);

  frame = gtk_frame_new (NULL);
  gtk_widget_set_opacity (frame, 0.9);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
  context = gtk_widget_get_style_context (frame);
  gtk_style_context_add_class (context, "documents-dropdown");
  gtk_container_add (GTK_CONTAINER (self), frame);

  priv->grid = gtk_grid_new ();
  gtk_orientable_set_orientation (GTK_ORIENTABLE (priv->grid), GTK_ORIENTATION_HORIZONTAL);
  gtk_container_add (GTK_CONTAINER (frame), priv->grid);

  gtk_container_add (GTK_CONTAINER (priv->grid), priv->source_view);
  gtk_container_add (GTK_CONTAINER (priv->grid), priv->type_view);
  gtk_container_add (GTK_CONTAINER (priv->grid), priv->match_view);

  photos_dropdown_hide (self);
  gtk_widget_show_all (GTK_WIDGET (self));
}


static void
photos_dropdown_class_init (PhotosDropdownClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->dispose = photos_dropdown_dispose;

  signals[ITEM_ACTIVATED] = g_signal_new ("item-activated",
                                          G_TYPE_FROM_CLASS (class),
                                          G_SIGNAL_RUN_LAST,
                                          G_STRUCT_OFFSET (PhotosDropdownClass,
                                                           item_activated),
                                          NULL, /*accumulator */
                                          NULL, /*accu_data */
                                          g_cclosure_marshal_VOID__VOID,
                                          G_TYPE_NONE,
                                          0);
}


GtkWidget *
photos_dropdown_new (void)
{
  return g_object_new (PHOTOS_TYPE_DROPDOWN, "halign", GTK_ALIGN_CENTER, "valign", GTK_ALIGN_START, NULL);
}


void
photos_dropdown_hide (PhotosDropdown *self)
{
  gtk_revealer_set_reveal_child (GTK_REVEALER (self), FALSE);
}


void
photos_dropdown_show (PhotosDropdown *self)
{
  gtk_revealer_set_reveal_child (GTK_REVEALER (self), TRUE);
}
