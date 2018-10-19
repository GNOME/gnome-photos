ACLOCAL_AMFLAGS = -I m4 -I subprojects/libgd ${ACLOCAL_FLAGS}

SUBDIRS = . data subprojects/libgd src po tests help

photosdocdir = $(docdir)
photosdoc_DATA = \
	ARTISTS \
	AUTHORS \
	NEWS \
	README \
	$(NULL)

EXTRA_DIST = \
	COPYING \
	$(photosdoc_DATA) \
	$(NULL)

CLEANFILES = \
	$(NULL)

DISTCLEANFILES = \
	$(NULL)

MAINTAINERCLEANFILES = \
	$(GITIGNORE_MAINTAINERCLEANFILES_M4_GETTEXT) \
	$(GITIGNORE_MAINTAINERCLEANFILES_M4_LIBTOOL) \
	$(GITIGNORE_MAINTAINERCLEANFILES_TOPLEVEL) \
	$(srcdir)/config.h.in \
	$(srcdir)/omf.make \
	$(srcdir)/xmldocs.make \
	$(srcdir)/gtk-doc.make \
	$(NULL)

GITIGNOREFILES = \
	config \
	$(NULL)

update-authors:
	$(top_srcdir)/update-authors.sh $(top_srcdir)

-include $(top_srcdir)/git.mk
