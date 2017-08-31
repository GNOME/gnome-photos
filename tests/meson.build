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
  test_env = [
    'LC_ALL=C',
    'GSETTINGS_BACKEND=memory',
  ]

  test(
    test_name,
    files(test_name),
    env: test_env,
  )
endif
