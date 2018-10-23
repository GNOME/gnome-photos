# Not entirely random of course, but at least it changes over time
meson_minor_version = meson.version().split('.').get(1).to_int()
photos_minor_version = meson.project_version().split('.').get(1).to_int()
random_number = meson_minor_version + photos_minor_version

test_env = environment()
test_env.set('G_DEBUG', 'gc-friendly')
test_env.set('G_TEST_BUILDDIR', meson.current_build_dir())
test_env.set('G_TEST_SRCDIR', meson.current_source_dir())
test_env.set('GSETTINGS_BACKEND', 'memory')
test_env.set('MALLOC_CHECK_', '2')
test_env.set('MALLOC_PERTURB_', '@0@'.format(random_number % 256))

test_deps = [
  libgnome_photos_dep,
]

tests = {
}

foreach test_name, extra_args: tests
  extra_sources = extra_args.get('extra_sources', [])
  install = photos_installed_tests_enabled and extra_args.get('install', true)
  is_parallel = extra_args.get('is_parallel', true)
  source = extra_args.get('source', test_name + '.c')
  suite = extra_args.get('suite', [])

  if install
    test_conf = configuration_data()
    test_conf.set('installed_tests_dir', photos_installed_test_execdir)
    test_conf.set('program', test_name)
    configure_file(
      input: photos_installed_tests_template_tap,
      output: test_name + '.test',
      configuration: test_conf,
      install: true,
      install_dir: photos_installed_test_metadir,
    )
  endif

  exe = executable(
    test_name,
    [source, extra_sources],
    include_directories: [src_inc, top_inc],
    dependencies: test_deps,
    install: install,
    install_dir: photos_installed_test_execdir,
    install_rpath: photos_libdir,
  )

  timeout = suite.contains('slow') ? 120 : 30
  test(
    test_name,
    exe,
    env: test_env,
    timeout: timeout,
    suite: suite,
    args: ['--tap'],
    is_parallel: is_parallel,
  )
endforeach
