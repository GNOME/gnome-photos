subdir('unit')

test_name = 'basic.py'

if photos_installed_tests_enabled
  test_conf = configuration_data()
  test_conf.set('installed_testdir', photos_installed_test_execdir)
  test_conf.set('program', test_name)

  configure_file(
    input: 'template.test.in',
    output: test_name + '.test',
    configuration: test_conf,
    install: true,
    install_dir: photos_installed_test_metadir,
  )

  install_data(
    [test_name, 'testutil.py'],
    install_dir: photos_installed_test_execdir,
  )
endif


tracker_sandbox_command = find_program(trackertestutils.get_pkgconfig_variable('command'))

if get_option('dogtail')
  test_env = [
    'LC_ALL=C',
    'G_TEST_BUILDDIR=@0@'.format(meson.current_build_dir()),
    'GSETTINGS_BACKEND=memory',
  ]

  test(
    test_name,
    tracker_sandbox_command,
    args: ['--index-tmpdir', files(test_name)],
    env: test_env,
  )
endif
