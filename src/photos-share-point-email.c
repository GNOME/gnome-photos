/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2016 – 2021 Red Hat, Inc.
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
#include <glib.h>
#include <glib/gi18n.h>
#include <libportal/portal.h>
#include <libportal-gtk3/portal-gtk3.h>

#include "photos-base-item.h"
#include "photos-error.h"
#include "photos-filterable.h"
#include "photos-glib.h"
#include "photos-share-point-email.h"
#include "photos-utils.h"


struct _PhotosSharePointEmail
{
  PhotosSharePoint parent_instance;
  GError *initialization_error;
  gboolean is_initialized;
};

static void photos_share_point_email_filterable_iface_init (PhotosFilterableInterface *iface);


G_DEFINE_TYPE_WITH_CODE (PhotosSharePointEmail, photos_share_point_email, PHOTOS_TYPE_SHARE_POINT,
                         G_IMPLEMENT_INTERFACE (PHOTOS_TYPE_FILTERABLE,
                                                photos_share_point_email_filterable_iface_init)
                         photos_utils_ensure_extension_points ();
                         g_io_extension_point_implement (PHOTOS_SHARE_POINT_EXTENSION_POINT_NAME,
                                                         g_define_type_id,
                                                         "email",
                                                         0));


static const gchar *
photos_share_point_email_get_id (PhotosFilterable *filterable)
{
  return "email";
}


static const gchar *
photos_share_point_email_get_name (PhotosSharePoint *share_point)
{
  return _("E-Mail");
}


static gboolean
photos_share_point_email_is_search_criterion (PhotosFilterable *filterable)
{
  return FALSE;
}


static gboolean
photos_share_point_email_needs_notification (PhotosSharePoint *share_point)
{
  return FALSE;
}


static void
photos_share_point_email_share_save_to_dir_finish (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  g_autoptr (GError) error = NULL;
  XdpPortal *portal = XDP_PORTAL (source_object);

  if (!xdp_portal_open_uri_finish (portal, res, &error))
    g_warning ("Unable to open URI: %s", error->message);
}


static void
photos_share_point_email_share_save_to_dir (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  XdpPortal *portal;
  XdpParent *parent_window;
  GApplication *app;
  GtkWindow *parent;
  GError *error;
  g_autoptr (GFile) file = NULL;
  g_autoptr (GTask) task = G_TASK (user_data);
  PhotosBaseItem *item = PHOTOS_BASE_ITEM (source_object);
  g_autofree gchar *escaped_path = NULL;
  g_autofree gchar *path = NULL;
  g_autofree gchar *uri = NULL;

  error = NULL;
  file = photos_base_item_save_to_dir_finish (item, res, &error);
  if (error != NULL)
    {
      g_task_return_error (task, error);
      goto out;
    }

  path = g_file_get_path (file);
  escaped_path = g_uri_escape_string (path, G_URI_RESERVED_CHARS_ALLOWED_IN_PATH, FALSE);
  uri = g_strconcat ("mailto:?attach=", escaped_path, NULL);

  app = g_application_get_default ();
  parent = gtk_application_get_active_window (GTK_APPLICATION (app));
  parent_window = xdp_parent_new_gtk (parent);

  portal = xdp_portal_new ();

  xdp_portal_open_uri (portal,
                       parent_window,
                       uri,
                       XDP_OPEN_URI_FLAG_NONE,
                       NULL,
                       photos_share_point_email_share_save_to_dir_finish,
                       NULL);

 out:
  return;
}


static void
photos_share_point_email_share_async (PhotosSharePoint *share_point,
                                      PhotosBaseItem *item,
                                      GCancellable *cancellable,
                                      GAsyncReadyCallback callback,
                                      gpointer user_data)
{
  PhotosSharePointEmail *self = PHOTOS_SHARE_POINT_EMAIL (share_point);
  g_autoptr (GFile) export = NULL;
  g_autoptr (GTask) task = NULL;
  const gchar *user_name;
  g_autofree gchar *export_dir = NULL;
  g_autofree gchar *template = NULL;

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, photos_share_point_email_share_async);

  user_name = g_get_user_name ();
  template = g_strdup_printf (PACKAGE_TARNAME "-%s-XXXXXX", user_name);
  export_dir = g_build_filename ("/var/tmp", template, NULL);
  g_mkdtemp (export_dir);

  export = g_file_new_for_path (export_dir);
  photos_base_item_save_to_dir_async (item,
                                      export,
                                      1.0,
                                      cancellable,
                                      photos_share_point_email_share_save_to_dir,
                                      g_object_ref (task));
}


static gboolean
photos_share_point_email_share_finish (PhotosSharePoint *share_point, GAsyncResult *res, gchar **out_uri, GError **error)
{
  PhotosSharePointEmail *self = PHOTOS_SHARE_POINT_EMAIL (share_point);
  GTask *task;

  g_return_val_if_fail (g_task_is_valid (res, self), FALSE);
  task = G_TASK (res);

  g_return_val_if_fail (g_task_get_source_tag (task) == photos_share_point_email_share_async, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  return g_task_propagate_boolean (task, error);
}


static void
photos_share_point_email_dispose (GObject *object)
{
  G_OBJECT_CLASS (photos_share_point_email_parent_class)->dispose (object);
}


static void
photos_share_point_email_finalize (GObject *object)
{
  PhotosSharePointEmail *self = PHOTOS_SHARE_POINT_EMAIL (object);

  g_clear_error (&self->initialization_error);

  G_OBJECT_CLASS (photos_share_point_email_parent_class)->finalize (object);
}


static void
photos_share_point_email_init (PhotosSharePointEmail *self)
{
}


static void
photos_share_point_email_class_init (PhotosSharePointEmailClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  PhotosSharePointClass *share_point_class = PHOTOS_SHARE_POINT_CLASS (class);

  object_class->dispose = photos_share_point_email_dispose;
  object_class->finalize = photos_share_point_email_finalize;
  share_point_class->get_name = photos_share_point_email_get_name;
  share_point_class->needs_notification = photos_share_point_email_needs_notification;
  share_point_class->share_async = photos_share_point_email_share_async;
  share_point_class->share_finish = photos_share_point_email_share_finish;
}


static void
photos_share_point_email_filterable_iface_init (PhotosFilterableInterface *iface)
{
  iface->get_id = photos_share_point_email_get_id;
  iface->is_search_criterion = photos_share_point_email_is_search_criterion;
}
