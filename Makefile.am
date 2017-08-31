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

# There is no way to generate the AUTHORS in tarball builds
EXTRA_DIST = \
	$(photosdoc_DATA) \
	AUTHORS \
	$(NULL)

# We don't want to clean the AUTHORS in tarball builds
CLEANFILES = \
	$(NULL)

DISTCLEANFILES = \
	$(NULL)

MAINTAINERCLEANFILES = \
	$(GITIGNORE_MAINTAINERCLEANFILES_M4_GETTEXT) \
	$(GITIGNORE_MAINTAINERCLEANFILES_M4_LIBTOOL) \
	$(GITIGNORE_MAINTAINERCLEANFILES_TOPLEVEL) \
	$(srcdir)/AUTHORS \
	$(srcdir)/config.h.in \
	$(srcdir)/omf.make \
	$(srcdir)/xmldocs.make \
	$(srcdir)/gtk-doc.make \
	$(NULL)

GITIGNOREFILES = \
	config \
	$(NULL)

AUTHORS:
	@if test -d "$(srcdir)/.git"; \
	then \
		echo Creating $@ && \
		( cd "$(top_srcdir)" && \
		  echo '# Generated — do not edit.'; echo; \
		  git log --no-merges --pretty=format:"%an" src \
		  | sort | uniq ) > $@.tmp \
		&& mv -f $@.tmp $@ \
		|| ( rm -f $@.tmp ; \
		     echo Failed to generate $@ >&2 ); \
	fi

update-authors:
	$(top_srcdir)/update-authors.sh $(top_srcdir) AUTHORS

.PHONY: AUTHORS

-include $(top_srcdir)/git.mk
