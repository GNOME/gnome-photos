project(
  'rawspeed',
  'cpp',
  license: 'LGPLv2+',
  meson_version: '>= 0.50.0',
)

cxx = meson.get_compiler('cpp')
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
if cxx.compiles(cpp_thread_local_support_source, name: 'C++ thread-local storage support')
  config_h.set('HAVE_CXX_THREAD_LOCAL', true)
endif

gcc_thread_local_support_source = '''
  static __thread int tls;

  int
  main (void)
  {
    return 0;
  }
'''
if cxx.compiles(gcc_thread_local_support_source, name: 'GCC thread-local storage support')
  config_h.set('HAVE_GCC_THREAD_LOCAL', true)
endif

openmp_dep = dependency('openmp', version: '>= 4.0')
config_h.set('HAVE_OPENMP', true)

zlib_dep = dependency('zlib')
config_h.set('HAVE_ZLIB', true)

configure_file(
  output: 'rawspeedconfig.h',
  configuration: config_h,
)

rawspeed_include = include_directories('.')


subdir('src')
