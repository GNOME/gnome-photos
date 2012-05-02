bin_PROGRAMS = gnome-photos

gnome_photos_built_sources = \
	photos-enums.c \
	photos-enums.h \
	photos-marshalers.c \
	photos-marshalers.h \
	$(NULL)

gnome_photos_SOURCES = \
	$(gnome_photos_built_sources) \
	gd-main-icon-view.c \
	gd-main-icon-view.h \
	gd-main-list-view.c \
	gd-main-list-view.h \
	gd-main-toolbar.c \
	gd-main-toolbar.h \
	gd-main-view.c \
	gd-main-view.h \
	gd-main-view-generic.c \
	gd-main-view-generic.h \
	gd-styled-text-renderer.c \
	gd-styled-text-renderer.h \
	gd-toggle-pixbuf-renderer.c \
	gd-toggle-pixbuf-renderer.h \
	gd-two-lines-renderer.c \
	gd-two-lines-renderer.h \
	photos-application.c \
	photos-application.h \
	photos-base-manager.c \
	photos-base-manager.h \
	photos-base-item.c \
	photos-base-item.h \
	photos-collection-manager.c \
	photos-collection-manager.h \
	photos-item-model.c \
	photos-item-model.h \
	photos-view-embed.c \
	photos-view-embed.h \
	photos-main-toolbar.c \
	photos-main-toolbar.h \
	photos-main-window.c \
	photos-main-window.h \
	photos-mode-controller.c \
	photos-mode-controller.h \
	photos-offset-controller.c \
	photos-offset-controller.h \
	photos-organize-collection-dialog.c \
	photos-organize-collection-dialog.h \
	photos-organize-collection-model.c \
	photos-organize-collection-model.h \
	photos-organize-collection-view.c \
	photos-organize-collection-view.h \
	photos-query.h \
	photos-query-builder.h \
	photos-query-builder.c \
	photos-selection-controller.c \
	photos-selection-controller.h \
	photos-selection-toolbar.c \
	photos-selection-toolbar.h \
	photos-settings.c \
	photos-settings.h \
	photos-source.c \
	photos-source.h \
	photos-source-manager.c \
	photos-source-manager.h \
	photos-utils.c \
	photos-utils.h \
	photos-view.c \
	photos-view.h \
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
	$(CLUTTER_CFLAGS) \
	$(CLUTTER_GTK_CFLAGS) \
	$(GDK_PIXBUF_CFLAGS) \
	$(GIO_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(GOA_CFLAGS) \
	$(GTK_CFLAGS) \
	$(TRACKER_CFLAGS) \
	$(NULL)

gnome_photos_LDFLAGS = 

gnome_photos_LDADD = \
	$(CLUTTER_LIBS) \
	$(CLUTTER_GTK_LIBS) \
	$(GDK_PIXBUF_LIBS) \
	$(GIO_LIBS) \
	$(GLIB_LIBS) \
	$(GOA_LIBS) \
	$(GTK_LIBS) \
	$(TRACKER_LIBS) \
	$(LIBM) \
	$(NULL)

if MAINTAINER_MODE

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
			--template photos-enums.h.template \
                        photos-mode-controller.h \
                   ) >> xgen-etbh \
		&& ( cmp -s xgen-etbh photos-enums.h || cp xgen-etbh photos-enums.h ) \
		&& rm -f xgen-etbh \
		&& echo timestamp > $(@F) \
	)

photos-enums.c: photos-enums.c.template
	$(AM_V_GEN) ( \
		cd $(srcdir) \
		&& ( glib-mkenums \
			--template photos-enums.c.template \
                        photos-mode-controller.h \
                   ) >> xgen-etbc \
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

endif
