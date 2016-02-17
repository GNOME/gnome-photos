/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2013 – 2016 Red Hat, Inc.
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
 *   + Eye of GNOME
 */

#ifndef PHOTOS_PRINT_OPERATION_H
#define PHOTOS_PRINT_OPERATION_H

#include <gegl.h>
#include <gtk/gtk.h>

#include "photos-base-item.h"

G_BEGIN_DECLS

#define PHOTOS_TYPE_PRINT_OPERATION (photos_print_operation_get_type ())

#define PHOTOS_PRINT_OPERATION(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
   PHOTOS_TYPE_PRINT_OPERATION, PhotosPrintOperation))

#define PHOTOS_PRINT_OPERATION_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
   PHOTOS_TYPE_PRINT_OPERATION, PhotosPrintOperationClass))

#define PHOTOS_IS_PRINT_OPERATION(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
   PHOTOS_TYPE_PRINT_OPERATION))

#define PHOTOS_IS_PRINT_OPERATION_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
   PHOTOS_TYPE_PRINT_OPERATION))

#define PHOTOS_PRINT_OPERATION_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
   PHOTOS_TYPE_PRINT_OPERATION, PhotosPrintOperationClass))

typedef struct _PhotosPrintOperation        PhotosPrintOperation;
typedef struct _PhotosPrintOperationClass   PhotosPrintOperationClass;
typedef struct _PhotosPrintOperationPrivate PhotosPrintOperationPrivate;

struct _PhotosPrintOperation
{
  GtkPrintOperation parent_instance;
  PhotosPrintOperationPrivate *priv;
};

struct _PhotosPrintOperationClass
{
  GtkPrintOperationClass parent_class;
};

GType               photos_print_operation_get_type           (void) G_GNUC_CONST;

GtkPrintOperation  *photos_print_operation_new                (PhotosBaseItem *item, GeglNode *node);

G_END_DECLS

#endif /* PHOTOS_PRINT_OPERATION_H */
