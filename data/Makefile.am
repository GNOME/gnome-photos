SUBDIRS = icons

appdatadir = $(datadir)/appdata
appdata_in_files = $(PACKAGE_TARNAME).appdata.xml.in
appdata_DATA = $(appdata_in_files:.xml.in=.xml)

desktopdir = $(datadir)/applications
desktop_in_in_files = $(PACKAGE_TARNAME).desktop.in.in
desktop_in_files = $(PACKAGE_TARNAME).desktop.in
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

EXTRA_DIST = \
	$(appdata_in_files) \
	$(desktop_in_in_files) \
	$(gsettings_SCHEMAS) \
	$(NULL)

CLEANFILES = \
	$(appdata_DATA) \
	$(desktop_DATA) \
	$(NULL)

DISTCLEANFILES = \
	$(desktop_in_files) \
	$(NULL)

-include $(top_srcdir)/git.mk
