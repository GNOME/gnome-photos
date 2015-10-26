/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2015 Alessandro Bono
 * Copyright © 2015 Red Hat, Inc.
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
#include <glib/gi18n.h>

#include "photos-base-item.h"
#include "photos-base-manager.h"
#include "photos-fetch-collection-state-job.h"
#include "photos-organize-collection-list.h"
#include "photos-search-context.h"


struct _PhotosOrganizeCollectionList
{
  GtkListBox parent_instance;
  PhotosBaseManager *item_mngr;
};

struct _PhotosOrganizeCollectionListClass
{
  GtkListBoxClass parent_class;
};


G_DEFINE_TYPE (PhotosOrganizeCollectionList, photos_organize_collection_list, GTK_TYPE_LIST_BOX);


static void
photos_organize_collection_list_fetch_collection_state_executed (GHashTable *collection_state, gpointer user_data)
{
  PhotosOrganizeCollectionList *self = PHOTOS_ORGANIZE_COLLECTION_LIST (user_data);
  GHashTableIter collection_state_iter;
  const gchar *idx;
  gpointer value;

  g_hash_table_iter_init (&collection_state_iter, collection_state);
  while (g_hash_table_iter_next (&collection_state_iter, (gpointer) &idx, (gpointer) &value))
    {
      PhotosBaseItem *item;
      gint state = GPOINTER_TO_INT (value);

      if (state & PHOTOS_COLLECTION_STATE_HIDDEN)
        continue;

      item = PHOTOS_BASE_ITEM (photos_base_manager_get_object_by_id (self->item_mngr, idx));
      //TODO create and add collection
    }

  g_object_unref (self);
}

gboolean
photos_organize_collection_list_is_empty (PhotosOrganizeCollectionList *self)
{
  return (gtk_container_get_children (GTK_CONTAINER (self)) == NULL);
}

gboolean
photos_organize_collection_list_is_valid_name (PhotosOrganizeCollectionList *self, const gchar *name)
{
  GList *children;
  GList *l;

  if (name == NULL || (g_strcmp0 (name, "") == 0))
    return FALSE;

  children = gtk_container_get_children (GTK_CONTAINER (self));
  for (l = children; l != NULL; l = l->next)
    {
      // TODO GIOExtension *extension = (GIOExtension *) l->data;
    }
  return TRUE;
}

static void
photos_organize_collection_list_object_added (PhotosBaseManager *manager, GObject *object, gpointer user_data)
{
  PhotosOrganizeCollectionList *self = PHOTOS_ORGANIZE_COLLECTION_LIST (user_data);
  //photos_organize_collection_model_refresh_state (self);
  // TODO
}


static void
photos_organize_collection_list_object_removed (PhotosBaseManager *manager, GObject *object, gpointer user_data)
{
  PhotosOrganizeCollectionList *self = PHOTOS_ORGANIZE_COLLECTION_LIST (user_data);
  // TODO
}

static void
photos_organize_collection_list_update_header_func (GtkListBoxRow *row, GtkListBoxRow *before)
{
  GtkWidget *current;

  if (before == NULL)
    {
      gtk_list_box_row_set_header (row, NULL);
      return;
    }

  current = gtk_list_box_row_get_header (row);
  if (current == NULL)
    {
      current = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
      gtk_widget_show (current);
      gtk_list_box_row_set_header (row, current);
    }
}


static gint
photos_organize_collection_list_sort_func (GtkListBoxRow *row1,
                                           GtkListBoxRow *row2,
                                           gpointer user_data)
{
  return -1;//TODO
}


static void
photos_organize_collection_list_dispose (GObject *object)
{
  PhotosOrganizeCollectionList *self = PHOTOS_ORGANIZE_COLLECTION_LIST (object);
  GList *rows;

  rows = gtk_container_get_children (GTK_CONTAINER (self));

  g_clear_object (&self->item_mngr);

  G_OBJECT_CLASS (photos_organize_collection_list_parent_class)->dispose (object);
}


static void
photos_organize_collection_list_init (PhotosOrganizeCollectionList *self)
{
  GApplication *app;
  PhotosFetchCollectionStateJob *job;
  PhotosSearchContextState *state;
  guint coll_added_id;
  guint coll_removed_id;

  app = g_application_get_default ();
  state = photos_search_context_get_state (PHOTOS_SEARCH_CONTEXT (app));

  self->item_mngr = g_object_ref (state->item_mngr);
  g_signal_connect_object (self->item_mngr,
                           "object-added",
                           G_CALLBACK (photos_organize_collection_list_object_added),
                           self,
                           0);
  g_signal_connect_object (self->item_mngr,
                           "object-removed",
                           G_CALLBACK (photos_organize_collection_list_object_removed),
                           self,
                           0);

  gtk_list_box_set_header_func (GTK_LIST_BOX (self),
                                (GtkListBoxUpdateHeaderFunc) photos_organize_collection_list_update_header_func,
                                NULL,
                                NULL);
  gtk_list_box_set_sort_func (GTK_LIST_BOX (self), photos_organize_collection_list_sort_func, NULL, NULL);

  //g_signal_connect () //TODO destoy signal

  job = photos_fetch_collection_state_job_new ();
  photos_fetch_collection_state_job_run (job,
                                         photos_organize_collection_list_fetch_collection_state_executed,
                                         g_object_ref (self));
  g_object_unref (job);
}


static void
photos_organize_collection_list_class_init (PhotosOrganizeCollectionListClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->dispose = photos_organize_collection_list_dispose;
}


GtkWidget *
photos_organize_collection_list_new ()
{
  return g_object_new (PHOTOS_TYPE_ORGANIZE_COLLECTION_LIST,
                       "vexpand", FALSE,
                       "margin", 0,
                       "selection-mode", GTK_SELECTION_NONE,
                       NULL);
}
