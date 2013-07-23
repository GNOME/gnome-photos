/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2013 Red Hat, Inc.
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
 *   + Clocks
 */


#include "config.h"

#include "photos-header-bar.h"


struct _PhotosHeaderBarPrivate
{
  GtkWidget *selection_menu;
  GtkWidget *stack_switcher;
  PhotosHeaderBarMode mode;
};


G_DEFINE_TYPE (PhotosHeaderBar, photos_header_bar, GTK_TYPE_HEADER_BAR);


static void
photos_header_bar_realize (GtkWidget *widget)
{
  PhotosHeaderBar *self = PHOTOS_HEADER_BAR (widget);
  PhotosHeaderBarPrivate *priv = self->priv;

  GTK_WIDGET_CLASS (photos_header_bar_parent_class)->realize (widget);

  if (priv->mode == PHOTOS_HEADER_BAR_MODE_NONE)
    photos_header_bar_set_mode (self, PHOTOS_HEADER_BAR_MODE_NORMAL);
}


static void
photos_header_bar_dispose (GObject *object)
{
  PhotosHeaderBar *self = PHOTOS_HEADER_BAR (object);
  PhotosHeaderBarPrivate *priv = self->priv;

  g_clear_object (&priv->selection_menu);
  g_clear_object (&priv->stack_switcher);

  G_OBJECT_CLASS (photos_header_bar_parent_class)->dispose (object);
}


static void
photos_header_bar_init (PhotosHeaderBar *self)
{
  PhotosHeaderBarPrivate *priv;

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, PHOTOS_TYPE_HEADER_BAR, PhotosHeaderBarPrivate);
  priv = self->priv;

  priv->stack_switcher = g_object_ref_sink (gtk_stack_switcher_new ());
}


static void
photos_header_bar_class_init (PhotosHeaderBarClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->dispose = photos_header_bar_dispose;
  widget_class->realize = photos_header_bar_realize;

  g_type_class_add_private (class, sizeof (PhotosHeaderBarPrivate));
}


GtkWidget *
photos_header_bar_new (void)
{
  return g_object_new (PHOTOS_TYPE_HEADER_BAR, NULL);
}


void
photos_header_bar_clear (PhotosHeaderBar *self)
{
  GList *children;
  GList *l;

  photos_header_bar_set_mode (self, PHOTOS_HEADER_BAR_MODE_NORMAL);
  children = gtk_container_get_children (GTK_CONTAINER (self));
  for (l = children; l != NULL; l = l->next)
    gtk_widget_destroy (GTK_WIDGET (l->data));
}


void
photos_header_bar_set_mode (PhotosHeaderBar *self, PhotosHeaderBarMode mode)
{
  PhotosHeaderBarPrivate *priv = self->priv;
  GtkStyleContext *context;
  GtkWidget *custom_title = NULL;

  if (priv->mode == mode || !gtk_widget_get_realized (GTK_WIDGET (self)))
    return;

  priv->mode = mode;
  context = gtk_widget_get_style_context (GTK_WIDGET (self));

  switch (priv->mode)
    {
    case PHOTOS_HEADER_BAR_MODE_NORMAL:
      gtk_style_context_remove_class (context, "selection-mode");
      custom_title = priv->stack_switcher;
      break;

    case PHOTOS_HEADER_BAR_MODE_SELECTION:
      gtk_style_context_add_class (context, "selection-mode");
      custom_title = priv->selection_menu;
      break;

    case PHOTOS_HEADER_BAR_MODE_STANDALONE:
      gtk_style_context_remove_class (context, "selection-mode");
      break;

    default:
      g_assert_not_reached ();
    }

  gtk_header_bar_set_custom_title (GTK_HEADER_BAR (self), custom_title);
}


void
photos_header_bar_set_selection_menu (PhotosHeaderBar *self, GdHeaderButton *selection_menu)
{
  PhotosHeaderBarPrivate *priv = self->priv;

  if (priv->selection_menu == GTK_WIDGET (selection_menu))
    return;

  g_clear_object (&priv->selection_menu);
  if (selection_menu != NULL)
    {
      GtkStyleContext *context;

      priv->selection_menu = g_object_ref_sink (selection_menu);
      context = gtk_widget_get_style_context (priv->selection_menu);
      gtk_style_context_add_class (context, "selection-menu");
    }

  if (priv->mode == PHOTOS_HEADER_BAR_MODE_SELECTION)
    gtk_header_bar_set_custom_title (GTK_HEADER_BAR (self), priv->selection_menu);
}


void
photos_header_bar_set_stack (PhotosHeaderBar *self, GtkStack *stack)
{
  gtk_stack_switcher_set_stack (GTK_STACK_SWITCHER (self->priv->stack_switcher), stack);
}
