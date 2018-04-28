# -*- mode: python -*-

from gi.repository import GLib, Gio

from dogtail.utils import isA11yEnabled, enableA11y
if not isA11yEnabled():
    enableA11y(True)

from dogtail import tree
from dogtail import utils
from dogtail.predicate import *
from dogtail.procedural import *

import os, sys
import subprocess

APPLICATION_ID = "org.gnome.Photos"

_bus = None

class IsTextEqual(Predicate):
    """Predicate subclass that looks for top-level windows"""
    def __init__(self, text):
        self.text = text

    def satisfiedByNode(self, node):
        try:
            textIface = node.queryText()
            #print textIface.getText(0, -1)
            return textIface.getText(0, -1) == self.text
        except NotImplementedError:
            return False

    def describeSearchResult(self):
        return '%s text node' % self.text

def _do_bus_call(method, params):
    global _bus

    if _bus == None:
        _bus = Gio.bus_get_sync(Gio.BusType.SESSION)

    _bus.call_sync(APPLICATION_ID, '/' + APPLICATION_ID.replace('.', '/'),
                   'org.freedesktop.Application',
                   method, params, None,
                   Gio.DBusCallFlags.NONE,
                   -1, None)

def start():
    builddir = os.environ.get('G_TEST_BUILDDIR', None)
    if builddir and not 'TESTUTIL_DONT_START' in os.environ:
        subprocess.Popen([os.path.join(builddir, '..', 'src', 'gnome-photos')],
                         cwd=os.path.join(builddir, '..'))
    else:
        _do_bus_call("Activate", GLib.Variant('(a{sv})', ([],)))

    app = tree.root.application('org.gnome.Photos')
    focus.application('org.gnome.Photos')

    return app

def init():
    pass

def fini():
    _do_bus_call("ActivateAction", GLib.Variant('(sava{sv})', ('quit', [], [])))
