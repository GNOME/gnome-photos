/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2017 Red Hat, Inc.
 * Copyright © 2017 Umang Jain
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
#include <glib/gi18n.h>

#include "photos-base-manager.h"
#include "photos-filterable.h"
#include "photos-item-manager.h"
#include "photos-search-context.h"
#include "photos-source-notification.h"
#include "photos-utils.h"


struct _PhotosSourceNotification
{
  GtkGrid parent_instance;
  PhotosBaseManager *src_mngr;
  PhotosModeController *mode_cntrlr;
  PhotosSource *source;
};

enum
{
  PROP_0,
  PROP_SOURCE
};

enum
{
  CLOSED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };


G_DEFINE_TYPE (PhotosSourceNotification, photos_source_notification, GTK_TYPE_GRID);


static void
photos_source_notification_close (PhotosSourceNotification *self)
{
  g_signal_emit (self, signals[CLOSED], 0);
}


static void
photos_source_notification_import_notify_sensitive (GObject *object)
{
  GtkStyleContext *context;
  gboolean sensitive;
  const gchar *class_name;
  const gchar *invert_class_name;

  sensitive = gtk_widget_get_sensitive (GTK_WIDGET (object));
  class_name = sensitive ? "photos-fade-in" : "photos-fade-out";
  invert_class_name = !sensitive ? "photos-fade-in" : "photos-fade-out";

  context = gtk_widget_get_style_context (GTK_WIDGET (object));
  gtk_style_context_remove_class (context, invert_class_name);
  gtk_style_context_add_class (context, class_name);
}


static void
photos_source_notification_settings_clicked (PhotosSourceNotification *self)
{
  g_autoptr (GAppInfo) app = NULL;
  g_autoptr (GAppLaunchContext) ctx = NULL;
  GoaAccount *account;
  GoaObject *object;
  const gchar *id;
  g_autofree gchar *command_line = NULL;

  object = photos_source_get_goa_object (self->source);
  g_return_if_fail (GOA_IS_OBJECT (object));

  account = goa_object_peek_account (object);
  id = goa_account_get_id (account);
  command_line = g_strconcat ("gnome-control-center online-accounts ", id, NULL);

  {
    g_autoptr (GError) error = NULL;

    app = g_app_info_create_from_commandline (command_line, NULL, G_APP_INFO_CREATE_NONE, &error);
    if (error != NULL)
      {
        g_warning ("Unable to launch gnome-control-center: %s", error->message);
        goto out;
      }
  }

  ctx = photos_utils_new_app_launch_context_from_widget (GTK_WIDGET (self));

  {
    g_autoptr (GError) error = NULL;

    g_app_info_launch (app, NULL, ctx, &error);
    if (error != NULL)
      {
        g_warning ("Unable to launch gnome-control-center: %s", error->message);
        goto out;
      }
  }

 out:
  return;
}


static void
photos_source_notification_window_mode_changed (PhotosSourceNotification *self,
                                                PhotosWindowMode mode,
                                                PhotosWindowMode old_mode)
{
  GMount *mount;

  g_return_if_fail (PHOTOS_IS_SOURCE_NOTIFICATION (self));

  mount = photos_source_get_mount (self->source);
  g_return_if_fail (G_IS_MOUNT (mount));

  switch (mode)
    {
    case PHOTOS_WINDOW_MODE_COLLECTION_VIEW:
    case PHOTOS_WINDOW_MODE_COLLECTIONS:
    case PHOTOS_WINDOW_MODE_EDIT:
    case PHOTOS_WINDOW_MODE_FAVORITES:
    case PHOTOS_WINDOW_MODE_OVERVIEW:
    case PHOTOS_WINDOW_MODE_PREVIEW:
    case PHOTOS_WINDOW_MODE_SEARCH:
      break;

    case PHOTOS_WINDOW_MODE_IMPORT:
      photos_source_notification_close (self);
      break;

    case PHOTOS_WINDOW_MODE_NONE:
    default:
      g_assert_not_reached ();
      break;
    }
}


static void
photos_source_notification_constructed (GObject *object)
{
  PhotosSourceNotification *self = PHOTOS_SOURCE_NOTIFICATION (object);
  GMount *mount;
  GoaObject *goa_object;
  GtkWidget *close;
  GtkWidget *image;

  G_OBJECT_CLASS (photos_source_notification_parent_class)->constructed (object);

  gtk_grid_set_column_spacing (GTK_GRID (self), 12);
  gtk_orientable_set_orientation (GTK_ORIENTABLE (self), GTK_ORIENTATION_HORIZONTAL);

  mount = photos_source_get_mount (self->source);
  goa_object = photos_source_get_goa_object (self->source);

  if (mount != NULL)
    {
      GtkStyleContext *context;
      GtkWidget *import;
      GtkWidget *labels;
      GtkWidget *primary_label;
      GtkWidget *secondary_label;
      const gchar *action_id;
      const gchar *action_namespace = "app";
      const gchar *id;
      g_autofree gchar *action_name = NULL;
      g_autofree gchar *name = NULL;

      labels = gtk_grid_new ();
      gtk_orientable_set_orientation (GTK_ORIENTABLE (labels), GTK_ORIENTATION_VERTICAL);
      gtk_grid_set_row_spacing (GTK_GRID (labels), 3);
      gtk_container_add (GTK_CONTAINER (self), labels);

      primary_label = gtk_label_new (_("New device discovered"));
      gtk_widget_set_halign (primary_label, GTK_ALIGN_START);
      gtk_widget_set_hexpand (primary_label, TRUE);
      gtk_container_add (GTK_CONTAINER (labels), primary_label);

      name = g_mount_get_name (mount);
      secondary_label = gtk_label_new (name);
      gtk_widget_set_halign (secondary_label, GTK_ALIGN_START);
      gtk_widget_set_hexpand (secondary_label, TRUE);
      context = gtk_widget_get_style_context (secondary_label);
      gtk_style_context_add_class (context, "dim-label");
      gtk_container_add (GTK_CONTAINER (labels), secondary_label);

      import = gtk_button_new_with_label (_("Import…"));
      gtk_widget_set_valign (import, GTK_ALIGN_CENTER);
      action_id = photos_base_manager_get_action_id (self->src_mngr);
      action_name = g_strconcat (action_namespace, ".", action_id, NULL);
      gtk_actionable_set_action_name (GTK_ACTIONABLE (import), action_name);
      id = photos_filterable_get_id (PHOTOS_FILTERABLE (self->source));
      gtk_actionable_set_action_target (GTK_ACTIONABLE (import), "s", id);
      gtk_container_add (GTK_CONTAINER (self), import);
      g_signal_connect (import,
                        "notify::sensitive",
                        G_CALLBACK (photos_source_notification_import_notify_sensitive),
                        NULL);

      g_signal_connect_object (self->mode_cntrlr,
                               "window-mode-changed",
                               G_CALLBACK (photos_source_notification_window_mode_changed),
                               self,
                               G_CONNECT_SWAPPED);
    }
  else if (goa_object != NULL)
    {
      GtkWidget *label;
      GtkWidget *settings;
      const gchar *name;
      g_autofree gchar *msg = NULL;

      name = photos_source_get_name (self->source);
      /* Translators: %s refers to an online account provider, e.g.,
       * "Facebook" or "Flickr".
       */
      msg = g_strdup_printf (_("Your %s credentials have expired"), name);

      label = gtk_label_new (msg);
      gtk_widget_set_halign (label, GTK_ALIGN_START);
      gtk_widget_set_hexpand (label, TRUE);
      gtk_container_add (GTK_CONTAINER (self), label);

      settings = gtk_button_new_with_label (_("Settings"));
      gtk_widget_set_valign (settings, GTK_ALIGN_CENTER);
      gtk_container_add (GTK_CONTAINER (self), settings);
      g_signal_connect_swapped (settings, "clicked", G_CALLBACK (photos_source_notification_settings_clicked), self);
    }

  image = gtk_image_new_from_icon_name ("window-close-symbolic", GTK_ICON_SIZE_INVALID);
  gtk_widget_set_margin_bottom (image, 2);
  gtk_widget_set_margin_top (image, 2);
  gtk_image_set_pixel_size (GTK_IMAGE (image), 16);

  close = gtk_button_new ();
  gtk_widget_set_valign (close, GTK_ALIGN_CENTER);
  gtk_widget_set_focus_on_click (close, FALSE);
  gtk_button_set_relief (GTK_BUTTON (close), GTK_RELIEF_NONE);
  gtk_button_set_image (GTK_BUTTON (close), image);
  gtk_container_add (GTK_CONTAINER (self), close);
  g_signal_connect_swapped (close, "clicked", G_CALLBACK (photos_source_notification_close), self);
}


static void
photos_source_notification_dispose (GObject *object)
{
  PhotosSourceNotification *self = PHOTOS_SOURCE_NOTIFICATION (object);

  g_clear_object (&self->src_mngr);
  g_clear_object (&self->mode_cntrlr);
  g_clear_object (&self->source);

  G_OBJECT_CLASS (photos_source_notification_parent_class)->dispose (object);
}


static void
photos_source_notification_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
  PhotosSourceNotification *self = PHOTOS_SOURCE_NOTIFICATION (object);

  switch (prop_id)
    {
    case PROP_SOURCE:
      g_value_set_object (value, self->source);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
photos_source_notification_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
  PhotosSourceNotification *self = PHOTOS_SOURCE_NOTIFICATION (object);

  switch (prop_id)
    {
    case PROP_SOURCE:
      self->source = PHOTOS_SOURCE (g_value_dup_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
photos_source_notification_init (PhotosSourceNotification *self)
{
  GApplication *app;
  PhotosSearchContextState *state;

  app = g_application_get_default ();
  state = photos_search_context_get_state (PHOTOS_SEARCH_CONTEXT (app));

  self->src_mngr = g_object_ref (state->src_mngr);
  self->mode_cntrlr = g_object_ref (state->mode_cntrlr);
}


static void
photos_source_notification_class_init (PhotosSourceNotificationClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->constructed = photos_source_notification_constructed;
  object_class->dispose = photos_source_notification_dispose;
  object_class->get_property = photos_source_notification_get_property;
  object_class->set_property = photos_source_notification_set_property;

  g_object_class_install_property (object_class,
                                   PROP_SOURCE,
                                   g_param_spec_object ("source",
                                                        "PhotosSource instance",
                                                        "The PhotosSource corresponding to this notification",
                                                        PHOTOS_TYPE_SOURCE,
                                                        G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE));

  signals[CLOSED] = g_signal_new ("closed",
                                  G_TYPE_FROM_CLASS (class),
                                  G_SIGNAL_RUN_LAST,
                                  0,
                                  NULL, /* accumulator */
                                  NULL, /* accu_data */
                                  g_cclosure_marshal_VOID__VOID,
                                  G_TYPE_NONE,
                                  0);
}


GtkWidget *
photos_source_notification_new (PhotosSource *source)
{
  GMount *mount;
  GoaObject *object;

  g_return_val_if_fail (PHOTOS_IS_SOURCE (source), NULL);

  mount = photos_source_get_mount (source);
  object = photos_source_get_goa_object (source);
  g_return_val_if_fail (G_IS_MOUNT (mount) || GOA_IS_OBJECT (object), NULL);

  if (object != NULL)
    {
      GoaAccount *account;
      gboolean attention_needed;

      account = goa_object_peek_account (object);
      g_return_val_if_fail (GOA_IS_ACCOUNT (account), NULL);

      attention_needed = goa_account_get_attention_needed (account);
      g_return_val_if_fail (attention_needed, NULL);
    }

  return g_object_new (PHOTOS_TYPE_SOURCE_NOTIFICATION, "source", source, NULL);
}


PhotosSource *
photos_source_notification_get_source (PhotosSourceNotification *self)
{
  g_return_val_if_fail (PHOTOS_IS_SOURCE_NOTIFICATION (self), NULL);
  return self->source;
}
