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

#include "photos-application.h"
#include "photos-base-item.h"
#include "photos-item-manager.h"
#include "photos-organize-collection-dialog.h"
#include "photos-properties-dialog.h"
#include "photos-selection-controller.h"
#include "photos-selection-toolbar.h"
#include "photos-utils.h"


struct _PhotosSelectionToolbarPrivate
{
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
  GtkWidget *toolbar_properties;
  GtkWidget *toolbar_trash;
  PhotosBaseManager *item_mngr;
  PhotosSelectionController *sel_cntrlr;
  gboolean inside_refresh;
};

G_DEFINE_TYPE (PhotosSelectionToolbar, photos_selection_toolbar, GTK_TYPE_TOOLBAR);


enum
{
  SELECTION_TOOLBAR_DEFAULT_WIDTH = 500
};


static void
photos_selection_toolbar_fade_in (PhotosSelectionToolbar *self)
{
  gtk_widget_show (GTK_WIDGET (self));
  /* TODO: animate the "opacity" to 1.0 in 300 ms using quadratic
   *       tweening
   */
  gtk_widget_set_opacity (GTK_WIDGET (self), 1.0);
}


static void
photos_selection_toolbar_fade_out (PhotosSelectionToolbar *self)
{
  /* TODO: animate the "opacity" to 0.0 in 300 ms using quadratic
   *       tweening
   */
  gtk_widget_set_opacity (GTK_WIDGET (self), 0.0);
  gtk_widget_hide (GTK_WIDGET (self));
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

  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (self));
  if (!gtk_widget_is_toplevel (toplevel))
    return;

  dialog = photos_organize_collection_dialog_new (GTK_WINDOW (toplevel));
  gtk_widget_show_all (dialog);
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
      const gchar *urn = (gchar *) l->data;
      PhotosBaseItem *item;
      gboolean favorite;

      item = PHOTOS_BASE_ITEM (photos_base_manager_get_object_by_id (priv->item_mngr, urn));
      favorite = photos_base_item_is_favorite (item);
      photos_base_item_set_favorite (item, !favorite);
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
      const gchar *urn = (gchar *) l->data;
      GdkScreen *screen;
      PhotosBaseItem *item;
      guint32 time;

      item = PHOTOS_BASE_ITEM (photos_base_manager_get_object_by_id (priv->item_mngr, urn));
      screen = gtk_widget_get_screen (GTK_WIDGET (button));
      time = gtk_get_current_event_time ();
      photos_base_item_open (item, screen, time);
    }
}


static void
photos_selection_toolbar_print_clicked (GtkButton *button, gpointer user_data)
{
  PhotosSelectionToolbar *self = PHOTOS_SELECTION_TOOLBAR (user_data);
  PhotosSelectionToolbarPrivate *priv = self->priv;
  GList *selection;
  GtkWidget *toplevel;
  PhotosBaseItem *item;
  const gchar *urn;

  selection = photos_selection_controller_get_selection (priv->sel_cntrlr);
  if (g_list_length (selection) != 1)
    return;

  urn = (gchar *) selection->data;
  item = PHOTOS_BASE_ITEM (photos_base_manager_get_object_by_id (priv->item_mngr, urn));
  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (button));
  photos_base_item_print (item, toplevel);
}


static void
photos_selection_toolbar_properties_response (GtkDialog *dialog, gint response_id, gpointer user_data)
{
  PhotosSelectionToolbar *self = PHOTOS_SELECTION_TOOLBAR (user_data);

  gtk_widget_destroy (GTK_WIDGET (dialog));
  photos_selection_toolbar_fade_in (self);
}


static void
photos_selection_toolbar_properties_clicked (GtkButton *button, gpointer user_data)
{
  PhotosSelectionToolbar *self = PHOTOS_SELECTION_TOOLBAR (user_data);
  PhotosSelectionToolbarPrivate *priv = self->priv;
  GList *selection;
  GList *windows;
  GtkApplication *app;
  GtkWidget *dialog;
  GtkWidget *toplevel;
  const gchar *urn;

  app = photos_application_new ();
  windows = gtk_application_get_windows (app);

  selection = photos_selection_controller_get_selection (priv->sel_cntrlr);
  urn = (gchar *) selection->data;

  dialog = photos_properties_dialog_new (GTK_WINDOW (windows->data), urn);
  gtk_widget_show_all (dialog);
  photos_selection_toolbar_fade_out (self);

  g_object_unref (app);

  g_signal_connect (dialog, "response", G_CALLBACK (photos_selection_toolbar_properties_response), self);
}


static void
photos_selection_toolbar_set_item_visibility (PhotosSelectionToolbar *self)
{
  PhotosSelectionToolbarPrivate *priv = self->priv;
  GList *apps = NULL;
  GList *l;
  GList *selection;
  gboolean show_favorite = TRUE;
  gboolean show_open = TRUE;
  gboolean show_print = TRUE;
  gboolean show_properties = TRUE;
  gboolean show_trash = TRUE;
  gchar *open_label;
  guint fav_count = 0;
  guint apps_length;
  guint sel_length;

  priv->inside_refresh = TRUE;

  selection = photos_selection_controller_get_selection (priv->sel_cntrlr);
  for (l = selection; l != NULL; l = g_list_next (l))
    {
      PhotosBaseItem *item;
      const gchar *default_app_name;
      const gchar *urn = (gchar *) l->data;

      item = PHOTOS_BASE_ITEM (photos_base_manager_get_object_by_id (priv->item_mngr, urn));

      if (photos_base_item_is_favorite (item))
        fav_count++;

      default_app_name = photos_base_item_get_default_app_name (item);
      if (default_app_name != NULL && g_list_find (apps, default_app_name) == NULL)
        apps = g_list_prepend (apps, (gpointer) g_strdup (default_app_name));

      show_trash = show_trash && photos_base_item_can_trash (item);
      show_print = show_print && !photos_base_item_is_collection (item);
    }

  sel_length = g_list_length (selection);
  show_favorite = show_favorite && ((fav_count == 0) || (fav_count == sel_length));

  apps_length = g_list_length (apps);
  show_open = (apps_length > 0);

  if (sel_length > 1)
    {
      show_print = FALSE;
      show_properties = FALSE;
    }

  if (apps_length == 1)
    /* Translators: this is the Open action in a context menu */
    open_label = g_strdup_printf (_("Open with %s"), (gchar *) apps->data);
  else
    /* Translators: this is the Open action in a context menu */
    open_label = g_strdup (_("Open"));

  gtk_widget_set_tooltip_text (priv->toolbar_open, open_label);
  g_free (open_label);
  g_list_free_full (apps, g_free);

  if (show_favorite)
    {
      GtkStyleContext *context;
      gchar *favorite_label;

      context = gtk_widget_get_style_context (priv->toolbar_favorite);

      if (fav_count == sel_length)
        {
          favorite_label = g_strdup (_("Remove from favorites"));
          gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->toolbar_favorite), TRUE);
          gtk_style_context_add_class (context, "documents-favorite");
        }
      else
        {
          favorite_label = g_strdup (_("Add to favorites"));
          gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->toolbar_favorite), FALSE);
          gtk_style_context_remove_class (context, "documents-favorite");
        }

      gtk_widget_reset_style (priv->toolbar_favorite);
      gtk_widget_set_tooltip_text (priv->toolbar_favorite, favorite_label);
      g_free (favorite_label);
    }

  gtk_widget_set_visible (priv->toolbar_print, show_print);
  gtk_widget_set_visible (priv->toolbar_properties, show_properties);
  gtk_widget_set_visible (priv->toolbar_trash, show_trash);
  gtk_widget_set_visible (priv->toolbar_open, show_open);
  gtk_widget_set_visible (priv->toolbar_favorite, show_favorite);

  priv->inside_refresh = FALSE;
}


static void
photos_selection_toolbar_set_item_listeners (PhotosSelectionToolbar *self, GList *selection)
{
  PhotosSelectionToolbarPrivate *priv = self->priv;
  GList *l;

  g_hash_table_foreach_remove (priv->item_listeners, photos_selection_toolbar_disconnect_listeners_foreach, NULL);

  for (l = selection; l != NULL; l = g_list_next (l))
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
      PhotosBaseItem *item;
      const gchar *urn = (gchar *) l->data;

      item = PHOTOS_BASE_ITEM (photos_base_manager_get_object_by_id (priv->item_mngr, urn));
      photos_base_item_trash (item);
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
  g_clear_object (&priv->sel_cntrlr);

  G_OBJECT_CLASS (photos_selection_toolbar_parent_class)->dispose (object);
}


static void
photos_selection_toolbar_init (PhotosSelectionToolbar *self)
{
  PhotosSelectionToolbarPrivate *priv;
  GtkWidget *image;
  GtkStyleContext *context;

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, PHOTOS_TYPE_SELECTION_TOOLBAR, PhotosSelectionToolbarPrivate);
  priv = self->priv;

  priv->item_listeners = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, g_object_unref);

  gtk_widget_set_halign (GTK_WIDGET (self), GTK_ALIGN_CENTER);
  gtk_widget_set_valign (GTK_WIDGET (self), GTK_ALIGN_END);
  gtk_widget_set_margin_bottom (GTK_WIDGET (self), 40);
  gtk_widget_set_opacity (GTK_WIDGET (self), 0.0);
  gtk_toolbar_set_show_arrow (GTK_TOOLBAR (self), FALSE);
  gtk_toolbar_set_icon_size (GTK_TOOLBAR (self), GTK_ICON_SIZE_LARGE_TOOLBAR);
  context = gtk_widget_get_style_context (GTK_WIDGET (self));
  gtk_style_context_add_class (context, "osd");
  gtk_widget_set_size_request (GTK_WIDGET (self), SELECTION_TOOLBAR_DEFAULT_WIDTH, -1);

  priv->left_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  priv->left_group = gtk_tool_item_new ();
  gtk_container_add (GTK_CONTAINER (priv->left_group), priv->left_box);
  gtk_toolbar_insert (GTK_TOOLBAR (self), priv->left_group, -1);

  priv->toolbar_favorite = gtk_toggle_button_new ();
  image = gtk_image_new_from_icon_name ("emblem-favorite-symbolic", GTK_ICON_SIZE_INVALID);
  gtk_image_set_pixel_size (GTK_IMAGE (image), 16);
  gtk_container_add (GTK_CONTAINER (priv->toolbar_favorite), image);
  gtk_container_add (GTK_CONTAINER (priv->left_box), priv->toolbar_favorite);
  g_signal_connect (priv->toolbar_favorite,
                    "clicked",
                    G_CALLBACK (photos_selection_toolbar_favorite_clicked),
                    self);

  priv->toolbar_open = gtk_button_new ();
  image = gtk_image_new_from_icon_name ("folder-symbolic", GTK_ICON_SIZE_INVALID);
  gtk_image_set_pixel_size (GTK_IMAGE (image), 16);
  gtk_container_add (GTK_CONTAINER (priv->toolbar_open), image);
  gtk_container_add (GTK_CONTAINER (priv->left_box), priv->toolbar_open);
  g_signal_connect (priv->toolbar_open,
                    "clicked",
                    G_CALLBACK (photos_selection_toolbar_open_clicked),
                    self);

  priv->toolbar_print = gtk_button_new ();
  image = gtk_image_new_from_icon_name ("printer-symbolic", GTK_ICON_SIZE_INVALID);
  gtk_image_set_pixel_size (GTK_IMAGE (image), 16);
  gtk_container_add (GTK_CONTAINER (priv->toolbar_print), image);
  gtk_widget_set_tooltip_text (GTK_WIDGET (priv->toolbar_print), _("Print"));
  gtk_container_add (GTK_CONTAINER (priv->left_box), priv->toolbar_print);
  g_signal_connect (priv->toolbar_print,
                    "clicked",
                    G_CALLBACK (photos_selection_toolbar_print_clicked),
                    self);

  priv->toolbar_trash = gtk_button_new ();
  image = gtk_image_new_from_icon_name ("user-trash-symbolic", GTK_ICON_SIZE_INVALID);
  gtk_image_set_pixel_size (GTK_IMAGE (image), 16);
  gtk_container_add (GTK_CONTAINER (priv->toolbar_trash), image);
  gtk_widget_set_tooltip_text (GTK_WIDGET (priv->toolbar_trash), _("Delete"));
  gtk_container_add (GTK_CONTAINER (priv->left_box), priv->toolbar_trash);
  g_signal_connect (priv->toolbar_trash,
                    "clicked",
                    G_CALLBACK (photos_selection_toolbar_trash_clicked),
                    self);

  priv->separator = gtk_separator_tool_item_new ();
  gtk_separator_tool_item_set_draw (GTK_SEPARATOR_TOOL_ITEM (priv->separator), FALSE);
  gtk_widget_set_visible (GTK_WIDGET (priv->separator), TRUE);
  gtk_tool_item_set_expand (priv->separator, TRUE);
  gtk_toolbar_insert (GTK_TOOLBAR (self), priv->separator, -1);

  priv->right_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  priv->right_group = gtk_tool_item_new ();
  gtk_container_add (GTK_CONTAINER (priv->right_group), priv->right_box);
  gtk_toolbar_insert (GTK_TOOLBAR (self), priv->right_group, -1);

  priv->toolbar_collection = gtk_button_new ();
  image = gtk_image_new_from_icon_name ("list-add-symbolic", GTK_ICON_SIZE_INVALID);
  gtk_image_set_pixel_size (GTK_IMAGE (image), 16);
  gtk_container_add (GTK_CONTAINER (priv->toolbar_collection), image);
  /* Translators: "Organize" refers to photos in this context */
  gtk_widget_set_tooltip_text (GTK_WIDGET (priv->toolbar_collection), C_("Toolbar button tooltip", "Organize"));
  gtk_container_add (GTK_CONTAINER (priv->right_box), priv->toolbar_collection);
  g_signal_connect (priv->toolbar_collection,
                    "clicked",
                    G_CALLBACK (photos_selection_toolbar_collection_clicked),
                    self);

  priv->toolbar_properties = gtk_button_new ();
  image = gtk_image_new_from_icon_name ("document-properties-symbolic", GTK_ICON_SIZE_INVALID);
  gtk_image_set_pixel_size (GTK_IMAGE (image), 16);
  gtk_container_add (GTK_CONTAINER (priv->toolbar_properties), image);
  gtk_widget_set_tooltip_text (GTK_WIDGET (priv->toolbar_properties), _("Properties"));
  gtk_container_add (GTK_CONTAINER (priv->right_box), priv->toolbar_properties);
  g_signal_connect (priv->toolbar_properties,
                    "clicked",
                    G_CALLBACK (photos_selection_toolbar_properties_clicked),
                    self);

  gtk_widget_show_all (GTK_WIDGET (self));

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


GtkWidget *
photos_selection_toolbar_new (void)
{
  return g_object_new (PHOTOS_TYPE_SELECTION_TOOLBAR, NULL);
}
