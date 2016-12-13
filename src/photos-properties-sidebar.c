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
#include "photos-filterable.h"
#include "photos-properties-sidebar.h"
#include "photos-utils.h"

struct _PhotosPropertiesSidebar
{
  GtkScrolledWindow parent_instance;
  GtkWidget *title_entry;
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
photos_properties_sidebar_name_update (PhotosPropertiesSidebar *self)
{
  const gchar *new_title;
  const gchar *urn;

  urn = photos_filterable_get_id (PHOTOS_FILTERABLE (self->item));
  new_title = gtk_entry_get_text (GTK_ENTRY (self->title_entry));
  photos_utils_set_edited_name (urn, new_title);
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
  gtk_widget_class_bind_template_child (widget_class, PhotosPropertiesSidebar, title_entry);
}

static void
photos_properties_sidebar_init (PhotosPropertiesSidebar *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

static void
photos_properties_sidebar_clear (PhotosPropertiesSidebar *self)
{
  /* FIXME: g_object_clear () gives problems */
  if (self->item)
    {
      g_object_unref (self->item);
      self->item = NULL;
    }

  gtk_entry_set_text (GTK_ENTRY (self->title_entry), "");
}

void
photos_properties_sidebar_set_item (PhotosPropertiesSidebar *self, PhotosBaseItem *item)
{
  photos_properties_sidebar_clear (self);

  self->item = g_object_ref (item);

  gtk_entry_set_text (GTK_ENTRY (self->title_entry), photos_base_item_get_name (item));
  g_signal_connect_swapped (self->title_entry,
                            "changed",
                            G_CALLBACK (photos_properties_sidebar_name_update),
                            self);
}
