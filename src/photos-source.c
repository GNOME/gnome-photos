/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2012 – 2017 Red Hat, Inc.
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

/* Based on code from:
 *   + Documents
 */


#include "config.h"

#include <gio/gio.h>

#include "egg-counter.h"
#include "photos-filterable.h"
#include "photos-query.h"
#include "photos-source.h"
#include "photos-utils.h"


struct _PhotosSource
{
  GObject parent_instance;
  GIcon *icon;
  GoaObject *object;
  gboolean builtin;
  gchar *id;
  gchar *name;
};

enum
{
  PROP_0,
  PROP_BUILTIN,
  PROP_ID,
  PROP_NAME,
  PROP_OBJECT
};

static void photos_source_filterable_iface_init (PhotosFilterableInterface *iface);


G_DEFINE_TYPE_WITH_CODE (PhotosSource, photos_source, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (PHOTOS_TYPE_FILTERABLE,
                                                photos_source_filterable_iface_init));
EGG_DEFINE_COUNTER (instances, "PhotosSource", "Instances", "Number of PhotosSource instances")


static const gchar *TRACKER_SCHEMA = "org.freedesktop.Tracker.Miner.Files";
static const gchar *TRACKER_KEY_RECURSIVE_DIRECTORIES = "index-recursive-directories";


static gchar *
photos_source_build_filter_local (void)
{
  g_autoptr (GSettings) settings = NULL;
  g_autoptr (GString) tracker_filter = NULL;
  g_auto (GStrv) tracker_dirs = NULL;
  g_autofree gchar *desktop_uri = NULL;
  g_autofree gchar *download_uri = NULL;
  g_autofree gchar *export_path = NULL;
  g_autofree gchar *export_uri = NULL;
  gchar *filter;
  const gchar *path;
  g_autofree gchar *pictures_uri = NULL;
  guint i;

  settings = g_settings_new (TRACKER_SCHEMA);
  tracker_dirs = g_settings_get_strv (settings, TRACKER_KEY_RECURSIVE_DIRECTORIES);
  tracker_filter = g_string_new ("");

  for (i = 0; tracker_dirs[i] != NULL; i++)
    {
      g_autofree gchar *tracker_uri = NULL;

      /* ignore special XDG placeholders, since we handle those internally */
      if (tracker_dirs[i][0] == '&' || tracker_dirs[i][0] == '$')
        continue;

      tracker_uri = photos_utils_convert_path_to_uri (tracker_dirs[i]);
      g_string_append_printf (tracker_filter, " || fn:contains (nie:url (?urn), '%s')", tracker_uri);
    }

  path = g_get_user_special_dir (G_USER_DIRECTORY_DESKTOP);
  desktop_uri = photos_utils_convert_path_to_uri (path);

  path = g_get_user_special_dir (G_USER_DIRECTORY_DOWNLOAD);
  download_uri = photos_utils_convert_path_to_uri (path);

  path = g_get_user_special_dir (G_USER_DIRECTORY_PICTURES);
  pictures_uri = photos_utils_convert_path_to_uri (path);

  export_path = g_build_filename (path, PHOTOS_EXPORT_SUBPATH, NULL);
  export_uri = photos_utils_convert_path_to_uri (export_path);

  filter = g_strdup_printf ("(((fn:contains (nie:url (?urn), '%s')"
                            "   || fn:contains (nie:url (?urn), '%s')"
                            "   || fn:contains (nie:url (?urn), '%s')"
                            "   %s)"
                            "  && !fn:contains (nie:url (?urn), '%s'))"
                            " || fn:starts-with (nao:identifier (?urn), '%s')"
                            " || (?urn = nfo:image-category-screenshot))",
                            desktop_uri,
                            download_uri,
                            pictures_uri,
                            tracker_filter->str,
                            export_uri,
                            PHOTOS_QUERY_LOCAL_COLLECTIONS_IDENTIFIER);

  return filter;
}


static gchar *
photos_source_build_filter_resource (PhotosSource *self)
{
  g_autofree gchar *filter = NULL;
  gchar *ret_val = NULL;

  g_return_val_if_fail (!self->builtin, NULL);

  if (self->object != NULL)
    filter = g_strdup_printf ("(nie:dataSource (?urn) = '%s')", self->id);
  else
    g_return_val_if_reached (NULL);

  ret_val = g_steal_pointer (&filter);
  return ret_val;
}


static gboolean
photos_source_get_builtin (PhotosFilterable *iface)
{
  PhotosSource *self = PHOTOS_SOURCE (iface);
  return self->builtin;
}


static gchar *
photos_source_get_filter (PhotosFilterable *iface)
{
  PhotosSource *self = PHOTOS_SOURCE (iface);

  g_assert_cmpstr (self->id, !=, PHOTOS_SOURCE_STOCK_ALL);

  if (g_strcmp0 (self->id, PHOTOS_SOURCE_STOCK_LOCAL) == 0)
    return photos_source_build_filter_local ();

  return photos_source_build_filter_resource (self);
}


static const gchar *
photos_source_get_id (PhotosFilterable *filterable)
{
  PhotosSource *self = PHOTOS_SOURCE (filterable);
  return self->id;
}


static void
photos_source_dispose (GObject *object)
{
  PhotosSource *self = PHOTOS_SOURCE (object);

  g_clear_object (&self->icon);
  g_clear_object (&self->object);

  G_OBJECT_CLASS (photos_source_parent_class)->dispose (object);
}


static void
photos_source_finalize (GObject *object)
{
  PhotosSource *self = PHOTOS_SOURCE (object);

  g_free (self->id);
  g_free (self->name);

  G_OBJECT_CLASS (photos_source_parent_class)->finalize (object);

  EGG_COUNTER_DEC (instances);
}


static void
photos_source_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
  PhotosSource *self = PHOTOS_SOURCE (object);

  switch (prop_id)
    {
    case PROP_BUILTIN:
      g_value_set_boolean (value, self->builtin);
      break;

    case PROP_ID:
      g_value_set_string (value, self->id);
      break;

    case PROP_NAME:
      g_value_set_string (value, self->name);
      break;

    case PROP_OBJECT:
      g_value_set_object (value, (gpointer) self->object);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
photos_source_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
  PhotosSource *self = PHOTOS_SOURCE (object);

  switch (prop_id)
    {
    case PROP_BUILTIN:
      self->builtin = g_value_get_boolean (value);
      break;

    case PROP_ID:
      self->id = g_value_dup_string (value);
      break;

    case PROP_NAME:
      self->name = g_value_dup_string (value);
      break;

    case PROP_OBJECT:
      {
        GoaAccount *account;
        const gchar *provider_icon;
        const gchar *provider_name;

        self->object = GOA_OBJECT (g_value_dup_object (value));
        if (self->object == NULL)
          break;

        g_return_if_fail (self->id == NULL);
        g_return_if_fail (self->name == NULL);

        account = goa_object_peek_account (self->object);
        self->id = g_strdup_printf ("gd:goa-account:%s", goa_account_get_id (account));

        provider_icon = goa_account_get_provider_icon (account);
        self->icon = g_icon_new_for_string (provider_icon, NULL); /* TODO: use a GError */

        provider_name = goa_account_get_provider_name (account);
        self->name = g_strdup (provider_name);
        break;
      }

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
photos_source_init (PhotosSource *self)
{
  EGG_COUNTER_INC (instances);
}


static void
photos_source_class_init (PhotosSourceClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->dispose = photos_source_dispose;
  object_class->finalize = photos_source_finalize;
  object_class->get_property = photos_source_get_property;
  object_class->set_property = photos_source_set_property;

  g_object_class_install_property (object_class,
                                   PROP_BUILTIN,
                                   g_param_spec_boolean ("builtin",
                                                         "",
                                                         "",
                                                         FALSE,
                                                         G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_ID,
                                   g_param_spec_string ("id",
                                                        "",
                                                        "",
                                                        NULL,
                                                        G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_NAME,
                                   g_param_spec_string ("name",
                                                        "",
                                                        "",
                                                        NULL,
                                                        G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_OBJECT,
                                   g_param_spec_object ("object",
                                                        "GoaObject instance",
                                                        "A GOA configured account from which the source was created",
                                                        GOA_TYPE_OBJECT,
                                                        G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE));
}


static void
photos_source_filterable_iface_init (PhotosFilterableInterface *iface)
{
  iface->get_builtin = photos_source_get_builtin;
  iface->get_filter = photos_source_get_filter;
  iface->get_id = photos_source_get_id;
}


PhotosSource *
photos_source_new (const gchar *id, const gchar *name, gboolean builtin)
{
  return g_object_new (PHOTOS_TYPE_SOURCE, "id", id, "name", name, "builtin", builtin, NULL);
}


PhotosSource *
photos_source_new_from_goa_object (GoaObject *object)
{
  g_return_val_if_fail (GOA_IS_OBJECT (object), NULL);
  return g_object_new (PHOTOS_TYPE_SOURCE, "object", object, NULL);
}


const gchar *
photos_source_get_name (PhotosSource *self)
{
  return self->name;
}


GoaObject *
photos_source_get_goa_object (PhotosSource *self)
{
  return self->object;
}


GIcon *
photos_source_get_icon (PhotosSource *self)
{
  return self->icon;
}
