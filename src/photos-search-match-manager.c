/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2014 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

/* Based on code from:
 *   + Documents
 */


#include "config.h"

#include <glib.h>
#include <glib/gi18n.h>

#include "photos-filterable.h"
#include "photos-query.h"
#include "photos-search-match.h"
#include "photos-search-match-manager.h"


struct _PhotosSearchMatchManagerPrivate
{
  PhotosSearchController *srch_cntrlr;
};

enum
{
  PROP_0,
  PROP_SEARCH_CONTROLLER
};


G_DEFINE_TYPE_WITH_PRIVATE (PhotosSearchMatchManager, photos_search_match_manager, PHOTOS_TYPE_BASE_MANAGER);


static gchar *
photos_search_match_manager_get_filter (PhotosBaseManager *mngr, gint flags)
{
  PhotosSearchMatchManager *self = PHOTOS_SEARCH_MATCH_MANAGER (mngr);
  GHashTable *objects;
  PhotosSearchMatch *search_match;
  const gchar *blank = "(true)";
  gchar *filter = NULL;
  gchar *ret_val = NULL;
  gchar **filters = NULL;
  gchar **terms = NULL;
  guint i;
  guint n_terms;

  if (!(flags & PHOTOS_QUERY_FLAGS_SEARCH))
    goto out;

  terms = photos_search_controller_get_terms (self->priv->srch_cntrlr);
  n_terms = g_strv_length (terms);
  if (n_terms == 0)
    goto out;

  objects = photos_base_manager_get_objects (PHOTOS_BASE_MANAGER (self));
  filters = (gchar **) g_malloc0_n (n_terms + 1, sizeof (gchar *));

  for (i = 0; terms[i] != NULL; i++)
    {
      GHashTableIter iter;
      const gchar *id;

      g_hash_table_iter_init (&iter, objects);
      while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &search_match))
        photos_search_match_set_filter_term (search_match, terms[i]);

      search_match = PHOTOS_SEARCH_MATCH (photos_base_manager_get_active_object (PHOTOS_BASE_MANAGER (self)));
      id = photos_filterable_get_id (PHOTOS_FILTERABLE (search_match));
      if (g_strcmp0 (id, PHOTOS_SEARCH_MATCH_STOCK_ALL) == 0)
        filter = photos_base_manager_get_all_filter (PHOTOS_BASE_MANAGER (self));
      else
        filter = photos_filterable_get_filter (PHOTOS_FILTERABLE (search_match));

      filters[i] = filter;
      filter = NULL;
    }

  filter = g_strjoinv (" && ", filters);
  ret_val = g_strconcat ("(", filter, ")", NULL);

 out:
  g_free (filter);
  g_strfreev (filters);
  g_strfreev (terms);
  return (ret_val == NULL) ? g_strdup (blank) : ret_val;
}


static void
photos_search_match_manager_dispose (GObject *object)
{
  PhotosSearchMatchManager *self = PHOTOS_SEARCH_MATCH_MANAGER (object);

  g_clear_object (&self->priv->srch_cntrlr);

  G_OBJECT_CLASS (photos_search_match_manager_parent_class)->dispose (object);
}


static void
photos_search_match_manager_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
  PhotosSearchMatchManager *self = PHOTOS_SEARCH_MATCH_MANAGER (object);

  switch (prop_id)
    {
    case PROP_SEARCH_CONTROLLER:
      self->priv->srch_cntrlr = PHOTOS_SEARCH_CONTROLLER (g_value_dup_object (value)); /* self is owned by context */
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
photos_search_match_manager_init (PhotosSearchMatchManager *self)
{
  PhotosSearchMatch *search_match;
  const gchar *author_filter;
  const gchar *title_filter;

  self->priv = photos_search_match_manager_get_instance_private (self);

  author_filter = "fn:contains ("
                  "  tracker:case-fold (tracker:coalesce (nco:fullname (?creator), nco:fullname(?publisher))),"
                  "  \"%s\")";
  title_filter = "fn:contains ("
                 "  tracker:case-fold (tracker:coalesce (nie:title (?urn), nfo:fileName(?urn))),"
                 "  \"%s\")";

  search_match = photos_search_match_new (PHOTOS_SEARCH_MATCH_STOCK_ALL,
                                          _("All"),
                                          "(false)"); /* unused */
  photos_base_manager_add_object (PHOTOS_BASE_MANAGER (self), G_OBJECT (search_match));
  g_object_unref (search_match);

  search_match = photos_search_match_new (PHOTOS_SEARCH_MATCH_STOCK_TITLE,
                                          /* Translators: "Title" refers to "Match Title" when searching. */
                                          C_("Search Filter", "Title"),
                                          title_filter);
  photos_base_manager_add_object (PHOTOS_BASE_MANAGER (self), G_OBJECT (search_match));
  g_object_unref (search_match);

  search_match = photos_search_match_new (PHOTOS_SEARCH_MATCH_STOCK_AUTHOR,
                                          /* Translators: "Author" refers to "Match Author" when searching. */
                                          C_("Search Filter", "Author"),
                                          author_filter);
  photos_base_manager_add_object (PHOTOS_BASE_MANAGER (self), G_OBJECT (search_match));
  g_object_unref (search_match);

  photos_base_manager_set_active_object_by_id (PHOTOS_BASE_MANAGER (self), PHOTOS_SEARCH_MATCH_STOCK_ALL);
}


static void
photos_search_match_manager_class_init (PhotosSearchMatchManagerClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  PhotosBaseManagerClass *base_manager_class = PHOTOS_BASE_MANAGER_CLASS (class);

  object_class->dispose = photos_search_match_manager_dispose;
  object_class->set_property = photos_search_match_manager_set_property;
  base_manager_class->get_filter = photos_search_match_manager_get_filter;

  g_object_class_install_property (object_class,
                                   PROP_SEARCH_CONTROLLER,
                                   g_param_spec_object ("search-controller",
                                                        "A PhotosSearchController",
                                                        "The search controller for this manager",
                                                        PHOTOS_TYPE_SEARCH_CONTROLLER,
                                                        G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE));
}


PhotosBaseManager *
photos_search_match_manager_new (PhotosSearchController *srch_cntrlr)
{
  /* Translators: this is a verb that refers to "All", "Title" and
   * "Author", as in "Match All", "Match Title" and "Match Author".
   */
  return g_object_new (PHOTOS_TYPE_SEARCH_MATCH_MANAGER,
                       "action-id", "search-match",
                       "search-controller", srch_cntrlr,
                       "title", _("Match"),
                       NULL);
}
