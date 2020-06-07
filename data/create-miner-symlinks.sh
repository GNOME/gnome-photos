#!/bin/bash

mkdir -p $MESON_INSTALL_DESTDIR_PREFIX/share/tracker/miners/
for miner in org.gnome.Photos.Tracker1.Miner.Files org.gnome.Photos.Tracker1.Miner.Extract; do
    ln -sf $MESON_INSTALL_PREFIX/share/dbus-1/services/$miner.service $MESON_INSTALL_DESTDIR_PREFIX/share/tracker/miners/
done
