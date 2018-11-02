include $(top_srcdir)/glib-tap.mk

AM_TESTS_ENVIRONMENT += \
	GSETTINGS_BACKEND=memory \
	$(NULL)

dist_test_data = \
	photos-test-pipeline-edited-00.xml \
	photos-test-pipeline-not-edited-00.xml \
	$(NULL)

test_programs = \
	photos-test-pipeline \
	$(NULL)

photos_test_pipeline_CPPFLAGS = \
	-DPACKAGE_TEST_DIR=\""${abs_srcdir}"\" \
	$(GDK_PIXBUF_CFLAGS) \
	$(GEGL_CFLAGS) \
	$(GIO_CFLAGS) \
	$(GLIB_CFLAGS) \
	-I$(top_builddir)/src \
	-I$(top_srcdir)/src \
	$(NULL)

photos_test_pipeline_LDADD = \
	$(GDK_PIXBUF_LIBS) \
	$(GEGL_LIBS) \
	$(GIO_LIBS) \
	$(GLIB_LIBS) \
	$(top_builddir)/src/libgnome-photos.la \
	$(NULL)

AM_CFLAGS = \
	$(WARN_CFLAGS) \
	$(NULL)

AM_LDFLAGS = \
	$(WARN_LDFLAGS) \
	$(NULL)

-include $(top_srcdir)/git.mk
