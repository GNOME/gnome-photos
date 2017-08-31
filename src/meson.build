common_sources = files(
  'egg-counter.c',
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
  'photos-quarks.c'
)

enum_headers = files(
  'photos-item-manager.h',
  'photos-operation-insta-common.h'
)

enum = 'photos-enums'

common_sources += gnome.mkenums(
  enum,
  sources: enum_headers,
  c_template: enum + '.c.template',
  h_template: enum + '.h.template'
)

resource_data = files('../data/vignette.png')

common_sources += gnome.compile_resources(
  'photos-resources-gegl',
  'photos-gegl.gresource.xml',
  source_dir: '.',
  c_name: 'photos_gegl',
  dependencies: resource_data,
  export: true
)

thumbnailer_dbus = 'photos-thumbnailer-dbus'

common_sources += gnome.gdbus_codegen(
  thumbnailer_dbus,
  thumbnailer_dbus + '.xml',
  interface_prefix: 'org.gnome.Photos.',
  namespace: 'Photos',
  annotations: ['org.gnome.Photos.Thumbnailer', 'org.gtk.GDBus.C.Name', 'ThumbnailerDBus']
)

sources = common_sources + files(
  'egg-animation.c',
  'egg-frame-source.c',
  'egg-heap.c',
  'egg-task-cache.c',
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
  'photos-google-item.c',
  'photos-header-bar.c',
  'photos-image-view.c',
  'photos-image-view-helper.c',
  'photos-indexing-notification.c',
  'photos-item-manager.c',
  'photos-local-item.c',
  'photos-main-toolbar.c',
  'photos-main-window.c',
  'photos-media-server-item.c',
  'photos-notification-manager.c',
  'photos-offset-collections-controller.c',
  'photos-offset-collection-view-controller.c',
  'photos-offset-controller.c',
  'photos-offset-favorites-controller.c',
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
  'photos-tool-enhance.c',
  'photos-tool-filter-button.c',
  'photos-tool-filters.c',
  'photos-tracker-change-event.c',
  'photos-tracker-change-monitor.c',
  'photos-tracker-collections-controller.c',
  'photos-tracker-collection-view-controller.c',
  'photos-tracker-controller.c',
  'photos-tracker-favorites-controller.c',
  'photos-tracker-overview-controller.c',
  'photos-tracker-search-controller.c',
  'photos-tracker-queue.c',
  'photos-update-mtime-job.c',
  'photos-utils.c',
  'photos-view-container.c',
  'photos-widget-shader.c',
  'photos-zoom-controls.c',
  'photos-main.c'
)

about_header = 'photos-about-data.h'

sources += custom_target(
  about_header,
  output: about_header,
  capture: true,
  depends: author,
  command: [find_program('photos-generate-about'), '--header', meson.build_root(), meson.source_root()]
)

about_source = 'photos-about-data.c'

sources += custom_target(
  about_source,
  output: about_source,
  capture: true,
  command: [find_program('photos-generate-about'), '--body', meson.build_root(), meson.source_root()]
)

marshal = 'photos-marshalers'

sources += gnome.genmarshal(
  marshal,
  sources: marshal + '.list',
  prefix: '_photos_marshal'
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
  'photos-main-toolbar.ui',
  'photos-main-window.ui',
  'photos-menus.ui',
  'photos-preview-menu.ui',
  'photos-selection-menu.ui',
  'photos-selection-toolbar.ui',
  'photos-share-dialog.ui',
  'photos-zoom-controls.ui'
)

sources += gnome.compile_resources(
  'photos-resources',
  'photos.gresource.xml',
  source_dir: '.',
  c_name: 'photos',
  dependencies: resource_data,
  export: true
)

dleyna_device = 'photos-dleyna-renderer-device'

sources += gnome.gdbus_codegen(
  dleyna_device,
  dleyna_device + '.xml',
  interface_prefix: 'com.intel.dLeynaRenderer.',
  namespace: 'Dleyna',
  annotations: ['com.intel.dLeynaRenderer.RendererDevice.GetIcon()[Bytes]', 'org.gtk.GDBus.C.ForceGVariant', 'true']
)

dleyna_manager = 'photos-dleyna-renderer-manager'

sources += gnome.gdbus_codegen(
  dleyna_manager,
  dleyna_manager + '.xml',
  interface_prefix: 'com.intel.dLeynaRenderer.',
  namespace: 'DleynaRenderer'
)

dleyna_push_host = 'photos-dleyna-renderer-push-host'

sources += gnome.gdbus_codegen(
  dleyna_push_host,
  dleyna_push_host + '.xml',
  interface_prefix: 'com.intel.dLeynaRenderer.',
  namespace: 'DleynaRenderer'
)

gom_miner = 'photos-gom-miner'

sources += gnome.gdbus_codegen(
  gom_miner,
  gom_miner + '.xml',
  interface_prefix: 'org.gnome.OnlineMiners.',
  namespace: 'Gom'
)

mpris_player = 'photos-mpris-player'

sources += gnome.gdbus_codegen(
  mpris_player,
  mpris_player + '.xml',
  interface_prefix: 'org.mpris.MediaPlayer2.',
  namespace: 'Mpris'
)

shell_search_provider = 'photos-'

sources += gnome.gdbus_codegen(
  'photos-shell-search-provider2',
  'org.gnome.ShellSearchProvider2.xml',
  interface_prefix: 'org.gnome.Shell.',
  namespace: 'Shell'
)

tracker_extract_priority = 'photos-tracker-extract-priority'

sources += gnome.gdbus_codegen(
  tracker_extract_priority,
  tracker_extract_priority + '.xml',
  interface_prefix: 'org.freedesktop.Tracker1.',
  namespace: 'Tracker'
)

tracker_resources = 'photos-tracker-resources'

sources += gnome.gdbus_codegen(
  tracker_resources,
  tracker_resources + '.xml',
  interface_prefix: 'org.freedesktop.Tracker1.',
  namespace: 'Tracker'
)

deps = common_deps + [
  goa_dep,
  libgd_dep,
  dependency('cairo', version: '>= 1.14.0'),
  dependency('cairo-gobject'),
  dependency('libgfbgraph-0.2', version: '>= 0.2.1'),
  dependency('libgdata', version: '>= 0.15.2'),
  dependency('geocode-glib-1.0'),
  dependency('gexiv2'),
  dependency('grilo-0.3', version: '>= 0.3.0'),
  dependency('gsettings-desktop-schemas'),
  dependency('gtk+-unix-print-3.0'),
  dependency('tracker-control-2.0')
]

cflags = [
  '-DPACKAGE_LIBEXEC_DIR="@0@"'.format(photos_libexecdir),
  '-DPACKAGE_LOCALE_DIR="@0@"'.format(photos_localedir)
]

executable(
  photos_name,
  sources,
  include_directories: top_inc,
  dependencies: deps,
  c_args: cflags,
  install: true,
  install_dir: photos_bindir
)

sources = common_sources + files(
  'photos-pixbuf.c',
  'photos-thumbnailer.c',
  'photos-thumbnailer-main.c'
)

executable(
  photos_name + '-thumbnailer',
  sources,
  include_directories: top_inc,
  dependencies: common_deps,
  c_args: cflags,
  install: true,
  install_dir: photos_libexecdir
)
