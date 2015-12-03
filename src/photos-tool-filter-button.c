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

#include <gdk/gdk.h>
#include <gio/gio.h>

#include "photos-application.h"
#include "photos-icons.h"
#include "photos-tool-filter-button.h"
#include "photos-utils.h"


struct _PhotosToolFilterButton
{
  GtkBin parent_instance;
  GtkWidget *button;
  gchar *label;
};

struct _PhotosToolFilterButtonClass
{
  GtkBinClass parent_class;
};

enum
{
  PROP_0,
  PROP_LABEL,

  /* GtkActionable properties */
  PROP_ACTION_NAME,
  PROP_ACTION_TARGET
};

static void photos_tool_filter_button_actionable_iface_init (GtkActionableInterface *iface);


G_DEFINE_TYPE_EXTENDED (PhotosToolFilterButton, photos_tool_filter_button, GTK_TYPE_BIN, 0,
                        G_IMPLEMENT_INTERFACE (GTK_TYPE_ACTIONABLE,
                                               photos_tool_filter_button_actionable_iface_init));


static const gchar *
photos_tool_filter_button_get_action_name (GtkActionable *actionable)
{
  PhotosToolFilterButton *self = PHOTOS_TOOL_FILTER_BUTTON (actionable);
  return gtk_actionable_get_action_name (GTK_ACTIONABLE (self->button));
}


static GVariant *
photos_tool_filter_button_get_action_target_value (GtkActionable *actionable)
{
  PhotosToolFilterButton *self = PHOTOS_TOOL_FILTER_BUTTON (actionable);
  return gtk_actionable_get_action_target_value (GTK_ACTIONABLE (self->button));
}


static void
photos_tool_filter_button_set_action_name (GtkActionable *actionable, const gchar *action_name)
{
  PhotosToolFilterButton *self = PHOTOS_TOOL_FILTER_BUTTON (actionable);
  gtk_actionable_set_action_name (GTK_ACTIONABLE (self->button), action_name);
}


static void
photos_tool_filter_button_set_action_target_value (GtkActionable *actionable, GVariant *action_target)
{
  PhotosToolFilterButton *self = PHOTOS_TOOL_FILTER_BUTTON (actionable);
  gtk_actionable_set_action_target_value (GTK_ACTIONABLE (self->button), action_target);
}


static void
photos_tool_filter_button_constructed (GObject *object)
{
  PhotosToolFilterButton *self = PHOTOS_TOOL_FILTER_BUTTON (object);
  GApplication *app;
  GdkPixbuf *preview_icon = NULL;
  GtkWidget *image;
  cairo_surface_t *preview_icon_surface = NULL;
  gint scale;

  G_OBJECT_CLASS (photos_tool_filter_button_parent_class)->constructed (object);

  app = g_application_get_default ();
  scale = photos_application_get_scale_factor (PHOTOS_APPLICATION (app));
  preview_icon = photos_utils_create_placeholder_icon_for_scale (PHOTOS_ICON_CONTENT_LOADING_SYMBOLIC, 96, scale);
  if (preview_icon != NULL)
    preview_icon_surface = gdk_cairo_surface_create_from_pixbuf (preview_icon, scale, NULL);

  image = gtk_image_new_from_surface (preview_icon_surface);
  self->button = gtk_button_new_with_label (self->label);
  gtk_button_set_always_show_image (GTK_BUTTON (self->button), TRUE);
  gtk_button_set_image (GTK_BUTTON (self->button), image);
  gtk_button_set_image_position (GTK_BUTTON (self->button), GTK_POS_TOP);
  gtk_button_set_relief (GTK_BUTTON (self->button), GTK_RELIEF_NONE);
  gtk_container_add (GTK_CONTAINER (self), self->button);

  g_clear_object (&preview_icon);
  g_clear_pointer (&preview_icon_surface, (GDestroyNotify) cairo_surface_destroy);
}


static void
photos_tool_filter_button_finalize (GObject *object)
{
  PhotosToolFilterButton *self = PHOTOS_TOOL_FILTER_BUTTON (object);

  g_free (self->label);

  G_OBJECT_CLASS (photos_tool_filter_button_parent_class)->finalize (object);
}


static void
photos_tool_filter_button_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
  PhotosToolFilterButton *self = PHOTOS_TOOL_FILTER_BUTTON (object);

  switch (prop_id)
    {
    case PROP_ACTION_NAME:
      {
        const gchar *action_name;

        action_name = photos_tool_filter_button_get_action_name (GTK_ACTIONABLE (self));
        g_value_set_string (value, action_name);
        break;
      }

    case PROP_ACTION_TARGET:
      {
        GVariant *action_target;

        action_target = photos_tool_filter_button_get_action_target_value (GTK_ACTIONABLE (self));
        g_value_set_variant (value, action_target);
        break;
      }

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
photos_tool_filter_button_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
  PhotosToolFilterButton *self = PHOTOS_TOOL_FILTER_BUTTON (object);

  switch (prop_id)
    {
    case PROP_LABEL:
      self->label = g_value_dup_string (value);
      break;

    case PROP_ACTION_NAME:
      {
        const gchar *action_name;

        action_name = g_value_get_string (value);
        photos_tool_filter_button_set_action_name (GTK_ACTIONABLE (self), action_name);
        break;
      }

    case PROP_ACTION_TARGET:
      {
        GVariant *action_target;

        action_target = g_value_get_variant (value);
        photos_tool_filter_button_set_action_target_value (GTK_ACTIONABLE (self), action_target);
        break;
      }

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
photos_tool_filter_button_init (PhotosToolFilterButton *self)
{
}


static void
photos_tool_filter_button_class_init (PhotosToolFilterButtonClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->constructed = photos_tool_filter_button_constructed;
  object_class->finalize = photos_tool_filter_button_finalize;
  object_class->get_property = photos_tool_filter_button_get_property;
  object_class->set_property = photos_tool_filter_button_set_property;

  g_object_class_install_property (object_class,
                                   PROP_LABEL,
                                   g_param_spec_string ("label",
                                                        "Label",
                                                        "The human-readable name of the filter",
                                                        NULL,
                                                        G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE));

  g_object_class_override_property (object_class, PROP_ACTION_NAME, "action-name");
  g_object_class_override_property (object_class, PROP_ACTION_TARGET, "action-target");
}


static void
photos_tool_filter_button_actionable_iface_init (GtkActionableInterface *iface)
{
  iface->get_action_name = photos_tool_filter_button_get_action_name;
  iface->get_action_target_value = photos_tool_filter_button_get_action_target_value;
  iface->set_action_name = photos_tool_filter_button_set_action_name;
  iface->set_action_target_value = photos_tool_filter_button_set_action_target_value;
}


GtkWidget *
photos_tool_filter_button_new (const gchar *label)
{
  return g_object_new (PHOTOS_TYPE_TOOL_FILTER_BUTTON, "label", label, NULL);
}


void
photos_tool_filter_button_set_image (PhotosToolFilterButton *self, GtkWidget *image)
{
  gtk_button_set_image (GTK_BUTTON (self->button), image);
}
