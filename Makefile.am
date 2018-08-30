ACLOCAL_AMFLAGS = -I m4 -I subprojects/libgd ${ACLOCAL_FLAGS}

SUBDIRS = . data subprojects/libgd src po tests help

photosdocdir = $(docdir)
photosdoc_DATA = \
	ARTISTS \
	AUTHORS \
	COPYING \
	NEWS \
	README \
	$(NULL)

EXTRA_DIST = \
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
	@if test -d "$(srcdir)/.git"; \
	then \
		echo Updating AUTHORS && \
		( cd "$(top_srcdir)" && \
		  echo '# Generated — do not edit.'; echo; \
		  git log --no-merges --pretty=format:"%an" src \
		  | sort | uniq ) > AUTHORS.tmp \
		&& mv -f AUTHORS.tmp AUTHORS \
		|| ( rm -f AUTHORS.tmp ; \
		     echo Failed to generate AUTHORS >&2 ); \
	fi

-include $(top_srcdir)/git.mk
