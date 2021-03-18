/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2016 – 2021 Red Hat, Inc.
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

#include "photos-png-count.h"


static void
photos_png_count_flush_data (png_structp png_ptr)
{
}


static void
photos_png_count_write_data (png_structp png_ptr, png_bytep data, png_size_t length)
{
  gsize *out_count;

  out_count = (gsize *) png_get_io_ptr (png_ptr);
  if (out_count != NULL)
    *out_count += (gsize) length;
}


void
photos_png_init_count (png_structp png_ptr, gsize *out_count)
{
  png_set_write_fn (png_ptr, out_count, photos_png_count_write_data, photos_png_count_flush_data);
  if (out_count != NULL)
    *out_count = 0;
}
