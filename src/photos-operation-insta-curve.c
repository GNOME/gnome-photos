/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2015 – 2021 Red Hat, Inc.
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

#include <math.h>

#include <babl/babl.h>
#include <gegl.h>

#include "photos-enums-gegl.h"
#include "photos-operation-insta-common.h"
#include "photos-operation-insta-curve.h"


typedef void (*PhotosOperationProcessFunc) (GeglOperation *, void *, void *, glong, const GeglRectangle *, gint);

struct _PhotosOperationInstaCurve
{
  GeglOperationPointFilter parent_instance;
  PhotosOperationInstaPreset preset;
  PhotosOperationProcessFunc process;
};

enum
{
  PROP_0,
  PROP_PRESET
};


G_DEFINE_TYPE (PhotosOperationInstaCurve, photos_operation_insta_curve, GEGL_TYPE_OPERATION_POINT_FILTER);


static const guint8 NINE_A[] =
{
  0, 1, 3, 4, 6, 7, 9, 10, 12, 13, 14, 16, 17, 19, 20, 22, 23, 25, 26, 28, 29, 31, 32, 34, 35, 37, 38, 39, 41, 42,
  44, 45, 46, 48, 49, 50, 52, 53, 54, 55, 57, 58, 59, 60, 61, 62, 64, 65, 66, 67, 68, 69, 70, 72, 73, 74, 75, 76,
  77, 78, 79, 80, 81, 82, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104,
  105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 125, 126,
  127, 128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 146, 147,
  148, 149, 150, 151, 152, 153, 153, 154, 155, 156, 157, 158, 159, 160, 160, 161, 162, 163, 164, 165, 166, 166, 167,
  168, 169, 170, 171, 172, 172, 173, 174, 175, 176, 177, 178, 178, 179, 180, 181, 182, 183, 183, 184, 185, 186, 187,
  188, 188, 189, 190, 191, 192, 193, 193, 194, 195, 196, 197, 198, 199, 199, 200, 201, 202, 203, 204, 204, 205, 206,
  207, 208, 209, 209, 210, 211, 212, 213, 214, 215, 215, 216, 217, 218, 219, 220, 221, 221, 222, 223, 224, 225, 226,
  227, 227, 228, 229, 230, 231, 232, 233, 233, 234, 235, 236, 237, 238, 239, 240, 241, 241, 242, 243, 244, 245, 246,
  247, 248, 249, 250, 250, 251, 252, 253, 254, 255, 255
};

static const guint8 NINE_R[] =
{
  58, 58, 58, 58, 58, 58, 58, 58, 58, 58, 58, 58, 58, 58, 58, 58, 58, 58, 58, 58, 58, 58, 58, 58, 58, 58, 58, 58,
  58, 58, 58, 58, 59, 60, 60, 61, 62, 62, 63, 63, 64, 64, 65, 66, 66, 67, 67, 68, 69, 69, 70, 70, 71, 72, 72, 73,
  74, 74, 75, 76, 77, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 95, 96, 97, 98, 99, 100,
  102, 103, 104, 105, 106, 108, 109, 110, 111, 112, 113, 114, 116, 117, 118, 119, 120, 121, 122, 123, 125, 126, 127,
  128, 129, 130, 131, 133, 134, 135, 136, 137, 138, 140, 141, 142, 143, 144, 146, 147, 148, 149, 151, 152, 153, 154,
  156, 157, 158, 160, 161, 162, 164, 165, 166, 168, 169, 170, 172, 173, 175, 176, 177, 179, 180, 182, 183, 185, 186,
  188, 189, 191, 192, 193, 194, 196, 197, 198, 199, 200, 201, 202, 203, 204, 204, 205, 206, 206, 207, 208, 208, 209,
  209, 210, 210, 211, 211, 212, 212, 212, 213, 213, 213, 213, 213, 214, 214, 214, 214, 214, 214, 214, 214, 214, 214,
  215, 215, 215, 215, 215, 215, 215, 215, 215, 215, 215, 215, 215, 215, 215, 215, 215, 215, 215, 215, 215, 215, 215,
  215, 215, 215, 215, 215, 215, 215, 215, 215, 215, 215, 215, 215, 214, 214, 214, 214, 214, 214, 214, 214, 214, 214,
  213, 213, 213, 213, 213, 213, 213, 212, 212, 212, 212, 212
};

static const guint8 NINE_G[] =
{
  40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40,
  40, 40, 40, 40, 40, 40, 41, 41, 42, 42, 42, 42, 43, 43, 43, 43, 44, 44, 44, 44, 45, 45, 45, 45, 46, 46, 46, 47,
  47, 48, 48, 48, 49, 49, 50, 50, 51, 52, 52, 53, 54, 54, 55, 56, 57, 58, 59, 60, 60, 61, 62, 63, 64, 65, 66, 67,
  68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95,
  96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118,
  119, 120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 132, 133, 134, 135, 136, 137, 138, 139, 140,
  141, 142, 143, 144, 145, 146, 147, 148, 149, 150, 151, 153, 154, 155, 156, 157, 158, 160, 161, 162, 163, 164, 166,
  167, 168, 169, 171, 172, 173, 174, 175, 176, 178, 179, 180, 181, 182, 183, 185, 186, 187, 188, 189, 190, 191, 192,
  193, 195, 196, 197, 198, 199, 200, 201, 202, 203, 205, 206, 207, 208, 209, 210, 211, 212, 214, 215, 216, 217, 218,
  220, 221, 222, 223, 225, 226, 227, 228, 230, 231, 232, 233, 235, 236, 237, 239, 240, 241, 242, 244, 245, 246, 247,
  249, 250, 251, 252, 254, 255, 255
};

static const guint8 NINE_B[] =
{
  45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45,
  45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 46, 46, 47, 47, 47, 48, 48, 48, 48, 49, 49, 49, 50, 50, 50, 51,
  51, 51, 52, 52, 53, 53, 54, 54, 55, 56, 56, 57, 58, 59, 60, 61, 62, 62, 63, 64, 65, 66, 68, 69, 70, 71, 72, 73,
  74, 75, 76, 77, 79, 80, 81, 82, 83, 84, 85, 86, 87, 89, 90, 91, 92, 93, 94, 96, 97, 98, 99, 100, 102, 103, 104,
  105, 107, 108, 109, 110, 112, 113, 114, 115, 117, 118, 119, 120, 122, 123, 124, 126, 127, 128, 130, 131, 133, 134,
  135, 137, 138, 140, 141, 143, 144, 145, 147, 148, 149, 151, 152, 153, 155, 156, 157, 159, 160, 161, 162, 164, 165,
  166, 167, 168, 170, 171, 172, 173, 174, 175, 176, 177, 178, 179, 180, 181, 182, 184, 185, 186, 187, 188, 188, 189,
  190, 191, 192, 193, 193, 194, 195, 195, 196, 196, 196, 197, 197, 197, 197, 198, 198, 198, 198, 198, 198, 198, 198,
  198, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199,
  199, 199, 199, 199, 198, 198, 198, 198, 198, 198, 197, 197, 197, 197, 197, 197, 197, 197, 197, 197, 197, 197, 197,
  197, 198, 198, 198, 198, 198, 198, 198
};

static const guint8 BRANNAN_A[] =
{
  0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30,
  31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58,
  59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86,
  87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111,
  112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134,
  135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157,
  158, 159, 160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175, 176, 177, 178, 179, 180,
  181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191, 192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203,
  204, 205, 206, 207, 208, 209, 210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220, 221, 222, 223, 224, 225, 226,
  227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239, 240, 241, 242, 243, 244, 245, 246, 247, 248, 249,
  250, 251, 252, 253, 254, 255, 255
};

static const guint8 BRANNAN_R[] =
{
  50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 51, 51, 51, 51, 51, 52, 53, 54, 55, 56, 57, 59, 60, 62, 63,
  64, 66, 67, 68, 69, 70, 71, 71, 72, 73, 73, 74, 75, 75, 76, 76, 77, 77, 78, 78, 79, 79, 80, 80, 81, 81, 82, 83,
  83, 84, 85, 86, 87, 88, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107,
  108, 109, 111, 112, 113, 114, 115, 116, 118, 119, 120, 121, 122, 124, 125, 126, 128, 129, 130, 132, 133, 134, 136,
  137, 139, 140, 141, 143, 144, 146, 147, 149, 150, 152, 153, 154, 156, 157, 159, 160, 162, 163, 164, 166, 167, 169,
  170, 171, 173, 174, 175, 177, 178, 179, 181, 182, 183, 185, 186, 187, 189, 190, 192, 193, 195, 196, 198, 199, 201,
  203, 204, 206, 207, 209, 210, 212, 213, 215, 216, 217, 219, 220, 221, 223, 224, 225, 226, 227, 228, 229, 230, 231,
  232, 233, 234, 235, 236, 236, 237, 238, 239, 239, 240, 241, 241, 242, 243, 243, 244, 244, 245, 246, 246, 247, 247,
  248, 248, 249, 249, 249, 250, 250, 251, 251, 251, 252, 252, 252, 253, 253, 253, 254, 254, 254, 254, 254, 254, 255,
  255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
  255, 255, 255, 255, 255, 255, 255, 255, 254, 254, 254, 254, 254, 254
};

static const guint8 BRANNAN_G[] =
{
  0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 2, 2, 2, 3, 4, 4, 5, 6, 7, 8, 10, 11, 12, 13, 14, 16, 17, 18, 19, 20,
  21, 23, 24, 25, 26, 27, 28, 29, 30, 32, 33, 34, 35, 36, 38, 39, 40, 41, 43, 44, 45, 47, 48, 50, 51, 53, 54, 56,
  57, 59, 61, 62, 64, 66, 68, 70, 72, 74, 76, 78, 80, 82, 84, 87, 89, 91, 93, 95, 97, 100, 102, 104, 106, 108, 110,
  112, 114, 116, 118, 120, 122, 124, 126, 128, 130, 132, 134, 136, 138, 140, 142, 144, 146, 148, 150, 152, 154, 156,
  158, 160, 161, 163, 165, 167, 168, 170, 172, 173, 175, 176, 178, 179, 181, 182, 183, 184, 186, 187, 188, 189, 190,
  191, 192, 193, 193, 194, 195, 196, 196, 197, 198, 198, 199, 200, 200, 201, 202, 202, 203, 203, 204, 204, 205, 205,
  206, 207, 207, 208, 208, 209, 210, 210, 211, 212, 212, 213, 214, 214, 215, 216, 217, 217, 218, 219, 219, 220, 221,
  221, 222, 222, 223, 224, 224, 225, 225, 226, 226, 227, 228, 228, 229, 229, 229, 230, 230, 231, 231, 232, 232, 233,
  233, 233, 234, 234, 234, 235, 235, 236, 236, 236, 237, 237, 237, 238, 238, 239, 239, 239, 240, 240, 240, 241, 241,
  241, 242, 242, 242, 243, 243, 243, 244, 244, 244, 245, 245, 245, 246, 246, 247, 247, 247, 248, 248, 249, 249, 250,
  250, 251, 251, 252, 252, 252
};

static const guint8 BRANNAN_B[] =
{
  48, 48, 48, 48, 48, 48, 48, 48, 49, 49, 49, 49, 49, 49, 49, 50, 50, 50, 51, 51, 51, 52, 52, 53, 53, 54, 54, 54,
  55, 55, 56, 56, 57, 57, 58, 58, 59, 60, 60, 61, 61, 62, 62, 63, 64, 64, 65, 66, 66, 67, 68, 68, 69, 70, 71, 71,
  72, 73, 74, 75, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 92, 93, 94, 95, 96, 98, 99, 100,
  101, 102, 103, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124,
  125, 126, 127, 128, 129, 130, 131, 132, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 141, 142, 143, 144, 145,
  146, 146, 147, 148, 148, 149, 150, 151, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160, 161, 162, 163, 164, 165,
  166, 168, 169, 170, 171, 172, 173, 174, 175, 176, 177, 178, 178, 179, 180, 181, 181, 182, 183, 183, 184, 184, 185,
  185, 185, 186, 186, 187, 187, 187, 188, 188, 188, 189, 189, 190, 190, 191, 191, 192, 193, 193, 194, 195, 195, 196,
  197, 198, 199, 200, 200, 201, 202, 203, 204, 205, 206, 206, 207, 208, 209, 210, 211, 211, 212, 213, 214, 214, 215,
  216, 216, 217, 218, 218, 219, 219, 220, 220, 221, 222, 222, 222, 223, 223, 224, 224, 224, 225, 225, 225, 225, 225,
  225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225
};

static const guint8 GOTHAM_A[] =
{
  0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30,
  31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58,
  59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86,
  87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111,
  112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134,
  135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157,
  158, 159, 160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175, 176, 177, 178, 179, 180,
  181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191, 192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203,
  204, 205, 206, 207, 208, 209, 210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220, 221, 222, 223, 224, 225, 226,
  227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239, 240, 241, 242, 243, 244, 245, 246, 247, 248, 249,
  250, 251, 252, 253, 254, 255, 255
};

static const guint8 GOTHAM_R[] =
{
  0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 3, 3, 3, 4, 4, 4, 5, 5, 5, 6, 6, 7, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11,
  12, 12, 12, 13, 13, 14, 14, 15, 15, 16, 16, 17, 17, 18, 18, 19, 19, 20, 20, 21, 21, 22, 22, 23, 23, 24, 25, 25,
  25, 26, 26, 27, 27, 28, 28, 29, 29, 30, 30, 31, 32, 32, 33, 33, 34, 34, 35, 35, 36, 36, 37, 38, 38, 39, 39, 40,
  41, 41, 42, 42, 43, 44, 44, 45, 45, 46, 47, 47, 48, 48, 49, 50, 50, 51, 52, 52, 53, 54, 54, 55, 56, 57, 58, 58,
  59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 70, 71, 73, 74, 76, 77, 79, 80, 82, 83, 84, 86, 87, 88, 90, 91, 92, 94,
  95, 96, 98, 99, 100, 102, 103, 104, 106, 107, 109, 110, 111, 113, 114, 116, 118, 119, 121, 123, 124, 126, 128,
  129, 131, 133, 135, 137, 138, 140, 142, 144, 146, 148, 149, 151, 153, 155, 156, 158, 160, 162, 163, 165, 167, 168,
  170, 172, 173, 175, 176, 178, 180, 181, 183, 184, 186, 187, 189, 191, 192, 194, 195, 197, 198, 200, 201, 203, 204,
  205, 207, 208, 210, 211, 213, 214, 215, 217, 218, 219, 221, 222, 223, 224, 225, 227, 228, 229, 230, 231, 233, 234,
  235, 237, 238, 239, 241, 242, 244, 245, 246, 248, 249, 251, 252, 254, 255, 255
};

static const guint8 GOTHAM_G[] =
{
  0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 6, 6, 6, 7, 7, 7, 8, 8,
  8, 9, 9, 9, 10, 10, 10, 11, 11, 12, 12, 12, 13, 13, 14, 14, 14, 15, 15, 16, 16, 17, 17, 18, 18, 19, 19, 20, 20,
  21, 21, 22, 22, 23, 23, 24, 24, 25, 26, 26, 27, 28, 28, 29, 30, 30, 31, 32, 32, 33, 34, 35, 35, 36, 37, 38, 39,
  39, 40, 41, 42, 43, 44, 45, 46, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65,
  67, 68, 69, 70, 71, 72, 73, 74, 75, 77, 78, 79, 81, 82, 83, 84, 86, 87, 88, 90, 91, 92, 93, 95, 96, 97, 99, 100,
  101, 102, 104, 105, 106, 108, 109, 110, 112, 113, 115, 116, 118, 119, 121, 122, 124, 125, 127, 128, 130, 132, 133,
  135, 137, 138, 140, 142, 143, 145, 147, 149, 150, 152, 154, 155, 157, 159, 160, 162, 163, 165, 167, 168, 170, 171,
  173, 174, 176, 177, 179, 180, 182, 183, 185, 186, 188, 189, 191, 192, 194, 195, 196, 198, 199, 201, 202, 204, 205,
  206, 208, 209, 211, 212, 213, 215, 216, 217, 219, 220, 221, 223, 224, 225, 226, 228, 229, 230, 232, 233, 234, 236,
  237, 239, 240, 242, 243, 245, 246, 248, 249, 251, 252, 254, 255, 255
};

static const guint8 GOTHAM_B[] =
{
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 2, 2, 2, 3, 3, 4, 4, 4, 5, 5, 6,
  6, 7, 7, 8, 9, 9, 10, 10, 11, 12, 12, 13, 13, 14, 15, 15, 16, 17, 17, 18, 19, 20, 20, 21, 22, 22, 23, 24, 24, 25,
  26, 27, 27, 28, 29, 30, 31, 32, 33, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51,
  52, 53, 54, 55, 56, 58, 59, 60, 61, 62, 63, 64, 66, 67, 68, 69, 70, 71, 73, 74, 75, 76, 77, 78, 79, 81, 82, 83,
  84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108,
  109, 110, 111, 112, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 131,
  132, 133, 134, 135, 136, 137, 138, 139, 140, 140, 141, 142, 143, 144, 145, 146, 148, 149, 150, 151, 152, 153, 155,
  156, 157, 158, 160, 161, 162, 164, 165, 166, 168, 169, 171, 172, 173, 175, 176, 178, 179, 181, 182, 184, 186, 187,
  189, 190, 192, 194, 195, 197, 199, 201, 202, 204, 206, 208, 210, 212, 214, 216, 218, 220, 222, 224, 226, 228, 230,
  232, 234, 235, 237, 239, 241, 243, 244, 246, 248, 250, 251, 253, 255, 255
};

static const guint8 NASHVILLE_A[] =
{
  0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30,
  31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58,
  59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86,
  87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111,
  112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134,
  135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157,
  158, 159, 160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175, 176, 177, 178, 179, 180,
  181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191, 192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203,
  204, 205, 206, 207, 208, 209, 210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220, 221, 222, 223, 224, 225, 226,
  227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239, 240, 241, 242, 243, 244, 245, 246, 247, 248, 249,
  250, 251, 252, 253, 254, 255, 255
};

static const guint8 NASHVILLE_R[] =
{
  56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56,
  56, 56, 56, 56, 56, 57, 57, 58, 58, 59, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 71, 72, 73, 75, 76, 78, 79,
  81, 82, 84, 85, 87, 88, 90, 91, 93, 95, 96, 98, 100, 102, 104, 106, 108, 110, 113, 115, 117, 120, 122, 124, 127,
  129, 131, 133, 136, 138, 140, 142, 144, 146, 148, 150, 152, 154, 155, 157, 159, 160, 162, 164, 165, 167, 168, 170,
  171, 173, 174, 175, 177, 178, 179, 181, 182, 183, 185, 186, 187, 189, 190, 191, 192, 194, 195, 196, 197, 198, 200,
  201, 202, 203, 204, 205, 206, 208, 209, 209, 210, 211, 212, 213, 214, 215, 216, 217, 217, 218, 219, 220, 220, 221,
  222, 223, 223, 224, 225, 226, 226, 227, 228, 228, 229, 230, 230, 231, 231, 232, 233, 233, 234, 234, 235, 235, 236,
  237, 237, 238, 238, 239, 239, 240, 240, 240, 241, 241, 242, 242, 243, 243, 243, 244, 244, 245, 245, 245, 246, 246,
  246, 247, 247, 247, 248, 248, 248, 248, 249, 249, 249, 249, 250, 250, 250, 250, 251, 251, 251, 251, 251, 252, 252,
  252, 252, 252, 253, 253, 253, 253, 253, 254, 254, 254, 254, 254, 254, 255, 255, 255, 255, 255, 255, 255, 255, 255,
  255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255
};

static const guint8 NASHVILLE_G[] =
{
  38, 39, 39, 40, 41, 41, 42, 42, 43, 44, 44, 45, 46, 46, 47, 48, 49, 50, 51, 52, 53, 55, 56, 57, 59, 60, 61, 63,
  64, 65, 67, 68, 69, 71, 72, 73, 74, 76, 77, 78, 80, 81, 82, 84, 85, 86, 87, 89, 90, 91, 93, 94, 95, 97, 98, 99,
  101, 102, 103, 104, 106, 107, 108, 110, 111, 112, 114, 115, 116, 118, 119, 121, 122, 123, 125, 126, 128, 129, 130,
  132, 133, 134, 136, 137, 138, 140, 141, 142, 143, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157,
  158, 158, 159, 160, 161, 162, 163, 163, 164, 165, 166, 166, 167, 168, 169, 169, 170, 171, 172, 172, 173, 174, 175,
  176, 176, 177, 178, 179, 180, 181, 181, 182, 183, 184, 185, 186, 187, 187, 188, 189, 189, 190, 191, 191, 192, 193,
  193, 194, 194, 195, 195, 196, 197, 197, 198, 198, 199, 199, 200, 200, 201, 201, 202, 202, 202, 203, 203, 204, 204,
  205, 205, 205, 206, 206, 207, 207, 207, 208, 208, 208, 209, 209, 209, 210, 210, 210, 211, 211, 211, 212, 212, 212,
  213, 213, 213, 213, 214, 214, 214, 214, 215, 215, 215, 215, 216, 216, 216, 216, 216, 217, 217, 217, 217, 217, 218,
  218, 218, 218, 218, 218, 219, 219, 219, 219, 219, 220, 220, 220, 220, 220, 220, 220, 221, 221, 221, 221, 221, 221,
  221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221
};

static const guint8 NASHVILLE_B[] =
{
  97, 98, 98, 99, 99, 100, 100, 101, 101, 102, 102, 103, 104, 104, 105, 105, 106, 107, 107, 108, 109, 110, 110, 111,
  112, 113, 114, 114, 115, 116, 116, 117, 118, 118, 119, 119, 120, 120, 121, 121, 122, 122, 123, 123, 124, 124, 124,
  125, 125, 126, 126, 127, 127, 127, 128, 128, 129, 129, 129, 130, 130, 131, 131, 132, 132, 132, 133, 133, 134, 134,
  135, 135, 136, 136, 136, 137, 137, 138, 138, 139, 139, 139, 140, 140, 141, 141, 142, 142, 142, 143, 143, 144, 144,
  144, 145, 145, 146, 146, 147, 147, 147, 148, 148, 149, 149, 150, 150, 151, 151, 151, 152, 152, 153, 153, 154, 154,
  154, 155, 155, 155, 156, 156, 156, 157, 157, 157, 158, 158, 158, 158, 158, 158, 159, 159, 159, 159, 159, 159, 159,
  159, 159, 159, 159, 160, 160, 160, 160, 160, 161, 161, 161, 162, 162, 162, 162, 163, 163, 163, 163, 164, 164, 164,
  164, 165, 165, 165, 165, 165, 165, 166, 166, 166, 166, 166, 166, 166, 166, 166, 166, 166, 166, 166, 166, 166, 166,
  166, 166, 167, 167, 167, 167, 167, 167, 167, 167, 167, 168, 168, 168, 168, 168, 168, 169, 169, 169, 169, 169, 170,
  170, 170, 170, 171, 171, 171, 171, 171, 172, 172, 172, 172, 172, 173, 173, 173, 173, 173, 173, 173, 174, 174, 174,
  174, 174, 174, 174, 174, 175, 175, 175, 175, 175, 175, 175, 175, 175, 175, 175, 176, 176, 176, 176, 176, 176, 176,
  176, 176, 176
};


static gfloat
photos_operation_insta_curve_interpolate (gfloat input, const guint8 *curve1, const guint8 *curve2)
{
  gfloat output;
  gfloat x;
  gfloat x1;
  gfloat x2;
  gfloat y;
  gfloat y1;
  gfloat y2;

  x = input * G_MAXUINT8;
  x1 = ceilf (x);
  x2 = floorf (x);
  y1 = (gfloat) curve1[(guint8) x1];

  if (GEGL_FLOAT_EQUAL (x1, x2))
    y = y1;
  else
    {
      y2 = (gfloat) curve1[(guint8) x2];
      y = y2 + (y1 - y2) * (x - x2) / (x1 - x2);
    }

  x = y;
  y = (gfloat) curve2[(guint8) x];

  output = y / G_MAXUINT8;
  return output;
}


static void
photos_operation_insta_curve_1977_process_alpha_float (GeglOperation *operation,
                                                       void *in_buf,
                                                       void *out_buf,
                                                       glong n_pixels,
                                                       const GeglRectangle *roi,
                                                       gint level)
{
  gfloat *in = in_buf;
  gfloat *out = out_buf;
  glong i;

  for (i = 0; i < n_pixels; i++)
    {
      out[0] = photos_operation_insta_curve_interpolate (in[0], NINE_R, NINE_A);
      out[1] = photos_operation_insta_curve_interpolate (in[1], NINE_G, NINE_A);
      out[2] = photos_operation_insta_curve_interpolate (in[2], NINE_B, NINE_A);
      out[3] = in[3];

      in += 4;
      out += 4;
    }
}


static void
photos_operation_insta_curve_1977_process_alpha_u8 (GeglOperation *operation,
                                                    void *in_buf,
                                                    void *out_buf,
                                                    glong n_pixels,
                                                    const GeglRectangle *roi,
                                                    gint level)
{
  guint8 *in = in_buf;
  guint8 *out = out_buf;
  glong i;

  for (i = 0; i < n_pixels; i++)
    {
      out[0] = NINE_R[in[0]];
      out[1] = NINE_G[in[1]];
      out[2] = NINE_B[in[2]];

      out[0] = NINE_A[out[0]];
      out[1] = NINE_A[out[1]];
      out[2] = NINE_A[out[2]];

      out[3] = in[3];

      in += 4;
      out += 4;
    }
}


static void
photos_operation_insta_curve_1977_process_float (GeglOperation *operation,
                                                 void *in_buf,
                                                 void *out_buf,
                                                 glong n_pixels,
                                                 const GeglRectangle *roi,
                                                 gint level)
{
  gfloat *in = in_buf;
  gfloat *out = out_buf;
  glong i;

  for (i = 0; i < n_pixels; i++)
    {
      out[0] = photos_operation_insta_curve_interpolate (in[0], NINE_R, NINE_A);
      out[1] = photos_operation_insta_curve_interpolate (in[1], NINE_G, NINE_A);
      out[2] = photos_operation_insta_curve_interpolate (in[2], NINE_B, NINE_A);

      in += 3;
      out += 3;
    }
}


static void
photos_operation_insta_curve_1977_process_u8 (GeglOperation *operation,
                                              void *in_buf,
                                              void *out_buf,
                                              glong n_pixels,
                                              const GeglRectangle *roi,
                                              gint level)
{
  guint8 *in = in_buf;
  guint8 *out = out_buf;
  glong i;

  for (i = 0; i < n_pixels; i++)
    {
      out[0] = NINE_R[in[0]];
      out[1] = NINE_G[in[1]];
      out[2] = NINE_B[in[2]];

      out[0] = NINE_A[out[0]];
      out[1] = NINE_A[out[1]];
      out[2] = NINE_A[out[2]];

      in += 3;
      out += 3;
    }
}


static void
photos_operation_insta_curve_brannan_process_alpha_float (GeglOperation *operation,
                                                          void *in_buf,
                                                          void *out_buf,
                                                          glong n_pixels,
                                                          const GeglRectangle *roi,
                                                          gint level)
{
  gfloat *in = in_buf;
  gfloat *out = out_buf;
  glong i;

  for (i = 0; i < n_pixels; i++)
    {
      const gfloat saturation = 0.1f;
      guint max;

      out[0] = photos_operation_insta_curve_interpolate (in[0], BRANNAN_R, BRANNAN_A);
      out[1] = photos_operation_insta_curve_interpolate (in[1], BRANNAN_G, BRANNAN_A);
      out[2] = photos_operation_insta_curve_interpolate (in[2], BRANNAN_B, BRANNAN_A);

      max = (out[0] > out[1]) ? 0 : 1;
      max = (out[max] > out[2]) ? max : 2;

      if (max != 0)
        out[0] += (guint8) ((out[max] - out[0]) * saturation + 0.5f);

      if (max != 1)
        out[1] += (guint8) ((out[max] - out[1]) * saturation + 0.5f);

      if (max != 2)
        out[2] += (guint8) ((out[max] - out[2]) * saturation + 0.5f);

      out[3] = in[3];

      in += 4;
      out += 4;
    }
}


static void
photos_operation_insta_curve_brannan_process_alpha_u8 (GeglOperation *operation,
                                                       void *in_buf,
                                                       void *out_buf,
                                                       glong n_pixels,
                                                       const GeglRectangle *roi,
                                                       gint level)
{
  guint8 *in = in_buf;
  guint8 *out = out_buf;
  glong i;

  for (i = 0; i < n_pixels; i++)
    {
      const gfloat saturation = 0.1f;
      guint max;

      out[0] = BRANNAN_R[in[0]];
      out[1] = BRANNAN_G[in[1]];
      out[2] = BRANNAN_B[in[2]];

      out[0] = BRANNAN_A[out[0]];
      out[1] = BRANNAN_A[out[1]];
      out[2] = BRANNAN_A[out[2]];

      max = (out[0] > out[1]) ? 0 : 1;
      max = (out[max] > out[2]) ? max : 2;

      if (max != 0)
        out[0] += (guint8) ((out[max] - out[0]) * saturation + 0.5f);

      if (max != 1)
        out[1] += (guint8) ((out[max] - out[1]) * saturation + 0.5f);

      if (max != 2)
        out[2] += (guint8) ((out[max] - out[2]) * saturation + 0.5f);

      out[3] = in[3];

      in += 4;
      out += 4;
    }
}


static void
photos_operation_insta_curve_brannan_process_float (GeglOperation *operation,
                                                    void *in_buf,
                                                    void *out_buf,
                                                    glong n_pixels,
                                                    const GeglRectangle *roi,
                                                    gint level)
{
  gfloat *in = in_buf;
  gfloat *out = out_buf;
  glong i;

  for (i = 0; i < n_pixels; i++)
    {
      const gfloat saturation = 0.1f;
      guint max;

      out[0] = photos_operation_insta_curve_interpolate (in[0], BRANNAN_R, BRANNAN_A);
      out[1] = photos_operation_insta_curve_interpolate (in[1], BRANNAN_G, BRANNAN_A);
      out[2] = photos_operation_insta_curve_interpolate (in[2], BRANNAN_B, BRANNAN_A);

      max = (out[0] > out[1]) ? 0 : 1;
      max = (out[max] > out[2]) ? max : 2;

      if (max != 0)
        out[0] += (guint8) ((out[max] - out[0]) * saturation + 0.5f);

      if (max != 1)
        out[1] += (guint8) ((out[max] - out[1]) * saturation + 0.5f);

      if (max != 2)
        out[2] += (guint8) ((out[max] - out[2]) * saturation + 0.5f);

      in += 3;
      out += 3;
    }
}


static void
photos_operation_insta_curve_brannan_process_u8 (GeglOperation *operation,
                                                 void *in_buf,
                                                 void *out_buf,
                                                 glong n_pixels,
                                                 const GeglRectangle *roi,
                                                 gint level)
{
  guint8 *in = in_buf;
  guint8 *out = out_buf;
  glong i;

  for (i = 0; i < n_pixels; i++)
    {
      const gfloat saturation = 0.1f;
      guint max;

      out[0] = BRANNAN_R[in[0]];
      out[1] = BRANNAN_G[in[1]];
      out[2] = BRANNAN_B[in[2]];

      out[0] = BRANNAN_A[out[0]];
      out[1] = BRANNAN_A[out[1]];
      out[2] = BRANNAN_A[out[2]];

      max = (out[0] > out[1]) ? 0 : 1;
      max = (out[max] > out[2]) ? max : 2;

      if (max != 0)
        out[0] += (guint8) ((out[max] - out[0]) * saturation + 0.5f);

      if (max != 1)
        out[1] += (guint8) ((out[max] - out[1]) * saturation + 0.5f);

      if (max != 2)
        out[2] += (guint8) ((out[max] - out[2]) * saturation + 0.5f);

      in += 3;
      out += 3;
    }
}


static void
photos_operation_insta_curve_gotham_process_alpha_float (GeglOperation *operation,
                                                         void *in_buf,
                                                         void *out_buf,
                                                         glong n_pixels,
                                                         const GeglRectangle *roi,
                                                         gint level)
{
  gfloat *in = in_buf;
  gfloat *out = out_buf;
  glong i;

  for (i = 0; i < n_pixels; i++)
    {
      out[0] = photos_operation_insta_curve_interpolate (in[0], GOTHAM_R, GOTHAM_A);
      out[1] = photos_operation_insta_curve_interpolate (in[1], GOTHAM_G, GOTHAM_A);
      out[2] = photos_operation_insta_curve_interpolate (in[2], GOTHAM_B, GOTHAM_A);
      out[3] = in[3];

      in += 4;
      out += 4;
    }
}


static void
photos_operation_insta_curve_gotham_process_alpha_u8 (GeglOperation *operation,
                                                      void *in_buf,
                                                      void *out_buf,
                                                      glong n_pixels,
                                                      const GeglRectangle *roi,
                                                      gint level)
{
  guint8 *in = in_buf;
  guint8 *out = out_buf;
  glong i;

  for (i = 0; i < n_pixels; i++)
    {
      out[0] = GOTHAM_R[in[0]];
      out[1] = GOTHAM_G[in[1]];
      out[2] = GOTHAM_B[in[2]];

      out[0] = GOTHAM_A[out[0]];
      out[1] = GOTHAM_A[out[1]];
      out[2] = GOTHAM_A[out[2]];

      out[3] = in[3];

      in += 4;
      out += 4;
    }
}


static void
photos_operation_insta_curve_gotham_process_float (GeglOperation *operation,
                                                   void *in_buf,
                                                   void *out_buf,
                                                   glong n_pixels,
                                                   const GeglRectangle *roi,
                                                   gint level)
{
  gfloat *in = in_buf;
  gfloat *out = out_buf;
  glong i;

  for (i = 0; i < n_pixels; i++)
    {
      out[0] = photos_operation_insta_curve_interpolate (in[0], GOTHAM_R, GOTHAM_A);
      out[1] = photos_operation_insta_curve_interpolate (in[1], GOTHAM_G, GOTHAM_A);
      out[2] = photos_operation_insta_curve_interpolate (in[2], GOTHAM_B, GOTHAM_A);

      in += 3;
      out += 3;
    }
}


static void
photos_operation_insta_curve_gotham_process_u8 (GeglOperation *operation,
                                                void *in_buf,
                                                void *out_buf,
                                                glong n_pixels,
                                                const GeglRectangle *roi,
                                                gint level)
{
  guint8 *in = in_buf;
  guint8 *out = out_buf;
  glong i;

  for (i = 0; i < n_pixels; i++)
    {
      out[0] = GOTHAM_R[in[0]];
      out[1] = GOTHAM_G[in[1]];
      out[2] = GOTHAM_B[in[2]];

      out[0] = GOTHAM_A[out[0]];
      out[1] = GOTHAM_A[out[1]];
      out[2] = GOTHAM_A[out[2]];

      in += 3;
      out += 3;
    }
}


static void
photos_operation_insta_curve_nashville_process_alpha_float (GeglOperation *operation,
                                                            void *in_buf,
                                                            void *out_buf,
                                                            glong n_pixels,
                                                            const GeglRectangle *roi,
                                                            gint level)
{
  gfloat *in = in_buf;
  gfloat *out = out_buf;
  glong i;

  for (i = 0; i < n_pixels; i++)
    {
      const gfloat brightness = -0.05f;
      const gfloat contrast = 1.1f;

      out[0] = (in[0] - 0.5f) * contrast + brightness + 0.5f;
      out[1] = (in[1] - 0.5f) * contrast + brightness + 0.5f;
      out[2] = (in[2] - 0.5f) * contrast + brightness + 0.5f;

      out[0] = photos_operation_insta_curve_interpolate (out[0], NASHVILLE_R, NASHVILLE_A);
      out[1] = photos_operation_insta_curve_interpolate (out[1], NASHVILLE_G, NASHVILLE_A);
      out[2] = photos_operation_insta_curve_interpolate (out[2], NASHVILLE_B, NASHVILLE_A);

      out[3] = in[3];

      in += 4;
      out += 4;
    }
}


static void
photos_operation_insta_curve_nashville_process_alpha_u8 (GeglOperation *operation,
                                                         void *in_buf,
                                                         void *out_buf,
                                                         glong n_pixels,
                                                         const GeglRectangle *roi,
                                                         gint level)
{
  guint8 *in = in_buf;
  guint8 *out = out_buf;
  glong i;

  for (i = 0; i < n_pixels; i++)
    {
      const gfloat brightness = -0.05f;
      const gfloat contrast = 1.1f;
      gfloat channel;

      channel = in[0] / 255.0f;
      channel = (channel - 0.5f) * contrast + brightness + 0.5f;
      channel = CLAMP (channel, 0.0f, 1.0f);
      out[0] = (guint8) (channel * 255.0f);

      channel = in[1] / 255.0f;
      channel = (channel - 0.5f) * contrast + brightness + 0.5f;
      channel = CLAMP (channel, 0.0f, 1.0f);
      out[1] = (guint8) (channel * 255.0f);

      channel = in[2] / 255.0f;
      channel = (channel - 0.5f) * contrast + brightness + 0.5f;
      channel = CLAMP (channel, 0.0f, 1.0f);
      out[2] = (guint8) (channel * 255.0f);

      out[0] = NASHVILLE_R[out[0]];
      out[1] = NASHVILLE_G[out[1]];
      out[2] = NASHVILLE_B[out[2]];

      out[0] = NASHVILLE_A[out[0]];
      out[1] = NASHVILLE_A[out[1]];
      out[2] = NASHVILLE_A[out[2]];

      out[3] = in[3];

      in += 4;
      out += 4;
    }
}


static void
photos_operation_insta_curve_nashville_process_float (GeglOperation *operation,
                                                      void *in_buf,
                                                      void *out_buf,
                                                      glong n_pixels,
                                                      const GeglRectangle *roi,
                                                      gint level)
{
  gfloat *in = in_buf;
  gfloat *out = out_buf;
  glong i;

  for (i = 0; i < n_pixels; i++)
    {
      const gfloat brightness = -0.05f;
      const gfloat contrast = 1.1f;

      out[0] = (in[0] - 0.5f) * contrast + brightness + 0.5f;
      out[1] = (in[1] - 0.5f) * contrast + brightness + 0.5f;
      out[2] = (in[2] - 0.5f) * contrast + brightness + 0.5f;

      out[0] = photos_operation_insta_curve_interpolate (out[0], NASHVILLE_R, NASHVILLE_A);
      out[1] = photos_operation_insta_curve_interpolate (out[1], NASHVILLE_G, NASHVILLE_A);
      out[2] = photos_operation_insta_curve_interpolate (out[2], NASHVILLE_B, NASHVILLE_A);

      in += 3;
      out += 3;
    }
}


static void
photos_operation_insta_curve_nashville_process_u8 (GeglOperation *operation,
                                                   void *in_buf,
                                                   void *out_buf,
                                                   glong n_pixels,
                                                   const GeglRectangle *roi,
                                                   gint level)
{
  guint8 *in = in_buf;
  guint8 *out = out_buf;
  glong i;

  for (i = 0; i < n_pixels; i++)
    {
      const gfloat brightness = -0.05f;
      const gfloat contrast = 1.1f;
      gfloat channel;

      channel = in[0] / 255.0f;
      channel = (channel - 0.5f) * contrast + brightness + 0.5f;
      channel = CLAMP (channel, 0.0f, 1.0f);
      out[0] = (guint8) (channel * 255.0f);

      channel = in[1] / 255.0f;
      channel = (channel - 0.5f) * contrast + brightness + 0.5f;
      channel = CLAMP (channel, 0.0f, 1.0f);
      out[1] = (guint8) (channel * 255.0f);

      channel = in[2] / 255.0f;
      channel = (channel - 0.5f) * contrast + brightness + 0.5f;
      channel = CLAMP (channel, 0.0f, 1.0f);
      out[2] = (guint8) (channel * 255.0f);

      out[0] = NASHVILLE_R[out[0]];
      out[1] = NASHVILLE_G[out[1]];
      out[2] = NASHVILLE_B[out[2]];

      out[0] = NASHVILLE_A[out[0]];
      out[1] = NASHVILLE_A[out[1]];
      out[2] = NASHVILLE_A[out[2]];

      in += 3;
      out += 3;
    }
}


static void
photos_operation_insta_curve_prepare (GeglOperation *operation)
{
  PhotosOperationInstaCurve *self = PHOTOS_OPERATION_INSTA_CURVE (operation);
  const Babl *format;
  const Babl *format_alpha_float;
  const Babl *format_alpha_u8;
  const Babl *format_float;
  const Babl *format_u8;
  const Babl *input_format;
  const Babl *type;
  const Babl *type_u8;
  gboolean has_alpha;

  input_format = gegl_operation_get_source_format (operation, "input");
  if (input_format == NULL)
    {
      has_alpha = TRUE;
      type = babl_type ("float");
    }
  else
    {
      has_alpha = babl_format_has_alpha (input_format);
      type = babl_format_get_type (input_format, 0);
    }

  format_alpha_float = babl_format ("R'G'B'A float");
  format_alpha_u8 = babl_format ("R'G'B'A u8");
  format_float = babl_format ("R'G'B' float");
  format_u8 = babl_format ("R'G'B' u8");
  type_u8 = babl_type ("u8");

  switch (self->preset)
    {
    case PHOTOS_OPERATION_INSTA_PRESET_1977:
      if (has_alpha)
        {
          if (type == type_u8)
            {
              format = format_alpha_u8;
              self->process = photos_operation_insta_curve_1977_process_alpha_u8;
            }
          else
            {
              format = format_alpha_float;
              self->process = photos_operation_insta_curve_1977_process_alpha_float;
            }
        }
      else
        {
          if (type == type_u8)
            {
              format = format_u8;
              self->process = photos_operation_insta_curve_1977_process_u8;
            }
          else
            {
              format = format_float;
              self->process = photos_operation_insta_curve_1977_process_float;
            }
        }
      break;

    case PHOTOS_OPERATION_INSTA_PRESET_BRANNAN:
      if (has_alpha)
        {
          if (type == type_u8)
            {
              format = format_alpha_u8;
              self->process = photos_operation_insta_curve_brannan_process_alpha_u8;
            }
          else
            {
              format = format_alpha_float;
              self->process = photos_operation_insta_curve_brannan_process_alpha_float;
            }
        }
      else
        {
          if (type == type_u8)
            {
              format = format_u8;
              self->process = photos_operation_insta_curve_brannan_process_u8;
            }
          else
            {
              format = format_float;
              self->process = photos_operation_insta_curve_brannan_process_float;
            }
        }
      break;

    case PHOTOS_OPERATION_INSTA_PRESET_GOTHAM:
      if (has_alpha)
        {
          if (type == type_u8)
            {
              format = format_alpha_u8;
              self->process = photos_operation_insta_curve_gotham_process_alpha_u8;
            }
          else
            {
              format = format_alpha_float;
              self->process = photos_operation_insta_curve_gotham_process_alpha_float;
            }
        }
      else
        {
          if (type == type_u8)
            {
              format = format_u8;
              self->process = photos_operation_insta_curve_gotham_process_u8;
            }
          else
            {
              format = format_float;
              self->process = photos_operation_insta_curve_gotham_process_float;
            }
        }
      break;

    case PHOTOS_OPERATION_INSTA_PRESET_NASHVILLE:
      if (has_alpha)
        {
          if (type == type_u8)
            {
              format = format_alpha_u8;
              self->process = photos_operation_insta_curve_nashville_process_alpha_u8;
            }
          else
            {
              format = format_alpha_float;
              self->process = photos_operation_insta_curve_nashville_process_alpha_float;
            }
        }
      else
        {
          if (type == type_u8)
            {
              format = format_u8;
              self->process = photos_operation_insta_curve_nashville_process_u8;
            }
          else
            {
              format = format_float;
              self->process = photos_operation_insta_curve_nashville_process_float;
            }
        }
      break;

    case PHOTOS_OPERATION_INSTA_PRESET_NONE:
    case PHOTOS_OPERATION_INSTA_PRESET_HEFE:
    case PHOTOS_OPERATION_INSTA_PRESET_CLARENDON:
    default:
      g_assert_not_reached ();
    }

  gegl_operation_set_format (operation, "input", format);
  gegl_operation_set_format (operation, "output", format);
}


static gboolean
photos_operation_insta_curve_process (GeglOperation *operation,
                                      void *in_buf,
                                      void *out_buf,
                                      glong n_pixels,
                                      const GeglRectangle *roi,
                                      gint level)
{
  PhotosOperationInstaCurve *self = PHOTOS_OPERATION_INSTA_CURVE (operation);

  self->process (operation, in_buf, out_buf, n_pixels, roi, level);
  return TRUE;
}


static void
photos_operation_insta_curve_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
  PhotosOperationInstaCurve *self = PHOTOS_OPERATION_INSTA_CURVE (object);

  switch (prop_id)
    {
    case PROP_PRESET:
      g_value_set_enum (value, (gint) self->preset);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
photos_operation_insta_curve_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
  PhotosOperationInstaCurve *self = PHOTOS_OPERATION_INSTA_CURVE (object);

  switch (prop_id)
    {
    case PROP_PRESET:
      self->preset = (PhotosOperationInstaPreset) g_value_get_enum (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
photos_operation_insta_curve_init (PhotosOperationInstaCurve *self)
{
}


static void
photos_operation_insta_curve_class_init (PhotosOperationInstaCurveClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GeglOperationClass *operation_class = GEGL_OPERATION_CLASS (class);
  GeglOperationPointFilterClass *point_filter_class = GEGL_OPERATION_POINT_FILTER_CLASS (class);

  operation_class->opencl_support = FALSE;

  object_class->get_property = photos_operation_insta_curve_get_property;
  object_class->set_property = photos_operation_insta_curve_set_property;
  operation_class->prepare = photos_operation_insta_curve_prepare;
  point_filter_class->process = photos_operation_insta_curve_process;

  g_object_class_install_property (object_class,
                                   PROP_PRESET,
                                   g_param_spec_enum ("preset",
                                                      "PhotosOperationInstaPreset enum",
                                                      "Which curve to apply",
                                                      PHOTOS_TYPE_OPERATION_INSTA_PRESET,
                                                      PHOTOS_OPERATION_INSTA_PRESET_NONE,
                                                      G_PARAM_CONSTRUCT | G_PARAM_READWRITE));

  gegl_operation_class_set_keys (operation_class,
                                 "name", "photos:insta-curve",
                                 "title", "Insta Curve",
                                 "description", "Apply a preset curve to an image",
                                 "categories", "hidden",
                                 NULL);
}
