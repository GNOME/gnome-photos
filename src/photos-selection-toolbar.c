/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2014 Pranav Kant
 * Copyright © 2012, 2013, 2014, 2015 Red Hat, Inc.
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

#include "photos-base-item.h"
#include "photos-base-manager.h"
#include "photos-delete-notification.h"
#include "photos-icons.h"
#include "photos-organize-collection-dialog.h"
#include "photos-properties-dialog.h"
#include "photos-search-context.h"
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

  if (response_id != GTK_RESPONSE_CLOSE)
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


static void
photos_selection_toolbar_delete (PhotosSelectionToolbar *self)
{
  PhotosSelectionToolbarPrivate *priv = self->priv;
  GList *items = NULL;
  GList *selection;
  GList *l;

  if (!photos_selection_controller_get_selection_mode (priv->sel_cntrlr))
    return;

  selection = photos_selection_controller_get_selection (priv->sel_cntrlr);
  for (l = selection; l != NULL; l = l->next)
    {
      PhotosBaseItem *item;
      const gchar *urn = (gchar *) l->data;

      item = PHOTOS_BASE_ITEM (photos_base_manager_get_object_by_id (priv->item_mngr, urn));
      items = g_list_prepend (items, g_object_ref (item));
    }

  /* Removing an item from the item manager changes the selection, so
   * we can't use the selection while removing items.
   */
  for (l = items; l != NULL; l = l->next)
    {
      PhotosBaseItem *item = PHOTOS_BASE_ITEM (l->data);
      photos_base_manager_remove_object (priv->item_mngr, G_OBJECT (item));
    }

  photos_delete_notification_new (items);
  photos_selection_controller_set_selection_mode (priv->sel_cntrlr, FALSE);

  g_list_free_full (items, g_object_unref);
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
  GApplication *app;
  GtkWidget *dialog;
  const gchar *urn;

  app = g_application_get_default ();
  windows = gtk_application_get_windows (GTK_APPLICATION (app));

  selection = photos_selection_controller_get_selection (priv->sel_cntrlr);
  urn = (gchar *) selection->data;

  dialog = photos_properties_dialog_new (GTK_WINDOW (windows->data), urn);
  gtk_widget_show_all (dialog);

  g_signal_connect (dialog, "response", G_CALLBACK (photos_selection_toolbar_properties_response), self);
}


static void
photos_selection_toolbar_set_item_visibility (PhotosSelectionToolbar *self)
{
  PhotosSelectionToolbarPrivate *priv = self->priv;
  GList *apps = NULL;
  GList *l;
  GList *selection;
  GtkWidget *image;
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

  gtk_button_set_label (GTK_BUTTON (priv->toolbar_open), open_label);
  g_free (open_label);
  g_list_free_full (apps, g_free);

  if (show_favorite && fav_count == sel_length)
    {
      favorite_label = g_strdup (_("Remove from favorites"));
      image = gtk_image_new_from_icon_name (PHOTOS_ICON_FAVORITE_SYMBOLIC, GTK_ICON_SIZE_BUTTON);
    }
  else
    {
      favorite_label = g_strdup (_("Add to favorites"));
      image = gtk_image_new_from_icon_name (PHOTOS_ICON_NOT_FAVORITE_SYMBOLIC, GTK_ICON_SIZE_BUTTON);
    }

  gtk_button_set_image (GTK_BUTTON (priv->toolbar_favorite), image);
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
      id = g_signal_connect_object (object,
                                    "info-updated",
                                    G_CALLBACK (photos_selection_toolbar_set_item_visibility),
                                    self,
                                    G_CONNECT_SWAPPED);
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
photos_selection_toolbar_dispose (GObject *object)
{
  PhotosSelectionToolbar *self = PHOTOS_SELECTION_TOOLBAR (object);
  PhotosSelectionToolbarPrivate *priv = self->priv;

  g_clear_pointer (&priv->item_listeners, (GDestroyNotify) g_hash_table_unref);
  g_clear_object (&priv->item_mngr);
  g_clear_object (&priv->sel_cntrlr);

  G_OBJECT_CLASS (photos_selection_toolbar_parent_class)->dispose (object);
}


static void
photos_selection_toolbar_init (PhotosSelectionToolbar *self)
{
  PhotosSelectionToolbarPrivate *priv;
  GAction *action;
  GApplication *app;
  GtkWidget *toolbar;
  PhotosSearchContextState *state;

  self->priv = photos_selection_toolbar_get_instance_private (self);
  priv = self->priv;

  app = g_application_get_default ();
  state = photos_search_context_get_state (PHOTOS_SEARCH_CONTEXT (app));

  priv->item_listeners = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, g_object_unref);

  toolbar = gtk_action_bar_new ();
  gtk_container_add (GTK_CONTAINER (self), toolbar);

  priv->toolbar_favorite = gtk_button_new ();
  gtk_action_bar_pack_start (GTK_ACTION_BAR (toolbar), priv->toolbar_favorite);
  g_signal_connect (priv->toolbar_favorite,
                    "clicked",
                    G_CALLBACK (photos_selection_toolbar_favorite_clicked),
                    self);

  priv->toolbar_open = gtk_button_new_with_label (_("Open"));
  gtk_action_bar_pack_start (GTK_ACTION_BAR (toolbar), priv->toolbar_open);
  g_signal_connect (priv->toolbar_open,
                    "clicked",
                    G_CALLBACK (photos_selection_toolbar_open_clicked),
                    self);

  priv->toolbar_print = gtk_button_new_with_label (_("Print"));
  gtk_action_bar_pack_start (GTK_ACTION_BAR (toolbar), priv->toolbar_print);
  g_signal_connect (priv->toolbar_print,
                    "clicked",
                    G_CALLBACK (photos_selection_toolbar_print_clicked),
                    self);

  priv->toolbar_trash = gtk_button_new_with_label (_("Delete"));
  gtk_actionable_set_action_name (GTK_ACTIONABLE (priv->toolbar_trash), "app.delete");
  gtk_action_bar_pack_start (GTK_ACTION_BAR (toolbar), priv->toolbar_trash);

  priv->toolbar_properties = gtk_button_new_with_label (_("Properties"));
  gtk_action_bar_pack_end (GTK_ACTION_BAR (toolbar), priv->toolbar_properties);
  g_signal_connect (priv->toolbar_properties,
                    "clicked",
                    G_CALLBACK (photos_selection_toolbar_properties_clicked),
                    self);

  priv->toolbar_collection = gtk_button_new_with_label (_("Add to Album"));
  gtk_action_bar_pack_end (GTK_ACTION_BAR (toolbar), priv->toolbar_collection);
  g_signal_connect (priv->toolbar_collection,
                    "clicked",
                    G_CALLBACK (photos_selection_toolbar_collection_clicked),
                    self);

  gtk_widget_show_all (GTK_WIDGET (self));

  priv->item_mngr = g_object_ref (state->item_mngr);

  priv->sel_cntrlr = photos_selection_controller_dup_singleton ();
  g_signal_connect (priv->sel_cntrlr,
                    "selection-changed",
                    G_CALLBACK (photos_selection_toolbar_selection_changed),
                    self);
  g_signal_connect (priv->sel_cntrlr,
                    "selection-mode-changed",
                    G_CALLBACK (photos_selection_toolbar_selection_mode_changed),
                    self);

  action = g_action_map_lookup_action (G_ACTION_MAP (app), "delete");
  g_signal_connect_object (action,
                           "activate",
                           G_CALLBACK (photos_selection_toolbar_delete),
                           self,
                           G_CONNECT_SWAPPED);
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
