#!/usr/bin/env python3

import glob
import os
import subprocess
import sys

if not os.environ.get('DESTDIR'):
  icondir = os.path.join(sys.argv[1], 'icons', 'hicolor')
  print('Update icon cache...')
  subprocess.call(['gtk-update-icon-cache', '-f', '-t', icondir])

  schemadir = os.path.join(sys.argv[1], 'glib-2.0', 'schemas')
  print('Compiling gsettings schemas...')
  subprocess.call(['glib-compile-schemas', schemadir])

  desktopdir = os.path.join(sys.argv[1], 'applications')
  search_pattern = '/*.desktop'
  print('Validate desktop files...')
  [subprocess.call(['desktop-file-validate', file])
   for file in glob.glob(desktopdir + search_pattern, recursive=False)]
