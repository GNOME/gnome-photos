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

#include <gio/gio.h>

#include "photos-removable-device-widget.h"


struct _PhotosRemovableDeviceWidget
{
  GtkBin parent_instance;
  GtkWidget *image;
  GtkWidget *label;
  PhotosSource *source;
};

enum
{
  PROP_0,
  PROP_SOURCE,
};


G_DEFINE_TYPE (PhotosRemovableDeviceWidget, photos_removable_device_widget, GTK_TYPE_BIN);


static void
photos_removable_device_widget_dispose (GObject *object)
{
  PhotosRemovableDeviceWidget *self = PHOTOS_REMOVABLE_DEVICE_WIDGET (object);

  g_clear_object (&self->source);

  G_OBJECT_CLASS (photos_removable_device_widget_parent_class)->dispose (object);
}


static void
photos_removable_device_widget_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
  PhotosRemovableDeviceWidget *self = PHOTOS_REMOVABLE_DEVICE_WIDGET (object);

  switch (prop_id)
    {
    case PROP_SOURCE:
      {
        PhotosSource *source;

        source = PHOTOS_SOURCE (g_value_get_object (value));
        photos_removable_device_widget_set_source (self, source);
        break;
      }

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
photos_removable_device_widget_init (PhotosRemovableDeviceWidget *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}


static void
photos_removable_device_widget_class_init (PhotosRemovableDeviceWidgetClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->dispose = photos_removable_device_widget_dispose;
  object_class->set_property = photos_removable_device_widget_set_property;

  g_object_class_install_property (object_class,
                                   PROP_SOURCE,
                                   g_param_spec_object ("source",
                                                        "Source",
                                                        "The GMount-backed source being displayed by this widget",
                                                        PHOTOS_TYPE_SOURCE,
                                                        G_PARAM_CONSTRUCT | G_PARAM_WRITABLE));

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Photos/removable-device-widget.ui");
  gtk_widget_class_bind_template_child (widget_class, PhotosRemovableDeviceWidget, image);
  gtk_widget_class_bind_template_child (widget_class, PhotosRemovableDeviceWidget, label);
}


GtkWidget *
photos_removable_device_widget_new (PhotosSource *source)
{
  g_return_val_if_fail (source == NULL || PHOTOS_IS_SOURCE (source), NULL);

  if (source != NULL)
    {
      GMount *mount;

      mount = photos_source_get_mount (source);
      g_return_val_if_fail (G_IS_MOUNT (mount), NULL);
    }

  return g_object_new (PHOTOS_TYPE_REMOVABLE_DEVICE_WIDGET, "source", source, NULL);
}


void
photos_removable_device_widget_set_source (PhotosRemovableDeviceWidget *self, PhotosSource *source)
{
  g_return_if_fail (PHOTOS_IS_REMOVABLE_DEVICE_WIDGET (self));
  g_return_if_fail (source == NULL || PHOTOS_IS_SOURCE (source));

  if (source != NULL)
    {
      GMount *mount;

      mount = photos_source_get_mount (source);
      g_return_if_fail (G_IS_MOUNT (mount));
    }

  if (!g_set_object (&self->source, source))
    goto out;

  if (self->source == NULL)
    {
      gtk_image_clear (GTK_IMAGE (self->image));
      gtk_label_set_label (GTK_LABEL (self->label), "");
    }
  else
    {
      GIcon *icon;
      const gchar *name;

      icon = photos_source_get_symbolic_icon (self->source);
      gtk_image_set_from_gicon (GTK_IMAGE (self->image), icon, GTK_ICON_SIZE_BUTTON);

      name = photos_source_get_name (source);
      gtk_label_set_label (GTK_LABEL (self->label), name);
    }

 out:
  return;
}
