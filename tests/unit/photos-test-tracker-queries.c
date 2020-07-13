/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2020 Sam Thursfield
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

#include <locale.h>

#include <glib.h>
#include <libtracker-sparql/tracker-sparql.h>

#include "photos-application.h"
#include "photos-debug.h"
#include "photos-query.h"
#include "photos-query-builder.h"
#include "photos-search-context.h"
#include "photos-search-provider.h"


typedef struct
{
  TrackerSparqlConnection *connection;
} PhotosTestTrackerQueriesFixture;


static void
photos_test_tracker_queries_setup (PhotosTestTrackerQueriesFixture *fixture, gconstpointer user_data)
{
  g_autoptr (GError) error = NULL;

  fixture->connection = tracker_sparql_connection_new (TRACKER_SPARQL_CONNECTION_FLAGS_NONE,
                                                       NULL,
                                                       tracker_sparql_get_ontology_nepomuk (),
                                                       NULL,
                                                       &error);
  g_assert_no_error (error);
}

static void
photos_test_tracker_queries_teardown (PhotosTestTrackerQueriesFixture *fixture, gconstpointer user_data)
{
  g_object_unref (fixture->connection);
}


static void
test_count_query (TrackerSparqlConnection *connection, PhotosSearchContextState *state, PhotosQueryFlags flags)
{
  g_autoptr (PhotosQuery) query;
  g_autoptr (GError) error = NULL;

  query = photos_query_builder_count_query (state, flags);

  g_debug ("Testing query: %s", photos_query_get_sparql (query));
  tracker_sparql_connection_query (connection,
                                   photos_query_get_sparql (query),
                                   NULL,
                                   &error);

  g_assert_no_error (error);
}

static void
photos_test_tracker_count_queries (PhotosTestTrackerQueriesFixture *fixture, gconstpointer user_data)
{
  g_autoptr (GApplication) app;
  g_autoptr (PhotosSearchProvider) search_provider;
  PhotosSearchContextState *state;

  app = photos_application_new ();
  search_provider = photos_search_provider_new ();
  state = photos_search_context_get_state (PHOTOS_SEARCH_CONTEXT (search_provider));

  test_count_query (fixture->connection, state, PHOTOS_QUERY_FLAGS_UNFILTERED);
  test_count_query (fixture->connection, state, PHOTOS_QUERY_FLAGS_COLLECTIONS);
  test_count_query (fixture->connection, state, PHOTOS_QUERY_FLAGS_FAVORITES);
  test_count_query (fixture->connection, state, PHOTOS_QUERY_FLAGS_IMPORT);
  test_count_query (fixture->connection, state, PHOTOS_QUERY_FLAGS_LOCAL);
  test_count_query (fixture->connection, state, PHOTOS_QUERY_FLAGS_OVERVIEW);
  test_count_query (fixture->connection, state, PHOTOS_QUERY_FLAGS_SEARCH);
}

gint
main (gint argc, gchar *argv[])
{
  gint exit_status;

  setlocale (LC_ALL, "");
  g_test_init (&argc, &argv, NULL);
  photos_debug_init ();

  g_test_add ("/tracker/queries/count",
              PhotosTestTrackerQueriesFixture,
              NULL,
              photos_test_tracker_queries_setup,
              photos_test_tracker_count_queries,
              photos_test_tracker_queries_teardown);

  exit_status = g_test_run ();

  return exit_status;
}
