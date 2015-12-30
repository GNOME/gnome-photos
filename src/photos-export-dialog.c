/*
 * Photos - access, organize and share your photos on GNOME
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

#include "config.h"

#include <glib.h>
#include <glib/gi18n.h>

#include "photos-export-dialog.h"
#include "photos-utils.h"


struct _PhotosExportDialog
{
  GtkDialog parent_instance;
  GCancellable *cancellable;
  GtkWidget *dir_entry;
  GtkWidget *folder_name_label;
  GtkWidget *full_button;
  GtkWidget *full_label;
  GtkWidget *msg_label;
  GtkWidget *reduced_button;
  GtkWidget *reduced_label;
  GtkWidget *size_label;
  PhotosBaseItem *item;
  gdouble reduced_zoom;
};

struct _PhotosExportDialogClass
{
  GtkDialogClass parent_class;
};


enum
{
  PROP_0,
  PROP_ITEM
};


G_DEFINE_TYPE (PhotosExportDialog, photos_export_dialog, GTK_TYPE_DIALOG);


static const gint PIXEL_SIZES[] = {2048, 1024};


static void
photos_export_dialog_guess_sizes (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosExportDialog *self;
  PhotosBaseItem *item = PHOTOS_BASE_ITEM (source_object);
  GError *error;
  gchar *size_str;
  gsize sizes[2];

  error = NULL;
  if (!photos_base_item_save_guess_sizes_finish (item, res, &sizes[0], &sizes[1], &error))
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Unable to guess sizes: %s", error->message);
      g_error_free (error);
      return;
    }

  self = PHOTOS_EXPORT_DIALOG (user_data);

  size_str = g_format_size ((guint64) sizes[0]);
  gtk_label_set_text (GTK_LABEL (self->full_label), size_str);
  g_free (size_str);

  if (self->reduced_zoom > 0.0)
    {
      gsize reduced_size;

      reduced_size = (gsize) (sizes[1] + (sizes[0] - sizes[1]) * (self->reduced_zoom - 0.5) / (1.0 - 0.5) + 0.5);
      size_str = g_format_size ((guint64) reduced_size);
      gtk_label_set_text (GTK_LABEL (self->reduced_label), size_str);
      g_free (size_str);
    }
}


static void
photos_export_dialog_show_size_options (PhotosExportDialog *self)
{
  gtk_widget_set_margin_bottom (self->dir_entry, 6);
  gtk_widget_set_margin_bottom (self->folder_name_label, 6);
  gtk_widget_show (self->full_label);
  gtk_widget_show (self->full_button);
  gtk_widget_show (self->reduced_button);
  gtk_widget_show (self->reduced_label);
  gtk_widget_show (self->size_label);
}


static void
photos_export_dialog_constructed (GObject *object)
{
  PhotosExportDialog *self = PHOTOS_EXPORT_DIALOG (object);

  G_OBJECT_CLASS (photos_export_dialog_parent_class)->constructed (object);

  if (photos_base_item_is_collection (self->item))
    {
      const gchar *name;

      name = photos_base_item_get_name_with_fallback (self->item);
      gtk_entry_set_text (GTK_ENTRY (self->dir_entry), name);
    }
  else
    {
      GDateTime *now;
      GeglRectangle bbox;
      gboolean got_bbox_edited;
      gchar *now_str;
      gint max_dimension;
      guint i;

      got_bbox_edited = photos_base_item_get_bbox_edited (self->item, &bbox);
      g_return_if_fail (got_bbox_edited);

      max_dimension = MAX (bbox.height, bbox.width);
      for (i = 0; i < G_N_ELEMENTS (PIXEL_SIZES); i++)
        {
          if (max_dimension > PIXEL_SIZES[i])
            {
              self->reduced_zoom = (gdouble) PIXEL_SIZES[i] / (gdouble) max_dimension;
              photos_export_dialog_show_size_options (self);
              photos_base_item_save_guess_sizes_async (self->item,
                                                       self->cancellable,
                                                       photos_export_dialog_guess_sizes,
                                                       self);
              break;
            }
        }

      now = g_date_time_new_now_local ();

      /* Translators: this is the default sub-directory where photos
       *  will be exported.
       */
      now_str = g_date_time_format (now, _("%e %B %Y"));

      gtk_entry_set_text (GTK_ENTRY (self->dir_entry), now_str);

      g_free (now_str);
      g_date_time_unref (now);
    }
}


static void
photos_export_dialog_dispose (GObject *object)
{
  PhotosExportDialog *self = PHOTOS_EXPORT_DIALOG (object);

  if (self->cancellable != NULL)
    g_cancellable_cancel (self->cancellable);

  g_clear_object (&self->cancellable);
  g_clear_object (&self->item);

  G_OBJECT_CLASS (photos_export_dialog_parent_class)->dispose (object);
}


static void
photos_export_dialog_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
  PhotosExportDialog *self = PHOTOS_EXPORT_DIALOG (object);

  switch (prop_id)
    {
    case PROP_ITEM:
      self->item = PHOTOS_BASE_ITEM (g_value_dup_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
photos_export_dialog_init (PhotosExportDialog *self)
{
  const gchar *pictures_path;
  gchar *msg;
  gchar *pictures_path_basename;

  gtk_widget_init_template (GTK_WIDGET (self));

  self->cancellable = g_cancellable_new ();

  pictures_path = g_get_user_special_dir (G_USER_DIRECTORY_PICTURES);
  pictures_path_basename = g_path_get_basename (pictures_path);
  msg = g_strdup_printf (_("Photos are exported to the %s ▶ %s folder."),
                         pictures_path_basename,
                         PHOTOS_EXPORT_SUBPATH);
  gtk_label_set_label (GTK_LABEL (self->msg_label), msg);

  self->reduced_zoom = -1.0;

  g_free (msg);
  g_free (pictures_path_basename);
}


static void
photos_export_dialog_class_init (PhotosExportDialogClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->constructed = photos_export_dialog_constructed;
  object_class->dispose = photos_export_dialog_dispose;
  object_class->set_property = photos_export_dialog_set_property;

  g_object_class_install_property (object_class,
                                   PROP_ITEM,
                                   g_param_spec_object ("item",
                                                        "PhotosBaseItem object",
                                                        "The item to export",
                                                        PHOTOS_TYPE_BASE_ITEM,
                                                        G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE));

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Photos/export-dialog.ui");
  gtk_widget_class_bind_template_child (widget_class, PhotosExportDialog, dir_entry);
  gtk_widget_class_bind_template_child (widget_class, PhotosExportDialog, folder_name_label);
  gtk_widget_class_bind_template_child (widget_class, PhotosExportDialog, full_button);
  gtk_widget_class_bind_template_child (widget_class, PhotosExportDialog, full_label);
  gtk_widget_class_bind_template_child (widget_class, PhotosExportDialog, msg_label);
  gtk_widget_class_bind_template_child (widget_class, PhotosExportDialog, reduced_button);
  gtk_widget_class_bind_template_child (widget_class, PhotosExportDialog, reduced_label);
  gtk_widget_class_bind_template_child (widget_class, PhotosExportDialog, size_label);
}


GtkWidget *
photos_export_dialog_new (GtkWindow *parent, PhotosBaseItem *item)
{
  g_return_val_if_fail (GTK_IS_WINDOW (parent), NULL);
  g_return_val_if_fail (PHOTOS_IS_BASE_ITEM (item), NULL);

  return g_object_new (PHOTOS_TYPE_EXPORT_DIALOG,
                       "item", item,
                       "transient-for", parent,
                       "use-header-bar", TRUE,
                       NULL);
}


const gchar *
photos_export_dialog_get_dir_name (PhotosExportDialog *self)
{
  const gchar *dir_name;

  g_return_val_if_fail (PHOTOS_IS_EXPORT_DIALOG (self), NULL);

  dir_name = gtk_entry_get_text (GTK_ENTRY (self->dir_entry));
  return dir_name;
}


gdouble
photos_export_dialog_get_zoom (PhotosExportDialog *self)
{
  gdouble ret_val = 1.0;

  g_return_val_if_fail (PHOTOS_IS_EXPORT_DIALOG (self), 1.0);

  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->reduced_button)))
    {
      g_return_val_if_fail (self->reduced_zoom > 0.0, 1.0);
      ret_val = self->reduced_zoom;
    }

  return ret_val;
}
