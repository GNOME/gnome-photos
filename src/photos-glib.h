/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2015 – 2021 Red Hat, Inc.
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

#ifndef PHOTOS_GLIB_H
#define PHOTOS_GLIB_H

#include <gio/gio.h>

G_BEGIN_DECLS

#define photos_glib_assert_strv_contains(strv, str) \
  G_STMT_START { \
    const gchar *const *__strv = (strv); \
    const gchar *__str = (str); \
    if (g_strv_contains (__strv, __str)); else \
      { \
        const gchar *expression = #strv " contains " #str; \
        photos_glib_assertion_message_strv_contains (G_LOG_DOMAIN, \
                                                     __FILE__, \
                                                     __LINE__, \
                                                     G_STRFUNC, \
                                                     expression, \
                                                     strv, \
                                                     str); \
      } \
  } G_STMT_END

gboolean              photos_glib_app_info_launch_uri            (GAppInfo *appinfo,
                                                                  const gchar *uri,
                                                                  GAppLaunchContext *launch_context,
                                                                  GError **error);

void                  photos_glib_assertion_message_strv_contains (const gchar *domain,
                                                                   const gchar *file,
                                                                   gint line,
                                                                   const gchar *function,
                                                                   const gchar *expression,
                                                                   const gchar *const *strv,
                                                                   const gchar *str);

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
