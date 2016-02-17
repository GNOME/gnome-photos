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
 *   + Documents
 */


#include "config.h"

#include <glib.h>

#include "photos-item-manager.h"
#include "photos-search-context.h"
#include "photos-search-controller.h"
#include "photos-search-match-manager.h"
#include "photos-search-type-manager.h"
#include "photos-source-manager.h"


G_DEFINE_INTERFACE (PhotosSearchContext, photos_search_context, G_TYPE_INVALID);


static void
photos_search_context_default_init (PhotosSearchContextInterface *iface)
{
}


PhotosSearchContextState *
photos_search_context_state_new (PhotosSearchContext *self)
{
  PhotosSearchContextState *state;

  state = g_slice_new0 (PhotosSearchContextState);
  state->item_mngr = photos_item_manager_new ();
  state->mode_cntrlr = g_object_ref (state->item_mngr);
  state->src_mngr = photos_source_manager_new ();
  state->srch_cntrlr = photos_search_controller_new ();
  state->srch_mtch_mngr = photos_search_match_manager_new (state->srch_cntrlr);
  state->srch_typ_mngr = photos_search_type_manager_new ();

  return state;
}


void
photos_search_context_state_free (PhotosSearchContextState *state)
{
  g_object_unref (state->item_mngr);
  g_object_unref (state->mode_cntrlr);
  g_object_unref (state->src_mngr);
  g_object_unref (state->srch_mtch_mngr);
  g_object_unref (state->srch_typ_mngr);
  g_object_unref (state->srch_cntrlr);
  g_slice_free (PhotosSearchContextState, state);
}


PhotosSearchContextState *
photos_search_context_get_state (PhotosSearchContext *self)
{
  g_return_val_if_fail (PHOTOS_IS_SEARCH_CONTEXT (self), NULL);
  return PHOTOS_SEARCH_CONTEXT_GET_INTERFACE (self)->get_state (self);
}
