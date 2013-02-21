AC_INIT([Photos],
        [3.7.4],
        [https://bugzilla.gnome.org/enter_bug.cgi?product=gnome-photos],
        [gnome-photos],
        [https://live.gnome.org/GnomePhotos])
AC_PREREQ(2.63)

AC_CONFIG_AUX_DIR([config])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([src])

m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

AM_INIT_AUTOMAKE([dist-xz no-dist-gzip tar-ustar])
AM_MAINTAINER_MODE([enable])

AC_PROG_CC
AC_PROG_CC_C_O
AC_HEADER_STDC

AC_CHECK_LIBM
AC_SUBST(LIBM)

GLIB_GSETTINGS

CLUTTER_GTK_MIN_VERSION=1.3.2
CLUTTER_MIN_VERSION=1.11
EXEMPI_MIN_VERSION=1.99.5
GLIB_MIN_VERSION=2.35.1
LIBEXIF_MIN_VERSION=0.6.14
LIBRSVG_MIN_VERSION=2.26.0

GNOME_COMMON_INIT
GNOME_COMPILE_WARNINGS([maximum])
GNOME_DEBUG_CHECK
GNOME_MAINTAINER_MODE_DEFINES

IT_PROG_INTLTOOL([0.35.0])

GETTEXT_PACKAGE=AC_PACKAGE_TARNAME
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED([GETTEXT_PACKAGE], ["$GETTEXT_PACKAGE"],
                   [GETTEXT package name])
AM_GLIB_GNU_GETTEXT

LT_INIT

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

PKG_CHECK_MODULES(CAIRO, [cairo])
PKG_CHECK_MODULES(CLUTTER, [clutter-1.0 >= $CLUTTER_MIN_VERSION])
PKG_CHECK_MODULES(CLUTTER_GTK, [clutter-gtk-1.0 >= $CLUTTER_GTK_MIN_VERSION])
PKG_CHECK_MODULES(EXEMPI, [exempi-2.0 >= $EXEMPI_MIN_VERSION])
PKG_CHECK_MODULES(GDK_PIXBUF, [gdk-pixbuf-2.0])
PKG_CHECK_MODULES(GLIB, [glib-2.0 >= $GLIB_MIN_VERSION])
PKG_CHECK_MODULES(GIO, [gio-2.0])

PKG_CHECK_MODULES(GNOME_DESKTOP, [gnome-desktop-3.0])
AC_DEFINE([GNOME_DESKTOP_USE_UNSTABLE_API], [], [We are aware that GNOME_DESKTOP's API can change])

PKG_CHECK_MODULES(GOA, [goa-1.0])
AC_DEFINE([GOA_API_IS_SUBJECT_TO_CHANGE], [], [We are aware that GOA's API can change])

PKG_CHECK_MODULES(GTK, [gtk+-3.0])
PKG_CHECK_MODULES(GTK_UNIX_PRINT, [gtk+-unix-print-3.0])
PKG_CHECK_MODULES(LCMS, [lcms2])
PKG_CHECK_MODULES(LIBEXIF, [libexif >= $LIBEXIF_MIN_VERSION])
PKG_CHECK_MODULES(LIBRSVG, [librsvg-2.0 >= $LIBRSVG_MIN_VERSION])
PKG_CHECK_MODULES(TRACKER, [tracker-miner-0.16 tracker-sparql-0.16])

LIBGD_INIT([
  main-toolbar
  main-icon-view
  main-view
  notification
  static
])

GLIB_COMPILE_RESOURCES=`$PKG_CONFIG --variable glib_compile_resources gio-2.0`
AC_SUBST(GLIB_COMPILE_RESOURCES)

AC_CONFIG_FILES([
Makefile
data/Makefile
data/gnome-photos.desktop.in
data/icons/Makefile
libgd/Makefile
src/Makefile
po/Makefile.in
])
AC_OUTPUT
