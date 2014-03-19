include $(top_srcdir)/glib-tap.mk

dogtail_tests = \
	basic.py \
	$(NULL)

TESTS_ENVIRONMENT += \
	LC_ALL=C \
	GSETTINGS_BACKEND=memory \
	$(NULL)

if ENABLE_DOGTAIL
dist_test_scripts = $(dogtail_tests)
dist_test_data = \
	testutil.py \
	$(NULL)

CLEANFILES += \
	*.pyc \
	$(NULL)

else
EXTRA_DIST += \
	$(dogtail_tests) \
	testutil.py \
	$(NULL)
endif

include $(top_srcdir)/git.mk
