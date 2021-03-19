/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2010 Klaus Post
    Copyright (C) 2014-2015 Pedro Côrte-Real
    Copyright (C) 2017 Roman Lebedev

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
*/

#include "decompressors/SamsungV0Decompressor.h"
#include "common/Common.h"                // for uint32, ushort16, int32
#include "common/Point.h"                 // for iPoint2D
#include "common/RawImage.h"              // for RawImage, RawImageData
#include "decoders/RawDecoderException.h" // for ThrowRDE
#include "io/BitPumpMSB32.h"              // for BitPumpMSB32
#include "io/ByteStream.h"                // for ByteStream
#include <algorithm>                      // for max
#include <cassert>                        // for assert
#include <iterator>                       // for advance, begin, end, next
#include <vector>                         // for vector

namespace rawspeed {

SamsungV0Decompressor::SamsungV0Decompressor(const RawImage& image,
                                             const ByteStream& bso,
                                             const ByteStream& bsr)
    : AbstractSamsungDecompressor(image) {
  if (mRaw->getCpp() != 1 || mRaw->getDataType() != TYPE_USHORT16 ||
      mRaw->getBpp() != 2)
    ThrowRDE("Unexpected component count / data type");

  const uint32 width = mRaw->dim.x;
  const uint32 height = mRaw->dim.y;

  if (width == 0 || height == 0 || width < 16 || width > 5546 || height > 3714)
    ThrowRDE("Unexpected image dimensions found: (%u; %u)", width, height);

  computeStripes(bso.peekStream(height, 4), bsr);
}

// FIXME: this is very close to IiqDecoder::computeSripes()
void SamsungV0Decompressor::computeStripes(ByteStream bso, ByteStream bsr) {
  const uint32 height = mRaw->dim.y;

  std::vector<uint32> offsets;
  offsets.reserve(1 + height);
  for (uint32 y = 0; y < height; y++)
    offsets.emplace_back(bso.getU32());
  offsets.emplace_back(bsr.getSize());

  stripes.reserve(height);

  auto offset_iterator = std::begin(offsets);
  bsr.skipBytes(*offset_iterator);

  auto next_offset_iterator = std::next(offset_iterator);
  while (next_offset_iterator < std::end(offsets)) {
    if (*offset_iterator >= *next_offset_iterator)
      ThrowRDE("Line offsets are out of sequence or slice is empty.");

    const auto size = *next_offset_iterator - *offset_iterator;
    assert(size > 0);

    stripes.emplace_back(bsr.getStream(size));

    std::advance(offset_iterator, 1);
    std::advance(next_offset_iterator, 1);
  }

  assert(stripes.size() == height);
}

void SamsungV0Decompressor::decompress() const {
  for (int y = 0; y < mRaw->dim.y; y++)
    decompressStrip(y, stripes[y]);

  // Swap red and blue pixels to get the final CFA pattern
  for (int y = 0; y < mRaw->dim.y - 1; y += 2) {
    auto* topline = reinterpret_cast<ushort16*>(mRaw->getData(0, y));
    auto* bottomline = reinterpret_cast<ushort16*>(mRaw->getData(0, y + 1));

    for (int x = 0; x < mRaw->dim.x - 1; x += 2) {
      ushort16 temp = topline[1];
      topline[1] = bottomline[0];
      bottomline[0] = temp;

      topline += 2;
      bottomline += 2;
    }
  }
}

int32 SamsungV0Decompressor::calcAdj(BitPumpMSB32* bits, int b) {
  int32 adj = 0;
  if (b)
    adj = (static_cast<int32>(bits->getBits(b)) << (32 - b) >> (32 - b));
  return adj;
}

void SamsungV0Decompressor::decompressStrip(uint32 y,
                                            const ByteStream& bs) const {
  const uint32 width = mRaw->dim.x;
  assert(width > 0);

  BitPumpMSB32 bits(bs);

  std::array<int, 4> len;
  for (int& i : len)
    i = y < 2 ? 7 : 4;

  auto* img = reinterpret_cast<ushort16*>(mRaw->getData(0, y));
  const auto* const past_last =
      reinterpret_cast<ushort16*>(mRaw->getData(width - 1, y) + mRaw->getBpp());
  ushort16* img_up = reinterpret_cast<ushort16*>(
      mRaw->getData(0, std::max(0, static_cast<int>(y) - 1)));
  ushort16* img_up2 = reinterpret_cast<ushort16*>(
      mRaw->getData(0, std::max(0, static_cast<int>(y) - 2)));

  // Image is arranged in groups of 16 pixels horizontally
  for (uint32 x = 0; x < width; x += 16) {
    bits.fill();
    bool dir = !!bits.getBitsNoFill(1);

    std::array<int, 4> op;
    for (int& i : op)
      i = bits.getBitsNoFill(2);

    for (int i = 0; i < 4; i++) {
      assert(op[i] >= 0 && op[i] <= 3);

      switch (op[i]) {
      case 3:
        len[i] = bits.getBits(4);
        break;
      case 2:
        len[i]--;
        break;
      case 1:
        len[i]++;
        break;
      default:
        // FIXME: it can be zero too.
        break;
      }

      if (len[i] < 0)
        ThrowRDE("Bit length less than 0.");
      if (len[i] > 16)
        ThrowRDE("Bit Length more than 16.");
    }

    if (dir) {
      // Upward prediction

      if (y < 2)
        ThrowRDE("Upward prediction for the first two rows. Raw corrupt");

      if (x + 16 >= width)
        ThrowRDE("Upward prediction for the last block of pixels. Raw corrupt");

      // First we decode even pixels
      for (int c = 0; c < 16; c += 2) {
        int b = len[c >> 3];
        int32 adj = calcAdj(&bits, b);

        img[c] = adj + img_up[c];
      }

      // Now we decode odd pixels
      // Why on earth upward prediction only looks up 1 line above
      // is beyond me, it will hurt compression a deal.
      for (int c = 1; c < 16; c += 2) {
        int b = len[2 | (c >> 3)];
        int32 adj = calcAdj(&bits, b);

        img[c] = adj + img_up2[c];
      }
    } else {
      // Left to right prediction
      // First we decode even pixels
      int pred_left = x != 0 ? img[-2] : 128;
      for (int c = 0; c < 16; c += 2) {
        int b = len[c >> 3];
        int32 adj = calcAdj(&bits, b);

        if (img + c < past_last)
          img[c] = adj + pred_left;
      }

      // Now we decode odd pixels
      pred_left = x != 0 ? img[-1] : 128;
      for (int c = 1; c < 16; c += 2) {
        int b = len[2 | (c >> 3)];
        int32 adj = calcAdj(&bits, b);

        if (img + c < past_last)
          img[c] = adj + pred_left;
      }
    }

    img += 16;
    img_up += 16;
    img_up2 += 16;
  }
}

} // namespace rawspeed
