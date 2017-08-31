subdir('icons')

appdata = photos_namespace + '.appdata.xml'

i18n.merge_file(
  appdata,
  input: appdata + '.in',
  output: appdata,
  po_dir: po_dir,
  install: true,
  install_dir: join_paths(photos_datadir, 'metainfo'),
)

desktop_conf = configuration_data()
desktop_conf.set('PACKAGE_TARNAME', meson.project_name())

desktop = photos_namespace + '.desktop'

desktop_in = configure_file(
  input: desktop + '.in.in',
  output: desktop + '.in',
  configuration: desktop_conf,
)

i18n.merge_file(
  desktop,
  type: 'desktop',
  input: desktop_in,
  output: desktop,
  po_dir: po_dir,
  install: true,
  install_dir: join_paths(photos_datadir, 'applications'),
)

service_conf = configuration_data()
service_conf.set('bindir', photos_bindir)

service = photos_namespace + '.service'

configure_file(
  input: service + '.in',
  output: service,
  configuration: service_conf,
  install: true,
  install_dir: dbus_service_dir,
)

install_data(
  photos_namespace + '.search-provider.ini',
  install_dir: join_paths(photos_datadir, 'gnome-shell', 'search-providers'),
)

install_data(
  photos_namespace.to_lower() + '.gschema.xml',
  install_dir: join_paths(photos_datadir, 'glib-2.0', 'schemas'),
)
