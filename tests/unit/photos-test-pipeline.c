/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2018 Red Hat, Inc.
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
#include <gegl.h>

#include "photos-debug.h"
#include "photos-gegl.h"
#include "photos-pipeline.h"


typedef struct _PhotosTestPipelineFixture PhotosTestPipelineFixture;

struct _PhotosTestPipelineFixture
{
  GMainContext *context;
  GMainLoop *loop;
};


static void
photos_test_pipeline_setup (PhotosTestPipelineFixture *fixture, gconstpointer user_data)
{
  fixture->context = g_main_context_new ();
  g_main_context_push_thread_default (fixture->context);
  fixture->loop = g_main_loop_new (fixture->context, FALSE);
}


static void
photos_test_pipeline_teardown (PhotosTestPipelineFixture *fixture, gconstpointer user_data)
{
  g_main_context_pop_thread_default (fixture->context);
  g_clear_pointer (&fixture->context, g_main_context_unref);
  g_clear_pointer (&fixture->loop, g_main_loop_unref);
}


static void
photos_test_pipeline_check_empty (PhotosPipeline *pipeline)
{
  g_autoptr (GSList) children = NULL;
  GeglNode *graph;
  GeglNode *input;
  GeglNode *output;
  GeglNode *previous;
  gboolean is_edited;
  const gchar *operation;
  g_autofree gchar *previous_pad_name = NULL;
  guint length;

  g_assert_true (PHOTOS_IS_PIPELINE (pipeline));

  graph = photos_pipeline_get_graph (pipeline);

  input = gegl_node_get_input_proxy (graph, "input");
  output = gegl_node_get_output_proxy (graph, "output");
  previous = gegl_node_get_producer (output, "input", &previous_pad_name);
  g_assert_true (previous == input);
  g_assert_cmpstr (previous_pad_name, ==, "output");

  children = gegl_node_get_children (graph);
  length = g_slist_length (children);
  g_assert_cmpuint (length, ==, 2);
  operation = gegl_node_get_operation (GEGL_NODE (children->data));
  g_assert_cmpstr (operation, ==, "gegl:nop");
  operation = gegl_node_get_operation (GEGL_NODE (children->next->data));
  g_assert_cmpstr (operation, ==, "gegl:nop");

  is_edited = photos_pipeline_is_edited (pipeline);
  g_assert_false (is_edited);
}


static void
photos_test_pipeline_null_parent_blank_uris_0_new (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  g_autoptr (PhotosPipeline) pipeline = NULL;
  PhotosTestPipelineFixture *fixture = (PhotosTestPipelineFixture *) user_data;

  {
    g_autoptr (GError) error = NULL;

    pipeline = photos_pipeline_new_finish (res, &error);
    g_assert_no_error (error);
  }

  photos_test_pipeline_check_empty (pipeline);
  g_main_loop_quit (fixture->loop);
}


static void
photos_test_pipeline_null_parent_blank_uris_0 (PhotosTestPipelineFixture *fixture, gconstpointer user_data)
{
  const gchar *const uris[] = { NULL };

  photos_pipeline_new_async (NULL, uris, NULL, photos_test_pipeline_null_parent_blank_uris_0_new, fixture);
  g_main_loop_run (fixture->loop);
}


static void
photos_test_pipeline_null_parent_blank_uris_1_new (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  g_autoptr (PhotosPipeline) pipeline = NULL;
  PhotosTestPipelineFixture *fixture = (PhotosTestPipelineFixture *) user_data;

  {
    g_autoptr (GError) error = NULL;

    pipeline = photos_pipeline_new_finish (res, &error);
    g_assert_no_error (error);
  }

  photos_test_pipeline_check_empty (pipeline);
  g_main_loop_quit (fixture->loop);
}


static void
photos_test_pipeline_null_parent_blank_uris_1 (PhotosTestPipelineFixture *fixture, gconstpointer user_data)
{
  const gchar *const uris[] = { NULL, "this-should-not-be-used", NULL };

  photos_pipeline_new_async (NULL, uris, NULL, photos_test_pipeline_null_parent_blank_uris_1_new, fixture);
  g_main_loop_run (fixture->loop);
}


static void
photos_test_pipeline_null_parent_blank_uris_2_new (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  g_autoptr (PhotosPipeline) pipeline = NULL;
  PhotosTestPipelineFixture *fixture = (PhotosTestPipelineFixture *) user_data;

  {
    g_autoptr (GError) error = NULL;

    pipeline = photos_pipeline_new_finish (res, &error);
    g_assert_no_error (error);
  }

  photos_test_pipeline_check_empty (pipeline);
  g_main_loop_quit (fixture->loop);
}


static void
photos_test_pipeline_null_parent_blank_uris_2 (PhotosTestPipelineFixture *fixture, gconstpointer user_data)
{
  const gchar *const uris[] = { "", NULL };

  photos_pipeline_new_async (NULL, uris, NULL, photos_test_pipeline_null_parent_blank_uris_2_new, fixture);
  g_main_loop_run (fixture->loop);
}


static void
photos_test_pipeline_null_parent_blank_uris_3_new (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  g_autoptr (PhotosPipeline) pipeline = NULL;
  PhotosTestPipelineFixture *fixture = (PhotosTestPipelineFixture *) user_data;

  {
    g_autoptr (GError) error = NULL;

    pipeline = photos_pipeline_new_finish (res, &error);
    g_assert_no_error (error);
  }

  photos_test_pipeline_check_empty (pipeline);
  g_main_loop_quit (fixture->loop);
}


static void
photos_test_pipeline_null_parent_blank_uris_3 (PhotosTestPipelineFixture *fixture, gconstpointer user_data)
{
  const gchar *const uris[] = { "", "this-should-not-be-used", NULL };

  photos_pipeline_new_async (NULL, uris, NULL, photos_test_pipeline_null_parent_blank_uris_3_new, fixture);
  g_main_loop_run (fixture->loop);
}


static void
photos_test_pipeline_null_parent_null_uris_new (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  g_autoptr (PhotosPipeline) pipeline = NULL;
  PhotosTestPipelineFixture *fixture = (PhotosTestPipelineFixture *) user_data;

  {
    g_autoptr (GError) error = NULL;

    pipeline = photos_pipeline_new_finish (res, &error);
    g_assert_no_error (error);
  }

  photos_test_pipeline_check_empty (pipeline);
  g_main_loop_quit (fixture->loop);
}


static void
photos_test_pipeline_null_parent_null_uris (PhotosTestPipelineFixture *fixture, gconstpointer user_data)
{
  photos_pipeline_new_async (NULL, NULL, NULL, photos_test_pipeline_null_parent_null_uris_new, fixture);
  g_main_loop_run (fixture->loop);
}


gint
main (gint argc, gchar *argv[])
{
  gint exit_status;

  setlocale (LC_ALL, "");
  g_test_init (&argc, &argv, NULL);
  photos_debug_init ();
  photos_gegl_init ();

  g_test_add ("/pipeline/new/null-parent-blank-uris-0",
              PhotosTestPipelineFixture,
              NULL,
              photos_test_pipeline_setup,
              photos_test_pipeline_null_parent_blank_uris_0,
              photos_test_pipeline_teardown);

  g_test_add ("/pipeline/new/null-parent-blank-uris-1",
              PhotosTestPipelineFixture,
              NULL,
              photos_test_pipeline_setup,
              photos_test_pipeline_null_parent_blank_uris_1,
              photos_test_pipeline_teardown);

  g_test_add ("/pipeline/new/null-parent-blank-uris-2",
              PhotosTestPipelineFixture,
              NULL,
              photos_test_pipeline_setup,
              photos_test_pipeline_null_parent_blank_uris_2,
              photos_test_pipeline_teardown);

  g_test_add ("/pipeline/new/null-parent-blank-uris-3",
              PhotosTestPipelineFixture,
              NULL,
              photos_test_pipeline_setup,
              photos_test_pipeline_null_parent_blank_uris_3,
              photos_test_pipeline_teardown);

  g_test_add ("/pipeline/new/null-parent-null-uris",
              PhotosTestPipelineFixture,
              NULL,
              photos_test_pipeline_setup,
              photos_test_pipeline_null_parent_null_uris,
              photos_test_pipeline_teardown);

  exit_status = g_test_run ();

  gegl_exit ();
  return exit_status;
}
