/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2013 – 2017 Red Hat, Inc.
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

#ifndef PHOTOS_SEARCH_CONTEXT_H
#define PHOTOS_SEARCH_CONTEXT_H

#include <glib-object.h>

G_BEGIN_DECLS

#define PHOTOS_TYPE_SEARCH_CONTEXT (photos_search_context_get_type ())
G_DECLARE_INTERFACE (PhotosSearchContext, photos_search_context, PHOTOS, SEARCH_CONTEXT, GObject);

typedef struct _PhotosSearchContextState PhotosSearchContextState;

struct _PhotosSearchContextState
{
  gpointer item_mngr;
  gpointer mode_cntrlr;
  gpointer src_mngr;
  gpointer srch_mtch_mngr;
  gpointer srch_typ_mngr;
  gpointer offset_cntrlr;
  gpointer srch_cntrlr;
};

PhotosSearchContextState      *photos_search_context_state_new      (PhotosSearchContext *self);

void                           photos_search_context_state_free     (PhotosSearchContextState *state);

struct _PhotosSearchContextInterface
{
  GTypeInterface parent_iface;

  /* virtual methods */
  PhotosSearchContextState *(*get_state) (PhotosSearchContext *self);
};

PhotosSearchContextState    *photos_search_context_get_state          (PhotosSearchContext *self);

G_END_DECLS

#endif /* PHOTOS_SEARCH_CONTEXT_H */
