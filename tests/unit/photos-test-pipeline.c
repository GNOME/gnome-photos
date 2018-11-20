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

#include <gegl.h>
#include <gio/gio.h>
#include <glib.h>

#include "photos-debug.h"
#include "photos-gegl.h"
#include "photos-pipeline.h"


typedef struct _PhotosTestPipelineFixture PhotosTestPipelineFixture;

struct _PhotosTestPipelineFixture
{
  GAsyncResult *res;
  GMainContext *context;
  GMainLoop *loop;
};


static gchar *
photos_test_pipeline_filename_to_uri (const gchar *filename)
{
  g_autoptr (GFile) file = NULL;
  g_autofree gchar *path_relative = NULL;
  gchar *uri = NULL;

  path_relative = g_test_build_filename (G_TEST_DIST, filename, NULL);
  file = g_file_new_for_path (path_relative);
  uri = g_file_get_uri (file);
  return uri;
}


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
  g_clear_object (&fixture->res);
  g_main_context_pop_thread_default (fixture->context);
  g_main_context_unref (fixture->context);
  g_main_loop_unref (fixture->loop);
}


static void
photos_test_pipeline_async (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosTestPipelineFixture *fixture = (PhotosTestPipelineFixture *) user_data;

  g_assert_null (fixture->res);
  fixture->res = g_object_ref (res);
  g_main_loop_quit (fixture->loop);
}


static void
photos_test_pipeline_check_empty (PhotosPipeline *pipeline, GeglNode *parent_expected)
{
  g_autoptr (GSList) children = NULL;
  GeglNode *graph;
  GeglNode *input;
  GeglNode *output;
  GeglNode *parent_actual;
  GeglNode *previous;
  gboolean is_edited;
  const gchar *operation;
  g_autofree gchar *previous_pad_name = NULL;
  guint length;

  g_assert_true (PHOTOS_IS_PIPELINE (pipeline));

  graph = photos_pipeline_get_graph (pipeline);
  parent_actual = gegl_node_get_parent (graph);
  g_assert_true (parent_actual == parent_expected);

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
photos_test_pipeline_check_full (PhotosPipeline *pipeline, GeglNode *parent_expected)
{
  g_autoptr (GSList) children = NULL;
  GeglNode *graph;
  GeglNode *input;
  GeglNode *output;
  GeglNode *parent_actual;
  GeglNode *previous;
  gboolean is_edited;
  const gchar *operation;
  g_autofree gchar *previous_pad_name = NULL;
  guint length;

  g_assert_true (PHOTOS_IS_PIPELINE (pipeline));

  graph = photos_pipeline_get_graph (pipeline);
  parent_actual = gegl_node_get_parent (graph);
  g_assert_true (parent_actual == parent_expected);

  children = gegl_node_get_children (graph);
  length = g_slist_length (children);
  g_assert_cmpuint (length, ==, 10);

  input = gegl_node_get_input_proxy (graph, "input");
  output = gegl_node_get_output_proxy (graph, "output");

  previous = gegl_node_get_producer (output, "input", &previous_pad_name);
  g_assert_true (previous != input);
  g_assert_cmpstr (previous_pad_name, ==, "output");

  operation = gegl_node_get_operation (GEGL_NODE (previous));
  g_assert_cmpstr (operation, ==, "gegl:brightness-contrast");

  previous = gegl_node_get_producer (previous, "input", &previous_pad_name);
  g_assert_true (previous != input);
  g_assert_cmpstr (previous_pad_name, ==, "output");

  operation = gegl_node_get_operation (GEGL_NODE (previous));
  g_assert_cmpstr (operation, ==, "gegl:exposure");

  previous = gegl_node_get_producer (previous, "input", &previous_pad_name);
  g_assert_true (previous != input);
  g_assert_cmpstr (previous_pad_name, ==, "output");

  operation = gegl_node_get_operation (GEGL_NODE (previous));
  g_assert_cmpstr (operation, ==, "gegl:unsharp-mask");

  previous = gegl_node_get_producer (previous, "input", &previous_pad_name);
  g_assert_true (previous != input);
  g_assert_cmpstr (previous_pad_name, ==, "output");

  operation = gegl_node_get_operation (GEGL_NODE (previous));
  g_assert_cmpstr (operation, ==, "photos:magic-filter");

  previous = gegl_node_get_producer (previous, "input", &previous_pad_name);
  g_assert_true (previous != input);
  g_assert_cmpstr (previous_pad_name, ==, "output");

  operation = gegl_node_get_operation (GEGL_NODE (previous));
  g_assert_cmpstr (operation, ==, "photos:saturation");

  previous = gegl_node_get_producer (previous, "input", &previous_pad_name);
  g_assert_true (previous != input);
  g_assert_cmpstr (previous_pad_name, ==, "output");

  operation = gegl_node_get_operation (GEGL_NODE (previous));
  g_assert_cmpstr (operation, ==, "gegl:shadows-highlights");

  previous = gegl_node_get_producer (previous, "input", &previous_pad_name);
  g_assert_true (previous != input);
  g_assert_cmpstr (previous_pad_name, ==, "output");

  operation = gegl_node_get_operation (GEGL_NODE (previous));
  g_assert_cmpstr (operation, ==, "gegl:noise-reduction");

  previous = gegl_node_get_producer (previous, "input", &previous_pad_name);
  g_assert_true (previous != input);
  g_assert_cmpstr (previous_pad_name, ==, "output");

  operation = gegl_node_get_operation (GEGL_NODE (previous));
  g_assert_cmpstr (operation, ==, "gegl:crop");

  previous = gegl_node_get_producer (previous, "input", &previous_pad_name);
  g_assert_true (previous == input);
  g_assert_cmpstr (previous_pad_name, ==, "output");

  is_edited = photos_pipeline_is_edited (pipeline);
  g_assert_true (is_edited);
}


static void
photos_test_pipeline_pipeline_new_async (GeglNode *parent,
                                         const gchar *const *filenames,
                                         GCancellable *cancellable,
                                         GAsyncReadyCallback callback,
                                         gpointer user_data)
{
  g_auto (GStrv) uris = NULL;
  guint i;
  guint length;

  length = g_strv_length ((GStrv) filenames);
  uris = (gchar **) g_malloc0_n (length + 1, sizeof (gchar *));
  for (i = 0; filenames[i] != 0; i++)
    {
      if (g_str_has_prefix (filenames[i], "photos-test-pipeline"))
        uris[i] = photos_test_pipeline_filename_to_uri (filenames[i]);
      else if (g_str_has_prefix (filenames[i], "this-does-not-exist"))
        uris[i] = g_strconcat ("file:///", filenames[i], NULL);
      else
        uris[i] = g_strdup (filenames[i]);
    }

  photos_pipeline_new_async (parent, (const gchar *const *) uris, cancellable, callback, user_data);
}


static void
photos_test_pipeline_no_parent_blank_uris_0 (PhotosTestPipelineFixture *fixture, gconstpointer user_data)
{
  g_autoptr (PhotosPipeline) pipeline = NULL;
  const gchar *const uris[] = { NULL };

  photos_pipeline_new_async (NULL, uris, NULL, photos_test_pipeline_async, fixture);
  g_main_loop_run (fixture->loop);

  {
    g_autoptr (GError) error = NULL;

    pipeline = photos_pipeline_new_finish (fixture->res, &error);
    g_assert_no_error (error);
  }

  photos_test_pipeline_check_empty (pipeline, NULL);
}


static void
photos_test_pipeline_no_parent_blank_uris_1 (PhotosTestPipelineFixture *fixture, gconstpointer user_data)
{
  g_autoptr (PhotosPipeline) pipeline = NULL;
  const gchar *const filenames[] = { NULL, "photos-test-pipeline-not-edited-00.xml", NULL };

  photos_test_pipeline_pipeline_new_async (NULL, filenames, NULL, photos_test_pipeline_async, fixture);
  g_main_loop_run (fixture->loop);

  {
    g_autoptr (GError) error = NULL;

    pipeline = photos_pipeline_new_finish (fixture->res, &error);
    g_assert_no_error (error);
  }

  photos_test_pipeline_check_empty (pipeline, NULL);
}


static void
photos_test_pipeline_no_parent_blank_uris_2 (PhotosTestPipelineFixture *fixture, gconstpointer user_data)
{
  g_autoptr (PhotosPipeline) pipeline = NULL;
  const gchar *const filenames[] = { NULL, "photos-test-pipeline-edited-00.xml", NULL };

  photos_test_pipeline_pipeline_new_async (NULL, filenames, NULL, photos_test_pipeline_async, fixture);
  g_main_loop_run (fixture->loop);

  {
    g_autoptr (GError) error = NULL;

    pipeline = photos_pipeline_new_finish (fixture->res, &error);
    g_assert_no_error (error);
  }

  photos_test_pipeline_check_empty (pipeline, NULL);
}


static void
photos_test_pipeline_no_parent_blank_uris_3 (PhotosTestPipelineFixture *fixture, gconstpointer user_data)
{
  g_autoptr (PhotosPipeline) pipeline = NULL;
  const gchar *const filenames[] = { NULL, "this-does-not-exist", NULL };

  photos_test_pipeline_pipeline_new_async (NULL, filenames, NULL, photos_test_pipeline_async, fixture);
  g_main_loop_run (fixture->loop);

  {
    g_autoptr (GError) error = NULL;

    pipeline = photos_pipeline_new_finish (fixture->res, &error);
    g_assert_no_error (error);
  }

  photos_test_pipeline_check_empty (pipeline, NULL);
}


static void
photos_test_pipeline_no_parent_blank_uris_4 (PhotosTestPipelineFixture *fixture, gconstpointer user_data)
{
  g_autoptr (PhotosPipeline) pipeline = NULL;
  const gchar *const uris[] = { NULL, "this-should-not-be-used", NULL };

  photos_pipeline_new_async (NULL, uris, NULL, photos_test_pipeline_async, fixture);
  g_main_loop_run (fixture->loop);

  {
    g_autoptr (GError) error = NULL;

    pipeline = photos_pipeline_new_finish (fixture->res, &error);
    g_assert_no_error (error);
  }

  photos_test_pipeline_check_empty (pipeline, NULL);
}


static void
photos_test_pipeline_no_parent_blank_uris_5 (PhotosTestPipelineFixture *fixture, gconstpointer user_data)
{
  g_autoptr (PhotosPipeline) pipeline = NULL;
  const gchar *const uris[] = { "", NULL };

  photos_pipeline_new_async (NULL, uris, NULL, photos_test_pipeline_async, fixture);
  g_main_loop_run (fixture->loop);

  {
    g_autoptr (GError) error = NULL;

    pipeline = photos_pipeline_new_finish (fixture->res, &error);
    g_assert_no_error (error);
  }

  photos_test_pipeline_check_empty (pipeline, NULL);
}


static void
photos_test_pipeline_no_parent_blank_uris_6 (PhotosTestPipelineFixture *fixture, gconstpointer user_data)
{
  g_autoptr (PhotosPipeline) pipeline = NULL;
  const gchar *const filenames[] = { "", "photos-test-pipeline-not-edited-00.xml", NULL };

  photos_test_pipeline_pipeline_new_async (NULL, filenames, NULL, photos_test_pipeline_async, fixture);
  g_main_loop_run (fixture->loop);

  {
    g_autoptr (GError) error = NULL;

    pipeline = photos_pipeline_new_finish (fixture->res, &error);
    g_assert_no_error (error);
  }

  photos_test_pipeline_check_empty (pipeline, NULL);
}


static void
photos_test_pipeline_no_parent_blank_uris_7 (PhotosTestPipelineFixture *fixture, gconstpointer user_data)
{
  g_autoptr (PhotosPipeline) pipeline = NULL;
  const gchar *const filenames[] = { "", "photos-test-pipeline-edited-00.xml", NULL };

  photos_test_pipeline_pipeline_new_async (NULL, filenames, NULL, photos_test_pipeline_async, fixture);
  g_main_loop_run (fixture->loop);

  {
    g_autoptr (GError) error = NULL;

    pipeline = photos_pipeline_new_finish (fixture->res, &error);
    g_assert_no_error (error);
  }

  photos_test_pipeline_check_empty (pipeline, NULL);
}


static void
photos_test_pipeline_no_parent_blank_uris_8 (PhotosTestPipelineFixture *fixture, gconstpointer user_data)
{
  g_autoptr (PhotosPipeline) pipeline = NULL;
  const gchar *const filenames[] = { "", "this-does-not-exist", NULL };

  photos_test_pipeline_pipeline_new_async (NULL, filenames, NULL, photos_test_pipeline_async, fixture);
  g_main_loop_run (fixture->loop);

  {
    g_autoptr (GError) error = NULL;

    pipeline = photos_pipeline_new_finish (fixture->res, &error);
    g_assert_no_error (error);
  }

  photos_test_pipeline_check_empty (pipeline, NULL);
}


static void
photos_test_pipeline_no_parent_blank_uris_9 (PhotosTestPipelineFixture *fixture, gconstpointer user_data)
{
  g_autoptr (PhotosPipeline) pipeline = NULL;
  const gchar *const uris[] = { "", "this-should-not-be-used", NULL };

  photos_pipeline_new_async (NULL, uris, NULL, photos_test_pipeline_async, fixture);
  g_main_loop_run (fixture->loop);

  {
    g_autoptr (GError) error = NULL;

    pipeline = photos_pipeline_new_finish (fixture->res, &error);
    g_assert_no_error (error);
  }

  photos_test_pipeline_check_empty (pipeline, NULL);
}


static void
photos_test_pipeline_no_parent_null_uris (PhotosTestPipelineFixture *fixture, gconstpointer user_data)
{
  g_autoptr (PhotosPipeline) pipeline = NULL;

  photos_pipeline_new_async (NULL, NULL, NULL, photos_test_pipeline_async, fixture);
  g_main_loop_run (fixture->loop);

  {
    g_autoptr (GError) error = NULL;

    pipeline = photos_pipeline_new_finish (fixture->res, &error);
    g_assert_no_error (error);
  }

  photos_test_pipeline_check_empty (pipeline, NULL);
}


static void
photos_test_pipeline_no_parent_with_uris_0 (PhotosTestPipelineFixture *fixture, gconstpointer user_data)
{
  g_autoptr (PhotosPipeline) pipeline = NULL;
  const gchar *const filenames[] = { "photos-test-pipeline-not-edited-00.xml", NULL };

  photos_test_pipeline_pipeline_new_async (NULL, filenames, NULL, photos_test_pipeline_async, fixture);
  g_main_loop_run (fixture->loop);

  {
    g_autoptr (GError) error = NULL;

    pipeline = photos_pipeline_new_finish (fixture->res, &error);
    g_assert_no_error (error);
  }

  photos_test_pipeline_check_empty (pipeline, NULL);
}


static void
photos_test_pipeline_no_parent_with_uris_1 (PhotosTestPipelineFixture *fixture, gconstpointer user_data)
{
  g_autoptr (PhotosPipeline) pipeline = NULL;
  const gchar *const filenames[] =
    {
      "photos-test-pipeline-not-edited-00.xml",
      "photos-test-pipeline-edited-00.xml",
      NULL
    };

  photos_test_pipeline_pipeline_new_async (NULL, filenames, NULL, photos_test_pipeline_async, fixture);
  g_main_loop_run (fixture->loop);

  {
    g_autoptr (GError) error = NULL;

    pipeline = photos_pipeline_new_finish (fixture->res, &error);
    g_assert_no_error (error);
  }

  photos_test_pipeline_check_empty (pipeline, NULL);
}


static void
photos_test_pipeline_no_parent_with_uris_2 (PhotosTestPipelineFixture *fixture, gconstpointer user_data)
{
  g_autoptr (PhotosPipeline) pipeline = NULL;
  const gchar *const filenames[] = { "photos-test-pipeline-not-edited-00.xml", "this-does-not-exist", NULL };

  photos_test_pipeline_pipeline_new_async (NULL, filenames, NULL, photos_test_pipeline_async, fixture);
  g_main_loop_run (fixture->loop);

  {
    g_autoptr (GError) error = NULL;

    pipeline = photos_pipeline_new_finish (fixture->res, &error);
    g_assert_no_error (error);
  }

  photos_test_pipeline_check_empty (pipeline, NULL);
}


static void
photos_test_pipeline_no_parent_with_uris_3 (PhotosTestPipelineFixture *fixture, gconstpointer user_data)
{
  g_autoptr (PhotosPipeline) pipeline = NULL;
  const gchar *const filenames[] = { "photos-test-pipeline-not-edited-00.xml", "this-should-not-be-used", NULL };

  photos_test_pipeline_pipeline_new_async (NULL, filenames, NULL, photos_test_pipeline_async, fixture);
  g_main_loop_run (fixture->loop);

  {
    g_autoptr (GError) error = NULL;

    pipeline = photos_pipeline_new_finish (fixture->res, &error);
    g_assert_no_error (error);
  }

  photos_test_pipeline_check_empty (pipeline, NULL);
}


static void
photos_test_pipeline_no_parent_with_uris_4 (PhotosTestPipelineFixture *fixture, gconstpointer user_data)
{
  g_autoptr (PhotosPipeline) pipeline = NULL;
  const gchar *const filenames[] = { "photos-test-pipeline-edited-00.xml", NULL };

  photos_test_pipeline_pipeline_new_async (NULL, filenames, NULL, photos_test_pipeline_async, fixture);
  g_main_loop_run (fixture->loop);

  {
    g_autoptr (GError) error = NULL;

    pipeline = photos_pipeline_new_finish (fixture->res, &error);
    g_assert_no_error (error);
  }

  photos_test_pipeline_check_full (pipeline, NULL);
}


static void
photos_test_pipeline_no_parent_with_uris_5 (PhotosTestPipelineFixture *fixture, gconstpointer user_data)
{
  g_autoptr (PhotosPipeline) pipeline = NULL;
  const gchar *const filenames[] =
    {
      "photos-test-pipeline-edited-00.xml",
      "photos-test-pipeline-not-edited-00.xml",
      NULL
    };

  photos_test_pipeline_pipeline_new_async (NULL, filenames, NULL, photos_test_pipeline_async, fixture);
  g_main_loop_run (fixture->loop);

  {
    g_autoptr (GError) error = NULL;

    pipeline = photos_pipeline_new_finish (fixture->res, &error);
    g_assert_no_error (error);
  }

  photos_test_pipeline_check_full (pipeline, NULL);
}


static void
photos_test_pipeline_no_parent_with_uris_6 (PhotosTestPipelineFixture *fixture, gconstpointer user_data)
{
  g_autoptr (PhotosPipeline) pipeline = NULL;
  const gchar *const filenames[] = { "photos-test-pipeline-edited-00.xml", "this-does-not-exist", NULL };

  photos_test_pipeline_pipeline_new_async (NULL, filenames, NULL, photos_test_pipeline_async, fixture);
  g_main_loop_run (fixture->loop);

  {
    g_autoptr (GError) error = NULL;

    pipeline = photos_pipeline_new_finish (fixture->res, &error);
    g_assert_no_error (error);
  }

  photos_test_pipeline_check_full (pipeline, NULL);
}


static void
photos_test_pipeline_no_parent_with_uris_7 (PhotosTestPipelineFixture *fixture, gconstpointer user_data)
{
  g_autoptr (PhotosPipeline) pipeline = NULL;
  const gchar *const filenames[] = { "photos-test-pipeline-edited-00.xml", "this-should-not-be-used", NULL };

  photos_test_pipeline_pipeline_new_async (NULL, filenames, NULL, photos_test_pipeline_async, fixture);
  g_main_loop_run (fixture->loop);

  {
    g_autoptr (GError) error = NULL;

    pipeline = photos_pipeline_new_finish (fixture->res, &error);
    g_assert_no_error (error);
  }

  photos_test_pipeline_check_full (pipeline, NULL);
}


static void
photos_test_pipeline_no_parent_with_uris_8 (PhotosTestPipelineFixture *fixture, gconstpointer user_data)
{
  g_autoptr (PhotosPipeline) pipeline = NULL;
  const gchar *const filenames[] = { "this-does-not-exist", NULL };

  photos_test_pipeline_pipeline_new_async (NULL, filenames, NULL, photos_test_pipeline_async, fixture);
  g_main_loop_run (fixture->loop);

  {
    g_autoptr (GError) error = NULL;

    pipeline = photos_pipeline_new_finish (fixture->res, &error);
    g_assert_no_error (error);
  }

  photos_test_pipeline_check_empty (pipeline, NULL);
}


static void
photos_test_pipeline_no_parent_with_uris_9 (PhotosTestPipelineFixture *fixture, gconstpointer user_data)
{
  g_autoptr (PhotosPipeline) pipeline = NULL;
  const gchar *const filenames[] = { "this-does-not-exist", "photos-test-pipeline-not-edited-00.xml", NULL };

  photos_test_pipeline_pipeline_new_async (NULL, filenames, NULL, photos_test_pipeline_async, fixture);
  g_main_loop_run (fixture->loop);

  {
    g_autoptr (GError) error = NULL;

    pipeline = photos_pipeline_new_finish (fixture->res, &error);
    g_assert_no_error (error);
  }

  photos_test_pipeline_check_empty (pipeline, NULL);
}


static void
photos_test_pipeline_no_parent_with_uris_10 (PhotosTestPipelineFixture *fixture, gconstpointer user_data)
{
  g_autoptr (PhotosPipeline) pipeline = NULL;
  const gchar *const filenames[] = { "this-does-not-exist", "photos-test-pipeline-edited-00.xml", NULL };

  photos_test_pipeline_pipeline_new_async (NULL, filenames, NULL, photos_test_pipeline_async, fixture);
  g_main_loop_run (fixture->loop);

  {
    g_autoptr (GError) error = NULL;

    pipeline = photos_pipeline_new_finish (fixture->res, &error);
    g_assert_no_error (error);
  }

  photos_test_pipeline_check_full (pipeline, NULL);
}


static void
photos_test_pipeline_no_parent_with_uris_11 (PhotosTestPipelineFixture *fixture, gconstpointer user_data)
{
  g_autoptr (PhotosPipeline) pipeline = NULL;
  const gchar *const filenames[] = { "this-does-not-exist", "this-does-not-exist-either", NULL };

  photos_test_pipeline_pipeline_new_async (NULL, filenames, NULL, photos_test_pipeline_async, fixture);
  g_main_loop_run (fixture->loop);

  {
    g_autoptr (GError) error = NULL;

    pipeline = photos_pipeline_new_finish (fixture->res, &error);
    g_assert_no_error (error);
  }

  photos_test_pipeline_check_empty (pipeline, NULL);
}


static void
photos_test_pipeline_no_parent_with_uris_12 (PhotosTestPipelineFixture *fixture, gconstpointer user_data)
{
  g_autoptr (PhotosPipeline) pipeline = NULL;
  const gchar *const filenames[] = { "this-does-not-exist", "this-should-not-be-used", NULL };

  photos_test_pipeline_pipeline_new_async (NULL, filenames, NULL, photos_test_pipeline_async, fixture);
  g_main_loop_run (fixture->loop);

  {
    g_autoptr (GError) error = NULL;

    pipeline = photos_pipeline_new_finish (fixture->res, &error);
    g_assert_error (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED);
  }

  g_assert_null (pipeline);
}


static void
photos_test_pipeline_no_parent_with_uris_13 (PhotosTestPipelineFixture *fixture, gconstpointer user_data)
{
  g_autoptr (PhotosPipeline) pipeline = NULL;
  const gchar *const filenames[] = { "this-should-not-be-used", NULL };

  photos_test_pipeline_pipeline_new_async (NULL, filenames, NULL, photos_test_pipeline_async, fixture);
  g_main_loop_run (fixture->loop);

  {
    g_autoptr (GError) error = NULL;

    pipeline = photos_pipeline_new_finish (fixture->res, &error);
    g_assert_error (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED);
  }

  g_assert_null (pipeline);
}


static void
photos_test_pipeline_no_parent_with_uris_14 (PhotosTestPipelineFixture *fixture, gconstpointer user_data)
{
  g_autoptr (PhotosPipeline) pipeline = NULL;
  const gchar *const filenames[] = { "this-should-not-be-used", "photos-test-pipeline-not-edited-00.xml", NULL };

  photos_test_pipeline_pipeline_new_async (NULL, filenames, NULL, photos_test_pipeline_async, fixture);
  g_main_loop_run (fixture->loop);

  {
    g_autoptr (GError) error = NULL;

    pipeline = photos_pipeline_new_finish (fixture->res, &error);
    g_assert_error (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED);
  }

  g_assert_null (pipeline);
}


static void
photos_test_pipeline_no_parent_with_uris_15 (PhotosTestPipelineFixture *fixture, gconstpointer user_data)
{
  g_autoptr (PhotosPipeline) pipeline = NULL;
  const gchar *const filenames[] = { "this-should-not-be-used", "photos-test-pipeline-edited-00.xml", NULL };

  photos_test_pipeline_pipeline_new_async (NULL, filenames, NULL, photos_test_pipeline_async, fixture);
  g_main_loop_run (fixture->loop);

  {
    g_autoptr (GError) error = NULL;

    pipeline = photos_pipeline_new_finish (fixture->res, &error);
    g_assert_error (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED);
  }

  g_assert_null (pipeline);
}


static void
photos_test_pipeline_no_parent_with_uris_16 (PhotosTestPipelineFixture *fixture, gconstpointer user_data)
{
  g_autoptr (PhotosPipeline) pipeline = NULL;
  const gchar *const filenames[] = { "this-should-not-be-used", "this-does-not-exist", NULL };

  photos_test_pipeline_pipeline_new_async (NULL, filenames, NULL, photos_test_pipeline_async, fixture);
  g_main_loop_run (fixture->loop);

  {
    g_autoptr (GError) error = NULL;

    pipeline = photos_pipeline_new_finish (fixture->res, &error);
    g_assert_error (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED);
  }

  g_assert_null (pipeline);
}


static void
photos_test_pipeline_no_parent_with_uris_17 (PhotosTestPipelineFixture *fixture, gconstpointer user_data)
{
  g_autoptr (PhotosPipeline) pipeline = NULL;
  const gchar *const uris[] = { "this-should-not-be-used", "this-should-not-be-used-either", NULL };

  photos_pipeline_new_async (NULL, uris, NULL, photos_test_pipeline_async, fixture);
  g_main_loop_run (fixture->loop);

  {
    g_autoptr (GError) error = NULL;

    pipeline = photos_pipeline_new_finish (fixture->res, &error);
    g_assert_error (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED);
  }

  g_assert_null (pipeline);
}


static void
photos_test_pipeline_with_parent_blank_uris_0 (PhotosTestPipelineFixture *fixture, gconstpointer user_data)
{
  g_autoptr (GeglNode) parent = NULL;
  g_autoptr (PhotosPipeline) pipeline = NULL;
  const gchar *const uris[] = { NULL };

  parent = gegl_node_new ();
  photos_pipeline_new_async (parent, uris, NULL, photos_test_pipeline_async, fixture);
  g_main_loop_run (fixture->loop);

  {
    g_autoptr (GError) error = NULL;

    pipeline = photos_pipeline_new_finish (fixture->res, &error);
    g_assert_no_error (error);
  }

  photos_test_pipeline_check_empty (pipeline, parent);
}


static void
photos_test_pipeline_with_parent_blank_uris_1 (PhotosTestPipelineFixture *fixture, gconstpointer user_data)
{
  g_autoptr (GeglNode) parent = NULL;
  g_autoptr (PhotosPipeline) pipeline = NULL;
  const gchar *const filenames[] = { NULL, "photos-test-pipeline-not-edited-00.xml", NULL };

  parent = gegl_node_new ();
  photos_test_pipeline_pipeline_new_async (parent, filenames, NULL, photos_test_pipeline_async, fixture);
  g_main_loop_run (fixture->loop);

  {
    g_autoptr (GError) error = NULL;

    pipeline = photos_pipeline_new_finish (fixture->res, &error);
    g_assert_no_error (error);
  }

  photos_test_pipeline_check_empty (pipeline, parent);
}


static void
photos_test_pipeline_with_parent_blank_uris_2 (PhotosTestPipelineFixture *fixture, gconstpointer user_data)
{
  g_autoptr (GeglNode) parent = NULL;
  g_autoptr (PhotosPipeline) pipeline = NULL;
  const gchar *const filenames[] = { NULL, "photos-test-pipeline-edited-00.xml", NULL };

  parent = gegl_node_new ();
  photos_test_pipeline_pipeline_new_async (parent, filenames, NULL, photos_test_pipeline_async, fixture);
  g_main_loop_run (fixture->loop);

  {
    g_autoptr (GError) error = NULL;

    pipeline = photos_pipeline_new_finish (fixture->res, &error);
    g_assert_no_error (error);
  }

  photos_test_pipeline_check_empty (pipeline, parent);
}


static void
photos_test_pipeline_with_parent_blank_uris_3 (PhotosTestPipelineFixture *fixture, gconstpointer user_data)
{
  g_autoptr (GeglNode) parent = NULL;
  g_autoptr (PhotosPipeline) pipeline = NULL;
  const gchar *const uris[] = { NULL, "this-does-not-exist", NULL };

  parent = gegl_node_new ();
  photos_pipeline_new_async (parent, uris, NULL, photos_test_pipeline_async, fixture);
  g_main_loop_run (fixture->loop);

  {
    g_autoptr (GError) error = NULL;

    pipeline = photos_pipeline_new_finish (fixture->res, &error);
    g_assert_no_error (error);
  }

  photos_test_pipeline_check_empty (pipeline, parent);
}


static void
photos_test_pipeline_with_parent_blank_uris_4 (PhotosTestPipelineFixture *fixture, gconstpointer user_data)
{
  g_autoptr (GeglNode) parent = NULL;
  g_autoptr (PhotosPipeline) pipeline = NULL;
  const gchar *const uris[] = { NULL, "this-should-not-be-used", NULL };

  parent = gegl_node_new ();
  photos_pipeline_new_async (parent, uris, NULL, photos_test_pipeline_async, fixture);
  g_main_loop_run (fixture->loop);

  {
    g_autoptr (GError) error = NULL;

    pipeline = photos_pipeline_new_finish (fixture->res, &error);
    g_assert_no_error (error);
  }

  photos_test_pipeline_check_empty (pipeline, parent);
}


static void
photos_test_pipeline_with_parent_blank_uris_5 (PhotosTestPipelineFixture *fixture, gconstpointer user_data)
{
  g_autoptr (GeglNode) parent = NULL;
  g_autoptr (PhotosPipeline) pipeline = NULL;
  const gchar *const uris[] = { "", NULL };

  photos_pipeline_new_async (NULL, uris, NULL, photos_test_pipeline_async, fixture);
  g_main_loop_run (fixture->loop);

  {
    g_autoptr (GError) error = NULL;

    pipeline = photos_pipeline_new_finish (fixture->res, &error);
    g_assert_no_error (error);
  }

  photos_test_pipeline_check_empty (pipeline, parent);
}


static void
photos_test_pipeline_with_parent_blank_uris_6 (PhotosTestPipelineFixture *fixture, gconstpointer user_data)
{
  g_autoptr (GeglNode) parent = NULL;
  g_autoptr (PhotosPipeline) pipeline = NULL;
  const gchar *const filenames[] = { "", "photos-test-pipeline-not-edited-00.xml", NULL };

  parent = gegl_node_new ();
  photos_test_pipeline_pipeline_new_async (parent, filenames, NULL, photos_test_pipeline_async, fixture);
  g_main_loop_run (fixture->loop);

  {
    g_autoptr (GError) error = NULL;

    pipeline = photos_pipeline_new_finish (fixture->res, &error);
    g_assert_no_error (error);
  }

  photos_test_pipeline_check_empty (pipeline, parent);
}


static void
photos_test_pipeline_with_parent_blank_uris_7 (PhotosTestPipelineFixture *fixture, gconstpointer user_data)
{
  g_autoptr (GeglNode) parent = NULL;
  g_autoptr (PhotosPipeline) pipeline = NULL;
  const gchar *const filenames[] = { "", "photos-test-pipeline-edited-00.xml", NULL };

  parent = gegl_node_new ();
  photos_test_pipeline_pipeline_new_async (parent, filenames, NULL, photos_test_pipeline_async, fixture);
  g_main_loop_run (fixture->loop);

  {
    g_autoptr (GError) error = NULL;

    pipeline = photos_pipeline_new_finish (fixture->res, &error);
    g_assert_no_error (error);
  }

  photos_test_pipeline_check_empty (pipeline, parent);
}


static void
photos_test_pipeline_with_parent_blank_uris_8 (PhotosTestPipelineFixture *fixture, gconstpointer user_data)
{
  g_autoptr (GeglNode) parent = NULL;
  g_autoptr (PhotosPipeline) pipeline = NULL;
  const gchar *const filenames[] = { "", "this-does-not-exist", NULL };

  parent = gegl_node_new ();
  photos_test_pipeline_pipeline_new_async (parent, filenames, NULL, photos_test_pipeline_async, fixture);
  g_main_loop_run (fixture->loop);

  {
    g_autoptr (GError) error = NULL;

    pipeline = photos_pipeline_new_finish (fixture->res, &error);
    g_assert_no_error (error);
  }

  photos_test_pipeline_check_empty (pipeline, parent);
}


static void
photos_test_pipeline_with_parent_blank_uris_9 (PhotosTestPipelineFixture *fixture, gconstpointer user_data)
{
  g_autoptr (GeglNode) parent = NULL;
  g_autoptr (PhotosPipeline) pipeline = NULL;
  const gchar *const uris[] = { "", "this-should-not-be-used", NULL };

  parent = gegl_node_new ();
  photos_pipeline_new_async (parent, uris, NULL, photos_test_pipeline_async, fixture);
  g_main_loop_run (fixture->loop);

  {
    g_autoptr (GError) error = NULL;

    pipeline = photos_pipeline_new_finish (fixture->res, &error);
    g_assert_no_error (error);
  }

  photos_test_pipeline_check_empty (pipeline, parent);
}


static void
photos_test_pipeline_with_parent_null_uris (PhotosTestPipelineFixture *fixture, gconstpointer user_data)
{
  g_autoptr (GeglNode) parent = NULL;
  g_autoptr (PhotosPipeline) pipeline = NULL;

  parent = gegl_node_new ();
  photos_pipeline_new_async (parent, NULL, NULL, photos_test_pipeline_async, fixture);
  g_main_loop_run (fixture->loop);

  {
    g_autoptr (GError) error = NULL;

    pipeline = photos_pipeline_new_finish (fixture->res, &error);
    g_assert_no_error (error);
  }

  photos_test_pipeline_check_empty (pipeline, parent);
}


static void
photos_test_pipeline_with_parent_with_uris_0 (PhotosTestPipelineFixture *fixture, gconstpointer user_data)
{
  g_autoptr (GeglNode) parent = NULL;
  g_autoptr (PhotosPipeline) pipeline = NULL;
  const gchar *const filenames[] = { "photos-test-pipeline-not-edited-00.xml", NULL };

  parent = gegl_node_new ();
  photos_test_pipeline_pipeline_new_async (parent, filenames, NULL, photos_test_pipeline_async, fixture);
  g_main_loop_run (fixture->loop);

  {
    g_autoptr (GError) error = NULL;

    pipeline = photos_pipeline_new_finish (fixture->res, &error);
    g_assert_no_error (error);
  }

  photos_test_pipeline_check_empty (pipeline, parent);
}


static void
photos_test_pipeline_with_parent_with_uris_1 (PhotosTestPipelineFixture *fixture, gconstpointer user_data)
{
  g_autoptr (GeglNode) parent = NULL;
  g_autoptr (PhotosPipeline) pipeline = NULL;
  const gchar *const filenames[] =
    {
      "photos-test-pipeline-not-edited-00.xml",
      "photos-test-pipeline-edited-00.xml",
      NULL
    };

  parent = gegl_node_new ();
  photos_test_pipeline_pipeline_new_async (parent, filenames, NULL, photos_test_pipeline_async, fixture);
  g_main_loop_run (fixture->loop);

  {
    g_autoptr (GError) error = NULL;

    pipeline = photos_pipeline_new_finish (fixture->res, &error);
    g_assert_no_error (error);
  }

  photos_test_pipeline_check_empty (pipeline, parent);
}


static void
photos_test_pipeline_with_parent_with_uris_2 (PhotosTestPipelineFixture *fixture, gconstpointer user_data)
{
  g_autoptr (GeglNode) parent = NULL;
  g_autoptr (PhotosPipeline) pipeline = NULL;
  const gchar *const filenames[] = { "photos-test-pipeline-not-edited-00.xml", "this-does-not-exist", NULL };

  parent = gegl_node_new ();
  photos_test_pipeline_pipeline_new_async (parent, filenames, NULL, photos_test_pipeline_async, fixture);
  g_main_loop_run (fixture->loop);

  {
    g_autoptr (GError) error = NULL;

    pipeline = photos_pipeline_new_finish (fixture->res, &error);
    g_assert_no_error (error);
  }

  photos_test_pipeline_check_empty (pipeline, parent);
}


static void
photos_test_pipeline_with_parent_with_uris_3 (PhotosTestPipelineFixture *fixture, gconstpointer user_data)
{
  g_autoptr (GeglNode) parent = NULL;
  g_autoptr (PhotosPipeline) pipeline = NULL;
  const gchar *const filenames[] = { "photos-test-pipeline-not-edited-00.xml", "this-should-not-be-used", NULL };

  parent = gegl_node_new ();
  photos_test_pipeline_pipeline_new_async (parent, filenames, NULL, photos_test_pipeline_async, fixture);
  g_main_loop_run (fixture->loop);

  {
    g_autoptr (GError) error = NULL;

    pipeline = photos_pipeline_new_finish (fixture->res, &error);
    g_assert_no_error (error);
  }

  photos_test_pipeline_check_empty (pipeline, parent);
}


static void
photos_test_pipeline_with_parent_with_uris_4 (PhotosTestPipelineFixture *fixture, gconstpointer user_data)
{
  g_autoptr (GeglNode) parent = NULL;
  g_autoptr (PhotosPipeline) pipeline = NULL;
  const gchar *const filenames[] = { "photos-test-pipeline-edited-00.xml", NULL };

  parent = gegl_node_new ();
  photos_test_pipeline_pipeline_new_async (parent, filenames, NULL, photos_test_pipeline_async, fixture);
  g_main_loop_run (fixture->loop);

  {
    g_autoptr (GError) error = NULL;

    pipeline = photos_pipeline_new_finish (fixture->res, &error);
    g_assert_no_error (error);
  }

  photos_test_pipeline_check_full (pipeline, parent);
}


static void
photos_test_pipeline_with_parent_with_uris_5 (PhotosTestPipelineFixture *fixture, gconstpointer user_data)
{
  g_autoptr (GeglNode) parent = NULL;
  g_autoptr (PhotosPipeline) pipeline = NULL;
  const gchar *const filenames[] =
    {
      "photos-test-pipeline-edited-00.xml",
      "photos-test-pipeline-not-edited-00.xml",
      NULL
    };

  parent = gegl_node_new ();
  photos_test_pipeline_pipeline_new_async (parent, filenames, NULL, photos_test_pipeline_async, fixture);
  g_main_loop_run (fixture->loop);

  {
    g_autoptr (GError) error = NULL;

    pipeline = photos_pipeline_new_finish (fixture->res, &error);
    g_assert_no_error (error);
  }

  photos_test_pipeline_check_full (pipeline, parent);
}


static void
photos_test_pipeline_with_parent_with_uris_6 (PhotosTestPipelineFixture *fixture, gconstpointer user_data)
{
  g_autoptr (GeglNode) parent = NULL;
  g_autoptr (PhotosPipeline) pipeline = NULL;
  const gchar *const filenames[] = { "photos-test-pipeline-edited-00.xml", "this-does-not-exist", NULL };

  parent = gegl_node_new ();
  photos_test_pipeline_pipeline_new_async (parent, filenames, NULL, photos_test_pipeline_async, fixture);
  g_main_loop_run (fixture->loop);

  {
    g_autoptr (GError) error = NULL;

    pipeline = photos_pipeline_new_finish (fixture->res, &error);
    g_assert_no_error (error);
  }

  photos_test_pipeline_check_full (pipeline, parent);
}


static void
photos_test_pipeline_with_parent_with_uris_7 (PhotosTestPipelineFixture *fixture, gconstpointer user_data)
{
  g_autoptr (GeglNode) parent = NULL;
  g_autoptr (PhotosPipeline) pipeline = NULL;
  const gchar *const filenames[] = { "photos-test-pipeline-edited-00.xml", "this-should-not-be-used", NULL };

  parent = gegl_node_new ();
  photos_test_pipeline_pipeline_new_async (parent, filenames, NULL, photos_test_pipeline_async, fixture);
  g_main_loop_run (fixture->loop);

  {
    g_autoptr (GError) error = NULL;

    pipeline = photos_pipeline_new_finish (fixture->res, &error);
    g_assert_no_error (error);
  }

  photos_test_pipeline_check_full (pipeline, parent);
}


static void
photos_test_pipeline_with_parent_with_uris_8 (PhotosTestPipelineFixture *fixture, gconstpointer user_data)
{
  g_autoptr (GeglNode) parent = NULL;
  g_autoptr (PhotosPipeline) pipeline = NULL;
  const gchar *const filenames[] = { "this-does-not-exist", NULL };

  parent = gegl_node_new ();
  photos_test_pipeline_pipeline_new_async (parent, filenames, NULL, photos_test_pipeline_async, fixture);
  g_main_loop_run (fixture->loop);

  {
    g_autoptr (GError) error = NULL;

    pipeline = photos_pipeline_new_finish (fixture->res, &error);
    g_assert_no_error (error);
  }

  photos_test_pipeline_check_empty (pipeline, parent);
}


static void
photos_test_pipeline_with_parent_with_uris_9 (PhotosTestPipelineFixture *fixture, gconstpointer user_data)
{
  g_autoptr (GeglNode) parent = NULL;
  g_autoptr (PhotosPipeline) pipeline = NULL;
  const gchar *const filenames[] = { "this-does-not-exist", "photos-test-pipeline-not-edited-00.xml", NULL };

  parent = gegl_node_new ();
  photos_test_pipeline_pipeline_new_async (parent, filenames, NULL, photos_test_pipeline_async, fixture);
  g_main_loop_run (fixture->loop);

  {
    g_autoptr (GError) error = NULL;

    pipeline = photos_pipeline_new_finish (fixture->res, &error);
    g_assert_no_error (error);
  }

  photos_test_pipeline_check_empty (pipeline, parent);
}


static void
photos_test_pipeline_with_parent_with_uris_10 (PhotosTestPipelineFixture *fixture, gconstpointer user_data)
{
  g_autoptr (GeglNode) parent = NULL;
  g_autoptr (PhotosPipeline) pipeline = NULL;
  const gchar *const filenames[] = { "this-does-not-exist", "photos-test-pipeline-edited-00.xml", NULL };

  parent = gegl_node_new ();
  photos_test_pipeline_pipeline_new_async (parent, filenames, NULL, photos_test_pipeline_async, fixture);
  g_main_loop_run (fixture->loop);

  {
    g_autoptr (GError) error = NULL;

    pipeline = photos_pipeline_new_finish (fixture->res, &error);
    g_assert_no_error (error);
  }

  photos_test_pipeline_check_full (pipeline, parent);
}


static void
photos_test_pipeline_with_parent_with_uris_11 (PhotosTestPipelineFixture *fixture, gconstpointer user_data)
{
  g_autoptr (GeglNode) parent = NULL;
  g_autoptr (PhotosPipeline) pipeline = NULL;
  const gchar *const filenames[] = { "this-does-not-exist", "this-does-not-exist-either", NULL };

  parent = gegl_node_new ();
  photos_test_pipeline_pipeline_new_async (parent, filenames, NULL, photos_test_pipeline_async, fixture);
  g_main_loop_run (fixture->loop);

  {
    g_autoptr (GError) error = NULL;

    pipeline = photos_pipeline_new_finish (fixture->res, &error);
    g_assert_no_error (error);
  }

  photos_test_pipeline_check_empty (pipeline, parent);
}


static void
photos_test_pipeline_with_parent_with_uris_12 (PhotosTestPipelineFixture *fixture, gconstpointer user_data)
{
  g_autoptr (GeglNode) parent = NULL;
  g_autoptr (PhotosPipeline) pipeline = NULL;
  const gchar *const filenames[] = { "this-does-not-exist", "this-should-not-be-used", NULL };

  parent = gegl_node_new ();
  photos_test_pipeline_pipeline_new_async (parent, filenames, NULL, photos_test_pipeline_async, fixture);
  g_main_loop_run (fixture->loop);

  {
    g_autoptr (GError) error = NULL;

    pipeline = photos_pipeline_new_finish (fixture->res, &error);
    g_assert_error (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED);
  }

  g_assert_null (pipeline);
}


static void
photos_test_pipeline_with_parent_with_uris_13 (PhotosTestPipelineFixture *fixture, gconstpointer user_data)
{
  g_autoptr (GeglNode) parent = NULL;
  g_autoptr (PhotosPipeline) pipeline = NULL;
  const gchar *const filenames[] = { "this-should-not-be-used", NULL };

  parent = gegl_node_new ();
  photos_test_pipeline_pipeline_new_async (parent, filenames, NULL, photos_test_pipeline_async, fixture);
  g_main_loop_run (fixture->loop);

  {
    g_autoptr (GError) error = NULL;

    pipeline = photos_pipeline_new_finish (fixture->res, &error);
    g_assert_error (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED);
  }

  g_assert_null (pipeline);
}


static void
photos_test_pipeline_with_parent_with_uris_14 (PhotosTestPipelineFixture *fixture, gconstpointer user_data)
{
  g_autoptr (GeglNode) parent = NULL;
  g_autoptr (PhotosPipeline) pipeline = NULL;
  const gchar *const filenames[] = { "this-should-not-be-used", "photos-test-pipeline-not-edited-00.xml", NULL };

  parent = gegl_node_new ();
  photos_test_pipeline_pipeline_new_async (parent, filenames, NULL, photos_test_pipeline_async, fixture);
  g_main_loop_run (fixture->loop);

  {
    g_autoptr (GError) error = NULL;

    pipeline = photos_pipeline_new_finish (fixture->res, &error);
    g_assert_error (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED);
  }

  g_assert_null (pipeline);
}


static void
photos_test_pipeline_with_parent_with_uris_15 (PhotosTestPipelineFixture *fixture, gconstpointer user_data)
{
  g_autoptr (GeglNode) parent = NULL;
  g_autoptr (PhotosPipeline) pipeline = NULL;
  const gchar *const filenames[] = { "this-should-not-be-used", "photos-test-pipeline-edited-00.xml", NULL };

  parent = gegl_node_new ();
  photos_test_pipeline_pipeline_new_async (parent, filenames, NULL, photos_test_pipeline_async, fixture);
  g_main_loop_run (fixture->loop);

  {
    g_autoptr (GError) error = NULL;

    pipeline = photos_pipeline_new_finish (fixture->res, &error);
    g_assert_error (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED);
  }

  g_assert_null (pipeline);
}


static void
photos_test_pipeline_with_parent_with_uris_16 (PhotosTestPipelineFixture *fixture, gconstpointer user_data)
{
  g_autoptr (GeglNode) parent = NULL;
  g_autoptr (PhotosPipeline) pipeline = NULL;
  const gchar *const filenames[] = { "this-should-not-be-used", "this-does-not-exist", NULL };

  parent = gegl_node_new ();
  photos_test_pipeline_pipeline_new_async (parent, filenames, NULL, photos_test_pipeline_async, fixture);
  g_main_loop_run (fixture->loop);

  {
    g_autoptr (GError) error = NULL;

    pipeline = photos_pipeline_new_finish (fixture->res, &error);
    g_assert_error (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED);
  }

  g_assert_null (pipeline);
}


static void
photos_test_pipeline_with_parent_with_uris_17 (PhotosTestPipelineFixture *fixture, gconstpointer user_data)
{
  g_autoptr (GeglNode) parent = NULL;
  g_autoptr (PhotosPipeline) pipeline = NULL;
  const gchar *const uris[] = { "this-should-not-be-used", "this-should-not-be-used-either", NULL };

  parent = gegl_node_new ();
  photos_pipeline_new_async (parent, uris, NULL, photos_test_pipeline_async, fixture);
  g_main_loop_run (fixture->loop);

  {
    g_autoptr (GError) error = NULL;

    pipeline = photos_pipeline_new_finish (fixture->res, &error);
    g_assert_error (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED);
  }

  g_assert_null (pipeline);
}


gint
main (gint argc, gchar *argv[])
{
  gint exit_status;

  setlocale (LC_ALL, "");
  g_test_init (&argc, &argv, NULL);
  photos_debug_init ();
  photos_gegl_init ();
  photos_gegl_ensure_builtins ();

  g_test_add ("/pipeline/new/no-parent-blank-uris-0",
              PhotosTestPipelineFixture,
              NULL,
              photos_test_pipeline_setup,
              photos_test_pipeline_no_parent_blank_uris_0,
              photos_test_pipeline_teardown);

  g_test_add ("/pipeline/new/no-parent-blank-uris-1",
              PhotosTestPipelineFixture,
              NULL,
              photos_test_pipeline_setup,
              photos_test_pipeline_no_parent_blank_uris_1,
              photos_test_pipeline_teardown);

  g_test_add ("/pipeline/new/no-parent-blank-uris-2",
              PhotosTestPipelineFixture,
              NULL,
              photos_test_pipeline_setup,
              photos_test_pipeline_no_parent_blank_uris_2,
              photos_test_pipeline_teardown);

  g_test_add ("/pipeline/new/no-parent-blank-uris-3",
              PhotosTestPipelineFixture,
              NULL,
              photos_test_pipeline_setup,
              photos_test_pipeline_no_parent_blank_uris_3,
              photos_test_pipeline_teardown);

  g_test_add ("/pipeline/new/no-parent-blank-uris-4",
              PhotosTestPipelineFixture,
              NULL,
              photos_test_pipeline_setup,
              photos_test_pipeline_no_parent_blank_uris_4,
              photos_test_pipeline_teardown);

  g_test_add ("/pipeline/new/no-parent-blank-uris-5",
              PhotosTestPipelineFixture,
              NULL,
              photos_test_pipeline_setup,
              photos_test_pipeline_no_parent_blank_uris_5,
              photos_test_pipeline_teardown);

  g_test_add ("/pipeline/new/no-parent-blank-uris-6",
              PhotosTestPipelineFixture,
              NULL,
              photos_test_pipeline_setup,
              photos_test_pipeline_no_parent_blank_uris_6,
              photos_test_pipeline_teardown);

  g_test_add ("/pipeline/new/no-parent-blank-uris-7",
              PhotosTestPipelineFixture,
              NULL,
              photos_test_pipeline_setup,
              photos_test_pipeline_no_parent_blank_uris_7,
              photos_test_pipeline_teardown);

  g_test_add ("/pipeline/new/no-parent-blank-uris-8",
              PhotosTestPipelineFixture,
              NULL,
              photos_test_pipeline_setup,
              photos_test_pipeline_no_parent_blank_uris_8,
              photos_test_pipeline_teardown);

  g_test_add ("/pipeline/new/no-parent-blank-uris-9",
              PhotosTestPipelineFixture,
              NULL,
              photos_test_pipeline_setup,
              photos_test_pipeline_no_parent_blank_uris_9,
              photos_test_pipeline_teardown);

  g_test_add ("/pipeline/new/no-parent-null-uris",
              PhotosTestPipelineFixture,
              NULL,
              photos_test_pipeline_setup,
              photos_test_pipeline_no_parent_null_uris,
              photos_test_pipeline_teardown);

  g_test_add ("/pipeline/new/no-parent-with-uris-0",
              PhotosTestPipelineFixture,
              NULL,
              photos_test_pipeline_setup,
              photos_test_pipeline_no_parent_with_uris_0,
              photos_test_pipeline_teardown);

  g_test_add ("/pipeline/new/no-parent-with-uris-1",
              PhotosTestPipelineFixture,
              NULL,
              photos_test_pipeline_setup,
              photos_test_pipeline_no_parent_with_uris_1,
              photos_test_pipeline_teardown);

  g_test_add ("/pipeline/new/no-parent-with-uris-2",
              PhotosTestPipelineFixture,
              NULL,
              photos_test_pipeline_setup,
              photos_test_pipeline_no_parent_with_uris_2,
              photos_test_pipeline_teardown);

  g_test_add ("/pipeline/new/no-parent-with-uris-3",
              PhotosTestPipelineFixture,
              NULL,
              photos_test_pipeline_setup,
              photos_test_pipeline_no_parent_with_uris_3,
              photos_test_pipeline_teardown);

  g_test_add ("/pipeline/new/no-parent-with-uris-4",
              PhotosTestPipelineFixture,
              NULL,
              photos_test_pipeline_setup,
              photos_test_pipeline_no_parent_with_uris_4,
              photos_test_pipeline_teardown);

  g_test_add ("/pipeline/new/no-parent-with-uris-5",
              PhotosTestPipelineFixture,
              NULL,
              photos_test_pipeline_setup,
              photos_test_pipeline_no_parent_with_uris_5,
              photos_test_pipeline_teardown);

  g_test_add ("/pipeline/new/no-parent-with-uris-6",
              PhotosTestPipelineFixture,
              NULL,
              photos_test_pipeline_setup,
              photos_test_pipeline_no_parent_with_uris_6,
              photos_test_pipeline_teardown);

  g_test_add ("/pipeline/new/no-parent-with-uris-7",
              PhotosTestPipelineFixture,
              NULL,
              photos_test_pipeline_setup,
              photos_test_pipeline_no_parent_with_uris_7,
              photos_test_pipeline_teardown);

  g_test_add ("/pipeline/new/no-parent-with-uris-8",
              PhotosTestPipelineFixture,
              NULL,
              photos_test_pipeline_setup,
              photos_test_pipeline_no_parent_with_uris_8,
              photos_test_pipeline_teardown);

  g_test_add ("/pipeline/new/no-parent-with-uris-9",
              PhotosTestPipelineFixture,
              NULL,
              photos_test_pipeline_setup,
              photos_test_pipeline_no_parent_with_uris_9,
              photos_test_pipeline_teardown);

  g_test_add ("/pipeline/new/no-parent-with-uris-10",
              PhotosTestPipelineFixture,
              NULL,
              photos_test_pipeline_setup,
              photos_test_pipeline_no_parent_with_uris_10,
              photos_test_pipeline_teardown);

  g_test_add ("/pipeline/new/no-parent-with-uris-11",
              PhotosTestPipelineFixture,
              NULL,
              photos_test_pipeline_setup,
              photos_test_pipeline_no_parent_with_uris_11,
              photos_test_pipeline_teardown);

  g_test_add ("/pipeline/new/no-parent-with-uris-12",
              PhotosTestPipelineFixture,
              NULL,
              photos_test_pipeline_setup,
              photos_test_pipeline_no_parent_with_uris_12,
              photos_test_pipeline_teardown);

  g_test_add ("/pipeline/new/no-parent-with-uris-13",
              PhotosTestPipelineFixture,
              NULL,
              photos_test_pipeline_setup,
              photos_test_pipeline_no_parent_with_uris_13,
              photos_test_pipeline_teardown);

  g_test_add ("/pipeline/new/no-parent-with-uris-14",
              PhotosTestPipelineFixture,
              NULL,
              photos_test_pipeline_setup,
              photos_test_pipeline_no_parent_with_uris_14,
              photos_test_pipeline_teardown);

  g_test_add ("/pipeline/new/no-parent-with-uris-15",
              PhotosTestPipelineFixture,
              NULL,
              photos_test_pipeline_setup,
              photos_test_pipeline_no_parent_with_uris_15,
              photos_test_pipeline_teardown);

  g_test_add ("/pipeline/new/no-parent-with-uris-16",
              PhotosTestPipelineFixture,
              NULL,
              photos_test_pipeline_setup,
              photos_test_pipeline_no_parent_with_uris_16,
              photos_test_pipeline_teardown);

  g_test_add ("/pipeline/new/no-parent-with-uris-17",
              PhotosTestPipelineFixture,
              NULL,
              photos_test_pipeline_setup,
              photos_test_pipeline_no_parent_with_uris_17,
              photos_test_pipeline_teardown);

  g_test_add ("/pipeline/new/with-parent-blank-uris-0",
              PhotosTestPipelineFixture,
              NULL,
              photos_test_pipeline_setup,
              photos_test_pipeline_with_parent_blank_uris_0,
              photos_test_pipeline_teardown);

  g_test_add ("/pipeline/new/with-parent-blank-uris-1",
              PhotosTestPipelineFixture,
              NULL,
              photos_test_pipeline_setup,
              photos_test_pipeline_with_parent_blank_uris_1,
              photos_test_pipeline_teardown);

  g_test_add ("/pipeline/new/with-parent-blank-uris-2",
              PhotosTestPipelineFixture,
              NULL,
              photos_test_pipeline_setup,
              photos_test_pipeline_with_parent_blank_uris_2,
              photos_test_pipeline_teardown);

  g_test_add ("/pipeline/new/with-parent-blank-uris-3",
              PhotosTestPipelineFixture,
              NULL,
              photos_test_pipeline_setup,
              photos_test_pipeline_with_parent_blank_uris_3,
              photos_test_pipeline_teardown);

  g_test_add ("/pipeline/new/with-parent-blank-uris-4",
              PhotosTestPipelineFixture,
              NULL,
              photos_test_pipeline_setup,
              photos_test_pipeline_with_parent_blank_uris_4,
              photos_test_pipeline_teardown);

  g_test_add ("/pipeline/new/with-parent-blank-uris-5",
              PhotosTestPipelineFixture,
              NULL,
              photos_test_pipeline_setup,
              photos_test_pipeline_with_parent_blank_uris_5,
              photos_test_pipeline_teardown);

  g_test_add ("/pipeline/new/with-parent-blank-uris-6",
              PhotosTestPipelineFixture,
              NULL,
              photos_test_pipeline_setup,
              photos_test_pipeline_with_parent_blank_uris_6,
              photos_test_pipeline_teardown);

  g_test_add ("/pipeline/new/with-parent-blank-uris-7",
              PhotosTestPipelineFixture,
              NULL,
              photos_test_pipeline_setup,
              photos_test_pipeline_with_parent_blank_uris_7,
              photos_test_pipeline_teardown);

  g_test_add ("/pipeline/new/with-parent-blank-uris-8",
              PhotosTestPipelineFixture,
              NULL,
              photos_test_pipeline_setup,
              photos_test_pipeline_with_parent_blank_uris_8,
              photos_test_pipeline_teardown);

  g_test_add ("/pipeline/new/with-parent-blank-uris-9",
              PhotosTestPipelineFixture,
              NULL,
              photos_test_pipeline_setup,
              photos_test_pipeline_with_parent_blank_uris_9,
              photos_test_pipeline_teardown);

  g_test_add ("/pipeline/new/with-parent-null-uris",
              PhotosTestPipelineFixture,
              NULL,
              photos_test_pipeline_setup,
              photos_test_pipeline_with_parent_null_uris,
              photos_test_pipeline_teardown);

  g_test_add ("/pipeline/new/with-parent-with-uris-0",
              PhotosTestPipelineFixture,
              NULL,
              photos_test_pipeline_setup,
              photos_test_pipeline_with_parent_with_uris_0,
              photos_test_pipeline_teardown);

  g_test_add ("/pipeline/new/with-parent-with-uris-1",
              PhotosTestPipelineFixture,
              NULL,
              photos_test_pipeline_setup,
              photos_test_pipeline_with_parent_with_uris_1,
              photos_test_pipeline_teardown);

  g_test_add ("/pipeline/new/with-parent-with-uris-2",
              PhotosTestPipelineFixture,
              NULL,
              photos_test_pipeline_setup,
              photos_test_pipeline_with_parent_with_uris_2,
              photos_test_pipeline_teardown);

  g_test_add ("/pipeline/new/with-parent-with-uris-3",
              PhotosTestPipelineFixture,
              NULL,
              photos_test_pipeline_setup,
              photos_test_pipeline_with_parent_with_uris_3,
              photos_test_pipeline_teardown);

  g_test_add ("/pipeline/new/with-parent-with-uris-4",
              PhotosTestPipelineFixture,
              NULL,
              photos_test_pipeline_setup,
              photos_test_pipeline_with_parent_with_uris_4,
              photos_test_pipeline_teardown);

  g_test_add ("/pipeline/new/with-parent-with-uris-5",
              PhotosTestPipelineFixture,
              NULL,
              photos_test_pipeline_setup,
              photos_test_pipeline_with_parent_with_uris_5,
              photos_test_pipeline_teardown);

  g_test_add ("/pipeline/new/with-parent-with-uris-6",
              PhotosTestPipelineFixture,
              NULL,
              photos_test_pipeline_setup,
              photos_test_pipeline_with_parent_with_uris_6,
              photos_test_pipeline_teardown);

  g_test_add ("/pipeline/new/with-parent-with-uris-7",
              PhotosTestPipelineFixture,
              NULL,
              photos_test_pipeline_setup,
              photos_test_pipeline_with_parent_with_uris_7,
              photos_test_pipeline_teardown);

  g_test_add ("/pipeline/new/with-parent-with-uris-8",
              PhotosTestPipelineFixture,
              NULL,
              photos_test_pipeline_setup,
              photos_test_pipeline_with_parent_with_uris_8,
              photos_test_pipeline_teardown);

  g_test_add ("/pipeline/new/with-parent-with-uris-9",
              PhotosTestPipelineFixture,
              NULL,
              photos_test_pipeline_setup,
              photos_test_pipeline_with_parent_with_uris_9,
              photos_test_pipeline_teardown);

  g_test_add ("/pipeline/new/with-parent-with-uris-10",
              PhotosTestPipelineFixture,
              NULL,
              photos_test_pipeline_setup,
              photos_test_pipeline_with_parent_with_uris_10,
              photos_test_pipeline_teardown);

  g_test_add ("/pipeline/new/with-parent-with-uris-11",
              PhotosTestPipelineFixture,
              NULL,
              photos_test_pipeline_setup,
              photos_test_pipeline_with_parent_with_uris_11,
              photos_test_pipeline_teardown);

  g_test_add ("/pipeline/new/with-parent-with-uris-12",
              PhotosTestPipelineFixture,
              NULL,
              photos_test_pipeline_setup,
              photos_test_pipeline_with_parent_with_uris_12,
              photos_test_pipeline_teardown);

  g_test_add ("/pipeline/new/with-parent-with-uris-13",
              PhotosTestPipelineFixture,
              NULL,
              photos_test_pipeline_setup,
              photos_test_pipeline_with_parent_with_uris_13,
              photos_test_pipeline_teardown);

  g_test_add ("/pipeline/new/with-parent-with-uris-14",
              PhotosTestPipelineFixture,
              NULL,
              photos_test_pipeline_setup,
              photos_test_pipeline_with_parent_with_uris_14,
              photos_test_pipeline_teardown);

  g_test_add ("/pipeline/new/with-parent-with-uris-15",
              PhotosTestPipelineFixture,
              NULL,
              photos_test_pipeline_setup,
              photos_test_pipeline_with_parent_with_uris_15,
              photos_test_pipeline_teardown);

  g_test_add ("/pipeline/new/with-parent-with-uris-16",
              PhotosTestPipelineFixture,
              NULL,
              photos_test_pipeline_setup,
              photos_test_pipeline_with_parent_with_uris_16,
              photos_test_pipeline_teardown);

  g_test_add ("/pipeline/new/with-parent-with-uris-17",
              PhotosTestPipelineFixture,
              NULL,
              photos_test_pipeline_setup,
              photos_test_pipeline_with_parent_with_uris_17,
              photos_test_pipeline_teardown);

  exit_status = g_test_run ();

  gegl_exit ();
  return exit_status;
}
