/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2015 – 2021 Red Hat, Inc.
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
#include "photos-export-notification.h"
#include "photos-glib.h"
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
  GList *items;
  gdouble reduced_zoom;
  gdouble current_size;
  gdouble current_reduced_size;
};

enum
{
  PROP_0,
  PROP_ITEMS
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

        self->current_size += full.bytes;
        size_str = photos_export_dialog_create_size_str (full.height, full.width, (guint64) self->current_size);
        size_str_markup = g_strdup_printf ("<small>%s</small>", size_str);
        gtk_label_set_markup (GTK_LABEL (self->full_label), size_str_markup);
      }

      self->reduced_zoom = reduced.zoom;
      if (self->reduced_zoom > 0.0)
        {
          g_autofree gchar *size_str = NULL;
          g_autofree gchar *size_str_markup = NULL;

          self->current_reduced_size += reduced.bytes;
          size_str = photos_export_dialog_create_size_str (reduced.height, reduced.width, (guint64) self->current_reduced_size);
          size_str_markup = g_strdup_printf ("<small>%s</small>", size_str);
          gtk_label_set_markup (GTK_LABEL (self->reduced_label), size_str_markup);
        }
    }

  photos_export_dialog_show_size_options (self, self->reduced_zoom > 0.0, FALSE);

  // Guess next item
  {
    gint current_index = g_list_index (self->items, item);
    GList *items;
    items = g_list_nth (self->items, current_index + 1);
    if (items != NULL)
      {
        photos_base_item_guess_save_sizes_async (items->data,
                                                 self->cancellable,
                                                 photos_export_dialog_guess_sizes,
                                                 self);
      }
  }
 out:
  return;
}


static void
photos_export_dialog_constructed (GObject *object)
{
  PhotosExportDialog *self = PHOTOS_EXPORT_DIALOG (object);

  G_OBJECT_CLASS (photos_export_dialog_parent_class)->constructed (object);

  if (g_list_length (self->items) > 1 && photos_base_item_is_collection (self->items->data))
    {
      const gchar *name;

      name = photos_base_item_get_name_with_fallback (self->items->data);
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
      photos_base_item_guess_save_sizes_async (self->items->data,
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

  G_OBJECT_CLASS (photos_export_dialog_parent_class)->dispose (object);
}


static void
photos_export_dialog_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
  PhotosExportDialog *self = PHOTOS_EXPORT_DIALOG (object);

  switch (prop_id)
    {
    case PROP_ITEMS:
      {
        GList *items;

        items = (GList *) g_value_get_pointer (value);
        self->items = g_list_copy_deep (items, (GCopyFunc) g_object_ref, NULL);
        break;
      }

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
  self->current_size = 0.0;
  self->current_reduced_size = 0.0;
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
                                   PROP_ITEMS,
                                   g_param_spec_pointer ("items",
                                                         "List of PhotosBaseItems",
                                                         "List of items to export",
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
photos_export_dialog_new (GtkWindow *parent, GList *items)
{
  GList *filtered = NULL;

  g_return_val_if_fail (GTK_IS_WINDOW (parent), NULL);

  for (GList *l = items; l != NULL; l = l->next)
    {
      if (!photos_base_item_is_collection (l->data))
        {
          filtered = g_list_prepend (filtered, l->data);
        }
    }
  g_return_val_if_fail (filtered != NULL, NULL);

  return g_object_new (PHOTOS_TYPE_EXPORT_DIALOG,
                       "items", filtered,
                       "transient-for", parent,
                       "use-header-bar", TRUE,
                       NULL);
}


static GFile *
photos_export_dialog_get_dir (PhotosExportDialog *self)
{
  g_autoptr (GFile) export_dir = NULL;
  GFile *export_sub_dir = NULL;
  const gchar *export_dir_name;
  const gchar *pictures_path;
  g_autofree gchar *export_path = NULL;

  g_return_val_if_fail (PHOTOS_IS_EXPORT_DIALOG (self), NULL);

  export_dir_name = gtk_entry_get_text (GTK_ENTRY (self->dir_entry));

  pictures_path = g_get_user_special_dir (G_USER_DIRECTORY_PICTURES);
  export_path = g_build_filename (pictures_path, PHOTOS_EXPORT_SUBPATH, NULL);
  export_dir = g_file_new_for_path (export_path);

  {
    g_autoptr (GError) error = NULL;

    export_sub_dir = g_file_get_child_for_display_name (export_dir, export_dir_name, &error);
    if (error != NULL)
      {
        g_warning ("Unable to get a child for %s: %s", export_dir_name, error->message);
        photos_export_notification_new_with_error (error);
        goto out;
      }
  }

  {
    g_autoptr (GError) error = NULL;

    if (!photos_glib_make_directory_with_parents (export_sub_dir, NULL, &error))
      {
        g_warning ("Unable to create %s: %s", export_path, error->message);
        photos_export_notification_new_with_error (error);
        g_object_unref (export_sub_dir);
        export_sub_dir = NULL;
      }
  }

  out:
    return export_sub_dir;
}


struct PhotosExportDialogData *
photos_export_dialog_get_export_data (PhotosExportDialog *self)
{
  struct PhotosExportDialogData *export_data;
  GFile *directory;

  g_return_val_if_fail (PHOTOS_IS_EXPORT_DIALOG (self), NULL);

  directory = photos_export_dialog_get_dir (self);
  g_return_val_if_fail (directory != NULL, NULL);

  export_data = g_malloc (sizeof (struct PhotosExportDialogData));
  export_data->directory = directory;
  export_data->items = self->items;
  export_data->zoom = 1.0;

  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->reduced_button)))
    {
      if (self->reduced_zoom >= 0.0)
        {
          export_data->zoom = self->reduced_zoom;
        }
    }

  return export_data;
}

void
photos_export_dialog_free_export_data (struct PhotosExportDialogData *export_data)
{
  g_object_unref (export_data->directory);
  g_list_free (export_data->items);
  g_free (export_data);
}

