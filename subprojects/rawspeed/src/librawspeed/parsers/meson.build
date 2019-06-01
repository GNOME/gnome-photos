sources = files(
  'CiffParser.cpp',
  'CiffParser.h',
  'CiffParserException.h',
  'FiffParser.cpp',
  'FiffParser.h',
  'FiffParserException.h',
  'RawParser.cpp',
  'RawParser.h',
  'RawParserException.h',
  'TiffParser.cpp',
  'TiffParser.h',
  'TiffParserException.h',
)

librawspeed_parsers = static_library(
  'rawspeed-parsers',
  sources,
  include_directories: [rawspeed_include, rawspeed_external_include, rawspeed_librawspeed_include],
)
