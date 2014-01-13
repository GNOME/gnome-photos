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
#include <libgd/gd.h>

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

G_DEFINE_TYPE_WITH_PRIVATE (PhotosSelectionToolbar, photos_selection_toolbar, GTK_TYPE_REVEALER);


enum
{
  SELECTION_TOOLBAR_DEFAULT_WIDTH = 500
};


static void
photos_selection_toolbar_dialog_response (GtkDialog *dialog, gint response_id, gpointer user_data)
{
  PhotosSelectionToolbar *self = PHOTOS_SELECTION_TOOLBAR (user_data);

  if (response_id != GTK_RESPONSE_OK)
    return;

  gtk_widget_destroy (GTK_WIDGET (dialog));
  photos_selection_controller_set_selection_mode (self->priv->sel_cntrlr, FALSE);
}


static void
photos_selection_toolbar_collection_clicked (GtkButton *button, gpointer user_data)
{
  PhotosSelectionToolbar *self = PHOTOS_SELECTION_TOOLBAR (user_data);
  GtkWidget *dialog;
  GtkWidget *toplevel;

  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (self));
  if (!gtk_widget_is_toplevel (toplevel))
    return;

  dialog = photos_organize_collection_dialog_new (GTK_WINDOW (toplevel));
  gtk_widget_show_all (dialog);
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

  photos_selection_controller_set_selection_mode (priv->sel_cntrlr, FALSE);
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

  photos_selection_controller_set_selection_mode (priv->sel_cntrlr, FALSE);
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
  photos_selection_controller_set_selection_mode (self->priv->sel_cntrlr, FALSE);
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
  const gchar *urn;

  app = photos_application_new ();
  windows = gtk_application_get_windows (app);

  selection = photos_selection_controller_get_selection (priv->sel_cntrlr);
  urn = (gchar *) selection->data;

  dialog = photos_properties_dialog_new (GTK_WINDOW (windows->data), urn);
  gtk_widget_show_all (dialog);

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
  GtkStyleContext *context;
  gboolean has_selection;
  gboolean show_collection;
  gboolean show_favorite;
  gboolean show_open;
  gboolean show_print;
  gboolean show_properties;
  gboolean show_trash;
  gchar *favorite_label;
  gchar *open_label;
  guint fav_count = 0;
  guint apps_length;
  guint sel_length;

  priv->inside_refresh = TRUE;

  selection = photos_selection_controller_get_selection (priv->sel_cntrlr);
  sel_length = g_list_length (selection);
  has_selection = sel_length > 0;

  show_collection = has_selection;
  show_favorite = has_selection;
  show_open = has_selection;
  show_print = has_selection;
  show_properties = has_selection;
  show_trash = has_selection;

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

  gd_header_button_set_label (GD_HEADER_BUTTON (priv->toolbar_open), open_label);
  g_free (open_label);
  g_list_free_full (apps, g_free);

  context = gtk_widget_get_style_context (priv->toolbar_favorite);
  if (show_favorite && fav_count == sel_length)
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

  gtk_widget_set_sensitive (priv->toolbar_collection, show_collection);
  gtk_widget_set_sensitive (priv->toolbar_print, show_print);
  gtk_widget_set_sensitive (priv->toolbar_properties, show_properties);
  gtk_widget_set_sensitive (priv->toolbar_trash, show_trash);
  gtk_widget_set_sensitive (priv->toolbar_open, show_open);
  gtk_widget_set_sensitive (priv->toolbar_favorite, show_favorite);

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

  photos_selection_toolbar_set_item_visibility (self);
  gtk_revealer_set_reveal_child (GTK_REVEALER (self), TRUE);
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
    gtk_revealer_set_reveal_child (GTK_REVEALER (self), FALSE);
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

  photos_selection_controller_set_selection_mode (priv->sel_cntrlr, FALSE);
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
  GtkWidget *toolbar;

  self->priv = photos_selection_toolbar_get_instance_private (self);
  priv = self->priv;

  priv->item_listeners = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, g_object_unref);

  toolbar = gtk_action_bar_new ();
  gtk_container_add (GTK_CONTAINER (self), toolbar);

  priv->toolbar_favorite = gd_header_toggle_button_new ();
  gd_header_button_set_symbolic_icon_name (GD_HEADER_BUTTON (priv->toolbar_favorite), "emblem-favorite-symbolic");
  gtk_action_bar_pack_start (GTK_ACTION_BAR (toolbar), priv->toolbar_favorite);
  g_signal_connect (priv->toolbar_favorite,
                    "clicked",
                    G_CALLBACK (photos_selection_toolbar_favorite_clicked),
                    self);

  priv->toolbar_open = gd_header_simple_button_new ();
  gd_header_button_set_label (GD_HEADER_BUTTON (priv->toolbar_open), _("Open"));
  gtk_action_bar_pack_start (GTK_ACTION_BAR (toolbar), priv->toolbar_open);
  g_signal_connect (priv->toolbar_open,
                    "clicked",
                    G_CALLBACK (photos_selection_toolbar_open_clicked),
                    self);

  priv->toolbar_print = gd_header_simple_button_new ();
  gd_header_button_set_label (GD_HEADER_BUTTON (priv->toolbar_print), _("Print"));
  gtk_action_bar_pack_start (GTK_ACTION_BAR (toolbar), priv->toolbar_print);
  g_signal_connect (priv->toolbar_print,
                    "clicked",
                    G_CALLBACK (photos_selection_toolbar_print_clicked),
                    self);

  priv->toolbar_trash = gd_header_simple_button_new ();
  gd_header_button_set_label (GD_HEADER_BUTTON (priv->toolbar_trash), _("Delete"));
  gtk_action_bar_pack_start (GTK_ACTION_BAR (toolbar), priv->toolbar_trash);
  g_signal_connect (priv->toolbar_trash,
                    "clicked",
                    G_CALLBACK (photos_selection_toolbar_trash_clicked),
                    self);

  priv->toolbar_properties = gd_header_simple_button_new ();
  gd_header_button_set_label (GD_HEADER_BUTTON (priv->toolbar_properties), _("Properties"));
  gtk_action_bar_pack_end (GTK_ACTION_BAR (toolbar), priv->toolbar_properties);
  g_signal_connect (priv->toolbar_properties,
                    "clicked",
                    G_CALLBACK (photos_selection_toolbar_properties_clicked),
                    self);

  priv->toolbar_collection = gd_header_simple_button_new ();
  gd_header_button_set_label (GD_HEADER_BUTTON (priv->toolbar_collection), _("Add to Album"));
  gtk_action_bar_pack_end (GTK_ACTION_BAR (toolbar), priv->toolbar_collection);
  g_signal_connect (priv->toolbar_collection,
                    "clicked",
                    G_CALLBACK (photos_selection_toolbar_collection_clicked),
                    self);

  gtk_widget_show_all (GTK_WIDGET (self));

  priv->item_mngr = photos_item_manager_dup_singleton ();

  priv->sel_cntrlr = photos_selection_controller_dup_singleton ();
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
}


GtkWidget *
photos_selection_toolbar_new (void)
{
  return g_object_new (PHOTOS_TYPE_SELECTION_TOOLBAR,
                       "transition-type", GTK_REVEALER_TRANSITION_TYPE_SLIDE_UP,
                       NULL);
}
