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


#include "config.h"

#include <gio/gio.h>

#include "photos-sparql-template.h"


struct _PhotosSparqlTemplate
{
  GObject parent_instance;
  gchar *template_path;
  gchar *template_text;
};

enum
{
  PROP_0,
  PROP_TEMPLATE_PATH
};


G_DEFINE_TYPE (PhotosSparqlTemplate, photos_sparql_template, G_TYPE_OBJECT);


enum
{
  MAX_SPARQL_TEMPLATE_SIZE = 1024 * 10
};


static void
photos_sparql_template_constructed (GObject *object)
{
  PhotosSparqlTemplate *self = PHOTOS_SPARQL_TEMPLATE (object);
  g_autoptr (GFile) file = NULL;
  g_autoptr (GFileInputStream) stream = NULL;
  gchar buffer[MAX_SPARQL_TEMPLATE_SIZE + 1];
  gsize bytes_read;

  G_OBJECT_CLASS (photos_sparql_template_parent_class)->constructed (object);

  file = g_file_new_for_uri (self->template_path);

  {
    g_autoptr (GError) error = NULL;

    stream = g_file_read (file, NULL, &error);
    if (error != NULL)
      {
        g_critical ("Unable to open template %s: %s", self->template_path, error->message);
        g_return_if_reached ();
      }
  }

  {
    g_autoptr (GError) error = NULL;

    if (!g_input_stream_read_all (G_INPUT_STREAM (stream), buffer, MAX_SPARQL_TEMPLATE_SIZE, &bytes_read, NULL, &error))
      {
        g_critical ("Unable to read template %s: %s", self->template_path, error->message);
        g_return_if_reached ();
      }
  }

  buffer[bytes_read] = '\0';
  self->template_text = g_strdup (buffer);
}


static void
photos_sparql_template_finalize (GObject *object)
{
  PhotosSparqlTemplate *self = PHOTOS_SPARQL_TEMPLATE (object);

  g_free (self->template_path);
  g_free (self->template_text);

  G_OBJECT_CLASS (photos_sparql_template_parent_class)->finalize (object);
}


static void
photos_sparql_template_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
  PhotosSparqlTemplate *self = PHOTOS_SPARQL_TEMPLATE (object);

  switch (prop_id)
    {
    case PROP_TEMPLATE_PATH:
      g_value_set_string (value, self->template_path);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}


static void
photos_sparql_template_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
  PhotosSparqlTemplate *self = PHOTOS_SPARQL_TEMPLATE (object);

  switch (prop_id)
    {
    case PROP_TEMPLATE_PATH:
      self->template_path = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}


static void
photos_sparql_template_init (PhotosSparqlTemplate *self)
{
}


static void
photos_sparql_template_class_init (PhotosSparqlTemplateClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = photos_sparql_template_constructed;
  object_class->finalize = photos_sparql_template_finalize;
  object_class->get_property = photos_sparql_template_get_property;
  object_class->set_property = photos_sparql_template_set_property;

  g_object_class_install_property (object_class,
                                   PROP_TEMPLATE_PATH,
                                   g_param_spec_string ("template-path",
                                                        "Template path",
                                                        "Path to the template file.",
                                                        NULL,
                                                        G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE));
}


PhotosSparqlTemplate *
photos_sparql_template_new (const gchar *template_path)
{
  return g_object_new (PHOTOS_TYPE_SPARQL_TEMPLATE, "template-path", template_path, NULL);
}


gchar *
photos_sparql_template_get_sparql (PhotosSparqlTemplate *self, const gchar *first_binding_name, ...)
{
  const gchar *name;
  const gchar *value;
  gchar *sparql = NULL;
  va_list ap;

  sparql = g_strdup (self->template_text);

  if (first_binding_name == NULL)
    goto out;

  /* FIXME: this is an inefficent way to do template substitutions
   * because we allocate and free a copy of the string for each binding.
   * We should check https://gitlab.gnome.org/GNOME/template-glib/
   */

  va_start (ap, first_binding_name);
  name = first_binding_name;
  do
    {
      g_autoptr (GRegex) regex = NULL;
      g_autofree gchar *name_regex = NULL;
      g_autofree gchar *str = NULL;

      value = va_arg (ap, const gchar *);
      if (value == NULL)
        {
          g_critical ("Missing value for argument \"%s\"", name);
          break;
        }

      name_regex = g_strdup_printf ("{{\\s?%s\\s?}}", name);
      regex = g_regex_new (name_regex, 0, 0, NULL);

      str = g_steal_pointer (&sparql);
      sparql = g_regex_replace_literal (regex, str, -1, 0, value, 0, NULL) ;

      name = va_arg (ap, const gchar *);
    } while (name != NULL);

  va_end (ap);

 out:
  return sparql;
}
