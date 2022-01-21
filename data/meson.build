subdir('icons')

appdata = photos_namespace + '.appdata.xml'

i18n.merge_file(
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
  install_dir: dbus_services_dir,
)

if photos_flatpak_enabled
  tracker_domain_conf = configuration_data()
  tracker_domain_conf.set('application_id', photos_namespace)
  tracker_domain_conf.set('libexecdir', photos_libexecdir)

  domain_ontology_rule = photos_namespace + '.rule'

  configure_file(
    input: domain_ontology_rule + '.in',
    output: domain_ontology_rule,
    configuration: tracker_domain_conf,
    install: true,
    install_dir: join_paths(photos_datadir, 'tracker3-miners', 'domain-ontologies'),
  )

  tracker_domain_services = [
    'Tracker3.Miner.Extract',
    'Tracker3.Miner.Files'
  ]

  foreach service: tracker_domain_services
    configure_file(
      input: '@0@.@1@.service.in'.format(photos_namespace, service),
      output: '@0@.@1@.service'.format(photos_namespace, service),
      configuration: tracker_domain_conf,
      install: true,
      install_dir: dbus_services_dir,
    )
  endforeach
endif

install_data(
  photos_namespace + '.search-provider.ini',
  install_dir: join_paths(photos_datadir, 'gnome-shell', 'search-providers'),
)

install_data(
  photos_namespace.to_lower() + '.gschema.xml',
  install_dir: join_paths(photos_datadir, 'glib-2.0', 'schemas'),
)
