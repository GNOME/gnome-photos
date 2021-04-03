/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2015 Alessandro Bono
 * Copyright © 2014 Pranav Kant
 * Copyright © 2012 – 2021 Red Hat, Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/* Based on code from:
 *   + Documents
 */


#include "config.h"

#include <glib.h>
#include <glib/gi18n.h>

#include "photos-base-item.h"
#include "photos-delete-notification.h"
#include "photos-item-manager.h"
#include "photos-organize-collection-dialog.h"
#include "photos-search-context.h"
#include "photos-selection-controller.h"
#include "photos-selection-toolbar.h"
#include "photos-utils.h"


struct _PhotosSelectionToolbar
{
  GtkActionBar parent_instance;
  GAction *selection_mode_action;
  GHashTable *item_listeners;
  GtkWidget *toolbar_collection;
  GtkWidget *toolbar_favorite;
  GtkWidget *toolbar_open;
  PhotosBaseManager *item_mngr;
  PhotosModeController *mode_cntrlr;
  PhotosSelectionController *sel_cntrlr;
  gboolean inside_refresh;
};


G_DEFINE_TYPE (PhotosSelectionToolbar, photos_selection_toolbar, GTK_TYPE_ACTION_BAR);


enum
{
  SELECTION_TOOLBAR_DEFAULT_WIDTH = 500
};


static void
photos_selection_toolbar_dialog_response (GtkDialog *dialog, gint response_id, gpointer user_data)
{
  PhotosSelectionToolbar *self = PHOTOS_SELECTION_TOOLBAR (user_data);
  GVariant *new_state;

  if (response_id != GTK_RESPONSE_CLOSE)
    goto out;

  gtk_widget_destroy (GTK_WIDGET (dialog));

  new_state = g_variant_new ("b", FALSE);
  g_action_change_state (self->selection_mode_action, new_state);

 out:
  return;
}


static void
photos_selection_toolbar_collection_clicked (GtkButton *button, gpointer user_data)
{
  PhotosSelectionToolbar *self = PHOTOS_SELECTION_TOOLBAR (user_data);
  GtkWidget *dialog;
  GtkWidget *toplevel;

  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (self));
  if (!gtk_widget_is_toplevel (toplevel))
    goto out;

  dialog = photos_organize_collection_dialog_new (GTK_WINDOW (toplevel));
  gtk_widget_show_all (dialog);
  g_signal_connect (dialog, "response", G_CALLBACK (photos_selection_toolbar_dialog_response), self);

 out:
  return;
}


static void
photos_selection_toolbar_delete (PhotosSelectionToolbar *self)
{
  g_autolist (PhotosBaseItem) items = NULL;
  GList *selection;
  GList *l;
  GVariant *new_state;

  if (!photos_utils_get_selection_mode ())
    goto out;

  selection = photos_selection_controller_get_selection (self->sel_cntrlr);
  for (l = selection; l != NULL; l = l->next)
    {
      PhotosBaseItem *item;
      const gchar *urn = (gchar *) l->data;

      item = PHOTOS_BASE_ITEM (photos_base_manager_get_object_by_id (self->item_mngr, urn));
      items = g_list_prepend (items, g_object_ref (item));
    }

  /* Hiding an item from the item manager changes the selection, so we
   * can't use the selection while removing items.
   */
  for (l = items; l != NULL; l = l->next)
    {
      PhotosBaseItem *item = PHOTOS_BASE_ITEM (l->data);
      photos_item_manager_hide_item (PHOTOS_ITEM_MANAGER (self->item_mngr), item);
    }

  photos_delete_notification_new (items);

  new_state = g_variant_new ("b", FALSE);
  g_action_change_state (self->selection_mode_action, new_state);

 out:
  return;
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
  g_autolist (PhotosBaseItem) items = NULL;
  GList *selection;
  GList *l;
  GVariant *new_state;

  if (self->inside_refresh)
    goto out;

  selection = photos_selection_controller_get_selection (self->sel_cntrlr);
  for (l = selection; l != NULL; l = l->next)
    {
      const gchar *urn = (gchar *) l->data;
      PhotosBaseItem *item;

      item = PHOTOS_BASE_ITEM (photos_base_manager_get_object_by_id (self->item_mngr, urn));
      items = g_list_prepend (items, g_object_ref (item));
    }

  /* photos_base_item_set_favorite will emit info-updated signal while
   * looping: there is a chance that the selection will get modified
   * while we are iterating over it. To avoid this we make a copy of
   * the selection and work on it.
   */
  for (l = items; l != NULL; l = l->next)
    {
      PhotosBaseItem *item = PHOTOS_BASE_ITEM (l->data);
      gboolean favorite;

      favorite = photos_base_item_is_favorite (item);
      photos_base_item_set_favorite (item, !favorite);
    }

  new_state = g_variant_new ("b", FALSE);
  g_action_change_state (G_ACTION (self->selection_mode_action), new_state);

 out:
  return;
}


static void
photos_selection_toolbar_set_item_visibility (PhotosSelectionToolbar *self)
{
  GList *apps = NULL;
  GList *l;
  GList *selection;
  GtkWidget *image;
  gboolean has_selection;
  gboolean enable_collection;
  gboolean enable_favorite;
  g_autofree gchar *favorite_label = NULL;
  g_autofree gchar *open_label = NULL;
  guint fav_count = 0;
  guint sel_length = 0;

  self->inside_refresh = TRUE;

  selection = photos_selection_controller_get_selection (self->sel_cntrlr);
  has_selection = selection != NULL;

  enable_collection = has_selection;
  enable_favorite = has_selection;

  for (l = selection; l != NULL; l = g_list_next (l))
    {
      PhotosBaseItem *item;
      const gchar *default_app_name;
      const gchar *urn = (gchar *) l->data;

      item = PHOTOS_BASE_ITEM (photos_base_manager_get_object_by_id (self->item_mngr, urn));

      enable_collection = enable_collection && !photos_base_item_is_collection (item);
      enable_favorite = enable_favorite && !photos_base_item_is_collection (item);

      if (photos_base_item_is_favorite (item))
        fav_count++;

      default_app_name = photos_base_item_get_default_app_name (item);
      if (default_app_name != NULL
          && g_list_find_custom (apps, default_app_name, (GCompareFunc) g_strcmp0) == NULL)
        apps = g_list_prepend (apps, (gpointer) g_strdup (default_app_name));

      sel_length++;
    }

  enable_favorite = enable_favorite && ((fav_count == 0) || (fav_count == sel_length));

  if (apps != NULL && apps->next == NULL) /* length == 1 */
    /* Translators: this is the Open action in a context menu */
    open_label = g_strdup_printf (_("Open with %s"), (gchar *) apps->data);
  else
    /* Translators: this is the Open action in a context menu */
    open_label = g_strdup (_("Open"));

  gtk_button_set_label (GTK_BUTTON (self->toolbar_open), open_label);

  if (enable_favorite && fav_count == sel_length)
    {
      favorite_label = g_strdup (_("Remove from favorites"));
      image = gtk_image_new_from_icon_name ("starred-symbolic", GTK_ICON_SIZE_BUTTON);
    }
  else
    {
      favorite_label = g_strdup (_("Add to favorites"));
      image = gtk_image_new_from_icon_name ("non-starred-symbolic", GTK_ICON_SIZE_BUTTON);
    }

  gtk_button_set_image (GTK_BUTTON (self->toolbar_favorite), image);
  gtk_widget_set_tooltip_text (self->toolbar_favorite, favorite_label);

  gtk_widget_set_sensitive (self->toolbar_collection, enable_collection);
  gtk_widget_set_sensitive (self->toolbar_favorite, enable_favorite);

  self->inside_refresh = FALSE;

  g_list_free_full (apps, g_free);
}


static void
photos_selection_toolbar_set_item_listeners (PhotosSelectionToolbar *self, GList *selection)
{
  GList *l;

  g_hash_table_foreach_remove (self->item_listeners, photos_selection_toolbar_disconnect_listeners_foreach, NULL);

  for (l = selection; l != NULL; l = g_list_next (l))
    {
      GObject *object;
      gchar *urn = (gchar *) l->data;
      gulong id;

      object = photos_base_manager_get_object_by_id (self->item_mngr, urn);
      id = g_signal_connect_object (object,
                                    "info-updated",
                                    G_CALLBACK (photos_selection_toolbar_set_item_visibility),
                                    self,
                                    G_CONNECT_SWAPPED);
      g_hash_table_insert (self->item_listeners, GUINT_TO_POINTER (id), g_object_ref (object));
    }
}


static void
photos_selection_toolbar_selection_changed (PhotosSelectionToolbar *self)
{
  GList *selection;
  PhotosWindowMode mode;

  if (!photos_utils_get_selection_mode ())
    goto out;

  mode = photos_mode_controller_get_window_mode (self->mode_cntrlr);
  if (mode == PHOTOS_WINDOW_MODE_IMPORT)
    goto out;

  selection = photos_selection_controller_get_selection (self->sel_cntrlr);
  photos_selection_toolbar_set_item_listeners (self, selection);

  photos_selection_toolbar_set_item_visibility (self);
  gtk_widget_show (GTK_WIDGET (self));

 out:
  return;
}


static void
photos_selection_toolbar_selection_mode_notify_state (PhotosSelectionToolbar *self)
{
  PhotosWindowMode mode;

  mode = photos_mode_controller_get_window_mode (self->mode_cntrlr);

  if (photos_utils_get_selection_mode () && mode != PHOTOS_WINDOW_MODE_IMPORT)
    photos_selection_toolbar_selection_changed (self);
  else
    gtk_widget_hide (GTK_WIDGET (self));
}


static void
photos_selection_toolbar_dispose (GObject *object)
{
  PhotosSelectionToolbar *self = PHOTOS_SELECTION_TOOLBAR (object);

  g_clear_pointer (&self->item_listeners, g_hash_table_unref);
  g_clear_object (&self->item_mngr);
  g_clear_object (&self->mode_cntrlr);
  g_clear_object (&self->sel_cntrlr);

  G_OBJECT_CLASS (photos_selection_toolbar_parent_class)->dispose (object);
}


static void
photos_selection_toolbar_init (PhotosSelectionToolbar *self)
{
  GAction *action;
  GApplication *app;
  PhotosSearchContextState *state;

  app = g_application_get_default ();
  state = photos_search_context_get_state (PHOTOS_SEARCH_CONTEXT (app));

  self->selection_mode_action = g_action_map_lookup_action (G_ACTION_MAP (app), "selection-mode");
  g_signal_connect_object (self->selection_mode_action,
                           "notify::state",
                           G_CALLBACK (photos_selection_toolbar_selection_mode_notify_state),
                           self,
                           G_CONNECT_SWAPPED);

  self->item_listeners = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, g_object_unref);

  gtk_widget_init_template (GTK_WIDGET (self));

  self->item_mngr = g_object_ref (state->item_mngr);
  self->mode_cntrlr = g_object_ref (state->mode_cntrlr);

  self->sel_cntrlr = photos_selection_controller_dup_singleton ();
  g_signal_connect_object (self->sel_cntrlr,
                           "selection-changed",
                           G_CALLBACK (photos_selection_toolbar_selection_changed),
                           self,
                           G_CONNECT_SWAPPED);

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
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->dispose = photos_selection_toolbar_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Photos/selection-toolbar.ui");
  gtk_widget_class_bind_template_child (widget_class, PhotosSelectionToolbar, toolbar_favorite);
  gtk_widget_class_bind_template_child (widget_class, PhotosSelectionToolbar, toolbar_open);
  gtk_widget_class_bind_template_child (widget_class, PhotosSelectionToolbar, toolbar_collection);
  gtk_widget_class_bind_template_callback (widget_class, photos_selection_toolbar_favorite_clicked);
  gtk_widget_class_bind_template_callback (widget_class, photos_selection_toolbar_collection_clicked);
}


GtkWidget *
photos_selection_toolbar_new (void)
{
  return g_object_new (PHOTOS_TYPE_SELECTION_TOOLBAR, NULL);
}
