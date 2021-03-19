/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2014 Klaus Post
    Copyright (C) 2014 Pedro Côrte-Real

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

#include "common/Common.h"                // for uint32
#include "common/RawImage.h"              // for RawImage
#include "decoders/AbstractTiffDecoder.h" // for AbstractTiffDecoder
#include "tiff/TiffIFD.h"                 // for TiffRootIFD (ptr only)
#include <utility>                        // for move
#include <vector>                         // for vector

namespace rawspeed {

class Buffer;
class ByteStream;
class CameraMetaData;
struct PhaseOneStrip;

class IiqDecoder final : public AbstractTiffDecoder {
  struct IiqOffset {
    uint32 n;
    uint32 offset;

    IiqOffset() = default;
    IiqOffset(uint32 block, uint32 offset_) : n(block), offset(offset_) {}
  };

  std::vector<PhaseOneStrip> computeSripes(const Buffer& raw_data,
                                           std::vector<IiqOffset>&& offsets,
                                           uint32 height) const;

public:
  static bool isAppropriateDecoder(const Buffer* file);
  static bool isAppropriateDecoder(const TiffRootIFD* rootIFD,
                                   const Buffer* file);

  IiqDecoder(TiffRootIFDOwner&& rootIFD, const Buffer* file)
      : AbstractTiffDecoder(move(rootIFD), file) {}

  RawImage decodeRawInternal() override;
  void checkSupportInternal(const CameraMetaData* meta) override;
  void decodeMetaDataInternal(const CameraMetaData* meta) override;

protected:
  int getDecoderVersion() const override { return 0; }
  uint32 black_level = 0;
  void CorrectPhaseOneC(ByteStream meta_data, uint32 split_row,
                        uint32 split_col);
  void CorrectQuadrantMultipliersCombined(ByteStream data, uint32 split_row,
                                          uint32 split_col);
  void correctSensorDefects(ByteStream data);
  void correctBadColumn(ushort16 col);
  void handleBadPixel(ushort16 col, ushort16 row);
};

} // namespace rawspeed
