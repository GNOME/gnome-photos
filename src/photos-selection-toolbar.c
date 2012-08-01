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
#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "photos-item-manager.h"
#include "photos-organize-collection-dialog.h"
#include "photos-selection-controller.h"
#include "photos-selection-toolbar.h"
#include "photos-utils.h"


struct _PhotosSelectionToolbarPrivate
{
  ClutterActor *actor;
  GHashTable *item_listeners;
  GtkToolItem *left_group;
  GtkToolItem *right_group;
  GtkToolItem *separator;
  GtkWidget *left_box;
  GtkWidget *right_box;
  GtkWidget *toolbar_collection;
  GtkWidget *toolbar_favorite;
  GtkWidget *toolbar_open;
  GtkWidget *toolbar_print;
  GtkWidget *toolbar_trash;
  GtkWidget *widget;
  PhotosBaseManager *item_mngr;
  PhotosSelectionController *sel_cntrlr;
  gboolean inside_refresh;
};


G_DEFINE_TYPE (PhotosSelectionToolbar, photos_selection_toolbar, G_TYPE_OBJECT);


static void
photos_selection_toolbar_fade_in (PhotosSelectionToolbar *self)
{
  PhotosSelectionToolbarPrivate *priv = self->priv;
  guint8 opacity;

  opacity = clutter_actor_get_opacity (priv->actor);
  if (opacity != 0)
    return;

  clutter_actor_show (priv->actor);
  clutter_actor_animate (priv->actor, CLUTTER_EASE_OUT_QUAD, 300, "opacity", 255, NULL);
}


static void
photos_selection_toolbar_fade_out (PhotosSelectionToolbar *self)
{
  ClutterAnimation *animation;
  PhotosSelectionToolbarPrivate *priv = self->priv;

  animation = clutter_actor_animate (priv->actor, CLUTTER_EASE_OUT_QUAD, 300, "opacity", 0, NULL);
  g_signal_connect_swapped (animation, "completed", G_CALLBACK (clutter_actor_hide), priv->actor);
}


static void
photos_selection_toolbar_dialog_response (GtkDialog *dialog, gint response_id, gpointer user_data)
{
  PhotosSelectionToolbar *self = PHOTOS_SELECTION_TOOLBAR (user_data);

  if (response_id != GTK_RESPONSE_OK)
    return;

  gtk_widget_destroy (GTK_WIDGET (dialog));
  photos_selection_toolbar_fade_in (self);
}


static void
photos_selection_toolbar_collection_clicked (GtkButton *button, gpointer user_data)
{
  PhotosSelectionToolbar *self = PHOTOS_SELECTION_TOOLBAR (user_data);
  PhotosSelectionToolbarPrivate *priv = self->priv;
  GtkWidget *dialog;
  GtkWidget *toplevel;

  toplevel = gtk_widget_get_toplevel (priv->widget);
  if (!gtk_widget_is_toplevel (toplevel))
    return;

  dialog = photos_organize_collection_dialog_new (GTK_WINDOW (toplevel));
  photos_selection_toolbar_fade_out (self);
  g_signal_connect (dialog, "response", G_CALLBACK (photos_selection_toolbar_dialog_response), self);
}


static gboolean
photos_selection_toolbar_disconnect_listeners_foreach (gpointer key, gpointer value, gpointer user_data)
{
  g_signal_handler_disconnect (value, GPOINTER_TO_UINT (key));
  return TRUE;
}


static void
photos_selection_toolbar_favorite_clicked (GtkButton *button, gpointer user_data)
{
  PhotosSelectionToolbar *self = PHOTOS_SELECTION_TOOLBAR (user_data);
  PhotosSelectionToolbarPrivate *priv = self->priv;
  GList *selection;
  GList *l;

  if (priv->inside_refresh)
    return;

  selection = photos_selection_controller_get_selection (priv->sel_cntrlr);
  for (l = selection; l != NULL; l = l->next)
    {
      /* TODO: fave the doc */
    }
}


static void
photos_selection_toolbar_open_clicked (GtkButton *button, gpointer user_data)
{
  PhotosSelectionToolbar *self = PHOTOS_SELECTION_TOOLBAR (user_data);
  PhotosSelectionToolbarPrivate *priv = self->priv;
  GList *selection;
  GList *l;

  selection = photos_selection_controller_get_selection (priv->sel_cntrlr);
  for (l = selection; l != NULL; l = l->next)
    {
      /* TODO: trash the doc */
    }
}


static void
photos_selection_toolbar_print_clicked (GtkButton *button, gpointer user_data)
{
}


static void
photos_selection_toolbar_set_item_visibility (PhotosSelectionToolbar *self)
{
  PhotosSelectionToolbarPrivate *priv = self->priv;

  priv->inside_refresh = TRUE;

  /* TODO: ... */

  priv->inside_refresh = FALSE;
}


static void
photos_selection_toolbar_set_item_listeners (PhotosSelectionToolbar *self, GList *selection)
{
  PhotosSelectionToolbarPrivate *priv = self->priv;
  GList *l;

  g_hash_table_foreach_remove (priv->item_listeners, photos_selection_toolbar_disconnect_listeners_foreach, NULL);

  for (l = g_list_first (selection); l != NULL; l = g_list_next (l))
    {
      GObject *object;
      gchar *urn = (gchar *) l->data;
      gulong id;

      object = photos_base_manager_get_object_by_id (priv->item_mngr, urn);
      id = g_signal_connect_swapped (object,
                                     "info-updated",
                                     G_CALLBACK (photos_selection_toolbar_set_item_visibility),
                                     self);
      g_hash_table_insert (priv->item_listeners, GUINT_TO_POINTER (id), g_object_ref (object));
    }
}


static void
photos_selection_toolbar_selection_changed (PhotosSelectionController *sel_cntrlr, gpointer user_data)
{
  PhotosSelectionToolbar *self = PHOTOS_SELECTION_TOOLBAR (user_data);
  PhotosSelectionToolbarPrivate *priv = self->priv;
  GList *selection;

  if (!photos_selection_controller_get_selection_mode (priv->sel_cntrlr))
    return;

  selection = photos_selection_controller_get_selection (priv->sel_cntrlr);
  photos_selection_toolbar_set_item_listeners (self, selection);

  if (g_list_length (selection) > 0)
    {
      photos_selection_toolbar_set_item_visibility (self);
      photos_selection_toolbar_fade_in (self);
    }
  else
    photos_selection_toolbar_fade_out (self);
}


static void
photos_selection_toolbar_selection_mode_changed (PhotosSelectionController *sel_cntrlr,
                                                 gboolean mode,
                                                 gpointer user_data)
{
  PhotosSelectionToolbar *self = PHOTOS_SELECTION_TOOLBAR (user_data);

  if (mode)
    photos_selection_toolbar_selection_changed (sel_cntrlr, self);
  else
    photos_selection_toolbar_fade_out (self);
}


static void
photos_selection_toolbar_trash_clicked (GtkButton *button, gpointer user_data)
{
  PhotosSelectionToolbar *self = PHOTOS_SELECTION_TOOLBAR (user_data);
  PhotosSelectionToolbarPrivate *priv = self->priv;
  GList *selection;
  GList *l;

  selection = photos_selection_controller_get_selection (priv->sel_cntrlr);
  for (l = selection; l != NULL; l = l->next)
    {
      /* TODO: trash the doc */
    }
}


static void
photos_selection_toolbar_dispose (GObject *object)
{
  PhotosSelectionToolbar *self = PHOTOS_SELECTION_TOOLBAR (object);
  PhotosSelectionToolbarPrivate *priv = self->priv;

  if (priv->item_listeners != NULL)
    {
      g_hash_table_unref (priv->item_listeners);
      priv->item_listeners = NULL;
    }

  g_clear_object (&priv->item_mngr);

  if (priv->sel_cntrlr != NULL)
    {
      g_object_unref (priv->sel_cntrlr);
      priv->sel_cntrlr = NULL;
    }

  G_OBJECT_CLASS (photos_selection_toolbar_parent_class)->dispose (object);
}


static void
photos_selection_toolbar_init (PhotosSelectionToolbar *self)
{
  PhotosSelectionToolbarPrivate *priv;
  GtkWidget *bin;
  GtkWidget *image;
  GtkStyleContext *context;

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, PHOTOS_TYPE_SELECTION_TOOLBAR, PhotosSelectionToolbarPrivate);
  priv = self->priv;

  priv->item_listeners = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, g_object_unref);

  priv->widget = gtk_toolbar_new ();
  gtk_toolbar_set_show_arrow (GTK_TOOLBAR (priv->widget), FALSE);
  gtk_toolbar_set_icon_size (GTK_TOOLBAR (priv->widget), GTK_ICON_SIZE_LARGE_TOOLBAR);
  context = gtk_widget_get_style_context (priv->widget);
  gtk_style_context_add_class (context, "osd");

  priv->actor = gtk_clutter_actor_new_with_contents (priv->widget);
  clutter_actor_set_opacity (priv->actor, 0);
  g_object_set (priv->actor, "show-on-set-parent", FALSE, NULL);

  bin = gtk_clutter_actor_get_widget (GTK_CLUTTER_ACTOR (priv->actor));
  photos_utils_alpha_gtk_widget (bin);

  priv->left_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  priv->left_group = gtk_tool_item_new ();
  gtk_container_add (GTK_CONTAINER (priv->left_group), priv->left_box);
  gtk_toolbar_insert (GTK_TOOLBAR (priv->widget), priv->left_group, -1);
  gtk_widget_show_all (GTK_WIDGET (priv->left_group));

  priv->toolbar_favorite = gtk_toggle_button_new ();
  image = gtk_image_new_from_icon_name ("emblem-favorite-symbolic", GTK_ICON_SIZE_INVALID);
  gtk_image_set_pixel_size (GTK_IMAGE (image), 32);
  gtk_container_add (GTK_CONTAINER (priv->toolbar_favorite), image);
  gtk_container_add (GTK_CONTAINER (priv->left_box), priv->toolbar_favorite);
  g_signal_connect (priv->toolbar_favorite,
                    "clicked",
                    G_CALLBACK (photos_selection_toolbar_favorite_clicked),
                    self);

  priv->toolbar_print = gtk_button_new ();
  image = gtk_image_new_from_icon_name ("printer-symbolic", GTK_ICON_SIZE_INVALID);
  gtk_image_set_pixel_size (GTK_IMAGE (image), 32);
  gtk_container_add (GTK_CONTAINER (priv->toolbar_print), image);
  gtk_widget_set_tooltip_text (GTK_WIDGET (priv->toolbar_print), _("Print"));
  gtk_container_add (GTK_CONTAINER (priv->left_box), priv->toolbar_print);
  g_signal_connect (priv->toolbar_print,
                    "clicked",
                    G_CALLBACK (photos_selection_toolbar_print_clicked),
                    self);

  priv->separator = gtk_separator_tool_item_new ();
  gtk_separator_tool_item_set_draw (GTK_SEPARATOR_TOOL_ITEM (priv->separator), FALSE);
  gtk_widget_set_visible (GTK_WIDGET (priv->separator), TRUE);
  gtk_tool_item_set_expand (priv->separator, TRUE);
  gtk_toolbar_insert (GTK_TOOLBAR (priv->widget), priv->separator, -1);

  priv->right_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  priv->right_group = gtk_tool_item_new ();
  gtk_container_add (GTK_CONTAINER (priv->right_group), priv->right_box);
  gtk_toolbar_insert (GTK_TOOLBAR (priv->widget), priv->right_group, -1);
  gtk_widget_show_all (GTK_WIDGET (priv->right_group));

  priv->toolbar_collection = gtk_button_new ();
  image = gtk_image_new_from_icon_name ("list-add-symbolic", GTK_ICON_SIZE_INVALID);
  gtk_image_set_pixel_size (GTK_IMAGE (image), 32);
  gtk_container_add (GTK_CONTAINER (priv->toolbar_collection), image);
  gtk_widget_set_tooltip_text (GTK_WIDGET (priv->toolbar_collection), _("Organize"));
  gtk_container_add (GTK_CONTAINER (priv->right_box), priv->toolbar_collection);
  g_signal_connect (priv->toolbar_collection,
                    "clicked",
                    G_CALLBACK (photos_selection_toolbar_collection_clicked),
                    self);

  priv->toolbar_trash = gtk_button_new ();
  image = gtk_image_new_from_icon_name ("user-trash-symbolic", GTK_ICON_SIZE_INVALID);
  gtk_image_set_pixel_size (GTK_IMAGE (image), 32);
  gtk_container_add (GTK_CONTAINER (priv->toolbar_trash), image);
  gtk_widget_set_tooltip_text (GTK_WIDGET (priv->toolbar_trash), _("Delete"));
  gtk_container_add (GTK_CONTAINER (priv->right_box), priv->toolbar_trash);
  g_signal_connect (priv->toolbar_trash,
                    "clicked",
                    G_CALLBACK (photos_selection_toolbar_trash_clicked),
                    self);

  priv->toolbar_open = gtk_button_new ();
  image = gtk_image_new_from_icon_name ("document-open-symbolic", GTK_ICON_SIZE_INVALID);
  gtk_image_set_pixel_size (GTK_IMAGE (image), 32);
  gtk_container_add (GTK_CONTAINER (priv->toolbar_open), image);
  gtk_container_add (GTK_CONTAINER (priv->right_box), priv->toolbar_open);
  g_signal_connect (priv->toolbar_open,
                    "clicked",
                    G_CALLBACK (photos_selection_toolbar_open_clicked),
                    self);

  gtk_widget_show (priv->widget);

  priv->item_mngr = photos_item_manager_new ();

  priv->sel_cntrlr = photos_selection_controller_new ();
  g_signal_connect (priv->sel_cntrlr,
                    "selection-changed",
                    G_CALLBACK (photos_selection_toolbar_selection_changed),
                    self);
  g_signal_connect (priv->sel_cntrlr,
                    "selection-mode-changed",
                    G_CALLBACK (photos_selection_toolbar_selection_mode_changed),
                    self);
}


static void
photos_selection_toolbar_class_init (PhotosSelectionToolbarClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->dispose = photos_selection_toolbar_dispose;

  g_type_class_add_private (class, sizeof (PhotosSelectionToolbarPrivate));
}


PhotosSelectionToolbar *
photos_selection_toolbar_new (void)
{
  return g_object_new (PHOTOS_TYPE_SELECTION_TOOLBAR, NULL);
}


ClutterActor *
photos_selection_toolbar_get_actor (PhotosSelectionToolbar *self)
{
  return self->priv->actor;
}
