#!/usr/bin/env python3

import glob
import os
import re
import subprocess
import sys

destdir = os.environ.get('DESTDIR', '')

if destdir:
    datadir = os.path.normpath(destdir + os.sep + sys.argv[1])
else:
    datadir = sys.argv[1]

icondir = os.path.join(datadir, 'icons', 'hicolor')

name_pattern = re.compile('hicolor_(?:apps)_(?:scalable|symbolic)_(.*)')
search_pattern = '/**/hicolor_*'

for file in glob.glob(icondir + search_pattern, recursive=True):
    os.rename(file, os.path.join(os.path.dirname(file), name_pattern.search(file).group(1)))

if not destdir:
    print('Updating icon cache...')
    subprocess.call(['gtk-update-icon-cache', '-f', '-t', icondir])

    schemadir = os.path.join(datadir, 'glib-2.0', 'schemas')
    print('Compiling gsettings schemas...')
    subprocess.call(['glib-compile-schemas', schemadir])

dbusservicesdir = sys.argv[2]
trackerdir = os.path.join(datadir, 'tracker', 'miners')

miners = ["org.gnome.Photos.Tracker1.Miner.Extract.service", "org.gnome.Photos.Tracker1.Miner.Files.service"]

os.makedirs(trackerdir, exist_ok=True)
for miner in miners:
    dst = os.path.join(trackerdir, miner)
    src = os.path.join(dbusservicesdir, miner)
    try:
        os.symlink(src, dst)
    except FileExistsError:
        pass
