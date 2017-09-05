icondir = ${datadir}/icons/hicolor

public_16x16_icondir = $(icondir)/16x16/apps
public_16x16_icon_DATA = hicolor/16x16/apps/org.gnome.Photos.png

public_22x22_icondir = $(icondir)/22x22/apps
public_22x22_icon_DATA = hicolor/22x22/apps/org.gnome.Photos.png

public_24x24_icondir = $(icondir)/24x24/apps
public_24x24_icon_DATA = hicolor/24x24/apps/org.gnome.Photos.png

public_32x32_icondir = $(icondir)/32x32/apps
public_32x32_icon_DATA = hicolor/32x32/apps/org.gnome.Photos.png

public_48x48_icondir = $(icondir)/48x48/apps
public_48x48_icon_DATA = hicolor/48x48/apps/org.gnome.Photos.png

public_256x256_icondir = $(icondir)/256x256/apps
public_256x256_icon_DATA = hicolor/256x256/apps/org.gnome.Photos.png

public_scalable_icondir = $(icondir)/scalable/apps
public_scalable_icon_DATA = hicolor/scalable/apps/org.gnome.Photos-symbolic.svg

noinst_DATA = \
	hicolor_org.gnome.Photos.svg \
	image-adjust-color-symbolic.svg \
	image-auto-adjust-symbolic.svg \
	image-crop-symbolic.svg \
	image-denoise-symbolic.svg \
	image-edit-symbolic.svg \
	image-filter-symbolic.svg \
	image-sharpen-symbolic.svg \
	$(NULL)

EXTRA_DIST = \
	$(public_16x16_icon_DATA) \
	$(public_22x22_icon_DATA) \
	$(public_24x24_icon_DATA) \
	$(public_32x32_icon_DATA) \
	$(public_48x48_icon_DATA) \
	$(public_256x256_icon_DATA) \
	$(public_scalable_icon_DATA) \
	$(noinst_DATA) \
	$(NULL)

gtk_update_icon_cache = gtk-update-icon-cache -f -t

update-icon-cache:
	@-if test -z "$(DESTDIR)"; then \
		echo "Updating Gtk icon cache."; \
		$(gtk_update_icon_cache) $(icondir); \
	else \
		echo "*** Icon cache not updated.  After (un)install, run this:"; \
		echo "***   $(gtk_update_icon_cache) $(icondir)"; \
	fi

install-data-hook: update-icon-cache

uninstall-hook: update-icon-cache

-include $(top_srcdir)/git.mk
