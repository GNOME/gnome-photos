Photos - access, organize and share your photos on GNOME
========================================================

Download: http://download.gnome.org/sources/gnome-photos
Project: https://gitlab.gnome.org/GNOME/gnome-photos
Website: https://wiki.gnome.org/Apps/Photos

----

Photos, like Documents, Music and Videos, is one of the core GNOME
applications meant for find and reminding the user about her content.
The internal architecture Photos is based on Documents -- the document
manager application for GNOME, because they share similar UI/UX
patterns and objectives.

Points to remember while hacking on Photos:

+ Avoid unnecessary divergences from Documents. Valid exceptions
  include changes needed to convert JavaScript idioms to their C
  equivalents.

+ Share the same set of widgets as Documents, or the other core GNOME
  applications, as much as possible.

+ Monitor changes in the Documents code base and clone them when
  relevant.

+ Follow the GNU coding style. To accomodate longer class and method
  names due to namespace prefixes, line lengths upto 120 characters
  are allowed.

----

Photos is licensed under the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or (at
your option) any later version.
