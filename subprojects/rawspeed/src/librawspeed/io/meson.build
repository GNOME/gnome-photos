sources = files(
  'BitPumpJPEG.h',
  'BitPumpLSB.h',
  'BitPumpMSB.h',
  'BitPumpMSB16.h',
  'BitPumpMSB32.h',
  'BitStream.h',
  'Buffer.cpp',
  'Buffer.h',
  'ByteStream.h',
  'Endianness.h',
  'FileIO.h',
  'FileIOException.h',
  'FileReader.cpp',
  'FileReader.h',
  'FileWriter.cpp',
  'FileWriter.h',
  'IOException.h',
)

librawspeed_io = static_library(
  'rawspeed-io',
  sources,
  include_directories: [rawspeed_include, rawspeed_external_include, rawspeed_librawspeed_include],
)
