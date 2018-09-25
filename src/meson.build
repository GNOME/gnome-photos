sources = files(
  'photos-quarks.c',
)

deps = [
  glib_dep,
]

libgnome_photos = shared_library(
  meson.project_name(),
  sources,
  dependencies: deps,
  include_directories: top_inc,
  install: true,
  install_dir: photos_libdir,
)

libgnome_photos_dep = declare_dependency(
  link_with: libgnome_photos,
)

common_sources = files(
  'photos-debug.c',
  'photos-error.c',
  'photos-gegl.c',
  'photos-glib.c',
  'photos-jpeg-count.c',
  'photos-operation-insta-curve.c',
  'photos-operation-insta-filter.c',
  'photos-operation-insta-hefe.c',
  'photos-operation-insta-hefe-curve.c',
  'photos-operation-insta-hefe-vignette.c',
  'photos-operation-jpg-guess-sizes.c',
  'photos-operation-png-guess-sizes.c',
  'photos-operation-saturation.c',
  'photos-operation-svg-multiply.c',
  'photos-pipeline.c',
  'photos-png-count.c',
)

enum_headers = files(
  'photos-gesture-zoom.h',
  'photos-item-manager.h',
  'photos-operation-insta-common.h',
  'photos-utils.h',
)

enum = 'photos-enums'

common_sources += gnome.mkenums(
  enum,
  sources: enum_headers,
  c_template: enum + '.c.template',
  h_template: enum + '.h.template',
)

resource_data = files('../data/vignette.png')

common_sources += gnome.compile_resources(
  'photos-resources-gegl',
  'photos-gegl.gresource.xml',
  source_dir: '.',
  c_name: 'photos_gegl',
  dependencies: resource_data,
  export: true,
)

thumbnailer_dbus = 'photos-thumbnailer-dbus'

common_sources += gnome.gdbus_codegen(
  thumbnailer_dbus,
  thumbnailer_dbus + '.xml',
  interface_prefix: photos_namespace + '.',
  namespace: 'Photos',
  annotations: [
    [photos_namespace + '.Thumbnailer', 'org.gtk.GDBus.C.Name', 'ThumbnailerDBus'],
    [photos_namespace + '.Thumbnailer.GenerateThumbnail()[pipeline_uris]', 'org.gtk.GDBus.C.ForceGVariant', 'true'],
  ],
  autocleanup: 'all',
)

common_deps = [
  babl_dep,
  gdk_pixbuf_dep,
  gegl_dep,
  gio_dep,
  gio_unix_dep,
  glib_dep,
  goa_dep,
  libdazzle_dep,
  libgnome_photos_dep,
  libjpeg_dep,
  libpng_dep,
  m_dep,
  tracker_sparql_dep,
]

sources = common_sources + files(
  'photos-application.c',
  'photos-base-manager.c',
  'photos-base-model.c',
  'photos-base-item.c',
  'photos-camera-cache.c',
  'photos-collection-icon-watcher.c',
  'photos-create-collection-icon-job.c',
  'photos-create-collection-job.c',
  'photos-delete-item-job.c',
  'photos-delete-notification.c',
  'photos-device-item.c',
  'photos-dlna-renderer.c',
  'photos-dlna-renderers-dialog.c',
  'photos-dlna-renderers-manager.c',
  'photos-done-notification.c',
  'photos-dropdown.c',
  'photos-export-dialog.c',
  'photos-export-notification.c',
  'photos-edit-palette.c',
  'photos-edit-palette-row.c',
  'photos-embed.c',
  'photos-empty-results-box.c',
  'photos-error-box.c',
  'photos-facebook-item.c',
  'photos-fetch-collection-state-job.c',
  'photos-fetch-collections-job.c',
  'photos-fetch-ids-job.c',
  'photos-fetch-metas-job.c',
  'photos-filterable.c',
  'photos-flickr-item.c',
  'photos-gesture-zoom.c',
  'photos-google-item.c',
  'photos-image-view.c',
  'photos-image-view-helper.c',
  'photos-import-dialog.c',
  'photos-indexing-notification.c',
  'photos-item-manager.c',
  'photos-local-item.c',
  'photos-main-toolbar.c',
  'photos-main-window.c',
  'photos-media-server-item.c',
  'photos-model-button.c',
  'photos-notification-manager.c',
  'photos-offset-collection-view-controller.c',
  'photos-offset-collections-controller.c',
  'photos-offset-controller.c',
  'photos-offset-favorites-controller.c',
  'photos-offset-import-controller.c',
  'photos-offset-overview-controller.c',
  'photos-offset-search-controller.c',
  'photos-organize-collection-dialog.c',
  'photos-organize-collection-model.c',
  'photos-organize-collection-view.c',
  'photos-overview-searchbar.c',
  'photos-preview-nav-buttons.c',
  'photos-preview-view.c',
  'photos-print-notification.c',
  'photos-print-operation.c',
  'photos-print-preview.c',
  'photos-print-setup.c',
  'photos-properties-dialog.c',
  'photos-query.c',
  'photos-query-builder.c',
  'photos-remote-display-manager.c',
  'photos-removable-device-widget.c',
  'photos-removable-devices-button.c',
  'photos-search-context.c',
  'photos-search-controller.c',
  'photos-search-match.c',
  'photos-search-match-manager.c',
  'photos-search-provider.c',
  'photos-search-type.c',
  'photos-search-type-manager.c',
  'photos-searchbar.c',
  'photos-selection-controller.c',
  'photos-selection-toolbar.c',
  'photos-set-collection-job.c',
  'photos-share-dialog.c',
  'photos-share-notification.c',
  'photos-share-point.c',
  'photos-share-point-email.c',
  'photos-share-point-google.c',
  'photos-share-point-manager.c',
  'photos-share-point-online.c',
  'photos-single-item-job.c',
  'photos-source.c',
  'photos-source-manager.c',
  'photos-source-notification.c',
  'photos-spinner-box.c',
  'photos-thumbnail-factory.c',
  'photos-tool.c',
  'photos-tool-colors.c',
  'photos-tool-crop.c',
  'photos-tool-crop-helper.c',
  'photos-tool-enhance.c',
  'photos-tool-filter-button.c',
  'photos-tool-filters.c',
  'photos-tracker-change-event.c',
  'photos-tracker-change-monitor.c',
  'photos-tracker-collection-view-controller.c',
  'photos-tracker-collections-controller.c',
  'photos-tracker-controller.c',
  'photos-tracker-favorites-controller.c',
  'photos-tracker-import-controller.c',
  'photos-tracker-overview-controller.c',
  'photos-tracker-search-controller.c',
  'photos-tracker-queue.c',
  'photos-update-mtime-job.c',
  'photos-utils.c',
  'photos-view-container.c',
  'photos-widget-shader.c',
  'photos-zoom-controls.c',
  'photos-main.c',
)

photos_generate_about = find_program('photos-generate-about')

about_header = 'photos-about-data.h'

sources += custom_target(
  about_header,
  output: about_header,
  capture: true,
  command: [photos_generate_about, '--header'],
)

about_source = 'photos-about-data.c'

sources += custom_target(
  about_source,
  output: about_source,
  capture: true,
  command: [photos_generate_about, '--body', artists, authors],
)

marshal = 'photos-marshalers'

sources += gnome.genmarshal(
  marshal,
  sources: marshal + '.list',
  prefix: '_photos_marshal',
)

resource_data = files(
  '../data/Adwaita.css',
  '../data/dnd-counter.svg',
  '../data/icons/image-adjust-color-symbolic.svg',
  '../data/icons/image-auto-adjust-symbolic.svg',
  '../data/icons/image-crop-symbolic.svg',
  '../data/icons/image-denoise-symbolic.svg',
  '../data/icons/image-edit-symbolic.svg',
  '../data/icons/image-filter-symbolic.svg',
  '../data/icons/image-sharpen-symbolic.svg',
  'photos-dlna-renderers-dialog.ui',
  'photos-dropdown.ui',
  'photos-embed.ui',
  'photos-export-dialog.ui',
  'photos-help-overlay.ui',
  'photos-import-dialog.ui',
  'photos-main-toolbar.ui',
  'photos-main-window.ui',
  'photos-menus.ui',
  'photos-preview-menu.ui',
  'photos-removable-devices-button.ui',
  'photos-removable-device-widget.ui',
  'photos-selection-menu.ui',
  'photos-selection-toolbar.ui',
  'photos-share-dialog.ui',
  'photos-zoom-controls.ui',
)

sources += gnome.compile_resources(
  'photos-resources',
  'photos.gresource.xml',
  source_dir: '.',
  c_name: 'photos',
  dependencies: resource_data,
  export: true,
)

dleyna_device = 'photos-dleyna-renderer-device'

sources += gnome.gdbus_codegen(
  dleyna_device,
  dleyna_device + '.xml',
  interface_prefix: 'com.intel.dLeynaRenderer.',
  namespace: 'Dleyna',
  annotations: [
    'com.intel.dLeynaRenderer.RendererDevice.GetIcon()[Bytes]', 'org.gtk.GDBus.C.ForceGVariant', 'true'
  ],
  autocleanup: 'all',
)

dleyna_manager = 'photos-dleyna-renderer-manager'

sources += gnome.gdbus_codegen(
  dleyna_manager,
  dleyna_manager + '.xml',
  interface_prefix: 'com.intel.dLeynaRenderer.',
  namespace: 'DleynaRenderer',
  autocleanup: 'all',
)

dleyna_push_host = 'photos-dleyna-renderer-push-host'

sources += gnome.gdbus_codegen(
  dleyna_push_host,
  dleyna_push_host + '.xml',
  interface_prefix: 'com.intel.dLeynaRenderer.',
  namespace: 'DleynaRenderer',
  autocleanup: 'all',
)

gom_miner = 'photos-gom-miner'

sources += gnome.gdbus_codegen(
  gom_miner,
  gom_miner + '.xml',
  interface_prefix: 'org.gnome.OnlineMiners.',
  namespace: 'Gom',
  autocleanup: 'all',
)

mpris_player = 'photos-mpris-player'

sources += gnome.gdbus_codegen(
  mpris_player,
  mpris_player + '.xml',
  interface_prefix: 'org.mpris.MediaPlayer2.',
  namespace: 'Mpris',
  autocleanup: 'all',
)

sources += gnome.gdbus_codegen(
  'photos-shell-search-provider2',
  'org.gnome.ShellSearchProvider2.xml',
  interface_prefix: 'org.gnome.Shell.',
  namespace: 'Shell',
  autocleanup: 'all',
)

tracker_extract_priority = 'photos-tracker-extract-priority'

sources += gnome.gdbus_codegen(
  tracker_extract_priority,
  tracker_extract_priority + '.xml',
  interface_prefix: 'org.freedesktop.Tracker1.',
  namespace: 'Tracker',
  autocleanup: 'all',
)

tracker_resources = 'photos-tracker-resources'

sources += gnome.gdbus_codegen(
  tracker_resources,
  tracker_resources + '.xml',
  interface_prefix: 'org.freedesktop.Tracker1.',
  namespace: 'Tracker',
  autocleanup: 'all',
)

deps = common_deps + [
  cairo_dep,
  geocode_glib_dep,
  gexiv_dep,
  grilo_dep,
  gsettings_desktop_schemas_dep,
  gtk_dep,
  gtk_unix_print_dep,
  libgd_dep,
  libgdata_dep,
  libgfgraph_dep,
  tracker_control_dep,
]

cflags = [
  '-DPACKAGE_LIBEXEC_DIR="@0@"'.format(photos_libexecdir),
  '-DPACKAGE_LOCALE_DIR="@0@"'.format(photos_localedir),
]

executable(
  meson.project_name(),
  sources,
  include_directories: top_inc,
  dependencies: deps,
  c_args: cflags,
  install: true,
  install_dir: photos_bindir,
  install_rpath: photos_libdir,
)

sources = common_sources + files(
  'photos-pixbuf.c',
  'photos-thumbnailer.c',
  'photos-thumbnailer-main.c',
)

executable(
  meson.project_name() + '-thumbnailer',
  sources,
  include_directories: top_inc,
  dependencies: common_deps,
  c_args: cflags,
  install: true,
  install_dir: photos_libexecdir,
  install_rpath: photos_libdir,
)
