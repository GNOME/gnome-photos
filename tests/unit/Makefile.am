include $(top_srcdir)/glib-tap.mk

TESTS_ENVIRONMENT += \
	GSETTINGS_BACKEND=memory \
	$(NULL)

test_programs = \
	photos-test-pipeline
	$(NULL)

AM_CFLAGS = \
	$(WARN_CFLAGS) \
	$(NULL)

AM_CPPFLAGS = \
	$(GLIB_CFLAGS) \
	-I$(top_builddir)/src \
	-I$(top_srcdir)/src \
	$(NULL)

LDADD = \
	$(GLIB_LIBS) \
	$(top_builddir)/src/libgnome-photos.la \
	$(NULL)

AM_LDFLAGS = \
	$(WARN_LDFLAGS) \
	$(NULL)

-include $(top_srcdir)/git.mk
