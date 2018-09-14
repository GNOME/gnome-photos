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

name_pattern = re.compile('hicolor_(?:apps)_(?:\d+x\d+|symbolic)_(.*)')
search_pattern = '/**/hicolor_*'

for file in glob.glob(icondir + search_pattern, recursive=True):
    os.rename(file, os.path.join(os.path.dirname(file), name_pattern.search(file).group(1)))

if not destdir:
    print('Updating icon cache...')
    subprocess.call(['gtk-update-icon-cache', '-f', '-t', icondir])

    schemadir = os.path.join(datadir, 'glib-2.0', 'schemas')
    print('Compiling gsettings schemas...')
    subprocess.call(['glib-compile-schemas', schemadir])
