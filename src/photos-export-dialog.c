/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2015 – 2017 Red Hat, Inc.
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
  GtkWidget *progress_label;
  GtkWidget *reduced_button;
  GtkWidget *reduced_label;
  GtkWidget *size_label;
  PhotosBaseItem *item;
  gdouble reduced_zoom;
};

enum
{
  PROP_0,
  PROP_ITEM
};


G_DEFINE_TYPE (PhotosExportDialog, photos_export_dialog, GTK_TYPE_DIALOG);


static gchar *
photos_export_dialog_create_size_str (gint height, gint width, guint64 size)
{
  g_autofree gchar *size_str = NULL;
  gchar *ret_val;

  size_str = g_format_size (size);

  /* Translators: this is the estimated size of the exported image in
   * the form "1600×1067 (0.6 GB)".
   */
  ret_val = g_strdup_printf (_("%d×%d (%s)"), width, height, size_str);

  return ret_val;
}


static void
photos_export_dialog_show_size_options (PhotosExportDialog *self, gboolean size_options, gboolean progress)
{
  GtkStyleContext *context;
  const gchar *class_name;
  const gchar *invert_class_name;

  class_name = progress ? "photos-fade-in" : "photos-fade-out";
  invert_class_name = !progress ? "photos-fade-in" : "photos-fade-out";

  gtk_widget_show (self->progress_label);
  context = gtk_widget_get_style_context (self->progress_label);
  gtk_style_context_remove_class (context, invert_class_name);
  gtk_style_context_add_class (context, class_name);

  class_name = size_options ? "photos-fade-in" : "photos-fade-out";
  invert_class_name = !size_options ? "photos-fade-in" : "photos-fade-out";

  gtk_widget_show (self->full_label);
  context = gtk_widget_get_style_context (self->full_label);
  gtk_style_context_remove_class (context, invert_class_name);
  gtk_style_context_add_class (context, class_name);

  gtk_widget_show (self->full_button);
  context = gtk_widget_get_style_context (self->full_button);
  gtk_style_context_remove_class (context, invert_class_name);
  gtk_style_context_add_class (context, class_name);

  gtk_widget_show (self->reduced_button);
  context = gtk_widget_get_style_context (self->reduced_button);
  gtk_style_context_remove_class (context, invert_class_name);
  gtk_style_context_add_class (context, class_name);

  gtk_widget_show (self->reduced_label);
  context = gtk_widget_get_style_context (self->reduced_label);
  gtk_style_context_remove_class (context, invert_class_name);
  gtk_style_context_add_class (context, class_name);

  gtk_widget_show (self->size_label);
  context = gtk_widget_get_style_context (self->size_label);
  gtk_style_context_remove_class (context, invert_class_name);
  gtk_style_context_add_class (context, class_name);
}


static void
photos_export_dialog_guess_sizes (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosExportDialog *self;
  PhotosBaseItem *item = PHOTOS_BASE_ITEM (source_object);
  PhotosBaseItemSize full;
  PhotosBaseItemSize reduced;
  gboolean success;

  {
    g_autoptr (GError) error = NULL;

    success = photos_base_item_guess_save_sizes_finish (item, res, &full, &reduced, &error);
    if (error != NULL)
      {
        if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
          goto out;

        g_warning ("Unable to guess sizes: %s", error->message);
      }
  }

  self = PHOTOS_EXPORT_DIALOG (user_data);

  if (success)
    {
      {
        g_autofree gchar *size_str = NULL;
        g_autofree gchar *size_str_markup = NULL;

        size_str = photos_export_dialog_create_size_str (full.height, full.width, (guint64) full.bytes);
        size_str_markup = g_strdup_printf ("<small>%s</small>", size_str);
        gtk_label_set_markup (GTK_LABEL (self->full_label), size_str_markup);
      }

      self->reduced_zoom = reduced.zoom;
      if (self->reduced_zoom > 0.0)
        {
          g_autofree gchar *size_str = NULL;
          g_autofree gchar *size_str_markup = NULL;

          size_str = photos_export_dialog_create_size_str (reduced.height, reduced.width, (guint64) reduced.bytes);
          size_str_markup = g_strdup_printf ("<small>%s</small>", size_str);
          gtk_label_set_markup (GTK_LABEL (self->reduced_label), size_str_markup);
        }
    }

  photos_export_dialog_show_size_options (self, self->reduced_zoom > 0.0, FALSE);

 out:
  return;
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
      g_autoptr (GDateTime) now = NULL;
      g_autofree gchar *now_str = NULL;

      now = g_date_time_new_now_local ();

      /* Translators: this is the default sub-directory where photos
       * will be exported.
       */
      now_str = g_date_time_format (now, _("%-d %B %Y"));

      gtk_entry_set_text (GTK_ENTRY (self->dir_entry), now_str);

      photos_export_dialog_show_size_options (self, FALSE, TRUE);
      photos_base_item_guess_save_sizes_async (self->item,
                                               self->cancellable,
                                               photos_export_dialog_guess_sizes,
                                               self);
    }

  gtk_widget_grab_focus (self->dir_entry);
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
  g_autofree gchar *progress_str_markup = NULL;

  gtk_widget_init_template (GTK_WIDGET (self));

  self->cancellable = g_cancellable_new ();

  progress_str_markup = g_strdup_printf ("<small>%s</small>", _("Calculating export size…"));
  gtk_label_set_markup (GTK_LABEL (self->progress_label), progress_str_markup);

  self->reduced_zoom = -1.0;
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
  gtk_widget_class_bind_template_child (widget_class, PhotosExportDialog, progress_label);
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
