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

#ifndef PHOTOS_SPINNER_BOX_H
#define PHOTOS_SPINNER_BOX_H

#include <clutter-gtk/clutter-gtk.h>

G_BEGIN_DECLS

#define PHOTOS_TYPE_SPINNER_BOX (photos_spinner_box_get_type ())

#define PHOTOS_SPINNER_BOX(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
   PHOTOS_TYPE_SPINNER_BOX, PhotosSpinnerBox))

#define PHOTOS_SPINNER_BOX_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
   PHOTOS_TYPE_SPINNER_BOX, PhotosSpinnerBoxClass))

#define PHOTOS_IS_SPINNER_BOX(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
   PHOTOS_TYPE_SPINNER_BOX))

#define PHOTOS_IS_SPINNER_BOX_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
   PHOTOS_TYPE_SPINNER_BOX))

#define PHOTOS_SPINNER_BOX_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
   PHOTOS_TYPE_SPINNER_BOX, PhotosSpinnerBoxClass))

typedef struct _PhotosSpinnerBox        PhotosSpinnerBox;
typedef struct _PhotosSpinnerBoxClass   PhotosSpinnerBoxClass;
typedef struct _PhotosSpinnerBoxPrivate PhotosSpinnerBoxPrivate;

struct _PhotosSpinnerBox
{
  GtkClutterActor parent_instance;
  PhotosSpinnerBoxPrivate *priv;
};

struct _PhotosSpinnerBoxClass
{
  GtkClutterActorClass parent_class;
};

GType                  photos_spinner_box_get_type               (void) G_GNUC_CONST;

ClutterActor          *photos_spinner_box_new                    (void);

void                   photos_spinner_box_move_in                (PhotosSpinnerBox *self);

void                   photos_spinner_box_move_in_delayed        (PhotosSpinnerBox *self, guint delay);

void                   photos_spinner_box_move_out               (PhotosSpinnerBox *self);

G_END_DECLS

#endif /* PHOTOS_SPINNER_BOX_H */
