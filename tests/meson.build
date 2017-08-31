enable_installed_tests = get_option('enable-installed-tests')

test_names = ['basic.py']

test_template = files('template.test.in')

foreach name: test_names
  test_conf = configuration_data()
  test_conf.set('installed_testdir', photos_installed_test_execdir)
  test_conf.set('program', name)

  configure_file(
    input: test_template,
    output: name + '.test',
    install: enable_installed_tests,
    install_dir: photos_installed_test_metadir,
    configuration: test_conf
  )
endforeach

if get_option('enable-dogtail')
  test_names += ['testutil.py']
endif

if enable_installed_tests
  install_data(
    test_names,
    install_dir: join_paths(photos_libexecdir, 'installed-tests', meson.project_name())
  )
endif

test_env = [
  'LC_ALL=C',
  'GSETTINGS_BACKEND=memory'
]

python = find_program('python2')

foreach name: test_names
  test_script = files(name)
  test(
    name,
    python,
    args: test_script,
    env: test_env
  )
endforeach
