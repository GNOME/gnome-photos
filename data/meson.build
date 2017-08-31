subdir('icons')

info = 'org.gnome.Photos.metainfo.xml'

i18n.merge_file(
  info,
  input: 'org.gnome.Photos.appdata.xml.in',
  output: info,
  po_dir: po_dir,
  install: true,
  install_dir: join_paths(photos_datadir, 'metainfo')
)

desktop_conf = configuration_data()
desktop_conf.set('PACKAGE_TARNAME', photos_name)

desktop = 'org.gnome.Photos.desktop'

desktop_in = configure_file(
  input: desktop + '.in.in',
  output: desktop + '.in',
  configuration: desktop_conf
)

i18n.merge_file(
  desktop,
  type: 'desktop',
  input: desktop_in,
  output: desktop,
  po_dir: po_dir,
  install: true,
  install_dir: join_paths(photos_datadir, 'applications')
)

service_conf = configuration_data()
service_conf.set('bindir', photos_bindir)

service = 'org.gnome.Photos.service'

configure_file(
  input: service + '.in',
  output: service,
  install: true,
  install_dir: join_paths(photos_datadir, 'dbus-1', 'services'),
  configuration: service_conf
)

install_data(
  'org.gnome.Photos.search-provider.ini',
  install_dir: join_paths(photos_datadir, 'gnome-shell', 'search-providers')
)

install_data(
  'org.gnome.photos.gschema.xml',
  install_dir: join_paths(photos_datadir, 'glib-2.0', 'schemas')
)
