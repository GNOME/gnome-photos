/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2020 Sam Thursfield <sam@afuera.me.uk>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

#define PHOTOS_TYPE_SPARQL_TEMPLATE (photos_sparql_template_get_type())
G_DECLARE_FINAL_TYPE (PhotosSparqlTemplate, photos_sparql_template, PHOTOS, SPARQL_TEMPLATE, GObject);

PhotosSparqlTemplate *photos_sparql_template_new (const gchar *template_path);

gchar *photos_sparql_template_get_sparql (PhotosSparqlTemplate *self,
                                          const gchar *first_binding_name,
                                          ...) G_GNUC_NULL_TERMINATED;

G_END_DECLS
