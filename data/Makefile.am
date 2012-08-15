SUBDIRS =

desktopdir = $(datadir)/applications
desktop_in_in_files = $(PACKAGE_TARNAME).desktop.in.in
desktop_in_files = $(PACKAGE_TARNAME).desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

gsettings_in_files = org.gnome.photos.gschema.xml.in
gsettings_SCHEMAS = $(gsettings_in_files:.xml.in=.xml)
.PRECIOUS: $(gsettings_SCHEMAS)

iconsdir = $(pkgdatadir)/icons
dist_icons_DATA = \
	thumbnail-frame.png \
	$(NULL)

@INTLTOOL_XML_NOMERGE_RULE@
@GSETTINGS_RULES@

EXTRA_DIST = \
	$(desktop_in_in_files) \
	$(gsettings_in_files) \
	$(NULL)

CLEANFILES = \
	$(desktop_DATA) \
	$(gsettings_SCHEMAS) \
	$(NULL)

DISTCLEANFILES = \
	$(desktop_in_files) \
	$(NULL)
