#! /usr/bin/python

from testutil import *

from gi.repository import Gio, GLib

import os, sys
import pyatspi
from dogtail import tree
from dogtail import utils
from dogtail.procedural import *

init()
try:
    app = start()

    albums_button = app.child('Albums')
    albums_button.click()
    photos_button = app.child('Photos')
    photos_button.click()
    favorites_button = app.child('Favorites')
    favorites_button.click()
finally:
    fini()
