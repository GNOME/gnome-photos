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

#include <clutter-gtk/clutter-gtk.h>
#include <gtk/gtk.h>

#include "photos-main-toolbar.h"
#include "photos-mode-controller.h"
#include "photos-view-embed.h"


struct _PhotosViewEmbedPrivate
{
  ClutterActor *background;
  ClutterActor *contents_actor;
  ClutterActor *notebook_actor;
  ClutterActor *view_actor;
  ClutterLayoutManager *contents_layout;
  ClutterLayoutManager *view_layout;
  GtkWidget *notebook;
  PhotosMainToolbar *toolbar;
  PhotosModeController *mode_cntrlr;
};


G_DEFINE_TYPE (PhotosViewEmbed, photos_view_embed, CLUTTER_TYPE_BOX);


static void
photos_view_embed_fullscreen_changed (PhotosModeController *mode_cntrlr, gboolean fullscreen, gpointer user_data)
{
}


static void
photos_view_embed_window_mode_changed (PhotosModeController *mode_cntrlr,
                                       PhotosWindowMode mode,
                                       PhotosWindowMode old_mode,
                                       gpointer user_data)
{
}


static void
photos_view_embed_dispose (GObject *object)
{
  PhotosViewEmbed *self = PHOTOS_VIEW_EMBED (object);
  PhotosViewEmbedPrivate *priv = self->priv;

  if (priv->mode_cntrlr != NULL)
    {
      g_object_unref (priv->mode_cntrlr);
      priv->mode_cntrlr = NULL;
    }

  G_OBJECT_CLASS (photos_view_embed_parent_class)->dispose (object);
}


static void
photos_view_embed_init (PhotosViewEmbed *self)
{
  PhotosViewEmbedPrivate *priv;
  ClutterActor *toolbar_actor;
  ClutterColor color = {255, 255, 255, 255};

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, PHOTOS_TYPE_VIEW_EMBED, PhotosViewEmbedPrivate);
  priv = self->priv;

  priv->contents_layout = clutter_box_layout_new ();
  clutter_box_layout_set_vertical (CLUTTER_BOX_LAYOUT (priv->contents_layout), TRUE);
  priv->contents_actor = clutter_box_new (priv->contents_layout);

  priv->toolbar = photos_main_toolbar_new ();
  toolbar_actor = photos_main_toolbar_get_actor (priv->toolbar);
  clutter_container_add_actor (CLUTTER_CONTAINER (priv->contents_actor), toolbar_actor);
  clutter_box_layout_set_fill (CLUTTER_BOX_LAYOUT (priv->contents_layout), toolbar_actor, TRUE, FALSE);

  priv->view_layout = clutter_bin_layout_new (CLUTTER_BIN_ALIGNMENT_CENTER, CLUTTER_BIN_ALIGNMENT_CENTER);
  priv->view_actor = clutter_box_new (priv->view_layout);
  clutter_box_layout_set_expand (CLUTTER_BOX_LAYOUT (priv->contents_layout), priv->view_actor, TRUE);
  clutter_box_layout_set_fill (CLUTTER_BOX_LAYOUT (priv->contents_layout), priv->view_actor, TRUE, TRUE);
  clutter_container_add_actor (CLUTTER_CONTAINER (priv->contents_actor), priv->view_actor);

  priv->notebook = gtk_notebook_new ();
  gtk_notebook_set_show_border (GTK_NOTEBOOK (priv->notebook), FALSE);
  gtk_notebook_set_show_tabs (GTK_NOTEBOOK (priv->notebook), FALSE);
  gtk_widget_show (priv->notebook);

  priv->notebook_actor = gtk_clutter_actor_new_with_contents (priv->notebook);
  clutter_bin_layout_add (CLUTTER_BIN_LAYOUT (priv->view_layout),
                          priv->notebook_actor,
                          CLUTTER_BIN_ALIGNMENT_FILL,
                          CLUTTER_BIN_ALIGNMENT_FILL);

  /* TODO: SpinnerBox */

  priv->background = clutter_rectangle_new_with_color (&color);
  clutter_bin_layout_add (CLUTTER_BIN_LAYOUT (priv->view_layout),
                          priv->background,
                          CLUTTER_BIN_ALIGNMENT_FILL,
                          CLUTTER_BIN_ALIGNMENT_FILL);
  clutter_actor_lower_bottom (priv->background);

  /* TODO: SearchBar.Dropdown,
   *       SelectionToolbar,
   *       ...
   */

  priv->mode_cntrlr = photos_mode_controller_new ();
  g_signal_connect (priv->mode_cntrlr,
                    "window-mode-changed",
                    G_CALLBACK (photos_view_embed_window_mode_changed),
                    self);
  g_signal_connect (priv->mode_cntrlr,
                    "fullscreen-changed",
                    G_CALLBACK (photos_view_embed_fullscreen_changed),
                    self);
}


static void
photos_view_embed_class_init (PhotosViewEmbedClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->dispose = photos_view_embed_dispose;

  g_type_class_add_private (class, sizeof (PhotosViewEmbedPrivate));
}


ClutterActor *
photos_view_embed_new (ClutterBinLayout *layout)
{
  PhotosViewEmbed *self;
  ClutterLayoutManager *overlay_layout;

  g_return_val_if_fail (CLUTTER_IS_BIN_LAYOUT (layout), NULL);
  self = g_object_new (PHOTOS_TYPE_VIEW_EMBED, "layout-manager", CLUTTER_LAYOUT_MANAGER (layout), NULL);

  /* "layout-manager" being a non-construct property we can not use
   * it from the constructed method :-(
   */
  overlay_layout = clutter_actor_get_layout_manager (CLUTTER_ACTOR (self));
  clutter_bin_layout_add (CLUTTER_BIN_LAYOUT (overlay_layout),
                          self->priv->contents_actor,
                          CLUTTER_BIN_ALIGNMENT_FILL,
                          CLUTTER_BIN_ALIGNMENT_FILL);

  return CLUTTER_ACTOR (self);
}
