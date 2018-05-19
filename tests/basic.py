#! /usr/bin/python3

from testutil import *

from gi.repository import Gio, GLib

import os, time, sys
import pyatspi
from dogtail import tree
from dogtail import utils
from dogtail.procedural import *

init()
try:
    app = start()

    albums_button = app.child('Albums')
    x = y = -1
    while x < 0 and y < 0:
        (x, y) = albums_button.position
        time.sleep(0.1)
    albums_button.click()
    photos_button = app.child('Photos')
    photos_button.click()
    favorites_button = app.child('Favorites')
    favorites_button.click()
finally:
    fini()
