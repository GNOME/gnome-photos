SUBDIRS =

desktopdir = $(datadir)/applications
desktop_in_files = $(PACKAGE_TARNAME).desktop.in.in
desktop_DATA = $(desktop_in_files:.desktop.in.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

gsettings_in_files = org.gnome.photos.gschema.xml.in
gsettings_SCHEMAS = $(gsettings_in_files:.xml.in=.xml)
.PRECIOUS: $(gsettings_SCHEMAS)
@INTLTOOL_XML_NOMERGE_RULE@
@GSETTINGS_RULES@

EXTRA_DIST = \
	$(desktop_in_files) \
	$(gsettings_in_files) \
	$(NULL)

DISTCLEANFILES = \
	$(desktop_DATA) \
	$(gsettings_SCHEMAS) \
	$(NULL)
