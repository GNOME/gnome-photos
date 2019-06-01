sources = files(
  'TiffEntry.cpp',
  'TiffEntry.h',
  'TiffIFD.cpp',
  'TiffIFD.h',
  'TiffTag.h',
  'CiffEntry.cpp',
  'CiffEntry.h',
  'CiffIFD.cpp',
  'CiffIFD.h',
  'CiffTag.h',
)

librawspeed_tiff = static_library(
  'rawspeed-tiff',
  sources,
  include_directories: [rawspeed_include, rawspeed_external_include, rawspeed_librawspeed_include],
)
