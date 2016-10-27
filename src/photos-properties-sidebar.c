/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2016 Alessandro Bono
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

#include "photos-base-item.h"
#include "photos-properties-sidebar.h"

struct _PhotosPropertiesSidebar
{
  GtkScrolledWindow parent_instance;
  PhotosBaseItem *item;
};

struct _PhotosPropertiesSidebarClass
{
  GtkScrolledWindowClass parent_class;
};

G_DEFINE_TYPE (PhotosPropertiesSidebar, photos_properties_sidebar, GTK_TYPE_SCROLLED_WINDOW)

GtkWidget *
photos_properties_sidebar_new (void)
{
  return g_object_new (PHOTOS_TYPE_PROPERTIES_SIDEBAR, NULL);
}

static void
photos_properties_sidebar_finalize (GObject *object)
{
  G_OBJECT_CLASS (photos_properties_sidebar_parent_class)->finalize (object);
}

static void
photos_properties_sidebar_class_init (PhotosPropertiesSidebarClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = photos_properties_sidebar_finalize;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Photos/properties-sidebar.ui");
}

static void
photos_properties_sidebar_init (PhotosPropertiesSidebar *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}
