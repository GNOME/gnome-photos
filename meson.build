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

gnome = import('gnome')
i18n = import('i18n')
pkg = import('pkgconfig')

source_root = meson.current_source_dir()
build_root = meson.current_build_dir()

po_dir = join_paths(source_root, 'po')

top_inc = include_directories('.')

cc = meson.get_compiler('c')

config_h = configuration_data()

# defines
set_defines = [
  # package
  ['PACKAGE_TARNAME', meson.project_name()],
  ['PACKAGE_URL', 'https://wiki.gnome.org/Apps/Photos'],
  ['PACKAGE_VERSION', photos_version],
  #
  ['G_LOG_DOMAIN', meson.project_name()],
  # i18n
  ['GETTEXT_PACKAGE', meson.project_name()],
]

git = find_program('git')
if git.found()
  res = run_command(git, '-C', source_root, 'describe')
  if res.returncode() == 0
    set_defines += [['PACKAGE_COMMIT_ID', res.stdout().strip()]]
  endif
endif

foreach define: set_defines
  config_h.set_quoted(define[0], define[1])
endforeach

config_h.set('PHOTOS_EPSILON', '1e-5')

# Support for nl_langinfo (_NL_MEASUREMENT_MEASUREMENT) (optional)
langinfo_measurement_src = '''
  #include <langinfo.h>
  int main() {
    char c;
    c = *((unsigned char *)  nl_langinfo(_NL_MEASUREMENT_MEASUREMENT));
  };
'''
config_h.set('HAVE__NL_MEASUREMENT_MEASUREMENT', cc.compiles(langinfo_measurement_src, name: 'Support for nl_langinfo'))

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

goa_dep = dependency('goa-1.0', version: '>= 3.8.0')
config_h.set('GOA_API_IS_SUBJECT_TO_CHANGE', true)

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
libjpeg_dep = dependency('libjpeg')
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

artists = files('ARTISTS')
authors = files('AUTHORS')

doc_data = artists + authors + files(
  'COPYING',
  'NEWS',
  'README',
)

install_data(
  doc_data,
  install_dir: photos_docdir,
)

subdir('data')
subdir('src')
subdir('po')
subdir('tests')
subdir('help')

script_name = 'update-authors'

run_target(
  script_name,
  command: [find_program(script_name + '.sh'), source_root, 'AUTHORS'],
)

meson.add_install_script(
  'meson_post_install.py',
  photos_datadir,
)
