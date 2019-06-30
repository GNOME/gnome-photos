/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2014 Klaus Post

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

#include "decompressors/PentaxDecompressor.h"
#include "common/Common.h"                // for uint32, uchar8, ushort16
#include "common/Point.h"                 // for iPoint2D
#include "common/RawImage.h"              // for RawImage, RawImageData
#include "decoders/RawDecoderException.h" // for ThrowRDE
#include "decompressors/HuffmanTable.h"   // for HuffmanTable
#include "io/BitPumpMSB.h"                // for BitPumpMSB, BitStream<>::f...
#include "io/Buffer.h"                    // for Buffer
#include "io/ByteStream.h"                // for ByteStream
#include <cassert>                        // for assert
#include <vector>                         // for vector

namespace rawspeed {

// 16 entries of codes per bit length
// 13 entries of code values
const std::array<std::array<std::array<uchar8, 16>, 2>, 1>
    PentaxDecompressor::pentax_tree = {{
        {{{0, 2, 3, 1, 1, 1, 1, 1, 1, 2, 0, 0, 0, 0, 0, 0},
          {3, 4, 2, 5, 1, 6, 0, 7, 8, 9, 10, 11, 12}}},
    }};

PentaxDecompressor::PentaxDecompressor(const RawImage& img,
                                       ByteStream* metaData)
    : mRaw(img), ht(SetupHuffmanTable(metaData)) {
  if (mRaw->getCpp() != 1 || mRaw->getDataType() != TYPE_USHORT16 ||
      mRaw->getBpp() != 2)
    ThrowRDE("Unexpected component count / data type");

  if (!mRaw->dim.x || !mRaw->dim.y || mRaw->dim.x % 2 != 0 ||
      mRaw->dim.x > 8384 || mRaw->dim.y > 6208) {
    ThrowRDE("Unexpected image dimensions found: (%u; %u)", mRaw->dim.x,
             mRaw->dim.y);
  }
}

HuffmanTable PentaxDecompressor::SetupHuffmanTable_Legacy() {
  HuffmanTable ht;

  /* Initialize with legacy data */
  auto nCodes = ht.setNCodesPerLength(Buffer(pentax_tree[0][0].data(), 16));
  assert(nCodes == 13); // see pentax_tree definition
  ht.setCodeValues(Buffer(pentax_tree[0][1].data(), nCodes));

  return ht;
}

HuffmanTable PentaxDecompressor::SetupHuffmanTable_Modern(ByteStream stream) {
  HuffmanTable ht;

  const uint32 depth = stream.getU16() + 12;
  if (depth > 15)
    ThrowRDE("Depth of huffman table is too great (%u).", depth);

  stream.skipBytes(12);

  std::array<uint32, 16> v0;
  std::array<uint32, 16> v1;
  for (uint32 i = 0; i < depth; i++)
    v0[i] = stream.getU16();
  for (uint32 i = 0; i < depth; i++) {
    v1[i] = stream.getByte();

    if (v1[i] == 0 || v1[i] > 12)
      ThrowRDE("Data corrupt: v1[%i]=%i, expected [1..12]", depth, v1[i]);
  }

  std::vector<uchar8> nCodesPerLength;
  nCodesPerLength.resize(17);

  std::array<uint32, 16> v2;
  /* Calculate codes and store bitcounts */
  for (uint32 c = 0; c < depth; c++) {
    v2[c] = v0[c] >> (12 - v1[c]);
    nCodesPerLength.at(v1[c])++;
  }

  assert(nCodesPerLength.size() == 17);
  assert(nCodesPerLength[0] == 0);
  auto nCodes = ht.setNCodesPerLength(Buffer(&nCodesPerLength[1], 16));
  assert(nCodes == depth);

  std::vector<uchar8> codeValues;
  codeValues.reserve(nCodes);

  /* Find smallest */
  for (uint32 i = 0; i < depth; i++) {
    uint32 sm_val = 0xfffffff;
    uint32 sm_num = 0xff;
    for (uint32 j = 0; j < depth; j++) {
      if (v2[j] <= sm_val) {
        sm_num = j;
        sm_val = v2[j];
      }
    }
    codeValues.push_back(sm_num);
    v2[sm_num] = 0xffffffff;
  }

  assert(codeValues.size() == nCodes);
  ht.setCodeValues(Buffer(codeValues.data(), nCodes));

  return ht;
}

HuffmanTable PentaxDecompressor::SetupHuffmanTable(ByteStream* metaData) {
  HuffmanTable ht;

  if (metaData)
    ht = SetupHuffmanTable_Modern(*metaData);
  else
    ht = SetupHuffmanTable_Legacy();

  ht.setup(true, false);

  return ht;
}

void PentaxDecompressor::decompress(const ByteStream& data) const {
  BitPumpMSB bs(data);
  uchar8* draw = mRaw->getData();

  assert(mRaw->dim.y > 0);
  assert(mRaw->dim.x > 0);
  assert(mRaw->dim.x % 2 == 0);

  std::array<int, 2> pUp1 = {{}};
  std::array<int, 2> pUp2 = {{}};

  for (int y = 0; y < mRaw->dim.y && mRaw->dim.x >= 2; y++) {
    auto* dest = reinterpret_cast<ushort16*>(&draw[y * mRaw->pitch]);

    pUp1[y & 1] += ht.decodeNext(bs);
    pUp2[y & 1] += ht.decodeNext(bs);

    int pLeft1 = dest[0] = pUp1[y & 1];
    int pLeft2 = dest[1] = pUp2[y & 1];

    for (int x = 2; x < mRaw->dim.x; x += 2) {
      pLeft1 += ht.decodeNext(bs);
      pLeft2 += ht.decodeNext(bs);

      dest[x] = pLeft1;
      dest[x + 1] = pLeft2;

      if (pLeft1 < 0 || pLeft1 > 65535)
        ThrowRDE("decoded value out of bounds at %d:%d", x, y);
      if (pLeft2 < 0 || pLeft2 > 65535)
        ThrowRDE("decoded value out of bounds at %d:%d", x, y);
    }
  }
}

} // namespace rawspeed
