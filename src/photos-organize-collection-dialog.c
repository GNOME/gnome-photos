/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2012, 2013, 2014 Red Hat, Inc.
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

#include "photos-organize-collection-dialog.h"
#include "photos-organize-collection-view.h"


struct _PhotosOrganizeCollectionDialogPrivate
{
  gboolean rename_mode;
  GtkButton *add_button_empty;
  GtkButton *add_button_collections;
  GtkButton *add_entry_empty;
  GtkButton *add_entry_collections;
  GtkWidget *collection_list;
  GtkWidget *scrolled_window_collections;
};


G_DEFINE_TYPE_WITH_PRIVATE (PhotosOrganizeCollectionDialog, photos_organize_collection_dialog, GTK_TYPE_WINDOW);

static void
photos_organize_collection_dialog_add_button_clicked (GtkButton *button, gpointer user_data)
{
    PhotosOrganizeCollectionDialogPrivate *priv = PHOTOS_ORGANIZE_COLLECTION_DIALOG (user_data)->priv;
    //TODO
}

static void
photos_organize_collection_dialog_delete_collection (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    PhotosOrganizeCollectionDialogPrivate *priv = PHOTOS_ORGANIZE_COLLECTION_DIALOG (user_data)->priv;
    const gchar *collId;
    collId = g_variant_get_string (parameter, NULL);
    //TODO
}

static gboolean
photos_organize_collection_dialog_key_pressed (PhotosOrganizeCollectionDialog *self, GdkEventKey *event, gpointer user_data)
{
  PhotosOrganizeCollectionDialogPrivate *priv = self->priv;

  if (event->keyval == GDK_KEY_Escape)
    {
      if (priv->rename_mode)
        gtk_widget_destroy (GTK_WIDGET (self));// TODO this._renameModeStop(false);
      else
        gtk_widget_destroy (GTK_WIDGET (self));

      return GDK_EVENT_STOP;
    }
  return GDK_EVENT_PROPAGATE;
}

static void
photos_organize_collection_dialog_rename_mode_start (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    PhotosOrganizeCollectionDialogPrivate *priv = PHOTOS_ORGANIZE_COLLECTION_DIALOG (user_data)->priv;
    const gchar *collId;
    collId = g_variant_get_string (parameter, NULL);
    photos_organize_collection_dialog_set_rename_mode (TRUE);
    // TODO
}

static void
photos_organize_collection_dialog_text_changed (GtkEntry *entry, gpointer user_data)
{
  GtkButton *add_button;

  PhotosOrganizeCollectionDialog *self = PHOTOS_ORGANIZE_COLLECTION_DIALOG (user_data);
  PhotosOrganizeCollectionDialogPrivate *priv = self->priv;

  gboolean sensitive = TRUE;  // TODO check if is valid
  if (priv->rename_mode)
    {
      // TODO
    }
  else
    {
      add_button = priv->add_button_empty;// TODO check if empty
      gtk_widget_set_sensitive (GTK_WIDGET (add_button), sensitive);
    }
}

static void
photos_organize_collection_dialog_init (PhotosOrganizeCollectionDialog *self)
{
  PhotosOrganizeCollectionDialogPrivate *priv;
  GSimpleActionGroup *action_group;
  GSimpleAction *delete_action;
  GSimpleAction *rename_action;
  GVariantType *parameter_type;

  self->priv = photos_organize_collection_dialog_get_instance_private (self);
  priv = self->priv;

  gtk_widget_init_template (GTK_WIDGET (self));

  priv->rename_mode = FALSE;

  g_signal_connect (self, "key-press-event", (GCallback) photos_organize_collection_dialog_key_pressed, NULL);
  g_signal_connect (priv->add_button_empty, "clicked", (GCallback) photos_organize_collection_dialog_add_button_clicked, self);
  g_signal_connect (priv->add_button_collections, "clicked", (GCallback) photos_organize_collection_dialog_add_button_clicked, self);
  g_signal_connect (priv->add_entry_empty, "changed", (GCallback) photos_organize_collection_dialog_text_changed, self);
  g_signal_connect (priv->add_entry_collections, "changed", (GCallback) photos_organize_collection_dialog_text_changed, self);

  action_group = g_simple_action_group_new ();
  parameter_type = g_variant_type_new ("s");
  delete_action = g_simple_action_new ("delete-collection", parameter_type);
  g_variant_type_free (parameter_type);
  parameter_type = g_variant_type_new ("s");
  rename_action = g_simple_action_new ("rename-collection", parameter_type);
  g_variant_type_free (parameter_type);
  g_action_map_add_action (G_ACTION_MAP (action_group), G_ACTION (delete_action));
  g_action_map_add_action (G_ACTION_MAP (action_group), G_ACTION (rename_action));
  gtk_widget_insert_action_group (GTK_WIDGET (self), "dialog", G_ACTION_GROUP (action_group));

  g_signal_connect (delete_action, "activate", (GCallback) photos_organize_collection_dialog_rename_mode_start, self);
  g_signal_connect (rename_action, "activate", (GCallback) photos_organize_collection_dialog_delete_collection, self);

  //TODO
}


static void
photos_organize_collection_dialog_class_init (PhotosOrganizeCollectionDialogClass *class)
{
  GtkWindowClass *window_class = GTK_WINDOW_CLASS (class);

  gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (window_class), "/org/gnome/Photos/organize-collection-dialog.ui");
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (window_class), PhotosOrganizeCollectionDialog, add_button_empty);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (window_class), PhotosOrganizeCollectionDialog, add_button_collections);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (window_class), PhotosOrganizeCollectionDialog, add_entry_empty);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (window_class), PhotosOrganizeCollectionDialog, add_entry_collections);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (window_class), PhotosOrganizeCollectionDialog, scrolled_window_collections);
}


GtkWidget *
photos_organize_collection_dialog_new (GtkWindow *parent)
{
  g_return_val_if_fail (GTK_IS_WINDOW (parent), NULL);

  return g_object_new (PHOTOS_TYPE_ORGANIZE_COLLECTION_DIALOG,
                       "transient-for", parent,
                       NULL);
}
