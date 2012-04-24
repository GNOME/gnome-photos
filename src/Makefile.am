bin_PROGRAMS = gnome-photos

gnome_photos_built_sources = \
	photos-enums.c \
	photos-enums.h \
	photos-marshalers.c \
	photos-marshalers.h \
	$(NULL)

gnome_photos_SOURCES = \
	$(gnome_photos_built_sources) \
	gd-main-toolbar.c \
	gd-main-toolbar.h \
	photos-application.c \
	photos-application.h \
	photos-base-manager.c \
	photos-base-manager.h \
	photos-main-toolbar.c \
	photos-main-toolbar.h \
	photos-main-window.c \
	photos-main-window.h \
	photos-mode-controller.c \
	photos-mode-controller.h \
	photos-selection-controller.c \
	photos-selection-controller.h \
	photos-main.c \
	$(NULL)

BUILT_SOURCES = \
	$(gnome_photos_built_sources) \
	$(NULL)

EXTRA_DIST = \
	photos-enums.c.template \
	photos-enums.h.template \
	photos-marshalers.list \
	$(null)

AM_CPPFLAGS = \
	-DPACKAGE_LOCALE_DIR=\""${datadir}/locale"\" \
	$(CLUTTER_GTK_CFLAGS) \
	$(GIO_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	$(TRACKER_CFLAGS) \
	$(NULL)

gnome_photos_LDFLAGS = 

gnome_photos_LDADD = \
	$(CLUTTER_GTK_LIBS) \
	$(GIO_LIBS) \
	$(GLIB_LIBS) \
	$(GTK_LIBS) \
	$(TRACKER_LIBS) \
	$(LIBM) \
	$(NULL)

CLEANFILES = \
	xgen-etbc \
	xgen-etbh \
	xgen-tmc \
	xgen-tmh \
	$(NULL)

DISTCLEANFILES = \
	$(gnome_photos_built_sources) \
	stamp-photos-enums.h \
	stamp-photos-marshalers.h \
	$(NULL)

photos-enums.h: stamp-photos-enums.h
	@true

stamp-photos-enums.h: photos-enums.h.template
	$(AM_V_GEN) ( \
		cd $(srcdir) \
		&& ( glib-mkenums \
			--template photos-enums.h.template $(gnome_photos_SOURCES) ) >> xgen-etbh \
		&& ( cmp -s xgen-etbh photos-enums.h || cp xgen-etbh photos-enums.h ) \
		&& rm -f xgen-etbh \
		&& echo timestamp > $(@F) \
	)

photos-enums.c: photos-enums.c.template
	$(AM_V_GEN) ( \
		cd $(srcdir) \
		&& ( glib-mkenums \
			--template photos-enums.c.template $(gnome_photos_SOURCES) ) >> xgen-etbc \
		&& ( cmp -s xgen-etbc photos-enums.c || cp xgen-etbc photos-enums.c ) \
		&& rm -f xgen-etbc \
	)

photos-marshalers.h: stamp-photos-marshalers.h
	@true

stamp-photos-marshalers.h: photos-marshalers.list Makefile
	$(AM_V_GEN) ( \
		cd $(srcdir) \
		&& ( glib-genmarshal \
			--prefix=_photos_marshal \
			--header photos-marshalers.list ) >> xgen-tmh \
		&& ( cmp -s xgen-tmh photos-marshalers.h || cp xgen-tmh photos-marshalers.h ) \
		&& rm -f xgen-tmh \
		&& echo timestamp > $(@F) \
	)

photos-marshalers.c: photos-marshalers.list photos-marshalers.h Makefile
	$(AM_V_GEN) ( \
		cd $(srcdir) \
		&& echo "#include \"photos-marshalers.h\"" > xgen-tmc \
		&& ( glib-genmarshal \
			--prefix=_photos_marshal \
			--body photos-marshalers.list ) >> xgen-tmc \
		&& cp xgen-tmc photos-marshalers.c \
		&& rm -f xgen-tmc \
	)
