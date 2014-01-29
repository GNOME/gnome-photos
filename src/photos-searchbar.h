/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2014 Red Hat, Inc.
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

#ifndef PHOTOS_SEARCHBAR_H
#define PHOTOS_SEARCHBAR_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define PHOTOS_TYPE_SEARCHBAR (photos_searchbar_get_type ())

#define PHOTOS_SEARCHBAR(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
   PHOTOS_TYPE_SEARCHBAR, PhotosSearchbar))

#define PHOTOS_SEARCHBAR_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
   PHOTOS_TYPE_SEARCHBAR, PhotosSearchbarClass))

#define PHOTOS_IS_SEARCHBAR(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
   PHOTOS_TYPE_SEARCHBAR))

#define PHOTOS_IS_SEARCHBAR_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
   PHOTOS_TYPE_SEARCHBAR))

#define PHOTOS_SEARCHBAR_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
   PHOTOS_TYPE_SEARCHBAR, PhotosSearchbarClass))

typedef struct _PhotosSearchbar        PhotosSearchbar;
typedef struct _PhotosSearchbarClass   PhotosSearchbarClass;
typedef struct _PhotosSearchbarPrivate PhotosSearchbarPrivate;

struct _PhotosSearchbar
{
  GtkRevealer parent_instance;
  PhotosSearchbarPrivate *priv;
};

struct _PhotosSearchbarClass
{
  GtkRevealerClass parent_class;

  /* virtual methods */
  void (*create_search_widgets) (PhotosSearchbar *self);
  void (*entry_changed) (PhotosSearchbar *self);
  void (*hide) (PhotosSearchbar *self);
  void (*show) (PhotosSearchbar *self);
};

GType                photos_searchbar_get_type                      (void) G_GNUC_CONST;

GtkWidget           *photos_searchbar_new                           (void);

gboolean             photos_searchbar_handle_event                  (PhotosSearchbar *self, GdkEventKey *event);

void                 photos_searchbar_hide                          (PhotosSearchbar *self);

gboolean             photos_searchbar_is_focus                      (PhotosSearchbar *self);

void                 photos_searchbar_set_search_change_blocked     (PhotosSearchbar *self,
                                                                     gboolean search_change_blocked);

void                 photos_searchbar_set_search_container          (PhotosSearchbar *self,
                                                                     GtkWidget *search_container);

void                 photos_searchbar_set_search_entry              (PhotosSearchbar *self, GtkWidget *search_entry);

void                 photos_searchbar_show                          (PhotosSearchbar *self);

G_END_DECLS

#endif /* PHOTOS_SEARCHBAR_H */
