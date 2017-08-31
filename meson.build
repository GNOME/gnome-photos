project(
  'gnome-photos', 'c',
  version: '3.29.91',
  license: 'GPL3+',
  default_options: 'buildtype=debugoptimized',
  meson_version: '>= 0.47.0',
)

photos_version = meson.project_version()

photos_prefix = get_option('prefix')
photos_bindir = join_paths(photos_prefix, get_option('bindir'))
photos_datadir = join_paths(photos_prefix, get_option('datadir'))
photos_libexecdir = join_paths(photos_prefix, get_option('libexecdir'))
photos_localedir = join_paths(photos_prefix, get_option('localedir'))

photos_docdir = join_paths(photos_datadir, 'doc', meson.project_name())

photos_installed_test_metadir = join_paths(photos_datadir, 'installed-tests', meson.project_name())
photos_installed_test_execdir = join_paths(photos_libexecdir, 'installed-tests', meson.project_name())

photos_namespace = 'org.gnome.Photos'

photos_buildtype = get_option('buildtype')

cc = meson.get_compiler('c')

config_h = configuration_data()

# defines
set_defines = [
  # package
  ['PACKAGE_TARNAME', meson.project_name()],
  ['PACKAGE_URL', 'https://wiki.gnome.org/Apps/Photos'],
  ['PACKAGE_VERSION', photos_version],
  # i18n
  ['GETTEXT_PACKAGE', meson.project_name()],
]

foreach define: set_defines
  config_h.set_quoted(define[0], define[1])
endforeach

config_h.set('PHOTOS_EPSILON', '1e-5',
             description: 'Tolerance when comparing single or double precision floating point values')

if not photos_buildtype.contains('debug')
  res = run_command(find_command('git'), 'describe')
  if res.returncode() == 0
    config_h.set('PACKAGE_COMMIT_ID', res.stdout().strip(),
                 description: 'Define to the description of this package\'s latest commit')
  endif
endif

# Support for nl_langinfo (_NL_MEASUREMENT_MEASUREMENT) (optional)
langinfo_measurement_src = '''
  #include <langinfo.h>
  int main() {
    char c;
    c = *((unsigned char *)  nl_langinfo(_NL_MEASUREMENT_MEASUREMENT));
  };
'''
config_h.set('HAVE__NL_MEASUREMENT_MEASUREMENT', cc.compiles(langinfo_measurement_src, name: 'Support for nl_langinfo'),
             description: 'Define if _NL_MEASUREMENT_MEASUREMENT is available')

common_flags = []

if photos_buildtype.contains('debug')
  common_flags += cc.get_supported_arguments([
    '-fno-strict-aliasing',
    '-Wcast-align',
    '-Wdeclaration-after-statement',
    '-Wformat=2',
    '-Winit-self',
    '-Winline',
    '-Wmissing-declarations',
    '-Wmissing-format-attribute',
    '-Wmissing-include-dirs',
    '-Wmissing-noreturn',
    '-Wmissing-prototypes',
    '-Wnested-externs',
    '-Wno-error=missing-field-initializers',
    '-Wno-error=unused-parameter',
    '-Wno-missing-field-initializers',
    '-Wno-unused-parameter',
    '-Wold-style-definition',
    '-Wpacked',
    '-Wredundant-decls',
    '-Wshadow',
    '-Wstrict-prototypes',
    '-Wswitch-default',
    '-Wswitch-enum',
    '-Wundef',
    '-Wunused-but-set-variable',
    '-Wwrite-strings',
  ])
elif photos_buildtype == 'release'
  common_flags += cc.get_supported_arguments([
    '-Wno-cast-function-type',
    '-Wno-error=cast-function-type',
  ])
endif

add_project_arguments(common_flags, language: 'c')

libgd = subproject(
  'libgd',
  default_options: [
    'static=true',
    'with-gtk-hacks=true',
    'with-main-box=true',
    'with-main-icon-box=true',
    'with-main-icon-view=true',
    'with-tagged-entry=true',
  ]
)
libgd_dep = libgd.get_variable('libgd_dep')

libjpeg_dep = dependency('libjpeg', required: false)
assert(libjpeg_dep.found(), 'JPEG library not found')
assert(cc.has_function('jpeg_destroy_decompress', dependencies: libjpeg_dep), 'jpeg_destroy_decompress not found')
assert(cc.has_header('jpeglib.h', dependencies: libjpeg_dep), 'JPEG headers not found')
assert(cc.has_function('jpeg_save_markers', dependencies: libjpeg_dep), 'JPEG library is too old')

goa_dep = dependency('goa-1.0', version: '>= 3.8.0')
config_h.set('GOA_API_IS_SUBJECT_TO_CHANGE', true,
             description: 'We are aware that GOA\'s API can change')

babl_dep = dependency('babl')
cairo_dep = dependency('cairo', version: '>= 1.14.0')
gdk_pixbuf_dep = dependency('gdk-pixbuf-2.0', version: '>= 2.32')
gegl_dep = dependency('gegl-0.4', version: '>= 0.4.0')
geocode_glib_dep = dependency('geocode-glib-1.0')
gexiv_dep = dependency('gexiv2', version: '>= 0.10.8')
gio_dep = dependency('gio-2.0')
gio_unix_dep = dependency('gio-unix-2.0')
glib_dep = dependency('glib-2.0', version: '>= 2.44.0')
grilo_dep = dependency('grilo-0.3', version: '>= 0.3.5')
gsettings_desktop_schemas_dep = dependency('gsettings-desktop-schemas')
gtk_dep = dependency('gtk+-3.0', version: '>= 3.22.16')
gtk_unix_print_dep = dependency('gtk+-unix-print-3.0')
libdazzle_dep = dependency('libdazzle-1.0', version: '>= 3.26.0')
libgdata_dep = dependency('libgdata', version: '>= 0.15.2')
libgfgraph_dep = dependency('libgfbgraph-0.2', version: '>= 0.2.1')
libpng_dep = dependency('libpng16')
tracker_control_dep = dependency('tracker-control-2.0')
tracker_sparql_dep = dependency('tracker-sparql-2.0')

m_dep = cc.find_library('m')
rt_dep = cc.find_library('rt')

# Check for dbus service dir
dbus_service_dir = dependency('dbus-1').get_pkgconfig_variable('session_bus_services_dir', define_variable: ['datadir', photos_datadir])

configure_file(
  output: 'config.h',
  configuration: config_h,
)

gnome = import('gnome')
i18n = import('i18n')
pkg = import('pkgconfig')

source_root = meson.current_source_dir()
build_root = meson.current_build_dir()

po_dir = join_paths(source_root, 'po')

top_inc = include_directories('.')

artists = files('ARTISTS')

doc_data = artists + files(
  'ChangeLog',
  'COPYING',
  'NEWS',
  'README',
)

install_data(
  doc_data,
  install_dir: photos_docdir,
)

name = 'AUTHORS'

authors = custom_target(
  name,
  output: name,
  command: [find_program('generate-authors.sh'), source_root, '@OUTPUT@'],
  install: true,
  install_dir: photos_docdir,
)

subdir('data')
subdir('src')
subdir('po')
subdir('tests')
subdir('help')

meson.add_install_script(
  'meson_post_install.py',
  photos_datadir,
)
