/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2015 – 2017 Red Hat, Inc.
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
 *   + GNU gettext
 */

#ifndef PHOTOS_GLIB_H
#define PHOTOS_GLIB_H

#include <string.h>

#include <gio/gio.h>

G_BEGIN_DECLS

#define PHOTOS_PC_(Context, String, String_Plural, N) \
  photos_glib_dnpgettext (NULL, Context "\004" String, String_Plural, (N), strlen (Context) + 1)

gboolean              photos_glib_app_info_launch_uri            (GAppInfo *appinfo,
                                                                  const gchar *uri,
                                                                  GAppLaunchContext *launch_context,
                                                                  GError **error);

const gchar          *photos_glib_dnpgettext                     (const gchar *domain,
                                                                  const gchar *msgctxtid,
                                                                  const gchar *msgid_plural,
                                                                  gulong n,
                                                                  gsize msgidoffset)
  G_GNUC_FORMAT (2) G_GNUC_FORMAT (3);

void                  photos_glib_file_copy_async                (GFile *source,
                                                                  GFile *destination,
                                                                  GFileCopyFlags flags,
                                                                  gint io_priority,
                                                                  GCancellable *cancellable,
                                                                  GAsyncReadyCallback callback,
                                                                  gpointer user_data);

GFile                *photos_glib_file_copy_finish               (GFile *source, GAsyncResult *res, GError **error);

void                  photos_glib_file_create_async              (GFile *file,
                                                                  GFileCreateFlags flags,
                                                                  gint io_priority,
                                                                  GCancellable *cancellable,
                                                                  GAsyncReadyCallback callback,
                                                                  gpointer user_data);

GFileOutputStream    *photos_glib_file_create_finish             (GFile *file,
                                                                  GAsyncResult *res,
                                                                  GFile **out_unique_file,
                                                                  GError **error);

gchar                *photos_glib_filename_strip_extension       (const gchar *filename_with_extension);

gboolean              photos_glib_make_directory_with_parents    (GFile *file,
                                                                  GCancellable *cancellable,
                                                                  GError **error);

G_END_DECLS

#endif /* PHOTOS_GLIB_H */
