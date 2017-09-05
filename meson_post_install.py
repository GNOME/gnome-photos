#!/usr/bin/env python3

import glob
import os
import subprocess
import sys

destdir = os.environ.get('DESTDIR', '')

if destdir:
    datadir = os.path.normpath(destdir + os.sep + sys.argv[1])
else:
    datadir = sys.argv[1]

icondir = os.path.join(datadir, 'icons', 'hicolor')

if not destdir:
    print('Updating icon cache...')
    subprocess.call(['gtk-update-icon-cache', '-f', '-t', icondir])

    schemadir = os.path.join(datadir, 'glib-2.0', 'schemas')
    print('Compiling gsettings schemas...')
    subprocess.call(['glib-compile-schemas', schemadir])
