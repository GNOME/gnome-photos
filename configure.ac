AC_INIT([Photos],
        [3.15.3],
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

AC_PROG_CC
AC_PROG_CC_C_O
AC_HEADER_STDC

AC_CHECK_LIBM
AC_SUBST(LIBM)

GLIB_GSETTINGS

CAIRO_MIN_VERSION=1.14.0
EXEMPI_MIN_VERSION=1.99.5
GDATA_MIN_VERSION=0.15.2
GFBGRAPH_MIN_VERSION=0.2.1
GLIB_MIN_VERSION=2.39.3
GOA_MIN_VERSION=3.8.0
GRILO_MIN_VERSION=0.2.6
GTK_MIN_VERSION=3.15.0
LIBEXIF_MIN_VERSION=0.6.14
LIBRSVG_MIN_VERSION=2.26.0

GNOME_COMPILE_WARNINGS([maximum])
GNOME_DEBUG_CHECK
GNOME_MAINTAINER_MODE_DEFINES

IT_PROG_INTLTOOL([0.50.1])

GETTEXT_PACKAGE=AC_PACKAGE_TARNAME
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED([GETTEXT_PACKAGE], ["$GETTEXT_PACKAGE"],
                   [GETTEXT package name])
AM_GLIB_GNU_GETTEXT

LT_INIT

# Used to keep eog-exif-util.c and eog-util.c aligned with EOG
AC_CHECK_FUNCS(strptime)

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

# ***************
# ZLIB (required)
# ***************

have_zlib=yes
AC_CHECK_HEADERS([zlib.h],
	[AC_CHECK_LIB([z], [inflate],
		[AC_CHECK_LIB([z], [crc32], [], [have_zlib=no])],
		[have_zlib=no])],
	[have_zlib=no])

if test x$have_zlib = xno; then
	AC_MSG_ERROR([No sufficient zlib library found on your system.])
fi

PKG_CHECK_MODULES(BABL, [babl])

PKG_CHECK_MODULES(CAIRO, [cairo >= $CAIRO_MIN_VERSION cairo-gobject])
AC_DEFINE([HAVE_CAIRO_GOBJECT], [], [We want gegl-gtk to use the cairo-gobject code path])

PKG_CHECK_MODULES(EXEMPI, [exempi-2.0 >= $EXEMPI_MIN_VERSION])
PKG_CHECK_MODULES(GFBGRAPH, [libgfbgraph-0.2 >= $GFBGRAPH_MIN_VERSION])
PKG_CHECK_MODULES(GDATA, [libgdata >= $GDATA_MIN_VERSION])
PKG_CHECK_MODULES(GEGL, [gegl-0.2])
PKG_CHECK_MODULES(GDK_PIXBUF, [gdk-pixbuf-2.0])
PKG_CHECK_MODULES(GLIB, [glib-2.0 >= $GLIB_MIN_VERSION])
PKG_CHECK_MODULES(GIO, [gio-2.0])

PKG_CHECK_MODULES(GNOME_DESKTOP, [gnome-desktop-3.0])
AC_DEFINE([GNOME_DESKTOP_USE_UNSTABLE_API], [], [We are aware that GNOME_DESKTOP's API can change])

PKG_CHECK_MODULES(GOA, [goa-1.0 >= $GOA_MIN_VERSION])
AC_DEFINE([GOA_API_IS_SUBJECT_TO_CHANGE], [], [We are aware that GOA's API can change])

PKG_CHECK_MODULES(GRILO, [grilo-0.2 >= $GRILO_MIN_VERSION])

PKG_CHECK_MODULES(GTK, [gtk+-3.0 >= $GTK_MIN_VERSION])
AC_DEFINE([HAVE_GTK3], [], [We want gegl-gtk to use the GTK+ 3.x code path])

PKG_CHECK_MODULES(GTK_UNIX_PRINT, [gtk+-unix-print-3.0])
PKG_CHECK_MODULES(LCMS, [lcms2])
PKG_CHECK_MODULES(LIBEXIF, [libexif >= $LIBEXIF_MIN_VERSION])
PKG_CHECK_MODULES(LIBRSVG, [librsvg-2.0 >= $LIBRSVG_MIN_VERSION])
PKG_CHECK_MODULES(TRACKER, [tracker-control-1.0 tracker-sparql-1.0])

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

YELP_HELP_INIT

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
