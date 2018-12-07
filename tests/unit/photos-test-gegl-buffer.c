/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2018 Red Hat, Inc.
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

#include <locale.h>

#include <gegl.h>
#include <gio/gio.h>
#include <glib.h>

#include "photos-debug.h"
#include "photos-gegl.h"
#include "photos-gegl-buffer-loader.h"
#include "photos-gegl-buffer-loader-builder.h"


typedef struct _PhotosTestGeglBufferFixture PhotosTestGeglBufferFixture;
typedef struct _PhotosTestGeglBufferPath PhotosTestGeglBufferPath;

struct _PhotosTestGeglBufferFixture
{
  const Babl *format;
  GAsyncResult *res;
  GFile *destination;
  GFile *source;
  GMainContext *context;
  GMainLoop *loop;
  GeglBuffer *buffer;
};

struct _PhotosTestGeglBufferPath
{
  const gchar *d;
  const gchar *fill;
  const gchar *stroke;
  gdouble stroke_width;
};


static gchar *
photos_test_gegl_buffer_filename_to_uri (const gchar *filename)
{
  g_autoptr (GFile) file = NULL;
  g_autofree gchar *path_relative = NULL;
  gchar *uri = NULL;

  path_relative = g_test_build_filename (G_TEST_DIST, filename, NULL);
  file = g_file_new_for_path (path_relative);
  uri = g_file_get_uri (file);
  return uri;
}


static void
photos_test_gegl_buffer_save_to_file (GeglBuffer *buffer, GFile *file)
{
  GeglNode *buffer_source;
  GeglNode *png_save;
  g_autoptr (GeglNode) graph = NULL;
  g_autofree gchar *path = NULL;

  g_assert_true (GEGL_IS_BUFFER (buffer));
  g_assert_true (G_IS_FILE (file));

  graph = gegl_node_new ();
  buffer_source = gegl_node_new_child (graph, "operation", "gegl:buffer-source", "buffer", buffer, NULL);

  path = g_file_get_path (file);
  png_save = gegl_node_new_child (graph, "operation", "gegl:png-save", "bitdepth", 8, "path", path, NULL);

  gegl_node_link (buffer_source, png_save);
  gegl_node_process (png_save);
}


static void
photos_test_gegl_buffer_setup (PhotosTestGeglBufferFixture *fixture, gconstpointer user_data)
{
  const Babl *format;
  g_autoptr (GeglBuffer) buffer = NULL;
  GeglColor *checkerboard_color1 = NULL; /* TODO: use g_autoptr */
  GeglColor *checkerboard_color2 = NULL; /* TODO: use g_autoptr */
  GeglNode *buffer_sink;
  GeglNode *checkerboard;
  GeglNode *convert_format;
  GeglNode *crop;
  GeglNode *tail;
  g_autoptr (GeglNode) graph = NULL;
  const PhotosTestGeglBufferPath paths[] =
    {
      {
        "M-122.304,84.285 "
        "C-122.304,84.285 -122.203,86.179 -123.027,86.16 "
        "C-123.851,86.141 -140.305,38.066 -160.833,40.309 "
        "C-160.833,40.309 -143.05,32.956 -122.304,84.285 "
        "z",
        "#ffffff",
        "#000000",
        0.172
      },
      {
        "M-118.774,81.262 "
        "C-118.774,81.262 -119.323,83.078 -120.092,82.779 "
        "C-120.86,82.481 -119.977,31.675 -140.043,26.801 "
        "C-140.043,26.801 -120.82,25.937 -118.774,81.262 "
        "z",
        "#ffffff",
        "#000000",
        0.172
      },
      {
        "M-91.284,123.59 "
        "C-91.284,123.59 -89.648,124.55 -90.118,125.227 "
        "C-90.589,125.904 -139.763,113.102 -149.218,131.459 "
        "C-149.218,131.459 -145.539,112.572 -91.284,123.59 "
        "z",
        "#ffffff",
        "#000000",
        0.172
      },
      {
        "M-94.093,133.801 "
        "C-94.093,133.801 -92.237,134.197 -92.471,134.988 "
        "C-92.704 135.779 -143.407 139.121 -146.597 159.522 "
        "C-146.597,159.522 -149.055,140.437 -94.093,133.801 "
        "z",
        "#ffffff",
        "#000000",
        0.172
      },
      {
        "M-98.304,128.276 "
        "C-98.304,128.276 -96.526,128.939 -96.872,129.687 "
        "C-97.218,130.435 -147.866,126.346 -153.998,146.064 "
        "C-153.998,146.064 -153.646,126.825 -98.304,128.276 "
        "z",
        "#ffffff",
        "#000000",
        0.172
      },
      {
        "M-109.009,110.072 "
        "C-109.009,110.072 -107.701,111.446 -108.34,111.967 "
        "C-108.979,112.488 -152.722,86.634 -166.869,101.676 "
        "C-166.869,101.676 -158.128,84.533 -109.009,110.072 "
        "z",
        "#ffffff",
        "#000000",
        0.172
      },
      {
        "M-116.554,114.263 "
        "C-116.554,114.263 -115.098,115.48 -115.674,116.071 "
        "C-116.25,116.661 -162.638,95.922 -174.992,112.469 "
        "C-174.992,112.469 -168.247,94.447 -116.554,114.263 "
        "z",
        "#ffffff",
        "#000000",
        0.172
      },
      {
        "M-119.154,118.335 "
        "C-119.154,118.335 -117.546,119.343 -118.036,120.006 "
        "C-118.526,120.669 -167.308,106.446 -177.291,124.522 "
        "C-177.291,124.522 -173.066,105.749 -119.154,118.335 "
        "z",
        "#ffffff",
        "#000000",
        0.172
      },
      {
        "M-108.42,118.949 "
        "C-108.42,118.949 -107.298,120.48 -107.999,120.915 "
        "C-108.7,121.35 -148.769,90.102 -164.727,103.207 "
        "C-164.727,103.207 -153.862,87.326 -108.42,118.949 "
        "z",
        "#ffffff",
        "#000000",
        0.172
      },
      {
        "M-128.2,90 "
        "C-128.2,90 -127.6,91.8 -128.4,92 "
        "C-129.2,92.2 -157.8,50.2 -177.001,57.8 "
        "C-177.001,57.8 -161.8,46 -128.2,90 "
        "z",
        "#ffffff",
        "#000000",
        0.172
      },
      {
        "M-127.505,96.979 "
        "C-127.505,96.979 -126.53,98.608 -127.269,98.975 "
        "C-128.007,99.343 -164.992,64.499 -182.101,76.061 "
        "C-182.101,76.061 -169.804,61.261 -127.505,96.979 "
        "z",
        "#ffffff",
        "#000000",
        0.172
      },
      {
        "M-127.62,101.349 "
        "C-127.62,101.349 -126.498,102.88 -127.199,103.315 "
        "C-127.9,103.749 -167.969,72.502 -183.927,85.607 "
        "C-183.927,85.607 -173.062,69.726 -127.62,101.349 "
        "z",
        "#ffffff",
        "#000000",
        0.172
      },
      {
        "M-129.83,103.065 "
        "C-129.327,109.113 -128.339,115.682 -126.6,118.801 "
        "C-126.6,118.801 -130.2,131.201 -121.4,144.401 "
        "C-121.4,144.401 -121.8,151.601 -120.2,154.801 "
        "C-120.2,154.801 -116.2,163.201 -111.4,164.001 "
        "C-107.516,164.648 -98.793,167.717 -88.932,169.121 "
        "C-88.932,169.121 -71.8,183.201 -75,196.001 "
        "C-75,196.001 -75.4,212.401 -79,214.001 "
        "C-79,214.001 -67.4,202.801 -77,219.601 "
        "L-81.4,238.401 "
        "C-81.4,238.401 -55.8,216.801 -71.4,235.201 "
        "L-81.4,261.201 "
        "C-81.4,261.201 -61.8,242.801 -69,251.201 "
        "L-72.2,260.001 "
        "C-72.2,260.001 -29,232.801 -59.8,262.401 "
        "C-59.8,262.401 -51.8,258.801 -47.4,261.601 "
        "C-47.4,261.601 -40.6,260.401 -41.4,262.001 "
        "C-41.4,262.001 -62.2,272.401 -65.8,290.801 "
        "C-65.8,290.801 -57.4,280.801 -60.6,291.601 "
        "L-60.2,303.201 "
        "C-60.2,303.201 -56.2,281.601 -56.6,319.201 "
        "C-56.6,319.201 -37.4,301.201 -49,322.001 "
        "L-49,338.801 "
        "C-49,338.801 -33.8,322.401 -40.2,335.201 "
        "C-40.2,335.201 -30.2,326.401 -34.2,341.601 "
        "C-34.2,341.601 -35,352.001 -30.6,340.801 "
        "C-30.6,340.801 -14.6,310.201 -20.6,336.401 "
        "C-20.6,336.401 -21.4,355.601 -16.6,340.801 "
        "C-16.6,340.801 -16.2,351.201 -7,358.401 "
        "C-7,358.401 -8.2,307.601 4.6,343.601 "
        "L8.6,360.001 "
        "C8.6,360.001 11.4,350.801 11,345.601 "
        "C11,345.601 25.8,329.201 19,353.601 "
        "C19,353.601 34.2,330.801 31,344.001 "
        "C31 344.001 23.4 360.001 25 364.801C25 364.801 41.8 330.001 43 328.401C43 328.401 41 370.802 51.8 334.801C51.8 334.801 57.4 346.801 54.6 351.201C54.6 351.201 62.6 343.201 61.8 340.001C61.8 340.001 66.4 331.801 69.2 345.401C69.2 345.401 71 354.801 72.6 351.601C72.6 351.601 76.6 375.602 77.8 352.801C77.8 352.801 79.4 339.201 72.2 327.601C72.2 327.601 73 324.401 70.2 320.401C70.2 320.401 83.8 342.001 76.6 313.201C76.6 313.201 87.801 321.201 89.001 321.201C89.001 321.201 75.4 298.001 84.2 302.801C84.2 302.801 79 292.401 97.001 304.401C97.001 304.401 81 288.401 98.601 298.001C98.601 298.001 106.601 304.401 99.001 294.401C99.001 294.401 84.6 278.401 106.601 296.401C106.601 296.401 118.201 312.801 119.001 315.601C119.001 315.601 109.001 286.401 104.601 283.601C104.601 283.601 113.001 247.201 154.201 262.801C154.201 262.801 161.001 280.001 165.401 261.601C165.401 261.601 178.201 255.201 189.401 282.801C189.401 282.801 193.401 269.201 192.601 266.401C192.601 266.401 199.401 267.601 198.601 266.401C198.601 266.401 211.801 270.801 213.001 270.001C213.001 270.001 219.801 276.801 220.201 273.201 "
        "C220.201,273.201 229.401,276.001 227.401,272.401 "
        "C227.401,272.401 236.201,288.001 236.601,291.601 "
        "L239.001,277.601 "
        "L241.001 280.401 "
        "C241.001,280.401 242.601,272.801 241.801,271.601 "
        "C241.001 270.401 261.801 278.401 266.601 299.201L268.601 307.601C268.601 307.601 274.601 292.801 273.001 288.801C273.001 288.801 278.201 289.601 278.601 294.001C278.601 294.001 282.601 270.801 277.801 264.801C277.801 264.801 282.201 264.001 283.401 267.601L283.401 260.401C283.401 260.401 290.601 261.201 290.601 258.801C290.601 258.801 295.001 254.801 297.001 259.601C297.001 259.601 284.601 224.401 303.001 243.601C303.001 243.601 310.201 254.401 306.601 235.601C303.001 216.801 299.001 215.201 303.801 214.801C303.801 214.801 304.601 211.201 302.601 209.601C300.601 208.001 303.801 209.601 303.801 209.601C303.801 209.601 308.601 213.601 303.401 191.601C303.401 191.601 309.801 193.201 297.801 164.001C297.801 164.001 300.601 161.601 296.601 153.201C296.601 153.201 304.601 157.601 307.401 156.001C307.401 156.001 307.001 154.401 303.801 150.401 "
        "C303.801,150.401 282.201,95.6 302.601,117.601 "
        "C302.601,117.601 314.451,131.151 308.051,108.351 "
        "C308.051,108.351 298.94,84.341 299.717,80.045 "
        "L-129.83,103.065 "
        "z",
        "#ffffff",
        "#000000",
        1
      },
      {
        "M299.717 80.245C300.345 80.426 302.551 81.55 303.801 83.2C303.801 83.2 310.601 94 305.401 75.6C305.401 75.6 296.201 46.8 305.001 58C305.001 58 311.001 65.2 307.801 51.6C303.936 35.173 301.401 28.8 301.401 28.8C301.401 28.8 313.001 33.6 286.201 -6L295.001 -2.4C295.001 -2.4 275.401 -42 253.801 -47.2L245.801 -53.2C245.801 -53.2 284.201 -91.2 271.401 -128C271.401 -128 264.601 -133.2 255.001 -124C255.001 -124 248.601 -119.2 242.601 -120.8C242.601 -120.8 211.801 -119.6 209.801 -119.6C207.801 -119.6 173.001 -156.8 107.401 -139.2C107.401 -139.2 102.201 -137.2 97.801 -138.4C97.801 -138.4 79.4 -154.4 30.6 -131.6C30.6 -131.6 20.6 -129.6 19 -129.6C17.4 -129.6 14.6 -129.6 6.6 -123.2C-1.4 -116.8 -1.8 -116 -3.8 -114.4C-3.8 -114.4 -20.2 -103.2 -25 -102.4C-25 -102.4 -36.6 -96 -41 -86L-44.6 -84.8C-44.6 -84.8 -46.2 -77.6 -46.6 -76.4C-46.6 -76.4 -51.4 -72.8 -52.2 -67.2C-52.2 -67.2 -61 -61.2 -60.6 -56.8C-60.6 -56.8 -62.2 -51.6 -63 -46.8C-63 -46.8 -70.2 -42 -69.4 -39.2C-69.4 -39.2 -77 -25.2 -75.8 -18.4C-75.8 -18.4 -82.2 -18.8 -85 -16.4C-85 -16.4 -85.8 -11.6 -87.4 -11.2C-87.4 -11.2 -90.2 -10 -87.8 -6C-87.8 -6 -89.4 -3.2 -89.8 -1.6C-89.8 -1.6 -89 1.2 -93.4 6.8C-93.4 6.8 -99.8 25.6 -97.8 30.8C-97.8 30.8 -97.4 35.6 -100.2 37.2C-100.2 37.2 -103.8 36.8 -95.4 48.8C-95.4 48.8 -94.6 50 -97.8 52.4 "
        "C-97.8 52.4 -115 56 -117.4 72.4C-117.4 72.4 -131 87.2 -131 92.4C-131 94.705 -130.729 97.852 -130.03 102.465C-130.03 102.465 -130.6 110.801 -103 111.601C-75.4 112.401 299.717 80.245 299.717 80.245 "
        "z",
        "#cc7226",
        "#000000",
        1.0
      },
      {
        "M-115.6,102.6 "
        "C-140.6,63.2 -126.2,119.601 -126.2,119.601 "
        "C-117.4 154.001 12.2 116.401 12.2 116.401C12.2 116.401 181.001 86 192.201 82C203.401 78 298.601 84.4 298.601 84.4L293.001 67.6C228.201 21.2 209.001 44.4 195.401 40.4C181.801 36.4 184.201 46 181.001 46.8C177.801 47.6 138.601 22.8 132.201 23.6C125.801 24.4 100.459 0.649 115.401 32.4C131.401 66.4 57 71.6 40.2 60.4C23.4 49.2 47.4 78.8 47.4 78.8C65.8 98.8 31.4 82 31.4 82C-3 69.2 -27 94.8 -30.2 95.6C-33.4 96.4 -38.2 99.6 -39 93.2C-39.8 86.8 -47.31 70.099 -79 96.4C-99 113.001 -112.8 91 -112.8 91 "
        "L-115.6,102.6 "
        "z",
        "#cc7226",
        "#00000000",
        1.0
      },
      {
        "M133.51,25.346 "
        "C127.11,26.146 101.743,2.407 116.71,34.146 "
        "C133.31,69.346 58.31,73.346 41.51,62.146 "
        "C24.709 50.946 48.71 80.546 48.71 80.546C67.11 100.546 32.709 83.746 32.709 83.746C-1.691 70.946 -25.691 96.546 -28.891 97.346C-32.091 98.146 -36.891 101.346 -37.691 94.946C-38.491 88.546 -45.87 72.012 -77.691 98.146C-98.927 115.492 -112.418 94.037 -112.418 94.037L-115.618 104.146C-140.618 64.346 -125.546 122.655 -125.546 122.655C-116.745 157.056 13.509 118.146 13.509 118.146C13.509 118.146 182.31 87.746 193.51 83.746C204.71 79.746 299.038 86.073 299.038 86.073L293.51 68.764C228.71 22.364 210.31 46.146 196.71 42.146C183.11 38.146 185.51 47.746 182.31 48.546C179.11 49.346 139.91 24.546 133.51 25.346 "
        "z",
        "#e87f3a",
        "#00000000",
        1.0
      },
      {
        "M134.819,27.091 "
        "C128.419,27.891 103.685,3.862 118.019,35.891 "
        "C134.219 72.092 59.619 75.092 42.819 63.892C26.019 52.692 50.019 82.292 50.019 82.292C68.419 102.292 34.019 85.492 34.019 85.492C-0.381 72.692 -24.382 98.292 -27.582 99.092C-30.782 99.892 -35.582 103.092 -36.382 96.692C-37.182 90.292 -44.43 73.925 -76.382 99.892C-98.855 117.983 -112.036 97.074 -112.036 97.074L-115.636 105.692C-139.436 66.692 -124.891 125.71 -124.891 125.71C-116.091 160.11 14.819 119.892 14.819 119.892C14.819 119.892 183.619 89.492 194.819 85.492C206.019 81.492 299.474 87.746 299.474 87.746L294.02 69.928C229.219 23.528 211.619 47.891 198.019 43.891C184.419 39.891 186.819 49.491 183.619 50.292C180.419 51.092 141.219 26.291 134.819 27.091 "
        "z",
        "#ea8c4d",
        "#00000000",
        1.0
      },
      {
        "M136.128 28.837C129.728 29.637 104.999 5.605 119.328 37.637C136.128 75.193 60.394 76.482 44.128 65.637C27.328 54.437 51.328 84.037 51.328 84.037C69.728 104.037 35.328 87.237 35.328 87.237C0.928 74.437 -23.072 100.037 -26.272 100.837C-29.472 101.637 -34.272 104.837 -35.072 98.437C-35.872 92.037 -42.989 75.839 -75.073 101.637C-98.782 120.474 -111.655 100.11 -111.655 100.11L-115.655 107.237C-137.455 70.437 -124.236 128.765 -124.236 128.765C-115.436 163.165 16.128 121.637 16.128 121.637C16.128 121.637 184.928 91.237 196.129 87.237C207.329 83.237 299.911 89.419 299.911 89.419L294.529 71.092C229.729 24.691 212.929 49.637 199.329 45.637C185.728 41.637 188.128 51.237 184.928 52.037C181.728 52.837 142.528 28.037 136.128 28.837 "
        "z",
        "#ec9961",
        "#00000000",
        1.0
      },
      {
        "M137.438,30.583 "
        "C131.037,31.383 106.814,7.129 120.637,39.383 "
        "C137.438,78.583 62.237,78.583 45.437,67.383 "
        "C28.637 56.183 52.637 85.783 52.637 85.783C71.037 105.783 36.637 88.983 36.637 88.983C2.237 76.183 -21.763 101.783 -24.963 102.583C-28.163 103.383 -32.963 106.583 -33.763 100.183C-34.563 93.783 -41.548 77.752 -73.763 103.383C-98.709 122.965 -111.273 103.146 -111.273 103.146L-115.673 108.783C-135.473 73.982 -123.582 131.819 -123.582 131.819C-114.782 166.22 17.437 123.383 17.437 123.383C17.437 123.383 186.238 92.983 197.438 88.983C208.638 84.983 300.347 91.092 300.347 91.092L295.038 72.255C230.238 25.855 214.238 51.383 200.638 47.383C187.038 43.383 189.438 52.983 186.238 53.783C183.038 54.583 143.838 29.783 137.438 30.583 "
        "z",
        "#eea575",
        "#00000000",
        1.0
      },
      {
        "M138.747 32.328C132.347 33.128 106.383 9.677 121.947 41.128C141.147 79.928 63.546 80.328 46.746 69.128C29.946 57.928 53.946 87.528 53.946 87.528C72.346 107.528 37.946 90.728 37.946 90.728C3.546 77.928 -20.454 103.528 -23.654 104.328C-26.854 105.128 -31.654 108.328 -32.454 101.928C-33.254 95.528 -40.108 79.665 -72.454 105.128C-98.636 125.456 -110.891 106.183 -110.891 106.183L-115.691 110.328C-133.691 77.128 -122.927 134.874 -122.927 134.874C-114.127 169.274 18.746 125.128 18.746 125.128C18.746 125.128 187.547 94.728 198.747 90.728C209.947 86.728 300.783 92.764 300.783 92.764L295.547 73.419C230.747 27.019 215.547 53.128 201.947 49.128C188.347 45.128 190.747 54.728 187.547 55.528C184.347 56.328 145.147 31.528 138.747 32.328 "
        "z",
        "#f1b288",
        "#00000000",
        1.0
      },
      {
        "M140.056,34.073 "
        "C133.655 34.873 107.313 11.613 123.255 42.873C143.656 82.874 64.855 82.074 48.055 70.874C31.255 59.674 55.255 89.274 55.255 89.274C73.655 109.274 39.255 92.474 39.255 92.474C4.855 79.674 -19.145 105.274 -22.345 106.074C-25.545 106.874 -30.345 110.074 -31.145 103.674C-31.945 97.274 -38.668 81.578 -71.145 106.874C-98.564 127.947 -110.509 109.219 -110.509 109.219L-115.709 111.874C-131.709 81.674 -122.273 137.929 -122.273 137.929C-113.473 172.329 20.055 126.874 20.055 126.874C20.055 126.874 188.856 96.474 200.056 92.474C211.256 88.474 301.22 94.437 301.22 94.437L296.056 74.583C231.256 28.183 216.856 54.874 203.256 50.874C189.656 46.873 192.056 56.474 188.856 57.274C185.656 58.074 146.456 33.273 140.056 34.073 "
        "z",
        "#f3bf9c",
        "#00000000",
        1.0
      },
      {
        "M141.365 35.819C134.965 36.619 107.523 13.944 124.565 44.619C146.565 84.219 66.164 83.819 49.364 72.619C32.564 61.419 56.564 91.019 56.564 91.019C74.964 111.019 40.564 94.219 40.564 94.219C6.164 81.419 -17.836 107.019 -21.036 107.819C-24.236 108.619 -29.036 111.819 -29.836 105.419C-30.636 99.019 -37.227 83.492 -69.836 108.619C-98.491 130.438 -110.127 112.256 -110.127 112.256L-115.727 113.419C-130.128 85.019 -121.618 140.983 -121.618 140.983C-112.818 175.384 21.364 128.619 21.364 128.619C21.364 128.619 190.165 98.219 201.365 94.219C212.565 90.219 301.656 96.11 301.656 96.11L296.565 75.746C231.765 29.346 218.165 56.619 204.565 52.619C190.965 48.619 193.365 58.219 190.165 59.019C186.965 59.819 147.765 35.019 141.365 35.819 "
        "z",
        "#f5ccb0",
        "#00000000",
        1.0
      },
      {
        "M142.674,37.565 "
        "C136.274,38.365 108.832,15.689 125.874,46.365 "
        "C147.874 85.965 67.474 85.565 50.674 74.365C33.874 63.165 57.874 92.765 57.874 92.765C76.274 112.765 41.874 95.965 41.874 95.965C7.473 83.165 -16.527 108.765 -19.727 109.565C-22.927 110.365 -27.727 113.565 -28.527 107.165C-29.327 100.765 -35.786 85.405 -68.527 110.365C-98.418 132.929 -109.745 115.293 -109.745 115.293L-115.745 114.965C-129.346 88.564 -120.963 144.038 -120.963 144.038C-112.163 178.438 22.673 130.365 22.673 130.365C22.673 130.365 191.474 99.965 202.674 95.965C213.874 91.965 302.093 97.783 302.093 97.783L297.075 76.91C232.274 30.51 219.474 58.365 205.874 54.365C192.274 50.365 194.674 59.965 191.474 60.765 "
        "C188.274,61.565 149.074,36.765 142.674,37.565 "
        "z",
        "#f8d8c4",
        "#00000000",
        1.0
      },
      {
        "M143.983,39.31 "
        "C137.583,40.11 110.529,17.223 127.183,48.11 "
        "C149.183 88.91 68.783 87.31 51.983 76.11C35.183 64.91 59.183 94.51 59.183 94.51C77.583 114.51 43.183 97.71 43.183 97.71C8.783 84.91 -15.217 110.51 -18.417 111.31C-21.618 112.11 -26.418 115.31 -27.218 108.91C-28.018 102.51 -34.346 87.318 -67.218 112.11C-98.345 135.42 -109.363 118.329 -109.363 118.329L-115.764 116.51C-128.764 92.51 -120.309 147.093 -120.309 147.093C-111.509 181.493 23.983 132.11 23.983 132.11C23.983 132.11 192.783 101.71 203.983 97.71C215.183 93.71 302.529 99.456 302.529 99.456L297.583 78.074C232.783 31.673 220.783 60.11 207.183 56.11C193.583 52.11 195.983 61.71 192.783 62.51C189.583 63.31 150.383 38.51 143.983 39.31 "
        "z",
        "#fae5d7",
        "#00000000",
        1.0
      },
      {
        "M51.4,85 "
        "C51.4,85 36.4,68.2 28,65.6 "
        "C28,65.6 14.6,58.8 -10,66.6",
        "#000000",
        "#4c0000",
        2.0
      },
      {
        "M24.8,64.2 "
        "C24.8,64.2 -0.4,56.2 -15.8,60.4 "
        "C-15.8,60.4 -34.2,62.4 -42.6,76.2",
        "#000000",
        "#4c0000",
        2.0
      },
      {
        "M293 111C293 111 281 103 279.5 105C279.5 105 290 111.5 292.5 120C292.5 120 291 111 293 111 "
        "z",
        "#cccccc",
        "#00000000",
        1.0
      },
      {
        "M301.5,191.5 "
        "L284,179.5 "
        "C284,179.5 303,196.5 303.5,200.5 "
        "L301.5,191.5 "
        "z",
        "#cccccc",
        "#00000000",
        1.0
      },
      {
        "M-89.25,169 "
        "L-67.25,173.75",
        "#000000",
        "#000000",
        1.0
      },
      {
        "M-39,331 "
        "C-39,331 -39.5,327.5 -48.5,338",
        "#000000",
        "#000000",
        1.0
      },
      {
        "M-33.5,336 "
        "C-33.5,336 -31.5,329.5 -38,334",
        "#000000",
        "#000000",
        1.0
      },
      {
        "M20.5,344.5 "
        "C20.5,344.5 22,333.5 10.5,346.5",
        "#000000",
        "#000000",
        1.0
      }
    };
  guint i;

  {
    g_autoptr (GError) error = NULL;
    g_autoptr (GFileIOStream) iostream = NULL;

    fixture->destination = g_file_new_tmp (PACKAGE_TARNAME "-destination-XXXXXX.png", &iostream, &error);
    g_assert_no_error (error);
  }

  {
    g_autoptr (GError) error = NULL;
    g_autoptr (GFileIOStream) iostream = NULL;

    fixture->source = g_file_new_tmp (PACKAGE_TARNAME "-source-XXXXXX.png", &iostream, &error);
    g_assert_no_error (error);
  }

  fixture->context = g_main_context_new ();
  g_main_context_push_thread_default (fixture->context);
  fixture->loop = g_main_loop_new (fixture->context, FALSE);

  graph = gegl_node_new ();

  checkerboard_color1 = gegl_color_new ("rgb(0.25, 0.25, 0.25)");
  checkerboard_color2 = gegl_color_new ("rgb(0.75, 0.75, 0.75)");
  checkerboard = gegl_node_new_child (graph,
                                      "operation", "gegl:checkerboard",
                                      "color1", checkerboard_color1,
                                      "color2", checkerboard_color2,
                                      "x", 5,
                                      "y", 5,
                                      NULL);

  crop = gegl_node_new_child (graph,
                              "operation", "gegl:crop",
                              "height", 2000.0,
                              "width", 640.0,
                              NULL);

  gegl_node_link (checkerboard, crop);
  tail = crop;

  for (i = 0; i < G_N_ELEMENTS (paths); i++)
    {
      GeglColor *fill = NULL; /* TODO: use g_autoptr */
      GeglColor *stroke = NULL; /* TODO: use g_autoptr */
      GeglNode *over;
      GeglNode *path;
      GeglPath *d = NULL; /* TODO: use g_autoptr */

      over = gegl_node_new_child (graph, "operation", "svg:src-over", NULL);

      d = gegl_path_new_from_string (paths[i].d);
      fill = gegl_color_new (paths[i].fill);
      stroke = gegl_color_new (paths[i].stroke);
      path = gegl_node_new_child (graph,
                                  "operation", "gegl:path",
                                  "d", d,
                                  "fill", fill,
                                  "stroke", stroke,
                                  "stroke-width", paths[i].stroke_width,
                                  NULL);

      gegl_node_connect_to (path, "output", over, "aux");
      gegl_node_link (tail, over);
      tail = over;

      g_object_unref (d);
      g_object_unref (fill);
      g_object_unref (stroke);
    }

  format = babl_format ("R'G'B'A u8");
  convert_format = gegl_node_new_child (graph, "operation", "gegl:convert-format", "format", format, NULL);

  buffer_sink = gegl_node_new_child (graph, "operation", "gegl:buffer-sink", "buffer", &buffer, NULL);

  gegl_node_link_many (tail, convert_format, buffer_sink, NULL);
  gegl_node_process (buffer_sink);

  fixture->buffer = g_object_ref (buffer);
  photos_test_gegl_buffer_save_to_file (fixture->buffer, fixture->source);

  fixture->format = gegl_buffer_get_format (fixture->buffer);
  g_assert_true (fixture->format == format);

  g_object_unref (checkerboard_color1);
  g_object_unref (checkerboard_color2);
}


static void
photos_test_gegl_buffer_teardown (PhotosTestGeglBufferFixture *fixture, gconstpointer user_data)
{
  g_clear_object (&fixture->res);

  g_file_delete (fixture->destination, NULL, NULL);
  g_object_unref (fixture->destination);

  g_file_delete (fixture->source, NULL, NULL);
  g_object_unref (fixture->source);

  g_main_context_pop_thread_default (fixture->context);
  g_main_context_unref (fixture->context);
  g_main_loop_unref (fixture->loop);

  g_object_unref (fixture->buffer);
}


static void
photos_test_gegl_buffer_async (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosTestGeglBufferFixture *fixture = (PhotosTestGeglBufferFixture *) user_data;

  g_assert_null (fixture->res);
  fixture->res = g_object_ref (res);
  g_main_loop_quit (fixture->loop);
}


static void
photos_test_gegl_buffer_loader_builder (void)
{
  GFile *file_loader;
  g_autoptr (GFile) file_builder = NULL;
  GeglBuffer *buffer;
  g_autoptr (PhotosGeglBufferLoader) loader = NULL;
  g_autoptr (PhotosGeglBufferLoaderBuilder) builder = NULL;
  gint height;
  gint width;

  builder = photos_gegl_buffer_loader_builder_new ();

  file_builder = g_file_new_for_uri ("resource:///org/gnome/Photos/gegl/vignette.png");
  photos_gegl_buffer_loader_builder_set_file (builder, file_builder);

  photos_gegl_buffer_loader_builder_set_height (builder, 200);
  photos_gegl_buffer_loader_builder_set_keep_aspect_ratio (builder, FALSE);
  photos_gegl_buffer_loader_builder_set_width (builder, 300);

  loader = photos_gegl_buffer_loader_builder_to_loader (builder);
  g_assert_true (PHOTOS_IS_GEGL_BUFFER_LOADER (loader));

  buffer = photos_gegl_buffer_loader_get_buffer (loader);
  g_assert_null (buffer);

  file_loader = photos_gegl_buffer_loader_get_file (loader);
  g_assert_true (file_loader == file_builder);

  height = photos_gegl_buffer_loader_get_height (loader);
  g_assert_cmpint (height, ==, 200);

  g_assert_false (photos_gegl_buffer_loader_get_keep_aspect_ratio (loader));

  width = photos_gegl_buffer_loader_get_width (loader);
  g_assert_cmpint (width, ==, 300);
}


static void
photos_test_gegl_buffer_loader_builder_defaults (void)
{
  GFile *file = NULL;
  GeglBuffer *buffer;
  g_autoptr (PhotosGeglBufferLoader) loader = NULL;
  g_autoptr (PhotosGeglBufferLoaderBuilder) builder = NULL;
  gint height;
  gint width;

  builder = photos_gegl_buffer_loader_builder_new ();
  loader = photos_gegl_buffer_loader_builder_to_loader (builder);
  g_assert_true (PHOTOS_IS_GEGL_BUFFER_LOADER (loader));

  buffer = photos_gegl_buffer_loader_get_buffer (loader);
  g_assert_null (buffer);

  file = photos_gegl_buffer_loader_get_file (loader);
  g_assert_null (file);

  height = photos_gegl_buffer_loader_get_height (loader);
  g_assert_cmpint (height, ==, -1);

  g_assert_true (photos_gegl_buffer_loader_get_keep_aspect_ratio (loader));

  width = photos_gegl_buffer_loader_get_width (loader);
  g_assert_cmpint (width, ==, -1);
}


static void
photos_test_gegl_buffer_new_from_file_0 (PhotosTestGeglBufferFixture *fixture, gconstpointer user_data)
{
  g_assert_not_reached ();
}


gint
main (gint argc, gchar *argv[])
{
  gint exit_status;

  setlocale (LC_ALL, "");
  g_setenv ("GEGL_THREADS", "1", FALSE);
  g_test_init (&argc, &argv, NULL);
  photos_debug_init ();
  photos_gegl_init ();
  photos_gegl_ensure_builtins ();

  g_test_add_func ("/gegl/buffer/loader-builder", photos_test_gegl_buffer_loader_builder);
  g_test_add_func ("/gegl/buffer/loader-builder-defaults", photos_test_gegl_buffer_loader_builder_defaults);

  g_test_add ("/gegl/buffer/new/from-file-0",
              PhotosTestGeglBufferFixture,
              NULL,
              photos_test_gegl_buffer_setup,
              photos_test_gegl_buffer_new_from_file_0,
              photos_test_gegl_buffer_teardown);

  exit_status = g_test_run ();

  gegl_exit ();
  return exit_status;
}
