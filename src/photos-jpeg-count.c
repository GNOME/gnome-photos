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


#include "config.h"

#include <stdio.h>

#include "photos-jpeg-count.h"


typedef struct _PhotosJpegCountDestMgr PhotosJpegCountDestMgr;

struct _PhotosJpegCountDestMgr
{
  struct jpeg_destination_mgr parent;
  gsize *out_count;
};

static JOCTET dummy_buffer[1];


static gboolean
photos_jpeg_count_empty_output_buffer (j_compress_ptr cinfo)
{
  PhotosJpegCountDestMgr *dest = (PhotosJpegCountDestMgr *) cinfo->dest;

  if (dest->out_count != NULL)
    *dest->out_count += G_N_ELEMENTS (dummy_buffer);

  dest->parent.next_output_byte = dummy_buffer;
  dest->parent.free_in_buffer = G_N_ELEMENTS (dummy_buffer);

  return TRUE;
}


static void
photos_jpeg_count_init_destination (j_compress_ptr cinfo)
{
}


static void
photos_jpeg_count_term_destination (j_compress_ptr cinfo)
{
  PhotosJpegCountDestMgr *dest = (PhotosJpegCountDestMgr *) cinfo->dest;

  if (dest->out_count != NULL)
    *dest->out_count += G_N_ELEMENTS (dummy_buffer) - dest->parent.free_in_buffer;
}


void
photos_jpeg_count_dest (j_compress_ptr cinfo, gsize *out_count)
{
  PhotosJpegCountDestMgr *dest;

  if (cinfo->dest == NULL)
    {
      cinfo->dest
        = (struct jpeg_destination_mgr *) (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo,
                                                                      JPOOL_PERMANENT,
                                                                      sizeof (PhotosJpegCountDestMgr));
    }

  dest = (PhotosJpegCountDestMgr *) cinfo->dest;
  dest->parent.init_destination = photos_jpeg_count_init_destination;
  dest->parent.empty_output_buffer = photos_jpeg_count_empty_output_buffer;
  dest->parent.term_destination = photos_jpeg_count_term_destination;
  dest->parent.next_output_byte = dummy_buffer;
  dest->parent.free_in_buffer = G_N_ELEMENTS (dummy_buffer);
  dest->out_count = out_count;

  if (dest->out_count != NULL)
    *dest->out_count = 0;
}
