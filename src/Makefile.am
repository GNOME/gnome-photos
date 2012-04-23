bin_PROGRAMS = gnome-photos

gnome_photos_SOURCES = \
	photos-application.c \
	photos-application.h \
	photos-main-window.c \
	photos-main-window.h \
	photos-main.c \
	$(NULL)

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
	$(NULL)
