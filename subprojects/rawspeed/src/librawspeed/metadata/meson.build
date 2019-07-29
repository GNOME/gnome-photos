dependencies = [pugixml_dep]

sources = files(
  'BlackArea.h',
  'Camera.cpp',
  'Camera.h',
  'CameraMetaData.cpp',
  'CameraMetaData.h',
  'CameraMetadataException.h',
  'CameraSensorInfo.cpp',
  'CameraSensorInfo.h',
  'ColorFilterArray.cpp',
  'ColorFilterArray.h',
)

librawspeed_metadata = static_library(
  'rawspeed-metadata',
  sources,
  dependencies: dependencies,
  include_directories: [rawspeed_include, rawspeed_external_include, rawspeed_librawspeed_include],
)
