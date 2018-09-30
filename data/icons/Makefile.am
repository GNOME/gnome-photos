public_icons_scalabledir = $(datadir)/icons/hicolor/scalable/apps
public_icons_scalable_DATA = hicolor/scalable/apps/org.gnome.Photos.svg

public_icons_symbolicdir = $(datadir)/icons/hicolor/symbolic/apps
public_icons_symbolic_DATA = hicolor/symbolic/apps/org.gnome.Photos-symbolic.svg

noinst_DATA = \
	image-adjust-color-symbolic.svg \
	image-auto-adjust-symbolic.svg \
	image-crop-symbolic.svg \
	image-denoise-symbolic.svg \
	image-edit-symbolic.svg \
	image-filter-symbolic.svg \
	image-sharpen-symbolic.svg \
	$(NULL)

EXTRA_DIST = \
	$(public_icons_scalable_DATA) \
	$(public_icons_symbolic_DATA) \
	$(noinst_DATA) \
	$(NULL)

gtk_update_icon_cache = gtk-update-icon-cache -f -t $(datadir)/icons/hicolor

update-icon-cache:
	@-if test -z "$(DESTDIR)"; then \
		echo "Updating Gtk icon cache."; \
                $(gtk_update_icon_cache); \
	else \
		echo "*** Icon cache not updated.  After (un)install, run this:"; \
                echo "***   $(gtk_update_icon_cache)"; \
	fi

install-data-hook: update-icon-cache

uninstall-hook: update-icon-cache

-include $(top_srcdir)/git.mk
