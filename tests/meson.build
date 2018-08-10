test_name = 'basic.py'

if get_option('installed_tests')
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

if get_option('dogtail')
  service_conf = configuration_data()
  service_conf.set('bindir', src_build_dir)

  service = photos_namespace + '.service'

  configure_file(
    input: service_in,
    output: service,
    configuration: service_conf
  )

  test_session_conf = configuration_data()
  test_session_conf.set('servicedir', meson.current_build_dir())

  test_session = 'test-session.conf'

  session_conf = configure_file(
    input: test_session + '.in',
    output: test_session,
    configuration: test_session_conf
  )

  test_env = [
    'LC_ALL=C',
    'GSETTINGS_BACKEND=memory',
  ]

  dbus_run_session = find_program('dbus-run-session')

  test(
    test_name,
    dbus_run_session,
    args: ['--config-file', session_conf, files(test_name)],
    env: test_env,
  )
endif
