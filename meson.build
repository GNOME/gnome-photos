project(
  'Photos', 'c',
  version: '3.27.1',
  license: 'GPL2+',
  default_options: [
    'buildtype=debugoptimized',
    'warning_level=1'
  ],
  meson_version: '>= 0.43.0'
)

photos_name = 'gnome-photos'
photos_version = meson.project_version()

photos_prefix = get_option('prefix')
photos_bindir = join_paths(photos_prefix, get_option('bindir'))
photos_datadir = join_paths(photos_prefix, get_option('datadir'))
photos_libexecdir = join_paths(photos_prefix, get_option('libexecdir'))
photos_localedir = join_paths(photos_prefix, get_option('localedir'))

photos_docdir = join_paths(photos_datadir, 'doc', photos_name)

photos_installed_test_metadir = join_paths(photos_datadir, 'installed-tests', meson.project_name())
photos_installed_test_execdir = join_paths(photos_libexecdir, 'installed-tests', meson.project_name())

photos_debug = get_option('buildtype').contains('debug')

cc = meson.get_compiler('c')

config_h = configuration_data()

# defines
set_defines = [
  # package
  ['PACKAGE', photos_name],
  ['PACKAGE_BUGREPORT', 'http://bugzilla.gnome.org/enter_bug.cgi?product=' + photos_name],
  ['PACKAGE_NAME', meson.project_name()],
  ['PACKAGE_STRING', '@0@ @1@'.format(meson.project_name(), photos_version)],
  ['PACKAGE_TARNAME', photos_name],
  ['PACKAGE_URL', 'https://wiki.gnome.org/Apps/Photos'],
  ['PACKAGE_VERSION', photos_version],
  ['VERSION', photos_version],
  # i18n
  ['GETTEXT_PACKAGE', photos_name],
  # source
  ['G_LOG_DOMAIN', photos_name]
]

foreach define: set_defines
  config_h.set_quoted(define[0], define[1])
endforeach

# debug options
config_h.set('NDEBUG', not photos_debug)

if not photos_debug
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
config_h.set('HAVE__NL_MEASUREMENT_MEASUREMENT', cc.compiles(langinfo_measurement_src),
             description: 'Define if _NL_MEASUREMENT_MEASUREMENT is available')

# headers
check_headers = [
  ['HAVE_DLFCN_H', 'dlfcn.h'],
  ['HAVE_INTTYPES_H', 'inttypes.h'],
  ['HAVE_LOCALE_H', 'locale.h'],
  ['HAVE_MEMORY_H', 'memory.h'],
  ['HAVE_STDINT_H', 'stdint.h'],
  ['HAVE_STDLIB_H', 'stdlib.h'],
  ['HAVE_STRINGS_H', 'strings.h'],
  ['HAVE_STRING_H', 'string.h'],
  ['HAVE_SYS_STAT_H', 'sys/stat.h'],
  ['HAVE_SYS_TYPES_H', 'sys/types.h'],
  ['HAVE_UNISTD_H', 'unistd.h']
]

foreach header: check_headers
  config_h.set(header[0], cc.has_header(header[1]))
endforeach

# functions
check_functions = [
  # i18n
  ['HAVE_DCGETTEXT', 'dcgettext'],
  ['HAVE_GETTEXT', 'gettext'],
  ['HAVE_ICONV', 'iconv']
]

if host_machine.system().contains('darwin')
  check_functions += [
    ['HAVE_CFLOCALECOPYCURRENT', 'CFLocaleCopyCurrent'],
    ['HAVE_CFPREFERENCESCOPYAPPVALUE', 'CFPreferencesCopyAppValue']
  ]
endif

foreach func: check_functions
  config_h.set(func[0], cc.has_function(func[1]))
endforeach

# symbols
check_symbols = [
  # i18n
  ['HAVE_LC_MESSAGES', 'locale.h', 'LC_MESSAGES'],
  ['HAVE_BIND_TEXTDOMAIN_CODESET', 'libintl.h', 'bind_textdomain_codeset']
]

foreach symbol: check_symbols
  config_h.set(symbol[0], cc.has_header_symbol(symbol[1], symbol[2]))
endforeach

# compiler flags
common_flags = ['-DHAVE_CONFIG_H']

if get_option('enable-rdtscp')
  common_flags += '-DEGG_HAVE_RDTSCP'
  config_h.set('HAVE_SCHED_GETCPU', cc.has_function('sched_getcpu'))
endif

if photos_debug
  test_cflags = [
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
    '-Wwrite-strings'
  ]

  foreach cflag: test_cflags
    if cc.has_argument(cflag)
      common_flags += [cflag]
    endif
  endforeach
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
    'with-tagged-entry=true'
  ]
)
libgd_dep = libgd.get_variable('libgd_dep')

libjpeg_dep = dependency('libjpeg', required: false)
assert(libjpeg_dep.found(), 'JPEG library not found')
assert(cc.has_function('jpeg_destroy_decompress', dependencies: libjpeg_dep), 'jpeg_destroy_decompress not found')
assert(cc.has_header('jpeglib.h', dependencies: libjpeg_dep), 'JPEG headers not found')
assert(cc.has_function('jpeg_save_markers', dependencies: libjpeg_dep), 'JPEG library is too old')
config_h.set('HAVE_LIBJPEG', true)

goa_dep = dependency('goa-1.0', version: '>= 3.8.0')
config_h.set('GOA_API_IS_SUBJECT_TO_CHANGE', true,
             description: 'We are aware that GOA\'s API can change')

common_deps = [
  libjpeg_dep,
  dependency('babl'),
  dependency('gegl-0.3', version: '>= 0.3.15'),
  dependency('gdk-pixbuf-2.0'),
  dependency('gio-2.0'),
  dependency('gio-unix-2.0'),
  dependency('glib-2.0', version: '>= 2.44.0'),
  dependency('gtk+-3.0', version: '>= 3.22.16'),
  dependency('libpng16'),
  dependency('tracker-sparql-2.0'),
  cc.find_library('m')
]

# FIXME: shm_open seems a hard dependency
if not cc.has_function('shm_open')
  rt_dep = cc.find_library('rt')
  assert(cc.has_function('shm_open', dependencies: rt_dep), 'shm_open support is missing')
  common_deps += rt_dep
endif
# FIXME: it might be unnecessary because egg-counter.c is expecting:
#        if (getenv ("EGG_COUNTER_DISABLE_SHM"))
config_h.set('HAVE_SHM_OPEN', true)

configure_file(
  output: 'config.h',
  configuration: config_h
)

doc_data = files(
  'ARTISTS',
  'ChangeLog',
  'COPYING',
  'NEWS',
  'README'
)

install_data(
  doc_data,
  install_dir: photos_docdir
)

authors = 'AUTHORS'

author = custom_target(
  authors,
  output: authors,
  command: [find_program('generate-authors.sh'), meson.source_root(), '@OUTPUT@'],
  install: true,
  install_dir: photos_docdir
)

gnome = import('gnome')
i18n = import('i18n')
pkg = import('pkgconfig')

po_dir = join_paths(meson.source_root(), 'po')

top_inc = include_directories('.')

subdir('data')
subdir('src')
subdir('po')
subdir('tests')
subdir('help')

meson.add_install_script(
  'meson_post_install.py',
  photos_datadir
)
