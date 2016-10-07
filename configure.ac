AC_INIT([Photos],
        [3.20.4],
        [https://bugzilla.gnome.org/enter_bug.cgi?product=gnome-photos],
        [gnome-photos],
        [https://wiki.gnome.org/Apps/Photos])
AC_PREREQ(2.63)

AC_CONFIG_AUX_DIR([config])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([src])

m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

AM_INIT_AUTOMAKE([dist-xz no-dist-gzip serial-tests tar-ustar])
AM_MAINTAINER_MODE([enable])

AX_IS_RELEASE([git-directory])

AX_CHECK_ENABLE_DEBUG([yes],,, [$ax_is_release])
AX_COMPILER_FLAGS([WARN_CFLAGS], [WARN_LDFLAGS], [$ax_is_release])

AC_PROG_CC
AC_PROG_CC_C_O
AC_HEADER_STDC

AC_CHECK_LIBM
AC_SUBST(LIBM)

GLIB_GSETTINGS

CAIRO_MIN_VERSION=1.14.0
GDATA_MIN_VERSION=0.15.2
GEGL_MIN_VERSION=0.3.5
GFBGRAPH_MIN_VERSION=0.2.1
GLIB_MIN_VERSION=2.39.3
GOA_MIN_VERSION=3.8.0
GRILO_MIN_VERSION=0.3.0
GTK_MIN_VERSION=3.19.1

AX_REQUIRE_DEFINED([IT_PROG_INTLTOOL])
IT_PROG_INTLTOOL([0.50.1])

GETTEXT_PACKAGE=AC_PACKAGE_TARNAME
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED([GETTEXT_PACKAGE], ["$GETTEXT_PACKAGE"],
                   [GETTEXT package name])
AM_GLIB_GNU_GETTEXT

LT_INIT

AC_CHECK_LIB([jpeg], [jpeg_destroy_decompress], , AC_MSG_ERROR([JPEG library not found]))
AC_MSG_CHECKING([for jpeglib.h])
AC_PREPROC_IFELSE([AC_LANG_SOURCE([[
#include <stdio.h>
#undef HAVE_STDDEF_H
#undef HAVE_STDLIB_H
#undef PACKAGE
#undef VERSION
#include <jpeglib.h>]])],
                  AC_MSG_RESULT([yes]),
                  AC_MSG_ERROR([JPEG header file not found]))
AC_CHECK_LIB([jpeg], [jpeg_save_markers], JPEG_LIBS='-ljpeg', AC_MSG_ERROR([JPEG library is too old]))
AC_SUBST(JPEG_LIBS)

# ****************************************************************
# Support for nl_langinfo (_NL_MEASUREMENT_MEASUREMENT) (optional)
# ****************************************************************
AC_MSG_CHECKING([for _NL_MEASUREMENT_MEASUREMENT])
AC_TRY_LINK([#include <langinfo.h>], [
char c;
c = *((unsigned char *)  nl_langinfo(_NL_MEASUREMENT_MEASUREMENT));
], have_langinfo_measurement=yes, have_langinfo_measurement=no)
AC_MSG_RESULT($have_langinfo_measurement)
if test "$have_langinfo_measurement" = "yes"; then
  AC_DEFINE([HAVE__NL_MEASUREMENT_MEASUREMENT], [1],
            [Define if _NL_MEASUREMENT_MEASUREMENT is available])
fi

PKG_CHECK_MODULES(BABL, [babl])
PKG_CHECK_MODULES(CAIRO, [cairo >= $CAIRO_MIN_VERSION cairo-gobject])
PKG_CHECK_MODULES(GFBGRAPH, [libgfbgraph-0.2 >= $GFBGRAPH_MIN_VERSION])
PKG_CHECK_MODULES(GDATA, [libgdata >= $GDATA_MIN_VERSION])
PKG_CHECK_MODULES(GEGL, [gegl-0.3 >= $GEGL_MIN_VERSION])
PKG_CHECK_MODULES(GEXIV2, [gexiv2])
PKG_CHECK_MODULES(GDK_PIXBUF, [gdk-pixbuf-2.0])
PKG_CHECK_MODULES(GLIB, [glib-2.0 >= $GLIB_MIN_VERSION])
PKG_CHECK_MODULES(GIO, [gio-2.0 gio-unix-2.0])

PKG_CHECK_MODULES(GNOME_DESKTOP, [gnome-desktop-3.0])
AC_DEFINE([GNOME_DESKTOP_USE_UNSTABLE_API], [], [We are aware that GNOME_DESKTOP's API can change])

PKG_CHECK_MODULES(GOA, [goa-1.0 >= $GOA_MIN_VERSION])
AC_DEFINE([GOA_API_IS_SUBJECT_TO_CHANGE], [], [We are aware that GOA's API can change])

PKG_CHECK_MODULES(GRILO, [grilo-0.3 >= $GRILO_MIN_VERSION])
PKG_CHECK_MODULES(GTK, [gtk+-3.0 >= $GTK_MIN_VERSION])
PKG_CHECK_MODULES(GTK_UNIX_PRINT, [gtk+-unix-print-3.0])
PKG_CHECK_MODULES(PNG, [libpng16])
PKG_CHECK_MODULES(TRACKER, [tracker-control-1.0 tracker-sparql-1.0])

AX_REQUIRE_DEFINED([LIBGD_INIT])
LIBGD_INIT([
  gtk-hacks
  main-icon-view
  main-view
  notification
  static
  tagged-entry
])

AC_ARG_ENABLE(dogtail,
              [AS_HELP_STRING([--enable-dogtail],
                              [test using dogtail [default=yes]])],,
              enable_dogtail=yes)
AM_CONDITIONAL(ENABLE_DOGTAIL, test "$enable_dogtail" != no)

AX_REQUIRE_DEFINED([GLIB_TESTS])
GLIB_TESTS

GLIB_COMPILE_RESOURCES=`$PKG_CONFIG --variable glib_compile_resources gio-2.0`
AC_SUBST(GLIB_COMPILE_RESOURCES)

AC_ARG_VAR([DESKTOP_FILE_VALIDATE], [the desktop-file-validate program])
AC_PATH_PROG([DESKTOP_FILE_VALIDATE], [desktop-file-validate], [])
if test -z "$DESKTOP_FILE_VALIDATE"; then
  AC_MSG_ERROR([desktop-file-validate not found])
fi

#############################################
#Documentation
#############################################

AX_REQUIRE_DEFINED([YELP_HELP_INIT])
YELP_HELP_INIT

# work around intltool-update issues during 'make distcheck'
AS_IF([test "x$0" != "x./configure"], [
  AC_SUBST([INTLTOOL_UPDATE], [/bin/true])
])

AC_CONFIG_FILES([
Makefile
data/Makefile
data/org.gnome.Photos.desktop.in
data/icons/Makefile
help/Makefile
libgd/Makefile
src/Makefile
tests/Makefile
po/Makefile.in
])
AC_OUTPUT
