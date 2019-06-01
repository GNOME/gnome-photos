dependencies = [openmp_dep]

sources = files(
  'Array2DRef.h',
  'ChecksumFile.cpp',
  'ChecksumFile.h',
  'Common.cpp',
  'Common.h',
  'Cpuid.cpp',
  'DefaultInitAllocatorAdaptor.h',
  'DngOpcodes.cpp',
  'DngOpcodes.h',
  'ErrorLog.cpp',
  'ErrorLog.h',
  'GetNumberOfProcessorCores.cpp',
  'Memory.cpp',
  'Memory.h',
  'Mutex.h',
  'NORangesSet.h',
  'Optional.h',
  'Point.h',
  'Range.h',
  'RawImage.cpp',
  'RawImage.h',
  'RawImageDataFloat.cpp',
  'RawImageDataU16.cpp',
  'RawspeedException.h',
  'SimpleLUT.h',
  'Spline.h',
  'TableLookUp.cpp',
  'TableLookUp.h',
)

librawspeed_common = static_library(
  'rawspeed-common',
  sources,
  dependencies: dependencies,
  include_directories: [rawspeed_include, rawspeed_external_include, rawspeed_librawspeed_include],
)
