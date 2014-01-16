/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2013 Intel Corporation. All rights reserved.
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

#include "photos-dlna-renderers-dialog.h"

#include <gio/gio.h>
#include <glib.h>
#include <glib/gi18n.h>

#include "photos-dleyna-renderer-device.h"
#include "photos-dleyna-renderer-push-host.h"
#include "photos-dlna-renderer.h"
#include "photos-dlna-renderers-manager.h"
#include "photos-item-manager.h"
#include "photos-local-item.h"
#include "photos-mode-controller.h"
#include "photos-remote-display-manager.h"


struct _PhotosDlnaRenderersDialogPrivate
{
  PhotosBaseManager *item_mngr;
  PhotosDlnaRenderersManager *renderers_mngr;
  PhotosRemoteDisplayManager *remote_mngr;
  PhotosModeController *mode_cntrlr;
  GtkListBox *listbox;
  gchar *urn;
};


enum
{
  PROP_0,
  PROP_URN
};


G_DEFINE_TYPE_WITH_PRIVATE (PhotosDlnaRenderersDialog, photos_dlna_renderers_dialog, GTK_TYPE_DIALOG);


static void
photos_dlna_renderers_separator_cb (GtkListBoxRow *row,
                                    GtkListBoxRow *before,
                                    gpointer user_data)
{
  GtkWidget *separator;

  separator = gtk_list_box_row_get_header (row);
  if (separator == NULL && before != NULL)
    {
      separator = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
      gtk_list_box_row_set_header (row, separator);
      gtk_widget_show (separator);
    }
}


static void
photos_dlna_renderers_dialog_row_activated_cb (PhotosDlnaRenderersDialog *self,
                                               GtkListBoxRow             *row,
                                               gpointer                   user_data)
{
  PhotosDlnaRenderersDialogPrivate *priv = self->priv;
  PhotosBaseItem *item;
  PhotosDlnaRenderer *renderer;

  renderer = g_object_get_data (G_OBJECT (row), "renderer");
  item = PHOTOS_BASE_ITEM (photos_base_manager_get_object_by_id (priv->item_mngr,
                                                                 priv->urn));
  photos_remote_display_manager_set_renderer (priv->remote_mngr, renderer);
  photos_remote_display_manager_render (priv->remote_mngr, item);

  photos_mode_controller_set_window_mode (priv->mode_cntrlr, PHOTOS_WINDOW_MODE_OVERVIEW);

  gtk_dialog_response (GTK_DIALOG (self), GTK_RESPONSE_ACCEPT);
}


static void
photos_dlna_renderers_dialog_dispose (GObject *object)
{
  PhotosDlnaRenderersDialog *self = PHOTOS_DLNA_RENDERERS_DIALOG (object);
  PhotosDlnaRenderersDialogPrivate *priv = self->priv;

  g_clear_object (&priv->item_mngr);
  g_clear_object (&priv->renderers_mngr);
  g_clear_object (&priv->remote_mngr);
  g_clear_object (&priv->mode_cntrlr);

  G_OBJECT_CLASS (photos_dlna_renderers_dialog_parent_class)->dispose (object);
}


static void
photos_dlna_renderers_dialog_finalize (GObject *object)
{
  PhotosDlnaRenderersDialog *self = PHOTOS_DLNA_RENDERERS_DIALOG (object);
  PhotosDlnaRenderersDialogPrivate *priv = self->priv;

  g_free (priv->urn);

  G_OBJECT_CLASS (photos_dlna_renderers_dialog_parent_class)->finalize (object);
}


static void
photos_dlna_renderers_dialog_set_property (GObject      *object,
                                           guint         prop_id,
                                           const GValue *value,
                                           GParamSpec   *pspec)
{
  PhotosDlnaRenderersDialog *self = PHOTOS_DLNA_RENDERERS_DIALOG (object);

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
photos_dlna_renderers_dialog_set_icon_cb (GObject      *source_object,
                                          GAsyncResult *res,
                                          gpointer      user_data)
{
  GdkPixbuf *pixbuf;
  GtkImage *image = GTK_IMAGE (user_data);
  PhotosDlnaRenderer *renderer = PHOTOS_DLNA_RENDERER (source_object);
  GError *error = NULL;

  pixbuf = photos_dlna_renderer_get_icon_finish (renderer, res, &error);
  if (error != NULL)
    {
      g_warning ("Unable to load renderer icon: %s", error->message);
      g_error_free (error);
      goto out;
    }

  gtk_image_set_from_pixbuf (image, pixbuf);

out:
  /* release the ref we took before the async call */
  g_object_unref (image);
}


static void
photos_dlna_renderers_dialog_add_renderer (PhotosDlnaRenderersDialog *self, PhotosDlnaRenderer *renderer)
{
  PhotosDlnaRenderersDialogPrivate *priv = self->priv;
  GIcon *icon;
  GtkWidget *row;
  GtkWidget *row_grid;
  GtkWidget *image;
  GtkWidget *label;
  const gchar *name;

  row = gtk_list_box_row_new ();
  gtk_container_add (GTK_CONTAINER (priv->listbox), row);

  row_grid = gtk_grid_new ();
  gtk_container_set_border_width (GTK_CONTAINER (row_grid), 12);
  gtk_orientable_set_orientation (GTK_ORIENTABLE (row_grid), GTK_ORIENTATION_HORIZONTAL);
  gtk_grid_set_column_spacing (GTK_GRID (row_grid), 12);
  gtk_container_add (GTK_CONTAINER (row), row_grid);

  g_object_set_data_full (G_OBJECT (row), "renderer", renderer, g_object_unref);

  name = photos_dlna_renderer_get_friendly_name (renderer);

  icon = g_themed_icon_new_with_default_fallbacks ("video-display-symbolic");
  image = gtk_image_new_from_gicon (icon, GTK_ICON_SIZE_DIALOG);

  g_object_ref (image); /* keep a ref for the following async call and release it in the callback */
  photos_dlna_renderer_get_icon (renderer,
                                 "",
                                 "",
                                 GTK_ICON_SIZE_DIALOG,
                                 NULL,
                                 photos_dlna_renderers_dialog_set_icon_cb,
                                 image);

  gtk_container_add (GTK_CONTAINER (row_grid), image);

  label = gtk_label_new (NULL);
  gtk_label_set_text (GTK_LABEL (label), name);
  gtk_container_add (GTK_CONTAINER (row_grid), label);
}


static void
photos_dlna_renderers_dialog_response_cb (GtkDialog *dialog,
                                          gint       response_id,
                                          gpointer   user_data)
{
  gtk_widget_destroy (GTK_WIDGET (dialog));
}


static void
photos_dlna_renderers_dialog_init (PhotosDlnaRenderersDialog *self)
{
  PhotosDlnaRenderersDialogPrivate *priv;
  GList *renderers;

  self->priv = photos_dlna_renderers_dialog_get_instance_private (self);
  priv = self->priv;

  priv->item_mngr = photos_item_manager_dup_singleton ();
  priv->renderers_mngr = photos_dlna_renderers_manager_dup_singleton ();
  priv->remote_mngr = photos_remote_display_manager_dup_singleton ();
  priv->mode_cntrlr = photos_mode_controller_dup_singleton ();

  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_list_box_set_header_func (priv->listbox, photos_dlna_renderers_separator_cb, NULL, NULL);

  renderers = photos_dlna_renderers_manager_dup_renderers (priv->renderers_mngr);

  while (renderers != NULL)
    {
      PhotosDlnaRenderer *renderer = PHOTOS_DLNA_RENDERER (renderers->data);

      photos_dlna_renderers_dialog_add_renderer (self, renderer);
      renderers = g_list_delete_link (renderers, renderers);
    }

  g_signal_connect (self, "response", G_CALLBACK (photos_dlna_renderers_dialog_response_cb), NULL);
}


static void
photos_dlna_renderers_dialog_class_init (PhotosDlnaRenderersDialogClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->dispose = photos_dlna_renderers_dialog_dispose;
  object_class->finalize = photos_dlna_renderers_dialog_finalize;
  object_class->set_property = photos_dlna_renderers_dialog_set_property;

  g_object_class_install_property (object_class,
                                   PROP_URN,
                                   g_param_spec_string ("urn",
                                                        "Uniform Resource Name",
                                                        "An unique ID associated with this item",
                                                        NULL,
                                                        G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE));

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/photos/dlna-renderers-dialog.ui");

  gtk_widget_class_bind_template_child_private (widget_class, PhotosDlnaRenderersDialog, listbox);
  gtk_widget_class_bind_template_callback (widget_class, photos_dlna_renderers_dialog_row_activated_cb);
}


GtkWidget *
photos_dlna_renderers_dialog_new (GtkWindow *parent, const gchar *urn)
{
  g_return_val_if_fail (GTK_IS_WINDOW (parent), NULL);

  return g_object_new (PHOTOS_TYPE_DLNA_RENDERERS_DIALOG, "transient-for", parent, "urn", urn, NULL);
}
