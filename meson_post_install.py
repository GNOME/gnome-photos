#!/usr/bin/env python3

import glob
import os
import re
import subprocess
import sys

icondir = os.path.join(sys.argv[1], 'icons', 'hicolor')

name_pattern = re.compile('hicolor_(?:apps)_(?:\d+x\d+|scalable)_(.*)')
search_pattern = '/**/hicolor_*'

for file in glob.glob(icondir + search_pattern, recursive=True):
    os.rename(file, os.path.join(os.path.dirname(file), name_pattern.search(file).group(1)))

if not os.environ.get('DESTDIR'):
    print('Update icon cache...')
    subprocess.call(['gtk-update-icon-cache', '-f', '-t', icondir])

    schemadir = os.path.join(sys.argv[1], 'glib-2.0', 'schemas')
    print('Compiling gsettings schemas...')
    subprocess.call(['glib-compile-schemas', schemadir])
