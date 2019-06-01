sources = files(
  'Cr2sRawInterpolator.cpp',
  'Cr2sRawInterpolator.h',
)

librawspeed_interpolators = static_library(
  'rawspeed-interpolators',
  sources,
  include_directories: [rawspeed_include, rawspeed_external_include, rawspeed_librawspeed_include],
)
