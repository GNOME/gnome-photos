enable_installed_tests = get_option('installed_tests')

test_names = ['basic.py']

test_template = files('template.test.in')

if enable_installed_tests
  foreach name: test_names
    test_conf = configuration_data()
    test_conf.set('installed_testdir', photos_installed_test_execdir)
    test_conf.set('program', name)

    configure_file(
      input: test_template,
      output: name + '.test',
      configuration: test_conf,
      install: true,
      install_dir: photos_installed_test_metadir
    )
  endforeach
endif

if get_option('dogtail')
  test_names += ['testutil.py']
endif

if enable_installed_tests
  install_data(
    test_names,
    install_dir: photos_installed_test_execdir
  )
endif

test_env = [
  'LC_ALL=C',
  'GSETTINGS_BACKEND=memory'
]

python = import('python').find_installation('python2')

foreach name: test_names
  test_script = files(name)
  test(
    name,
    python,
    args: test_script,
    env: test_env
  )
endforeach
