/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2013 – 2017 Red Hat, Inc.
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

#ifndef PHOTOS_SEARCH_CONTEXT_H
#define PHOTOS_SEARCH_CONTEXT_H

#include <glib-object.h>

G_BEGIN_DECLS

#define PHOTOS_TYPE_SEARCH_CONTEXT (photos_search_context_get_type ())

#define PHOTOS_SEARCH_CONTEXT(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
   PHOTOS_TYPE_SEARCH_CONTEXT, PhotosSearchContext))

#define PHOTOS_IS_SEARCH_CONTEXT(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
   PHOTOS_TYPE_SEARCH_CONTEXT))

#define PHOTOS_SEARCH_CONTEXT_GET_INTERFACE(inst) \
  (G_TYPE_INSTANCE_GET_INTERFACE ((inst), \
   PHOTOS_TYPE_SEARCH_CONTEXT, PhotosSearchContextInterface))

typedef struct _PhotosSearchContextState PhotosSearchContextState;

typedef struct _PhotosSearchContext          PhotosSearchContext;
typedef struct _PhotosSearchContextInterface PhotosSearchContextInterface;

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

GType                        photos_search_context_get_type           (void) G_GNUC_CONST;

PhotosSearchContextState    *photos_search_context_get_state          (PhotosSearchContext *self);

G_END_DECLS

#endif /* PHOTOS_SEARCH_CONTEXT_H */
