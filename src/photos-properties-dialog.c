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
#include <glib.h>
#include <glib/gi18n.h>

#include "photos-camera-cache.h"
#include "photos-facebook-item.h"
#include "photos-flickr-item.h"
#include "photos-item-manager.h"
#include "photos-local-item.h"
#include "photos-properties-dialog.h"
#include "photos-utils.h"


struct _PhotosPropertiesDialogPrivate
{
  GCancellable *cancellable;
  GtkWidget *camera_w;
  GtkWidget *grid;
  GtkWidget *title_entry;
  PhotosBaseManager *item_mngr;
  PhotosCameraCache *camera_cache;
  gchar *urn;
  guint title_entry_timeout;
};


enum
{
  PROP_0,
  PROP_URN
};


G_DEFINE_TYPE_WITH_PRIVATE (PhotosPropertiesDialog, photos_properties_dialog, GTK_TYPE_DIALOG);


enum
{
  TITLE_ENTRY_TIMEOUT = 200 /* ms */
};


static void
photos_properties_dialog_get_camera (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosPropertiesDialog *self;
  PhotosPropertiesDialogPrivate *priv;
  GError *error;
  gchar *camera;

  error = NULL;
  camera = photos_camera_cache_get_camera_finish (PHOTOS_CAMERA_CACHE (source_object), res, &error);
  if (error != NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Unable to get camera: %s", error->message);
      g_error_free (error);
      return;
    }

  /* Now we know that self has not been destroyed. */
  self = PHOTOS_PROPERTIES_DIALOG (user_data);
  priv = self->priv;

  if (camera != NULL)
    {
      GtkWidget *camera_data;

      camera_data = gtk_label_new (camera);
      gtk_widget_set_halign (camera_data, GTK_ALIGN_START);
      gtk_grid_attach_next_to (GTK_GRID (priv->grid), camera_data, priv->camera_w, GTK_POS_RIGHT, 2, 1);
      gtk_widget_show (camera_data);
    }

  g_free (camera);
}


static gboolean
photos_properties_dialog_title_entry_timeout (gpointer user_data)
{
  PhotosPropertiesDialog *self = PHOTOS_PROPERTIES_DIALOG (user_data);
  PhotosPropertiesDialogPrivate *priv = self->priv;
  const gchar *new_title;

  priv->title_entry_timeout = 0;
  new_title = gtk_entry_get_text (GTK_ENTRY (priv->title_entry));
  photos_utils_set_edited_name (priv->urn, new_title);

  g_object_unref (self);
  return G_SOURCE_REMOVE;
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
                                             g_object_ref (self));
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
  GtkWidget *exposure_time_w = NULL;
  GtkWidget *flash_w = NULL;
  GtkWidget *fnumber_w = NULL;
  GtkWidget *focal_length_w = NULL;
  GtkWidget *height_w = NULL;
  GtkWidget *iso_speed_w = NULL;
  GtkWidget *item_type;
  GtkWidget *item_type_data;
  GtkWidget *source;
  GtkWidget *source_data;
  GtkWidget *title;
  GtkWidget *width_w = NULL;
  GQuark equipment;
  GQuark flash;
  PhotosBaseItem *item;
  const gchar *author;
  const gchar *name;
  const gchar *type_description;
  gchar *date_created_str = NULL;
  gchar *date_modified_str;
  gdouble exposure_time;
  gdouble fnumber;
  gdouble focal_length;
  gdouble iso_speed;
  gint64 ctime;
  gint64 height;
  gint64 mtime;
  gint64 width;

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

  priv->grid = gtk_grid_new ();
  gtk_widget_set_halign (priv->grid, GTK_ALIGN_CENTER);
  gtk_widget_set_margin_start (priv->grid, 24);
  gtk_widget_set_margin_end (priv->grid, 24);
  gtk_widget_set_margin_bottom (priv->grid, 12);
  gtk_widget_set_margin_top (priv->grid, 12);
  gtk_orientable_set_orientation (GTK_ORIENTABLE (priv->grid), GTK_ORIENTATION_VERTICAL);
  gtk_grid_set_column_homogeneous (GTK_GRID (priv->grid), TRUE);
  gtk_grid_set_column_spacing (GTK_GRID (priv->grid), 24);
  gtk_grid_set_row_homogeneous (GTK_GRID (priv->grid), TRUE);
  gtk_grid_set_row_spacing (GTK_GRID (priv->grid), 6);

  content_area = gtk_dialog_get_content_area (GTK_DIALOG (self));
  gtk_box_pack_start (GTK_BOX (content_area), priv->grid, TRUE, TRUE, 2);

  /* Translators: this is the label next to the photo title in the
   * properties dialog
   */
  title = gtk_label_new (C_("Document Title", "Title"));
  gtk_widget_set_halign (title, GTK_ALIGN_END);
  context = gtk_widget_get_style_context (title);
  gtk_style_context_add_class (context, "dim-label");
  gtk_container_add (GTK_CONTAINER (priv->grid), title);

  author = photos_base_item_get_author (item);
  if (author != NULL && author[0] != '\0')
    {
      /* Translators: this is the label next to the photo author in
       * the properties dialog
       */
      author_w = gtk_label_new (C_("Document Author", "Author"));
      gtk_widget_set_halign (author_w, GTK_ALIGN_END);
      context = gtk_widget_get_style_context (author_w);
      gtk_style_context_add_class (context, "dim-label");
      gtk_container_add (GTK_CONTAINER (priv->grid), author_w);
    }

  source = gtk_label_new (_("Source"));
  gtk_widget_set_halign (source, GTK_ALIGN_END);
  context = gtk_widget_get_style_context (source);
  gtk_style_context_add_class (context, "dim-label");
  gtk_container_add (GTK_CONTAINER (priv->grid), source);

  date_modified_w = gtk_label_new (_("Date Modified"));
  gtk_widget_set_halign (date_modified_w, GTK_ALIGN_END);
  context = gtk_widget_get_style_context (date_modified_w);
  gtk_style_context_add_class (context, "dim-label");
  gtk_container_add (GTK_CONTAINER (priv->grid), date_modified_w);

  if (date_created_str != NULL)
    {
      date_created_w = gtk_label_new (_("Date Created"));
      gtk_widget_set_halign (date_created_w, GTK_ALIGN_END);
      context = gtk_widget_get_style_context (date_created_w);
      gtk_style_context_add_class (context, "dim-label");
      gtk_container_add (GTK_CONTAINER (priv->grid), date_created_w);
    }

  /* Translators: this is the label next to the photo type in the
   * properties dialog
   */
  item_type = gtk_label_new (C_("Document Type", "Type"));
  gtk_widget_set_halign (item_type, GTK_ALIGN_END);
  context = gtk_widget_get_style_context (item_type);
  gtk_style_context_add_class (context, "dim-label");
  gtk_container_add (GTK_CONTAINER (priv->grid), item_type);

  width = photos_base_item_get_width (item);
  if (width > 0)
    {
      width_w = gtk_label_new (_("Width"));
      gtk_widget_set_halign (width_w, GTK_ALIGN_END);
      context = gtk_widget_get_style_context (width_w);
      gtk_style_context_add_class (context, "dim-label");
      gtk_container_add (GTK_CONTAINER (priv->grid), width_w);
    }

  height = photos_base_item_get_height (item);
  if (height > 0)
    {
      height_w = gtk_label_new (_("Height"));
      gtk_widget_set_halign (height_w, GTK_ALIGN_END);
      context = gtk_widget_get_style_context (height_w);
      gtk_style_context_add_class (context, "dim-label");
      gtk_container_add (GTK_CONTAINER (priv->grid), height_w);
    }

  equipment = photos_base_item_get_equipment (item);
  photos_camera_cache_get_camera_async (priv->camera_cache,
                                        equipment,
                                        priv->cancellable,
                                        photos_properties_dialog_get_camera,
                                        self);
  if (equipment != 0)
    {
      priv->camera_w = gtk_label_new (_("Camera"));
      gtk_widget_set_halign (priv->camera_w, GTK_ALIGN_END);
      context = gtk_widget_get_style_context (priv->camera_w);
      gtk_style_context_add_class (context, "dim-label");
      gtk_container_add (GTK_CONTAINER (priv->grid), priv->camera_w);
    }

  exposure_time = photos_base_item_get_exposure_time (item);
  if (exposure_time > 0.0)
    {
      exposure_time_w = gtk_label_new (_("Exposure"));
      gtk_widget_set_halign (exposure_time_w, GTK_ALIGN_END);
      context = gtk_widget_get_style_context (exposure_time_w);
      gtk_style_context_add_class (context, "dim-label");
      gtk_container_add (GTK_CONTAINER (priv->grid), exposure_time_w);
    }

  fnumber = photos_base_item_get_fnumber (item);
  if (fnumber > 0.0)
    {
      fnumber_w = gtk_label_new (_("Aperture"));
      gtk_widget_set_halign (fnumber_w, GTK_ALIGN_END);
      context = gtk_widget_get_style_context (fnumber_w);
      gtk_style_context_add_class (context, "dim-label");
      gtk_container_add (GTK_CONTAINER (priv->grid), fnumber_w);
    }

  focal_length = photos_base_item_get_focal_length (item);
  if (focal_length > 0.0)
    {
      focal_length_w = gtk_label_new (_("Focal Length"));
      gtk_widget_set_halign (focal_length_w, GTK_ALIGN_END);
      context = gtk_widget_get_style_context (focal_length_w);
      gtk_style_context_add_class (context, "dim-label");
      gtk_container_add (GTK_CONTAINER (priv->grid), focal_length_w);
    }

  iso_speed = photos_base_item_get_iso_speed (item);
  if (iso_speed > 0.0)
    {
      iso_speed_w = gtk_label_new (_("ISO Speed"));
      gtk_widget_set_halign (iso_speed_w, GTK_ALIGN_END);
      context = gtk_widget_get_style_context (iso_speed_w);
      gtk_style_context_add_class (context, "dim-label");
      gtk_container_add (GTK_CONTAINER (priv->grid), iso_speed_w);
    }

  flash = photos_base_item_get_flash (item);
  if (flash != 0)
    {
      flash_w = gtk_label_new (_("Flash"));
      gtk_widget_set_halign (flash_w, GTK_ALIGN_END);
      context = gtk_widget_get_style_context (flash_w);
      gtk_style_context_add_class (context, "dim-label");
      gtk_container_add (GTK_CONTAINER (priv->grid), flash_w);
    }

  name = photos_base_item_get_name (item);

  if (PHOTOS_IS_LOCAL_ITEM (item))
    {
      priv->title_entry = gtk_entry_new ();
      gtk_widget_set_halign (priv->title_entry, GTK_ALIGN_START);
      gtk_widget_set_hexpand (priv->title_entry, TRUE);
      gtk_entry_set_activates_default (GTK_ENTRY (priv->title_entry), TRUE);
      gtk_entry_set_text (GTK_ENTRY (priv->title_entry), name);
      gtk_entry_set_width_chars (GTK_ENTRY (priv->title_entry), 40);
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

  gtk_grid_attach_next_to (GTK_GRID (priv->grid), priv->title_entry, title, GTK_POS_RIGHT, 2, 1);

  if (author_w != NULL)
    {
      GtkWidget *author_data;

      author_data = gtk_label_new (author);
      gtk_widget_set_halign (author_data, GTK_ALIGN_START);
      gtk_grid_attach_next_to (GTK_GRID (priv->grid), author_data, author_w, GTK_POS_RIGHT, 2, 1);
    }

  if (PHOTOS_IS_FACEBOOK_ITEM (item))
    {
      const gchar *source_name;

      source_name = photos_base_item_get_source_name (item);
      source_data = gtk_link_button_new_with_label ("https://www.facebook.com/", source_name);
      gtk_widget_set_halign (source_data, GTK_ALIGN_START);
    }
  else if (PHOTOS_IS_FLICKR_ITEM (item))
    {
      const gchar *source_name;

      source_name = photos_base_item_get_source_name (item);
      source_data = gtk_link_button_new_with_label ("https://www.flickr.com/", source_name);
      gtk_widget_set_halign (source_data, GTK_ALIGN_START);
    }
  else /* local item */
    {
      if (photos_base_item_is_collection (item))
        {
          const gchar *source_name;

          source_name = photos_base_item_get_source_name (item);
          source_data = gtk_label_new (source_name);
          gtk_widget_set_halign (source_data, GTK_ALIGN_START);
        }
      else
        {
          GFile *file;
          GFile *source_link;
          GtkWidget *label;
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

          label = gtk_bin_get_child (GTK_BIN (source_data));
          gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);

          g_object_unref (source_link);
          g_object_unref (file);
        }
    }

  gtk_grid_attach_next_to (GTK_GRID (priv->grid), source_data, source, GTK_POS_RIGHT, 2, 1);

  date_modified_data = gtk_label_new (date_modified_str);
  gtk_widget_set_halign (date_modified_data, GTK_ALIGN_START);
  gtk_grid_attach_next_to (GTK_GRID (priv->grid), date_modified_data, date_modified_w, GTK_POS_RIGHT, 2, 1);

  if (date_created_w != NULL)
    {
      GtkWidget *date_created_data;

      date_created_data = gtk_label_new (date_created_str);
      gtk_widget_set_halign (date_created_data, GTK_ALIGN_START);
      gtk_grid_attach_next_to (GTK_GRID (priv->grid), date_created_data, date_created_w, GTK_POS_RIGHT, 2, 1);
    }

  type_description = photos_base_item_get_type_description (item);
  item_type_data = gtk_label_new (type_description);
  gtk_widget_set_halign (item_type_data, GTK_ALIGN_START);
  gtk_grid_attach_next_to (GTK_GRID (priv->grid), item_type_data, item_type, GTK_POS_RIGHT, 2, 1);

  if (width_w != NULL)
    {
      GtkWidget *width_data;
      gchar *width_str;

      width_str = g_strdup_printf ("%"G_GINT64_FORMAT" pixels", width);
      width_data = gtk_label_new (width_str);
      gtk_widget_set_halign (width_data, GTK_ALIGN_START);
      gtk_grid_attach_next_to (GTK_GRID (priv->grid), width_data, width_w, GTK_POS_RIGHT, 2, 1);
      g_free (width_str);
    }

  if (height_w != NULL)
    {
      GtkWidget *height_data;
      gchar *height_str;

      height_str = g_strdup_printf ("%"G_GINT64_FORMAT" pixels", height);
      height_data = gtk_label_new (height_str);
      gtk_widget_set_halign (height_data, GTK_ALIGN_START);
      gtk_grid_attach_next_to (GTK_GRID (priv->grid), height_data, height_w, GTK_POS_RIGHT, 2, 1);
      g_free (height_str);
    }

  if (exposure_time_w != NULL)
    {
      GtkWidget *exposure_time_data;
      gchar *exposure_time_str;

      exposure_time_str = g_strdup_printf ("%.3lf sec", exposure_time);
      exposure_time_data = gtk_label_new (exposure_time_str);
      gtk_widget_set_halign (exposure_time_data, GTK_ALIGN_START);
      gtk_grid_attach_next_to (GTK_GRID (priv->grid), exposure_time_data, exposure_time_w, GTK_POS_RIGHT, 2, 1);
      g_free (exposure_time_str);
    }

  if (fnumber_w != NULL)
    {
      GtkWidget *fnumber_data;
      gchar *fnumber_str;

      fnumber_str = g_strdup_printf ("f/%.1lf", fnumber);
      fnumber_data = gtk_label_new (fnumber_str);
      gtk_widget_set_halign (fnumber_data, GTK_ALIGN_START);
      gtk_grid_attach_next_to (GTK_GRID (priv->grid), fnumber_data, fnumber_w, GTK_POS_RIGHT, 2, 1);
      g_free (fnumber_str);
    }

  if (focal_length_w != NULL)
    {
      GtkWidget *focal_length_data;
      gchar *focal_length_str;

      focal_length_str = g_strdup_printf ("%.0lf mm", focal_length);
      focal_length_data = gtk_label_new (focal_length_str);
      gtk_widget_set_halign (focal_length_data, GTK_ALIGN_START);
      gtk_grid_attach_next_to (GTK_GRID (priv->grid), focal_length_data, focal_length_w, GTK_POS_RIGHT, 2, 1);
      g_free (focal_length_str);
    }

  if (iso_speed_w != NULL)
    {
      GtkWidget *iso_speed_data;
      gchar *iso_speed_str;

      iso_speed_str = g_strdup_printf ("%.0lf", iso_speed);
      iso_speed_data = gtk_label_new (iso_speed_str);
      gtk_widget_set_halign (iso_speed_data, GTK_ALIGN_START);
      gtk_grid_attach_next_to (GTK_GRID (priv->grid), iso_speed_data, iso_speed_w, GTK_POS_RIGHT, 2, 1);
      g_free (iso_speed_str);
    }

  if (flash_w != NULL)
    {
      GtkWidget *flash_data;
      gchar *flash_str;

      if (flash == PHOTOS_FLASH_OFF)
        flash_str = g_strdup (_("Off, did not fire"));
      else if (flash == PHOTOS_FLASH_ON)
        flash_str = g_strdup (_("On, fired"));
      else
        g_assert_not_reached ();

      flash_data = gtk_label_new (flash_str);
      gtk_widget_set_halign (flash_data, GTK_ALIGN_START);
      gtk_grid_attach_next_to (GTK_GRID (priv->grid), flash_data, flash_w, GTK_POS_RIGHT, 2, 1);
      g_free (flash_str);
    }

  g_free (date_created_str);
  g_free (date_modified_str);
}


static void
photos_properties_dialog_dispose (GObject *object)
{
  PhotosPropertiesDialog *self = PHOTOS_PROPERTIES_DIALOG (object);
  PhotosPropertiesDialogPrivate *priv = self->priv;

  if (priv->cancellable != NULL)
    {
      g_cancellable_cancel (priv->cancellable);
      g_clear_object (&priv->cancellable);
    }

  g_clear_object (&priv->item_mngr);
  g_clear_object (&priv->camera_cache);

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

  self->priv = photos_properties_dialog_get_instance_private (self);
  priv = self->priv;

  priv->cancellable = g_cancellable_new ();
  priv->item_mngr = photos_item_manager_dup_singleton ();
  priv->camera_cache = photos_camera_cache_dup_singleton ();

  gtk_dialog_add_button (GTK_DIALOG (self), _("Done"), GTK_RESPONSE_CLOSE);
  gtk_dialog_set_default_response (GTK_DIALOG (self), GTK_RESPONSE_CLOSE);
}


static void
photos_properties_dialog_class_init (PhotosPropertiesDialogClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->constructed = photos_properties_dialog_constructed;
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
                       "use-header-bar", TRUE,
                       NULL);
}
