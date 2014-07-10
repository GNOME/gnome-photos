SUBDIRS = icons

appdatadir = $(datadir)/appdata
appdata_in_files = org.gnome.Photos.appdata.xml.in
appdata_DATA = $(appdata_in_files:.xml.in=.xml)

desktopdir = $(datadir)/applications
desktop_in_in_files = org.gnome.Photos.desktop.in.in
desktop_in_files = org.gnome.Photos.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

gsettings_SCHEMAS = org.gnome.photos.gschema.xml
.PRECIOUS: $(gsettings_SCHEMAS)

iconsdir = $(pkgdatadir)/icons
dist_icons_DATA = \
	thumbnail-frame.png \
	$(NULL)

@INTLTOOL_XML_RULE@
@GSETTINGS_RULES@

searchproviderdir = $(datadir)/gnome-shell/search-providers
dist_searchprovider_DATA = org.gnome.Photos.search-provider.ini

servicedir = $(datadir)/dbus-1/services
service_in_files = org.gnome.Photos.service.in
service_DATA = $(service_in_files:.service.in=.service)

EXTRA_DIST = \
	$(appdata_in_files) \
	$(desktop_in_in_files) \
	$(gsettings_SCHEMAS) \
	$(service_in_files) \
	dnd-counter.svg \
	sidebar-radio-checked.svg \
	sidebar-radio-prelight.svg \
	sidebar-radio-selected.svg \
	sidebar-radio-selected-prelight.svg \
	$(NULL)

CLEANFILES = \
	$(appdata_DATA) \
	$(desktop_DATA) \
	$(service_DATA) \
	$(NULL)

DISTCLEANFILES = \
	$(desktop_in_files) \
	$(NULL)

desktop-file-validate: $(desktop_DATA)
	for f in $^; do \
		$(DESKTOP_FILE_VALIDATE) $$f; \
	done

check-local: desktop-file-validate

.PHONY: desktop-file-validate

org.gnome.Photos.service: org.gnome.Photos.service.in
	$(AM_V_GEN)	\
		[ -d $(@D) ] || $(mkdir_p) $(@D) ; \
		sed -e "s|\@bindir\@|$(bindir)|" $< > $@.tmp && mv $@.tmp $@

-include $(top_srcdir)/git.mk
