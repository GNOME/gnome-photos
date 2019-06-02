/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2017 Axel Waggershauser

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

#pragma once

#include "common/Common.h" // for uchar8, uint32
#include "io/BitStream.h"  // for BitStreamCacheRightInLeftOut, BitStream
#include "io/Buffer.h"     // for Buffer::size_type
#include "io/Endianness.h" // for getBE

namespace rawspeed {

struct JPEGBitPumpTag;

// The JPEG data is ordered in MSB bit order,
// i.e. we push into the cache from the right and read it from the left
using BitPumpJPEG = BitStream<JPEGBitPumpTag, BitStreamCacheRightInLeftOut>;

template <> struct BitStreamTraits<BitPumpJPEG> final {
  static constexpr bool canUseWithHuffmanTable = true;
};

template <>
inline BitPumpJPEG::size_type BitPumpJPEG::fillCache(const uchar8* input,
                                                     size_type bufferSize,
                                                     size_type* bufPos) {
  static_assert(BitStreamCacheBase::MaxGetBits >= 32, "check implementation");

  // short-cut path for the most common case (no FF marker in the next 4 bytes)
  // this is slightly faster than the else-case alone.
  // TODO: investigate applicability of vector intrinsics to speed up if-cascade
  if (input[0] != 0xFF &&
      input[1] != 0xFF &&
      input[2] != 0xFF &&
      input[3] != 0xFF ) {
    cache.push(getBE<uint32>(input), 32);
    return 4;
  }

  size_type p = 0;
  for (size_type i = 0; i < 4; ++i) {
    // Pre-execute most common case, where next byte is 'normal'/non-FF
    const int c0 = input[p++];
    cache.push(c0, 8);
    if (c0 == 0xFF) {
      // Found FF -> pre-execute case of FF/00, which represents an FF data byte -> ignore the 00
      const int c1 = input[p++];
      if (c1 != 0) {
        // Found FF/xx with xx != 00. This is the end of stream marker.

        // Clear low 8 bits (0xFF, from c0) that we optimistically pushed.
        // We should not pop() them, to avoid issues with fillLevel becoming 0.
        cache.cache &= ~0xFFULL;
        // And fully fill the empty space in cache with zeros.
        cache.cache <<= 64 - cache.fillLevel;
        cache.fillLevel = 64;

        // No further reading from this buffer shall happen.
        // Do signal that by stating that we are at the end of the buffer.
        *bufPos = bufferSize;
        return 0;
      }
    }
  }
  return p;
}

template <> inline BitPumpJPEG::size_type BitPumpJPEG::getBufferPosition() const
{
  // the current number of bytes we consumed -> at the end of the stream pos, it
  // points to the JPEG marker FF
  return pos;
}

} // namespace rawspeed
