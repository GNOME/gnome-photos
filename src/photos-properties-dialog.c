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

/* Based on code from:
 *   + Documents
 */


#include "config.h"

#include <gio/gio.h>
#include <glib.h>
#include <glib/gi18n.h>

#include "photos-item-manager.h"
#include "photos-local-item.h"
#include "photos-properties-dialog.h"


struct _PhotosPropertiesDialogPrivate
{
  GtkWidget *title_entry;
  PhotosBaseManager *item_mngr;
  gchar *urn;
  guint title_entry_timeout;
};


enum
{
  PROP_0,
  PROP_URN
};


G_DEFINE_TYPE (PhotosPropertiesDialog, photos_properties_dialog, GTK_TYPE_DIALOG);


enum
{
  TITLE_ENTRY_TIMEOUT = 200 /* ms */
};


static gboolean
photos_properties_dialog_title_entry_timeout (gpointer user_data)
{
  PhotosPropertiesDialog *self = PHOTOS_PROPERTIES_DIALOG (user_data);
  PhotosPropertiesDialogPrivate *priv = self->priv;
  const gchar *new_title;

  priv->title_entry_timeout = 0;
  new_title = gtk_entry_get_text (GTK_ENTRY (priv->title_entry));
  photos_utils_set_edited_name (priv->urn, new_title);
}


static void
photos_properties_dialog_title_entry_changed (GtkEditable *editable, gpointer user_data)
{
  PhotosPropertiesDialog *self = PHOTOS_PROPERTIES_DIALOG (user_data);
  PhotosPropertiesDialogPrivate *priv = self->priv;

  if (priv->title_entry_timeout != 0)
    {
      g_source_remove (priv->title_entry_timeout);
      priv->title_entry_timeout = 0;
    }

  priv->title_entry_timeout = g_timeout_add (TITLE_ENTRY_TIMEOUT,
                                             photos_properties_dialog_title_entry_timeout,
                                             self);
}


static void
photos_properties_dialog_constructed (GObject *object)
{
  PhotosPropertiesDialog *self = PHOTOS_PROPERTIES_DIALOG (object);
  PhotosPropertiesDialogPrivate *priv = self->priv;
  GDateTime *date_modified;
  GtkStyleContext *context;
  GtkWidget *author_w = NULL;
  GtkWidget *content_area;
  GtkWidget *date_created_w = NULL;
  GtkWidget *date_modified_data;
  GtkWidget *date_modified_w;
  GtkWidget *grid;
  GtkWidget *item_type;
  GtkWidget *item_type_data;
  GtkWidget *source;
  GtkWidget *source_data;
  GtkWidget *title;
  PhotosBaseItem *item;
  const gchar *author;
  const gchar *name;
  const gchar *type_description;
  gchar *date_created_str = NULL;
  gchar *date_modified_str;
  gint64 ctime;
  gint64 mtime;

  G_OBJECT_CLASS (photos_properties_dialog_parent_class)->constructed (object);

  item = PHOTOS_BASE_ITEM (photos_base_manager_get_object_by_id (priv->item_mngr, priv->urn));

  mtime = photos_base_item_get_mtime (item);
  date_modified = g_date_time_new_from_unix_local (mtime);
  date_modified_str = g_date_time_format (date_modified, "%c");
  g_date_time_unref (date_modified);

  ctime = photos_base_item_get_date_created (item);
  if (ctime != -1)
    {
      GDateTime *date_created;

      date_created = g_date_time_new_from_unix_local (ctime);
      date_created_str = g_date_time_format (date_created, "%c");
      g_date_time_unref (date_created);
    }

  grid = gtk_grid_new ();
  gtk_widget_set_halign (grid, GTK_ALIGN_CENTER);
  gtk_widget_set_margin_left (grid, 24);
  gtk_widget_set_margin_right (grid, 24);
  gtk_widget_set_margin_bottom (grid, 12);
  gtk_orientable_set_orientation (GTK_ORIENTABLE (grid), GTK_ORIENTATION_VERTICAL);
  gtk_grid_set_column_homogeneous (GTK_GRID (grid), TRUE);
  gtk_grid_set_column_spacing (GTK_GRID (grid), 24);
  gtk_grid_set_row_spacing (GTK_GRID (grid), 12);

  content_area = gtk_dialog_get_content_area (GTK_DIALOG (self));
  gtk_box_pack_start (GTK_BOX (content_area), grid, TRUE, TRUE, 2);

  title = gtk_label_new (_("Title"));
  gtk_widget_set_halign (title, GTK_ALIGN_END);
  context = gtk_widget_get_style_context (title);
  gtk_style_context_add_class (context, "dim-label");
  gtk_container_add (GTK_CONTAINER (grid), title);

  author = photos_base_item_get_author (item);
  if (author != NULL && author[0] != '\0')
    {
      author_w = gtk_label_new (_("Author"));
      gtk_widget_set_halign (author_w, GTK_ALIGN_END);
      context = gtk_widget_get_style_context (author_w);
      gtk_style_context_add_class (context, "dim-label");
      gtk_container_add (GTK_CONTAINER (grid), author_w);
    }

  source = gtk_label_new (_("Source"));
  gtk_widget_set_halign (source, GTK_ALIGN_END);
  context = gtk_widget_get_style_context (source);
  gtk_style_context_add_class (context, "dim-label");
  gtk_container_add (GTK_CONTAINER (grid), source);

  date_modified_w = gtk_label_new (_("Date Modified"));
  gtk_widget_set_halign (date_modified_w, GTK_ALIGN_END);
  context = gtk_widget_get_style_context (date_modified_w);
  gtk_style_context_add_class (context, "dim-label");
  gtk_container_add (GTK_CONTAINER (grid), date_modified_w);

  if (date_created_str != NULL)
    {
      date_created_w = gtk_label_new (_("Date Created"));
      gtk_widget_set_halign (date_created_w, GTK_ALIGN_END);
      context = gtk_widget_get_style_context (date_created_w);
      gtk_style_context_add_class (context, "dim-label");
      gtk_container_add (GTK_CONTAINER (grid), date_created_w);
    }

  item_type = gtk_label_new (_("Type"));
  gtk_widget_set_halign (item_type, GTK_ALIGN_END);
  context = gtk_widget_get_style_context (item_type);
  gtk_style_context_add_class (context, "dim-label");
  gtk_container_add (GTK_CONTAINER (grid), item_type);

  name = photos_base_item_get_name (item);

  if (PHOTOS_IS_LOCAL_ITEM (item))
    {
      priv->title_entry = gtk_entry_new ();
      gtk_widget_set_halign (priv->title_entry, GTK_ALIGN_START);
      gtk_widget_set_hexpand (priv->title_entry, TRUE);
      gtk_entry_set_text (GTK_ENTRY (priv->title_entry), name);
      gtk_editable_set_editable (GTK_EDITABLE (priv->title_entry), TRUE);

      g_signal_connect (priv->title_entry,
                        "changed",
                        G_CALLBACK (photos_properties_dialog_title_entry_changed),
                        self);
    }
  else
    {
      priv->title_entry = gtk_label_new (name);
      gtk_widget_set_halign (priv->title_entry, GTK_ALIGN_START);
    }

  gtk_grid_attach_next_to (GTK_GRID (grid), priv->title_entry, title, 1, 2, 1);

  if (author_w != NULL)
    {
      GtkWidget *author_data;

      author_data = gtk_label_new (author);
      gtk_widget_set_halign (author_data, GTK_ALIGN_START);
      gtk_grid_attach_next_to (GTK_GRID (grid), author_data, author_w, 1, 2, 1);
    }

  if (PHOTOS_IS_LOCAL_ITEM (item))
    {
      GFile *file;
      GFile *source_link;
      const gchar *uri;
      gchar *source_path;
      gchar *source_uri;

      uri = photos_base_item_get_uri (item);
      file = g_file_new_for_uri (uri);
      source_link = g_file_get_parent (file);
      source_path = g_file_get_path (source_link);
      source_uri = g_file_get_uri (source_link);

      source_data = gtk_link_button_new_with_label (source_uri, source_path);
      gtk_widget_set_halign (source_data, GTK_ALIGN_START);
    }

  gtk_grid_attach_next_to (GTK_GRID (grid), source_data, source, 1, 2, 1);

  date_modified_data = gtk_label_new (date_modified_str);
  gtk_widget_set_halign (date_modified_data, GTK_ALIGN_START);
  gtk_grid_attach_next_to (GTK_GRID (grid), date_modified_data, date_modified_w, 1, 2, 1);

  if (date_created_w != NULL)
    {
      GtkWidget *date_created_data;

      date_created_data = gtk_label_new (date_created_str);
      gtk_widget_set_halign (date_created_data, GTK_ALIGN_START);
      gtk_grid_attach_next_to (GTK_GRID (grid), date_created_data, date_created_w, 1, 2, 1);
    }

  type_description = photos_base_item_get_type_description (item);
  item_type_data = gtk_label_new (type_description);
  gtk_widget_set_halign (item_type_data, GTK_ALIGN_START);
  gtk_grid_attach_next_to (GTK_GRID (grid), item_type_data, item_type, 1, 2, 1);

  gtk_widget_show_all (GTK_WIDGET (self));
}


static void
photos_properties_dialog_dispose (GObject *object)
{
  PhotosPropertiesDialog *self = PHOTOS_PROPERTIES_DIALOG (object);
  PhotosPropertiesDialogPrivate *priv = self->priv;

  g_clear_object (&priv->item_mngr);

  G_OBJECT_CLASS (photos_properties_dialog_parent_class)->dispose (object);
}


static void
photos_properties_dialog_finalize (GObject *object)
{
  PhotosPropertiesDialog *self = PHOTOS_PROPERTIES_DIALOG (object);
  PhotosPropertiesDialogPrivate *priv = self->priv;

  g_free (priv->urn);

  if (priv->title_entry_timeout != 0)
    g_source_remove (priv->title_entry_timeout);

  G_OBJECT_CLASS (photos_properties_dialog_parent_class)->finalize (object);
}


static void
photos_properties_dialog_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
  PhotosPropertiesDialog *self = PHOTOS_PROPERTIES_DIALOG (object);

  switch (prop_id)
    {
    case PROP_URN:
      self->priv->urn = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
photos_properties_dialog_init (PhotosPropertiesDialog *self)
{
  PhotosPropertiesDialogPrivate *priv;

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                            PHOTOS_TYPE_PROPERTIES_DIALOG,
                                            PhotosPropertiesDialogPrivate);
  priv = self->priv;

  priv->item_mngr = photos_item_manager_new ();
  gtk_dialog_add_button (GTK_DIALOG (self), _("Done"), GTK_RESPONSE_OK);
}


static void
photos_properties_dialog_class_init (PhotosPropertiesDialogClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->constructed= photos_properties_dialog_constructed;
  object_class->dispose = photos_properties_dialog_dispose;
  object_class->finalize = photos_properties_dialog_finalize;
  object_class->set_property = photos_properties_dialog_set_property;

  g_object_class_install_property (object_class,
                                   PROP_URN,
                                   g_param_spec_string ("urn",
                                                        "Uniform Resource Name",
                                                        "An unique ID associated with this item",
                                                        NULL,
                                                        G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE));

  g_type_class_add_private (class, sizeof (PhotosPropertiesDialogPrivate));
}


GtkWidget *
photos_properties_dialog_new (GtkWindow *parent, const gchar *urn)
{
  g_return_val_if_fail (GTK_IS_WINDOW (parent), NULL);

  return g_object_new (PHOTOS_TYPE_PROPERTIES_DIALOG,
                       "default-width", 400,
                       "destroy-with-parent", TRUE,
                       "hexpand", TRUE,
                       "modal", TRUE,
                       "resizable", FALSE,
                       "title", _("Properties"),
                       "transient-for", parent,
                       "urn", urn,
                       NULL);
}
