project(
  'rawspeed',
  'cpp',
  license: 'LGPLv2+',
  meson_version: '>= 0.50.0',
)

cxx = meson.get_compiler('cpp')

if get_option('lto')
  if cxx.get_id() == 'gcc'
    add_project_arguments('-flto', language: 'cpp')
  endif
endif

rawspeed_datadir = get_option('pkgdatadir')
rawspeed_libdir = get_option('pkglibdir')

config_h = configuration_data()

cpp_thread_local_support_source = '''
  static thread_local int tls;

  int
  main (void)
  {
    return 0;
  }
'''
config_h.set(
  'HAVE_CXX_THREAD_LOCAL',
  cxx.compiles(cpp_thread_local_support_source, name: 'C++ thread-local storage support')
)

gcc_thread_local_support_source = '''
  static __thread int tls;

  int
  main (void)
  {
    return 0;
  }
'''
config_h.set(
  'HAVE_GCC_THREAD_LOCAL',
  cxx.compiles(gcc_thread_local_support_source, name: 'GCC thread-local storage support')
)

config_h.set('HAVE_POSIX_MEMALIGN', cxx.has_header_symbol('cstdlib', 'posix_memalign'))
config_h.set('HAVE_ALIGNED_ALLOC', cxx.has_header_symbol('cstdlib', 'aligned_alloc'))
config_h.set('HAVE_MM_MALLOC', cxx.has_header_symbol('xmmintrin.h', '_mm_malloc'))
config_h.set('HAVE_ALIGNED_MALLOC', cxx.has_header_symbol('malloc.h', '_aligned_malloc'))

libjpeg_dep = dependency('libjpeg', version: '>= 2.0.0')
config_h.set('HAVE_JPEG', true)
config_h.set('HAVE_JPEG_MEM_SRC', true)

openmp_dep = dependency('openmp', version: '>= 4.0')
config_h.set('HAVE_OPENMP', true)
config_h.set('ompfirstprivateclause', 'firstprivate(__VA_ARGS__)')

zlib_dep = dependency('zlib')
config_h.set('HAVE_ZLIB', true)

pugixml_dep = cxx.find_library('pugixml')
config_h.set('HAVE_PUGIXML', true)

configure_file(
  input: 'config.h.in',
  output: 'rawspeedconfig.h',
  configuration: config_h,
)

rawspeed_include = include_directories('.')

subdir('data')
subdir('src')
