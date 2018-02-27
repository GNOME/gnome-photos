/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2018 Red Hat, Inc.
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


#include "config.h"

#include <dazzle.h>
#include <gio/gio.h>
#include <glib/gi18n.h>
#include <tracker-sparql.h>

#include "photos-base-item.h"
#include "photos-debug.h"
#include "photos-filterable.h"
#include "photos-import-dialog.h"
#include "photos-item-manager.h"
#include "photos-model-button.h"
#include "photos-query.h"
#include "photos-query-builder.h"
#include "photos-search-context.h"
#include "photos-tracker-queue.h"
#include "photos-utils.h"


struct _PhotosImportDialog
{
  GtkDialog parent_instance;
  DzlFuzzyMutableIndex *index;
  GCancellable *cancellable;
  GHashTable *collections;
  GList *recent_collections;
  GSimpleAction *add_existing_action;
  GSimpleActionGroup *action_group;
  GtkWidget *add_existing_button;
  GtkWidget *add_existing_collection_name_button;
  GtkWidget *add_existing_collection_name_label;
  GtkWidget *add_existing_label;
  GtkWidget *collections_popover;
  GtkWidget *collections_popover_grid;
  GtkWidget *collections_popover_search_entry;
  GtkWidget *create_new_button;
  GtkWidget *create_new_entry;
  GtkWidget *create_new_error_label;
  GtkWidget *create_new_error_revealer;
  GtkWidget *create_new_label;
  PhotosBaseItem *default_collection;
  PhotosBaseManager *item_mngr;
  PhotosTrackerQueue *queue;
  gint64 time;
};

enum
{
  PROP_0,
  PROP_TIME
};


G_DEFINE_TYPE (PhotosImportDialog, photos_import_dialog, GTK_TYPE_DIALOG);


enum
{
  MAX_MATCHES = 4
};


static void
photos_import_dialog_add_collection (PhotosImportDialog *self, PhotosBaseItem *collection)
{
  const gchar *id;
  const gchar *identifier;

  g_return_if_fail (photos_base_item_is_collection (collection));

  id = photos_filterable_get_id (PHOTOS_FILTERABLE (collection));
  identifier = photos_base_item_get_identifier (collection);
  g_return_if_fail (g_strcmp0 (id, PHOTOS_COLLECTION_SCREENSHOT) == 0
                    || (identifier != NULL && g_str_has_prefix (identifier,
                                                                PHOTOS_QUERY_LOCAL_COLLECTIONS_IDENTIFIER)));

  g_hash_table_insert (self->collections, (gpointer) id, g_object_ref (collection));
}


static void
photos_import_dialog_add_existing_notify_state (PhotosImportDialog *self)
{
  g_autoptr (GVariant) state = NULL;
  PhotosBaseItem *collection;
  const gchar *id;
  const gchar *name;

  g_return_if_fail (PHOTOS_IS_IMPORT_DIALOG (self));

  state = g_action_get_state (G_ACTION (self->add_existing_action));
  g_return_if_fail (state != NULL);

  id = g_variant_get_string (state, NULL);
  collection = g_hash_table_lookup (self->collections, id);
  g_return_if_fail (PHOTOS_IS_BASE_ITEM (collection));

  name = photos_base_item_get_name (collection);
  gtk_label_set_label (GTK_LABEL (self->add_existing_collection_name_label), name);
}


static gchar *
photos_import_dialog_create_default_collection_name (PhotosImportDialog *self)
{
  g_autoptr (GDateTime) time = NULL;
  gchar *time_str = NULL;

  time = g_date_time_new_from_unix_local (self->time);

  /* Translators: this is the name of the default album that will be
   * created for imported photos.
   */
  time_str = g_date_time_format (time, _("%-d %B %Y"));

  return time_str;
}


static gboolean
photos_import_dialog_index_contains (PhotosImportDialog *self, const gchar *key)
{
  const DzlFuzzyMutableIndexMatch *match;
  g_autoptr (GArray) matches = NULL;
  gboolean ret_val = FALSE;

  g_return_val_if_fail (self->index != NULL, FALSE);

  matches = dzl_fuzzy_mutable_index_match (self->index, key, 1);
  if (matches->len == 0)
    goto out;

  match = &g_array_index (matches, DzlFuzzyMutableIndexMatch, 0);
  if (g_strcmp0 (key, match->key) == 0)
    ret_val = TRUE;

 out:
  return ret_val;
}


static void
photos_import_dialog_initialize_create_new_entry (PhotosImportDialog *self)
{
  g_autofree gchar *default_collection_name = NULL;

  g_return_if_fail (self->index != NULL);

  if (self->default_collection != NULL)
    goto out;

  default_collection_name = photos_import_dialog_create_default_collection_name (self);
  if (photos_import_dialog_index_contains (self, default_collection_name))
    goto out;

  gtk_entry_set_text (GTK_ENTRY (self->create_new_entry), default_collection_name);

 out:
  return;
}


static void
photos_import_dialog_update_response_sensitivity (PhotosImportDialog *self)
{
  gboolean sensitive = TRUE;
  gboolean show_create_new_error = FALSE;

  g_return_if_fail (PHOTOS_IS_IMPORT_DIALOG (self));
  g_return_if_fail (self->index != NULL);

  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->create_new_button)))
    {
      guint16 text_length;

      text_length = gtk_entry_get_text_length (GTK_ENTRY (self->create_new_entry));
      if (text_length == 0)
        {
          sensitive = FALSE;
          show_create_new_error = FALSE;
        }
      else
        {
          const gchar *text;

          text = gtk_entry_get_text (GTK_ENTRY (self->create_new_entry));
          if (photos_import_dialog_index_contains (self, text))
            {
              /* Translators: this is an error message shown below the
               * entry to create a new album when importing content, and
               * should be short enough to not increase the width of the
               * dialog.
               */
              gtk_label_set_label (GTK_LABEL (self->create_new_error_label),
                                   _("An album with that name already exists"));
              sensitive = FALSE;
              show_create_new_error = TRUE;
            }
          else if (self->default_collection != NULL)
            {
              g_autofree gchar *default_collection_name = NULL;

              default_collection_name = photos_import_dialog_create_default_collection_name (self);
              if (g_strcmp0 (text, default_collection_name) == 0)
                {
                  /* Translators: this is an error message shown below the
                   * entry to create a new album when importing content, and
                   * should be short enough to not increase the width of the
                   * dialog.
                   */
                  gtk_label_set_label (GTK_LABEL (self->create_new_error_label),
                                       _("An album for that date already exists"));
                  sensitive = FALSE;
                  show_create_new_error = TRUE;
                }
            }
        }
    }

  if (sensitive)
    g_return_if_fail (!show_create_new_error);

  gtk_revealer_set_reveal_child (GTK_REVEALER (self->create_new_error_revealer), show_create_new_error);
  gtk_dialog_set_response_sensitive (GTK_DIALOG (self), GTK_RESPONSE_OK, sensitive);
}


static void
photos_import_dialog_add_existing_button_toggled (PhotosImportDialog *self)
{
  g_return_if_fail (PHOTOS_IS_IMPORT_DIALOG (self));

  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->add_existing_button)))
    gtk_widget_grab_focus (self->add_existing_collection_name_button);

  photos_import_dialog_update_response_sensitivity (self);
}


static gboolean
photos_import_dialog_add_existing_collection_name_focus_in_event (PhotosImportDialog *self)
{
  g_return_val_if_fail (PHOTOS_IS_IMPORT_DIALOG (self), GDK_EVENT_PROPAGATE);

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->add_existing_button), TRUE);
  return GDK_EVENT_PROPAGATE;
}


static void
photos_import_dialog_add_existing_collection_name_toggled (PhotosImportDialog *self)
{
  g_return_if_fail (PHOTOS_IS_IMPORT_DIALOG (self));

  if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->add_existing_collection_name_button)))
    goto out;

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->add_existing_button), TRUE);
  gtk_widget_grab_focus (self->collections_popover_search_entry);

 out:
  return;
}


static GtkWidget *
photos_import_dialog_create_collection_button (PhotosBaseItem *collection)
{
  GtkWidget *collection_button;
  GtkWidget *collection_label;
  const gchar *id;
  const gchar *name;

  id = photos_filterable_get_id (PHOTOS_FILTERABLE (collection));
  name = photos_base_item_get_name (collection);

  collection_button = photos_model_button_new ();
  gtk_actionable_set_action_name (GTK_ACTIONABLE (collection_button), "dialog.add-existing");
  gtk_actionable_set_action_target (GTK_ACTIONABLE (collection_button), "s", id);

  collection_label = gtk_label_new (name);
  gtk_widget_set_halign (collection_label, GTK_ALIGN_START);
  gtk_widget_set_hexpand (collection_label, TRUE);
  gtk_container_add (GTK_CONTAINER (collection_button), collection_label);

  return collection_button;
}


static void
photos_import_dialog_collections_popover_search_changed (PhotosImportDialog *self)
{
  guint16 text_length;

  gtk_container_foreach (GTK_CONTAINER (self->collections_popover_grid), (GtkCallback) gtk_widget_destroy, NULL);

  text_length = gtk_entry_get_text_length (GTK_ENTRY (self->collections_popover_search_entry));
  if (text_length == 0)
    {
      GList *l;

      for (l = self->recent_collections; l != NULL; l = l->next)
        {
          GtkWidget *collection_button;
          PhotosBaseItem *collection = PHOTOS_BASE_ITEM (l->data);

          collection_button = photos_import_dialog_create_collection_button (collection);
          gtk_container_add (GTK_CONTAINER (self->collections_popover_grid), collection_button);
          gtk_widget_show_all (collection_button);
        }
    }
  else
    {
      g_autoptr (GArray) matches = NULL;
      const gchar *text;
      guint i;

      text = gtk_entry_get_text (GTK_ENTRY (self->collections_popover_search_entry));
      matches = dzl_fuzzy_mutable_index_match (self->index, text, MAX_MATCHES);

      for (i = 0; i < matches->len; i++)
        {
          const DzlFuzzyMutableIndexMatch *match;
          GtkWidget *collection_button;
          PhotosBaseItem *collection;

          match = &g_array_index (matches, DzlFuzzyMutableIndexMatch, i);
          collection = PHOTOS_BASE_ITEM (match->value);
          collection_button = photos_import_dialog_create_collection_button (collection);
          gtk_container_add (GTK_CONTAINER (self->collections_popover_grid), collection_button);
          gtk_widget_show_all (collection_button);
        }

      g_return_if_fail (matches->len <= MAX_MATCHES);
    }
}


static void
photos_import_dialog_create_new_button_toggled (PhotosImportDialog *self)
{
  g_return_if_fail (PHOTOS_IS_IMPORT_DIALOG (self));

  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->create_new_button)))
    {
      guint16 text_length;

      text_length = gtk_entry_get_text_length (GTK_ENTRY (self->create_new_entry));
      if (text_length == 0)
        photos_import_dialog_initialize_create_new_entry (self);

      gtk_widget_grab_focus (self->create_new_entry);
    }

  photos_import_dialog_update_response_sensitivity (self);
}


static void
photos_import_dialog_create_new_entry_changed (PhotosImportDialog *self)
{
  g_return_if_fail (PHOTOS_IS_IMPORT_DIALOG (self));
  g_return_if_fail (self->index != NULL);
  g_return_if_fail (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->create_new_button)));

  photos_import_dialog_update_response_sensitivity (self);
}


static gboolean
photos_import_dialog_create_new_entry_focus_in_event (PhotosImportDialog *self)
{
  g_return_val_if_fail (PHOTOS_IS_IMPORT_DIALOG (self), GDK_EVENT_PROPAGATE);

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->create_new_button), TRUE);
  return GDK_EVENT_PROPAGATE;
}


static void
photos_import_dialog_enable_create_new (PhotosImportDialog *self, gboolean enable)
{
  gtk_widget_set_sensitive (self->create_new_label, enable);
  gtk_widget_set_sensitive (self->create_new_entry, enable);

  if (enable && gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->create_new_button)))
    {
      guint16 text_length;

      text_length = gtk_entry_get_text_length (GTK_ENTRY (self->create_new_entry));
      if (text_length == 0)
        photos_import_dialog_initialize_create_new_entry (self);

      gtk_widget_grab_focus (self->create_new_entry);
    }
}


static void
photos_import_dialog_show_add_existing (PhotosImportDialog *self, gboolean show)
{
  GtkStyleContext *context;
  const gchar *class_name;
  const gchar *invert_class_name;

  class_name = show ? "photos-fade-in" : "photos-fade-out";
  invert_class_name = !show ? "photos-fade-in" : "photos-fade-out";

  gtk_widget_set_sensitive (self->create_new_button, show);
  gtk_widget_show (self->create_new_button);
  context = gtk_widget_get_style_context (self->create_new_button);
  gtk_style_context_remove_class (context, invert_class_name);
  gtk_style_context_add_class (context, class_name);

  gtk_widget_set_sensitive (self->add_existing_button, show);
  gtk_widget_show (self->add_existing_button);
  context = gtk_widget_get_style_context (self->add_existing_button);
  gtk_style_context_remove_class (context, invert_class_name);
  gtk_style_context_add_class (context, class_name);

  gtk_widget_set_sensitive (self->add_existing_label, show);
  gtk_widget_show (self->add_existing_label);
  context = gtk_widget_get_style_context (self->add_existing_label);
  gtk_style_context_remove_class (context, invert_class_name);
  gtk_style_context_add_class (context, class_name);

  gtk_widget_set_sensitive (self->add_existing_collection_name_button, show);
  gtk_widget_show (self->add_existing_collection_name_button);
  context = gtk_widget_get_style_context (self->add_existing_collection_name_button);
  gtk_style_context_remove_class (context, invert_class_name);
  gtk_style_context_add_class (context, class_name);
}


static gint
photos_import_dialog_sort_func (gconstpointer a, gconstpointer b, gpointer user_data)
{
  PhotosImportDialog *self;
  PhotosBaseItem *collection_a;
  PhotosBaseItem *collection_b;
  gint ret_val;
  gint64 mtime_a;
  gint64 mtime_b;

  g_return_val_if_fail (PHOTOS_IS_BASE_ITEM ((gpointer) a), 0);
  g_return_val_if_fail (PHOTOS_IS_BASE_ITEM ((gpointer) b), 0);
  g_return_val_if_fail (PHOTOS_IS_IMPORT_DIALOG (user_data), 0);

  self = PHOTOS_IMPORT_DIALOG (user_data);

  collection_a = PHOTOS_BASE_ITEM ((gpointer) a);
  if (collection_a == self->default_collection)
    mtime_a = G_MAXINT64;
  else
    mtime_a = photos_base_item_get_mtime (collection_a);

  collection_b = PHOTOS_BASE_ITEM ((gpointer) b);
  if (collection_b == self->default_collection)
    mtime_b = G_MAXINT64;
  else
    mtime_b = photos_base_item_get_mtime (collection_b);

  if (mtime_a > mtime_b)
    ret_val = -1;
  else if (mtime_a == mtime_b)
    ret_val = 0;
  else
    ret_val = 1;

  return ret_val;
}


static void
photos_import_dialog_initialize_index_and_popover (PhotosImportDialog *self)
{
  GHashTableIter iter;
  GList *extra_collections = NULL;
  GList *l;
  PhotosBaseItem *collection;
  guint n_buttons = 0;

  g_clear_pointer (&self->index, (GDestroyNotify) dzl_fuzzy_mutable_index_unref);
  g_clear_pointer (&self->recent_collections, (GDestroyNotify) photos_utils_object_list_free_full);
  gtk_container_foreach (GTK_CONTAINER (self->collections_popover_grid), (GtkCallback) gtk_widget_destroy, NULL);
  gtk_dialog_set_response_sensitive (GTK_DIALOG (self), GTK_RESPONSE_OK, FALSE);
  photos_import_dialog_enable_create_new (self, FALSE);
  photos_import_dialog_show_add_existing (self, FALSE);

  self->index = dzl_fuzzy_mutable_index_new_with_free_func (FALSE, g_object_unref);

  dzl_fuzzy_mutable_index_begin_bulk_insert (self->index);

  g_hash_table_iter_init (&iter, self->collections);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &collection))
    {
      const gchar *name;

      name = photos_base_item_get_name (collection);
      dzl_fuzzy_mutable_index_insert (self->index, name, g_object_ref (collection));
      self->recent_collections = g_list_prepend (self->recent_collections, g_object_ref (collection));
    }

  dzl_fuzzy_mutable_index_end_bulk_insert (self->index);

  self->recent_collections = g_list_sort_with_data (self->recent_collections, photos_import_dialog_sort_func, self);
  for (l = self->recent_collections; l != NULL && n_buttons < MAX_MATCHES; l = l->next)
    {
      GtkWidget *collection_button;

      collection = PHOTOS_BASE_ITEM (l->data);
      collection_button = photos_import_dialog_create_collection_button (collection);
      gtk_container_add (GTK_CONTAINER (self->collections_popover_grid), collection_button);
      gtk_widget_show_all (collection_button);
      n_buttons++;
    }

  if (self->recent_collections != NULL)
    {
      GVariant *state;
      const gchar *id;

      if (l != NULL)
        {
          l->prev->next = NULL;
          l->prev = NULL;
          extra_collections = g_steal_pointer (&l);
        }

      collection = PHOTOS_BASE_ITEM (self->recent_collections->data);
      id = photos_filterable_get_id (PHOTOS_FILTERABLE (collection));
      state = g_variant_new_string (id);
      g_action_change_state (G_ACTION (self->add_existing_action), state);

      photos_import_dialog_show_add_existing (self, TRUE);
    }

  photos_import_dialog_enable_create_new (self, TRUE);
  photos_import_dialog_update_response_sensitivity (self);

  g_list_free_full (extra_collections, g_object_unref);
  g_return_if_fail (g_list_length (self->recent_collections) <= MAX_MATCHES);
}


static void
photos_import_dialog_fetch_collections_local_cursor_next (GObject *source_object,
                                                          GAsyncResult *res,
                                                          gpointer user_data)
{
  PhotosImportDialog *self;
  g_autoptr (PhotosBaseItem) collection = NULL;
  TrackerSparqlCursor *cursor = TRACKER_SPARQL_CURSOR (source_object);
  gboolean success;
  const gchar *identifier;
  g_autofree gchar *identifier_time = NULL;

  {
    g_autoptr (GError) error = NULL;

    /* Note that tracker_sparql_cursor_next_finish can return FALSE even
     * without an error.
     */
    success = tracker_sparql_cursor_next_finish (cursor, res, &error);
    if (error != NULL)
      {
        if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
          g_warning ("Unable to fetch local collections: %s", error->message);

        goto out;
      }
  }

  self = PHOTOS_IMPORT_DIALOG (user_data);

  if (!success)
    {
      photos_import_dialog_initialize_index_and_popover (self);
      goto out;
    }

  collection = photos_item_manager_create_item (PHOTOS_ITEM_MANAGER (self->item_mngr), G_TYPE_NONE, cursor, FALSE);
  photos_import_dialog_add_collection (self, collection);

  identifier = photos_base_item_get_identifier (collection);
  identifier_time = g_strdup_printf ("%s%" G_GINT64_FORMAT, PHOTOS_QUERY_LOCAL_COLLECTIONS_IDENTIFIER, self->time);
  if (g_strcmp0 (identifier, identifier_time) == 0)
    {
      const gchar *id;

      id = photos_filterable_get_id (PHOTOS_FILTERABLE (collection));
      photos_debug (PHOTOS_DEBUG_IMPORT, "Default collection already exists: %s", id);
      g_set_object (&self->default_collection, collection);
    }

  tracker_sparql_cursor_next_async (cursor,
                                    self->cancellable,
                                    photos_import_dialog_fetch_collections_local_cursor_next,
                                    self);

 out:
  return;
}


static void
photos_import_dialog_fetch_collections_local_query_executed (GObject *source_object,
                                                             GAsyncResult *res,
                                                             gpointer user_data)
{
  PhotosImportDialog *self;
  TrackerSparqlConnection *connection = TRACKER_SPARQL_CONNECTION (source_object);
  TrackerSparqlCursor *cursor = NULL; /* TODO: use g_autoptr */

  {
    g_autoptr (GError) error = NULL;

    cursor = tracker_sparql_connection_query_finish (connection, res, &error);
    if (error != NULL)
      {
        if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
          g_warning ("Unable to fetch local collections: %s", error->message);

        goto out;
      }
  }

  self = PHOTOS_IMPORT_DIALOG (user_data);

  if (cursor == NULL)
    goto out;

  tracker_sparql_cursor_next_async (cursor,
                                    self->cancellable,
                                    photos_import_dialog_fetch_collections_local_cursor_next,
                                    self);

 out:
  g_clear_object (&cursor);
  return;
}


static void
photos_import_dialog_dispose (GObject *object)
{
  PhotosImportDialog *self = PHOTOS_IMPORT_DIALOG (object);

  if (self->cancellable != NULL)
    {
      g_cancellable_cancel (self->cancellable);
      g_clear_object (&self->cancellable);
    }

  g_clear_object (&self->add_existing_action);
  g_clear_object (&self->action_group);
  g_clear_object (&self->default_collection);
  g_clear_object (&self->item_mngr);
  g_clear_object (&self->queue);
  g_clear_pointer (&self->index, (GDestroyNotify) dzl_fuzzy_mutable_index_unref);
  g_clear_pointer (&self->collections, (GDestroyNotify) g_hash_table_unref);
  g_clear_pointer (&self->recent_collections, (GDestroyNotify) photos_utils_object_list_free_full);

  G_OBJECT_CLASS (photos_import_dialog_parent_class)->dispose (object);
}


static void
photos_import_dialog_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
  PhotosImportDialog *self = PHOTOS_IMPORT_DIALOG (object);

  switch (prop_id)
    {
    case PROP_TIME:
      self->time = g_value_get_int64 (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
photos_import_dialog_init (PhotosImportDialog *self)
{
  GApplication *app;
  GVariant *initial_state;
  PhotosSearchContextState *state;

  gtk_widget_init_template (GTK_WIDGET (self));

  app = g_application_get_default ();
  state = photos_search_context_get_state (PHOTOS_SEARCH_CONTEXT (app));

  self->cancellable = g_cancellable_new ();
  self->collections = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_object_unref);

  self->action_group = g_simple_action_group_new ();
  gtk_widget_insert_action_group (GTK_WIDGET (self), "dialog", G_ACTION_GROUP (self->action_group));

  initial_state = g_variant_new_string ("");
  self->add_existing_action = g_simple_action_new_stateful ("add-existing", G_VARIANT_TYPE_STRING, initial_state);
  g_signal_connect_swapped (self->add_existing_action,
                            "notify::state",
                            G_CALLBACK (photos_import_dialog_add_existing_notify_state),
                            self);
  g_action_map_add_action (G_ACTION_MAP (self->action_group), G_ACTION (self->add_existing_action));

  self->item_mngr = g_object_ref (state->item_mngr);

  {
    g_autoptr (GError) error = NULL;

    self->queue = photos_tracker_queue_dup_singleton (NULL, &error);
    if (G_UNLIKELY (error != NULL))
      g_warning ("Unable to create PhotosTrackerQueue: %s", error->message);
  }

  if (G_LIKELY (self->queue != NULL))
    {
      g_autoptr (PhotosQuery) query = NULL;

      query = photos_query_builder_fetch_collections_local (state);
      photos_tracker_queue_select (self->queue,
                                   query,
                                   self->cancellable,
                                   photos_import_dialog_fetch_collections_local_query_executed,
                                   self,
                                   NULL);
    }

  photos_import_dialog_show_add_existing (self, FALSE);
  gtk_dialog_set_response_sensitive (GTK_DIALOG (self), GTK_RESPONSE_OK, FALSE);
}


static void
photos_import_dialog_class_init (PhotosImportDialogClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->dispose = photos_import_dialog_dispose;
  object_class->set_property = photos_import_dialog_set_property;

  g_object_class_install_property (object_class,
                                   PROP_TIME,
                                   g_param_spec_int64 ("time",
                                                       "Time",
                                                       "The timestamp for the default new collection name",
                                                       0,
                                                       G_MAXINT64,
                                                       0,
                                                       G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE));

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Photos/import-dialog.ui");
  gtk_widget_class_bind_template_callback (widget_class, photos_import_dialog_add_existing_button_toggled);
  gtk_widget_class_bind_template_callback (widget_class,
                                           photos_import_dialog_add_existing_collection_name_focus_in_event);
  gtk_widget_class_bind_template_callback (widget_class, photos_import_dialog_add_existing_collection_name_toggled);
  gtk_widget_class_bind_template_callback (widget_class, photos_import_dialog_collections_popover_search_changed);
  gtk_widget_class_bind_template_callback (widget_class, photos_import_dialog_create_new_button_toggled);
  gtk_widget_class_bind_template_callback (widget_class, photos_import_dialog_create_new_entry_changed);
  gtk_widget_class_bind_template_callback (widget_class, photos_import_dialog_create_new_entry_focus_in_event);
  gtk_widget_class_bind_template_child (widget_class, PhotosImportDialog, add_existing_button);
  gtk_widget_class_bind_template_child (widget_class, PhotosImportDialog, add_existing_collection_name_button);
  gtk_widget_class_bind_template_child (widget_class, PhotosImportDialog, add_existing_collection_name_label);
  gtk_widget_class_bind_template_child (widget_class, PhotosImportDialog, add_existing_label);
  gtk_widget_class_bind_template_child (widget_class, PhotosImportDialog, collections_popover);
  gtk_widget_class_bind_template_child (widget_class, PhotosImportDialog, collections_popover_grid);
  gtk_widget_class_bind_template_child (widget_class, PhotosImportDialog, collections_popover_search_entry);
  gtk_widget_class_bind_template_child (widget_class, PhotosImportDialog, create_new_button);
  gtk_widget_class_bind_template_child (widget_class, PhotosImportDialog, create_new_entry);
  gtk_widget_class_bind_template_child (widget_class, PhotosImportDialog, create_new_error_label);
  gtk_widget_class_bind_template_child (widget_class, PhotosImportDialog, create_new_error_revealer);
  gtk_widget_class_bind_template_child (widget_class, PhotosImportDialog, create_new_label);
}


GtkWidget *
photos_import_dialog_new (GtkWindow *parent, gint64 time)
{
  g_return_val_if_fail (GTK_IS_WINDOW (parent), NULL);
  g_return_val_if_fail (time >= 0, NULL);

  return g_object_new (PHOTOS_TYPE_IMPORT_DIALOG,
                       "time", time,
                       "transient-for", parent,
                       "use-header-bar", TRUE,
                       NULL);
}


PhotosBaseItem *
photos_import_dialog_get_collection (PhotosImportDialog *self)
{
  PhotosBaseItem *ret_val = NULL;

  g_return_val_if_fail (PHOTOS_IS_IMPORT_DIALOG (self), NULL);

  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->add_existing_button)))
    {
      g_autoptr (GVariant) state = NULL;
      PhotosBaseItem *collection;
      const gchar *id;

      state = g_action_get_state (G_ACTION (self->add_existing_action));
      g_return_val_if_fail (state != NULL, NULL);

      id = g_variant_get_string (state, NULL);
      collection = g_hash_table_lookup (self->collections, id);
      g_return_val_if_fail (PHOTOS_IS_BASE_ITEM (collection), NULL);

      ret_val = collection;
    }

  return ret_val;
}


const gchar *
photos_import_dialog_get_name (PhotosImportDialog *self, gchar **out_identifier_tag)
{
  const gchar *collection_name = NULL;
  g_autofree gchar *identifier_tag = NULL;

  g_return_val_if_fail (PHOTOS_IS_IMPORT_DIALOG (self), NULL);

  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->create_new_button)))
    {
      g_autofree gchar *default_collection_name = NULL;

      collection_name = gtk_entry_get_text (GTK_ENTRY (self->create_new_entry));
      default_collection_name = photos_import_dialog_create_default_collection_name (self);
      if (g_strcmp0 (collection_name, default_collection_name) == 0)
        identifier_tag = g_strdup_printf ("%" G_GINT64_FORMAT, self->time);
    }

  if (out_identifier_tag != NULL)
    *out_identifier_tag = g_steal_pointer (&identifier_tag);

  return collection_name;
}
