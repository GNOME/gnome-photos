#!/usr/bin/env python3

import os
import subprocess
import sys

if not os.environ.get('DESTDIR'):
    datadir = sys.argv[1]

    icondir = os.path.join(datadir, 'icons', 'hicolor')
    print('Updating icon cache...')
    subprocess.call(['gtk-update-icon-cache', '-f', '-t', icondir])

    schemadir = os.path.join(datadir, 'glib-2.0', 'schemas')
    print('Compiling gsettings schemas...')
    subprocess.call(['glib-compile-schemas', schemadir])
