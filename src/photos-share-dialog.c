/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2016 Umang Jain
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

#include "photos-share-dialog.h"
#include "photos-share-point-manager.h"


struct _PhotosShareDialog
{
  GtkDialog parent_instance;
  PhotosBaseManager *shr_pnt_mngr;
  PhotosSharePoint *selected;
  GtkWidget *flow_box;
  PhotosBaseItem *item;
};

enum
{
  PROP_0,
  PROP_ITEM
};


G_DEFINE_TYPE (PhotosShareDialog, photos_share_dialog, GTK_TYPE_DIALOG);


static void
photos_share_dialog_child_activated (PhotosShareDialog *self, GtkFlowBoxChild *child)
{
  g_return_if_fail (PHOTOS_IS_SHARE_DIALOG (self));
  g_return_if_fail (GTK_IS_FLOW_BOX_CHILD (child));

  self->selected = PHOTOS_SHARE_POINT (g_object_get_data (G_OBJECT (child), "share-point"));
  g_return_if_fail (PHOTOS_IS_SHARE_POINT (self->selected));

  gtk_dialog_response (GTK_DIALOG (self), GTK_RESPONSE_OK);
}


static void
photos_share_dialog_constructed (GObject *object)
{
  PhotosShareDialog *self = PHOTOS_SHARE_DIALOG (object);
  GList *l;
  GList *share_points = NULL;

  G_OBJECT_CLASS (photos_share_dialog_parent_class)->constructed (object);

  share_points = photos_share_point_manager_get_for_item (PHOTOS_SHARE_POINT_MANAGER (self->shr_pnt_mngr), self->item);
  for (l = share_points; l != NULL; l = l->next)
    {
      GIcon *icon;
      GtkWidget *child;
      GtkWidget *grid;
      GtkWidget *image;
      GtkWidget *label;
      PhotosSharePoint *share_point = PHOTOS_SHARE_POINT (l->data);
      const gchar *name;

      child = gtk_flow_box_child_new ();
      gtk_widget_set_halign (child, GTK_ALIGN_START);
      gtk_container_set_border_width (GTK_CONTAINER (child), 18);
      gtk_container_add (GTK_CONTAINER (self->flow_box), child);

      grid = gtk_grid_new ();
      gtk_orientable_set_orientation (GTK_ORIENTABLE (grid), GTK_ORIENTATION_VERTICAL);
      gtk_grid_set_row_spacing (GTK_GRID (grid), 18);
      gtk_container_add (GTK_CONTAINER (child), grid);

      icon = photos_share_point_get_icon (share_point);
      image = gtk_image_new_from_gicon (icon, GTK_ICON_SIZE_DIALOG);
      gtk_container_add (GTK_CONTAINER (grid), image);

      name = photos_share_point_get_name (share_point);
      label = gtk_label_new (name);
      gtk_container_add (GTK_CONTAINER (grid), label);

      g_object_set_data_full (G_OBJECT (child), "share-point", g_object_ref (share_point), g_object_unref);
    }

  g_list_free_full (share_points, g_object_unref);
}


static void
photos_share_dialog_dispose (GObject *object)
{
  PhotosShareDialog *self = PHOTOS_SHARE_DIALOG (object);

  g_clear_object (&self->item);
  g_clear_object (&self->shr_pnt_mngr);

  G_OBJECT_CLASS (photos_share_dialog_parent_class)->dispose (object);
}


static void
photos_share_dialog_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
  PhotosShareDialog *self = PHOTOS_SHARE_DIALOG (object);

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
photos_share_dialog_init (PhotosShareDialog *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
  self->shr_pnt_mngr = photos_share_point_manager_dup_singleton ();
}


static void
photos_share_dialog_class_init (PhotosShareDialogClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->constructed = photos_share_dialog_constructed;
  object_class->dispose = photos_share_dialog_dispose;
  object_class->set_property = photos_share_dialog_set_property;

  g_object_class_install_property (object_class,
                                   PROP_ITEM,
                                   g_param_spec_object ("item",
                                                        "PhotosBaseItem object",
                                                        "The item to share",
                                                        PHOTOS_TYPE_BASE_ITEM,
                                                        G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE));

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Photos/share-dialog.ui");
  gtk_widget_class_bind_template_child (widget_class, PhotosShareDialog, flow_box);
  gtk_widget_class_bind_template_callback (widget_class, photos_share_dialog_child_activated);
}


GtkWidget *
photos_share_dialog_new (GtkWindow *parent, PhotosBaseItem *item)
{
  g_return_val_if_fail (GTK_IS_WINDOW (parent), NULL);
  g_return_val_if_fail (PHOTOS_IS_BASE_ITEM (item), NULL);

  return g_object_new (PHOTOS_TYPE_SHARE_DIALOG,
                       "item", item,
                       "transient-for", parent,
                       "use-header-bar", TRUE,
                       NULL);
}


PhotosSharePoint *
photos_share_dialog_get_selected_share_point (PhotosShareDialog *self)
{
  return self->selected;
}
