/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2012 Red Hat, Inc.
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

/* Based on code from:
 *   + Documents
 */

#ifndef PHOTOS_PROPERTIES_DIALOG_H
#define PHOTOS_PROPERTIES_DIALOG_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define PHOTOS_TYPE_PROPERTIES_DIALOG (photos_properties_dialog_get_type ())

#define PHOTOS_PROPERTIES_DIALOG(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
   PHOTOS_TYPE_PROPERTIES_DIALOG, PhotosPropertiesDialog))

#define PHOTOS_PROPERTIES_DIALOG_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
   PHOTOS_TYPE_PROPERTIES_DIALOG, PhotosPropertiesDialogClass))

#define PHOTOS_IS_PROPERTIES_DIALOG(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
   PHOTOS_TYPE_PROPERTIES_DIALOG))

#define PHOTOS_IS_PROPERTIES_DIALOG_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
   PHOTOS_TYPE_PROPERTIES_DIALOG))

#define PHOTOS_PROPERTIES_DIALOG_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
   PHOTOS_TYPE_PROPERTIES_DIALOG, PhotosPropertiesDialogClass))

typedef struct _PhotosPropertiesDialog        PhotosPropertiesDialog;
typedef struct _PhotosPropertiesDialogClass   PhotosPropertiesDialogClass;
typedef struct _PhotosPropertiesDialogPrivate PhotosPropertiesDialogPrivate;

struct _PhotosPropertiesDialog
{
  GtkDialog parent_instance;
  PhotosPropertiesDialogPrivate *priv;
};

struct _PhotosPropertiesDialogClass
{
  GtkDialogClass parent_class;
};

GType               photos_properties_dialog_get_type           (void) G_GNUC_CONST;

GtkWidget          *photos_properties_dialog_new                (GtkWindow *parent, const gchar *urn);

G_END_DECLS

#endif /* PHOTOS_PROPERTIES_DIALOG_H */
