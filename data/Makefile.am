SUBDIRS =

desktopdir = $(datadir)/applications
desktop_in_files = $(PACKAGE_TARNAME).desktop.in.in
desktop_DATA = $(desktop_in_files:.desktop.in.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

EXTRA_DIST = \
	$(desktop_in_files) \
	$(NULL)

DISTCLEANFILES = \
	$(desktop_DATA)
