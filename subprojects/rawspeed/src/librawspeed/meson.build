librawspeed_default_library = get_option('default_library')
rawspeed_librawspeed_include = include_directories('.')

librawspeed_install = true
if librawspeed_default_library == 'static'
  librawspeed_install = false
endif

subdir('common')
subdir('decoders')
subdir('decompressors')
subdir('interpolators')
subdir('io')
subdir('metadata')
subdir('parsers')
subdir('tiff')

librawspeed_libraries = [
  librawspeed_common,
  librawspeed_decoders,
  librawspeed_decompressors,
  librawspeed_interpolators,
  librawspeed_io,
  librawspeed_metadata,
  librawspeed_parsers,
  librawspeed_tiff,
]

librawspeed_sources = []

librawspeed = library(
  meson.project_name(),
  librawspeed_sources,
  include_directories: rawspeed_librawspeed_include,
  install: librawspeed_install,
  install_dir: rawspeed_libdir,
  link_with: librawspeed_libraries,
)

rawspeed_dep = declare_dependency(link_with: librawspeed)
