/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2012 – 2017 Red Hat, Inc.
 * Copyright © 2017 Shivam Tripathi
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

#include <geocode-glib/geocode-glib.h>
#include <gio/gio.h>
#include <glib.h>
#include <glib/gi18n.h>

#include "photos-base-manager.h"
#include "photos-camera-cache.h"
#include "photos-local-item.h"
#include "photos-properties-dialog.h"
#include "photos-quarks.h"
#include "photos-query-builder.h"
#include "photos-search-context.h"
#include "photos-tracker-queue.h"
#include "photos-utils.h"


struct _PhotosPropertiesDialog
{
  GtkDialog parent_instance;
  GCancellable *cancellable;
  GtkWidget *camera_w;
  GtkWidget *grid;
  GtkWidget *title_entry;
  GtkWidget *modified_data;
  GtkWidget *revert_button;
  GtkWidget *location_w;
  PhotosBaseManager *item_mngr;
  PhotosCameraCache *camera_cache;
  PhotosTrackerQueue *queue;
  gchar *urn;
  guint title_entry_timeout;
};

enum
{
  PROP_0,
  PROP_URN
};

enum
{
  DISCARD_ALL_EDITS,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };


G_DEFINE_TYPE (PhotosPropertiesDialog, photos_properties_dialog, GTK_TYPE_DIALOG);


enum
{
  TITLE_ENTRY_TIMEOUT = 200 /* ms */
};


static void
photos_properties_dialog_get_camera (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosPropertiesDialog *self;
  g_autofree gchar *camera = NULL;

  {
    g_autoptr (GError) error = NULL;

    camera = photos_camera_cache_get_camera_finish (PHOTOS_CAMERA_CACHE (source_object), res, &error);
    if (error != NULL)
      {
        if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
          g_warning ("Unable to get camera: %s", error->message);
        return;
      }
  }

  /* Now we know that self has not been destroyed. */
  self = PHOTOS_PROPERTIES_DIALOG (user_data);

  if (camera != NULL)
    {
      GtkWidget *camera_data;

      camera_data = gtk_label_new (camera);
      gtk_widget_set_halign (camera_data, GTK_ALIGN_START);
      gtk_grid_attach_next_to (GTK_GRID (self->grid), camera_data, self->camera_w, GTK_POS_RIGHT, 2, 1);
      gtk_widget_show (camera_data);
    }
}


static void
photos_properties_dialog_location_reverse_resolve (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosPropertiesDialog *self;
  g_autoptr (GeocodePlace) place = NULL;
  GeocodeReverse *reverse = GEOCODE_REVERSE (source_object);
  GtkWidget *location_data;
  const gchar *location_area;
  const gchar *location_country;
  const gchar *location_town;
  g_autofree gchar *location_str = NULL;

  {
    g_autoptr (GError) error = NULL;

    place = geocode_reverse_resolve_finish (reverse, res, &error);
    if (error != NULL)
      {
        if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
          g_warning ("Unable to resolve latitude and longitude: %s", error->message);
        goto out;
      }
  }

  self = PHOTOS_PROPERTIES_DIALOG (user_data);

  location_area = geocode_place_get_area (place);
  location_town = geocode_place_get_town (place);
  location_country =  geocode_place_get_country (place);
  location_str = g_strdup_printf ("%s, %s, %s", location_area, location_town, location_country);

  location_data = gtk_label_new (location_str);
  gtk_widget_set_halign (location_data, GTK_ALIGN_START);
  gtk_grid_attach_next_to (GTK_GRID (self->grid), location_data, self->location_w, GTK_POS_RIGHT, 2, 1);
  gtk_widget_show (location_data);

 out:
  return;
}


static void
photos_properties_dialog_location_cursor_next (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosPropertiesDialog *self;
  g_autoptr (GeocodeLocation) location = NULL;
  g_autoptr (GeocodeReverse) reverse = NULL;
  TrackerSparqlCursor *cursor = TRACKER_SPARQL_CURSOR (source_object);
  gboolean success;
  gdouble latitude;
  gdouble longitude;

  {
    g_autoptr (GError) error = NULL;

    /* Note that tracker_sparql_cursor_next_finish can return FALSE even
     * without an error.
     */
    success = tracker_sparql_cursor_next_finish (cursor, res, &error);
    if (error != NULL)
      {
        if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
          g_warning ("Unable to read latitude and longitude: %s", error->message);
        goto out;
      }
  }

  self = PHOTOS_PROPERTIES_DIALOG (user_data);

  /* Note that the following SPARQL query:
   *   SELECT slo:latitude (<(foo)>) slo:longitude (<(foo)>) WHERE {}
   * ... will not return an empty cursor, but:
   *   (null), (null)
   */
  if (!success)
    {
      g_warning ("Cursor is empty — possibly wrong SPARQL query");
      goto out;
    }

  latitude = tracker_sparql_cursor_get_double (cursor, 0);
  longitude = tracker_sparql_cursor_get_double (cursor, 1);
  location = geocode_location_new (latitude, longitude, GEOCODE_LOCATION_ACCURACY_UNKNOWN);
  reverse = geocode_reverse_new_for_location (location);
  geocode_reverse_resolve_async (reverse,
                                 self->cancellable,
                                 photos_properties_dialog_location_reverse_resolve,
                                 self);

 out:
  return;
}


static void
photos_properties_dialog_location_query_executed (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosPropertiesDialog *self = PHOTOS_PROPERTIES_DIALOG (user_data);
  TrackerSparqlConnection *connection = TRACKER_SPARQL_CONNECTION (source_object);
  TrackerSparqlCursor *cursor = NULL; /* TODO: use g_autoptr */

  {
    g_autoptr (GError) error = NULL;

    cursor = tracker_sparql_connection_query_finish (connection, res, &error);
    if (error != NULL)
      {
        g_warning ("Unable to query latitude and longitude: %s", error->message);
        goto out;
      }
  }

  tracker_sparql_cursor_next_async (cursor,
                                    self->cancellable,
                                    photos_properties_dialog_location_cursor_next,
                                    self);

 out:
  g_clear_object (&cursor);
}


static void
photos_properties_dialog_modified_updated (PhotosPropertiesDialog *self, gboolean is_edited)
{
  GtkStyleContext *context;

  if (is_edited)
    {
      gtk_label_set_text (GTK_LABEL (self->modified_data), _("Edited in Photos"));
      context = gtk_widget_get_style_context (self->modified_data);
      gtk_style_context_remove_class (context, "photos-fade-out");
      gtk_style_context_add_class (context, "photos-fade-in");
      gtk_widget_show (self->modified_data);

      context = gtk_widget_get_style_context (self->revert_button);
      gtk_style_context_remove_class (context, "photos-fade-out");
      gtk_style_context_add_class (context, "photos-fade-in");
      gtk_widget_show (self->revert_button);
    }
  else
    {
      gtk_label_set_text (GTK_LABEL (self->modified_data), _("Untouched"));
      context = gtk_widget_get_style_context (self->modified_data);
      gtk_style_context_remove_class (context, "photos-fade-out");
      gtk_style_context_add_class (context, "photos-fade-in");
      gtk_widget_show (self->modified_data);

      context = gtk_widget_get_style_context (self->revert_button);
      gtk_style_context_remove_class (context, "photos-fade-in");
      gtk_style_context_add_class (context, "photos-fade-out");
    }
}


static void
photos_properties_dialog_name_update (PhotosPropertiesDialog *self)
{
  const gchar *new_title;

  new_title = gtk_entry_get_text (GTK_ENTRY (self->title_entry));
  photos_utils_set_edited_name (self->urn, new_title);
}


static void
photos_properties_dialog_pipeline_is_edited (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosPropertiesDialog *self;
  PhotosBaseItem *item = PHOTOS_BASE_ITEM (source_object);
  gboolean is_edited;

  {
    g_autoptr (GError) error = NULL;

    is_edited = photos_base_item_pipeline_is_edited_finish (item, res, &error);
    if (error != NULL)
      {
        if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
          g_warning ("Unable to check if item has been edited or not: %s", error->message);
        return;
      }
  }

  self = PHOTOS_PROPERTIES_DIALOG (user_data);

  photos_properties_dialog_modified_updated (self, is_edited);
}


static void
photos_properties_dialog_remove_timeout (PhotosPropertiesDialog *self)
{
  if (self->title_entry_timeout != 0)
    {
      g_source_remove (self->title_entry_timeout);
      self->title_entry_timeout = 0;
    }
}


static gboolean
photos_properties_dialog_title_entry_timeout (gpointer user_data)
{
  PhotosPropertiesDialog *self = PHOTOS_PROPERTIES_DIALOG (user_data);

  self->title_entry_timeout = 0;
  photos_properties_dialog_name_update (self);

  return G_SOURCE_REMOVE;
}


static void
photos_properties_dialog_title_entry_changed (GtkEditable *editable, gpointer user_data)
{
  PhotosPropertiesDialog *self = PHOTOS_PROPERTIES_DIALOG (user_data);

  photos_properties_dialog_remove_timeout (self);

  self->title_entry_timeout = g_timeout_add (TITLE_ENTRY_TIMEOUT,
                                             photos_properties_dialog_title_entry_timeout,
                                             self);
}


static void
photos_properties_dialog_revert_clicked (PhotosPropertiesDialog *self)
{
  g_signal_emit (self, signals[DISCARD_ALL_EDITS], 0);
  photos_properties_dialog_modified_updated (self, FALSE);
}


static void
photos_properties_dialog_constructed (GObject *object)
{
  PhotosPropertiesDialog *self = PHOTOS_PROPERTIES_DIALOG (object);
  GApplication *app;
  g_autoptr (GDateTime) date_modified = NULL;
  GtkStyleContext *context;
  GtkWidget *author_w = NULL;
  GtkWidget *content_area;
  GtkWidget *date_created_w = NULL;
  GtkWidget *date_modified_data;
  GtkWidget *date_modified_w;
  GtkWidget *dimensions_w = NULL;
  GtkWidget *exposure_time_w = NULL;
  GtkWidget *flash_w = NULL;
  GtkWidget *fnumber_w = NULL;
  GtkWidget *focal_length_w = NULL;
  GtkWidget *iso_speed_w = NULL;
  GtkWidget *item_type;
  GtkWidget *item_type_data;
  GtkWidget *modified_w = NULL;
  GtkWidget *source;
  GtkWidget *source_data;
  GtkWidget *title;
  GQuark equipment;
  GQuark flash;
  PhotosBaseItem *item;
  PhotosSearchContextState *state;
  const gchar *author;
  const gchar *location;
  const gchar *name;
  const gchar *type_description;
  g_autofree gchar *date_created_str = NULL;
  g_autofree gchar *date_modified_str = NULL;
  gdouble exposure_time;
  gdouble fnumber;
  gdouble focal_length;
  gdouble iso_speed;
  gint64 ctime;
  gint64 height;
  gint64 mtime;
  gint64 width;

  G_OBJECT_CLASS (photos_properties_dialog_parent_class)->constructed (object);

  app = g_application_get_default ();
  state = photos_search_context_get_state (PHOTOS_SEARCH_CONTEXT (app));

  self->item_mngr = g_object_ref (state->item_mngr);

  {
    g_autoptr (GError) error = NULL;

    self->queue = photos_tracker_queue_dup_singleton (NULL, &error);
    if (G_UNLIKELY (error != NULL))
      g_warning ("Unable to create PhotosTrackerQueue: %s", error->message);
  }

  item = PHOTOS_BASE_ITEM (photos_base_manager_get_object_by_id (self->item_mngr, self->urn));

  mtime = photos_base_item_get_mtime (item);
  date_modified = g_date_time_new_from_unix_local (mtime);
  date_modified_str = g_date_time_format (date_modified, "%c");

  ctime = photos_base_item_get_date_created (item);
  if (ctime != -1)
    {
      g_autoptr (GDateTime) date_created = NULL;

      date_created = g_date_time_new_from_unix_local (ctime);
      date_created_str = g_date_time_format (date_created, "%c");
    }

  self->grid = gtk_grid_new ();
  gtk_widget_set_halign (self->grid, GTK_ALIGN_CENTER);
  gtk_widget_set_margin_start (self->grid, 24);
  gtk_widget_set_margin_end (self->grid, 24);
  gtk_widget_set_margin_bottom (self->grid, 12);
  gtk_widget_set_margin_top (self->grid, 12);
  gtk_orientable_set_orientation (GTK_ORIENTABLE (self->grid), GTK_ORIENTATION_VERTICAL);
  gtk_grid_set_column_homogeneous (GTK_GRID (self->grid), TRUE);
  gtk_grid_set_column_spacing (GTK_GRID (self->grid), 24);
  gtk_grid_set_row_homogeneous (GTK_GRID (self->grid), TRUE);
  gtk_grid_set_row_spacing (GTK_GRID (self->grid), 6);

  content_area = gtk_dialog_get_content_area (GTK_DIALOG (self));
  gtk_box_pack_start (GTK_BOX (content_area), self->grid, TRUE, TRUE, 2);

  /* Translators: this is the label next to the photo title in the
   * properties dialog
   */
  title = gtk_label_new (C_("Document Title", "Title"));
  gtk_widget_set_halign (title, GTK_ALIGN_END);
  context = gtk_widget_get_style_context (title);
  gtk_style_context_add_class (context, "dim-label");
  gtk_container_add (GTK_CONTAINER (self->grid), title);

  author = photos_base_item_get_author (item);
  if (author != NULL && author[0] != '\0')
    {
      /* Translators: this is the label next to the photo author in
       * the properties dialog
       */
      author_w = gtk_label_new (C_("Document Author", "Author"));
      gtk_widget_set_halign (author_w, GTK_ALIGN_END);
      context = gtk_widget_get_style_context (author_w);
      gtk_style_context_add_class (context, "dim-label");
      gtk_container_add (GTK_CONTAINER (self->grid), author_w);
    }

  source = gtk_label_new (_("Source"));
  gtk_widget_set_halign (source, GTK_ALIGN_END);
  context = gtk_widget_get_style_context (source);
  gtk_style_context_add_class (context, "dim-label");
  gtk_container_add (GTK_CONTAINER (self->grid), source);

  date_modified_w = gtk_label_new (_("Date Modified"));
  gtk_widget_set_halign (date_modified_w, GTK_ALIGN_END);
  context = gtk_widget_get_style_context (date_modified_w);
  gtk_style_context_add_class (context, "dim-label");
  gtk_container_add (GTK_CONTAINER (self->grid), date_modified_w);

  if (date_created_str != NULL)
    {
      date_created_w = gtk_label_new (_("Date Created"));
      gtk_widget_set_halign (date_created_w, GTK_ALIGN_END);
      context = gtk_widget_get_style_context (date_created_w);
      gtk_style_context_add_class (context, "dim-label");
      gtk_container_add (GTK_CONTAINER (self->grid), date_created_w);
    }

  /* Translators: this is the label next to the photo type in the
   * properties dialog
   */
  item_type = gtk_label_new (C_("Document Type", "Type"));
  gtk_widget_set_halign (item_type, GTK_ALIGN_END);
  context = gtk_widget_get_style_context (item_type);
  gtk_style_context_add_class (context, "dim-label");
  gtk_container_add (GTK_CONTAINER (self->grid), item_type);

  height = photos_base_item_get_height (item);
  width = photos_base_item_get_width (item);
  if (height > 0 && width > 0)
    {
      dimensions_w = gtk_label_new (_("Dimensions"));
      gtk_widget_set_halign (dimensions_w, GTK_ALIGN_END);
      context = gtk_widget_get_style_context (dimensions_w);
      gtk_style_context_add_class (context, "dim-label");
      gtk_container_add (GTK_CONTAINER (self->grid), dimensions_w);
    }

  location = photos_base_item_get_location (item);
  if (location != NULL && location[0] != '\0' && G_LIKELY (self->queue != NULL))
    {
      g_autoptr (PhotosQuery) query = NULL;

      self->location_w = gtk_label_new (_("Location"));
      gtk_widget_set_halign (self->location_w, GTK_ALIGN_END);
      context = gtk_widget_get_style_context (self->location_w);
      gtk_style_context_add_class (context, "dim-label");
      gtk_container_add (GTK_CONTAINER (self->grid), self->location_w);

      query = photos_query_builder_location_query (state, location);
      photos_tracker_queue_select (self->queue,
                                   query,
                                   NULL,
                                   photos_properties_dialog_location_query_executed,
                                   g_object_ref (self),
                                   g_object_unref);
    }

  equipment = photos_base_item_get_equipment (item);
  if (equipment != 0)
    {
      self->camera_w = gtk_label_new (_("Camera"));
      gtk_widget_set_halign (self->camera_w, GTK_ALIGN_END);
      context = gtk_widget_get_style_context (self->camera_w);
      gtk_style_context_add_class (context, "dim-label");
      gtk_container_add (GTK_CONTAINER (self->grid), self->camera_w);
    }

  exposure_time = photos_base_item_get_exposure_time (item);
  if (exposure_time > 0.0)
    {
      exposure_time_w = gtk_label_new (_("Exposure"));
      gtk_widget_set_halign (exposure_time_w, GTK_ALIGN_END);
      context = gtk_widget_get_style_context (exposure_time_w);
      gtk_style_context_add_class (context, "dim-label");
      gtk_container_add (GTK_CONTAINER (self->grid), exposure_time_w);
    }

  fnumber = photos_base_item_get_fnumber (item);
  if (fnumber > 0.0)
    {
      fnumber_w = gtk_label_new (_("Aperture"));
      gtk_widget_set_halign (fnumber_w, GTK_ALIGN_END);
      context = gtk_widget_get_style_context (fnumber_w);
      gtk_style_context_add_class (context, "dim-label");
      gtk_container_add (GTK_CONTAINER (self->grid), fnumber_w);
    }

  focal_length = photos_base_item_get_focal_length (item);
  if (focal_length > 0.0)
    {
      focal_length_w = gtk_label_new (_("Focal Length"));
      gtk_widget_set_halign (focal_length_w, GTK_ALIGN_END);
      context = gtk_widget_get_style_context (focal_length_w);
      gtk_style_context_add_class (context, "dim-label");
      gtk_container_add (GTK_CONTAINER (self->grid), focal_length_w);
    }

  iso_speed = photos_base_item_get_iso_speed (item);
  if (iso_speed > 0.0)
    {
      iso_speed_w = gtk_label_new (_("ISO Speed"));
      gtk_widget_set_halign (iso_speed_w, GTK_ALIGN_END);
      context = gtk_widget_get_style_context (iso_speed_w);
      gtk_style_context_add_class (context, "dim-label");
      gtk_container_add (GTK_CONTAINER (self->grid), iso_speed_w);
    }

  flash = photos_base_item_get_flash (item);
  if (flash != 0)
    {
      flash_w = gtk_label_new (_("Flash"));
      gtk_widget_set_halign (flash_w, GTK_ALIGN_END);
      context = gtk_widget_get_style_context (flash_w);
      gtk_style_context_add_class (context, "dim-label");
      gtk_container_add (GTK_CONTAINER (self->grid), flash_w);
    }

  if (!photos_base_item_is_collection (item))
    {
      modified_w = gtk_label_new (_("Modifications"));
      gtk_widget_set_halign (modified_w, GTK_ALIGN_END);
      gtk_widget_set_valign (modified_w, GTK_ALIGN_BASELINE);
      gtk_widget_set_vexpand (modified_w, TRUE);
      context = gtk_widget_get_style_context (modified_w);
      gtk_style_context_add_class (context, "dim-label");
      gtk_container_add (GTK_CONTAINER (self->grid), modified_w);
    }

  name = photos_base_item_get_name (item);

  if (PHOTOS_IS_LOCAL_ITEM (item))
    {
      self->title_entry = gtk_entry_new ();
      gtk_widget_set_halign (self->title_entry, GTK_ALIGN_START);
      gtk_widget_set_hexpand (self->title_entry, TRUE);
      gtk_entry_set_activates_default (GTK_ENTRY (self->title_entry), TRUE);
      gtk_entry_set_text (GTK_ENTRY (self->title_entry), name);
      gtk_entry_set_width_chars (GTK_ENTRY (self->title_entry), 40);
      gtk_editable_set_editable (GTK_EDITABLE (self->title_entry), TRUE);

      g_signal_connect (self->title_entry,
                        "changed",
                        G_CALLBACK (photos_properties_dialog_title_entry_changed),
                        self);
    }
  else
    {
      self->title_entry = gtk_label_new (name);
      gtk_widget_set_halign (self->title_entry, GTK_ALIGN_START);
      gtk_label_set_ellipsize (GTK_LABEL (self->title_entry), PANGO_ELLIPSIZE_END);
      gtk_label_set_max_width_chars (GTK_LABEL (self->title_entry), 40);
    }

  gtk_grid_attach_next_to (GTK_GRID (self->grid), self->title_entry, title, GTK_POS_RIGHT, 2, 1);

  if (author_w != NULL)
    {
      GtkWidget *author_data;

      author_data = gtk_label_new (author);
      gtk_widget_set_halign (author_data, GTK_ALIGN_START);
      gtk_grid_attach_next_to (GTK_GRID (self->grid), author_data, author_w, GTK_POS_RIGHT, 2, 1);
    }

  source_data = photos_base_item_get_source_widget (item);
  gtk_grid_attach_next_to (GTK_GRID (self->grid), source_data, source, GTK_POS_RIGHT, 2, 1);

  date_modified_data = gtk_label_new (date_modified_str);
  gtk_widget_set_halign (date_modified_data, GTK_ALIGN_START);
  gtk_grid_attach_next_to (GTK_GRID (self->grid), date_modified_data, date_modified_w, GTK_POS_RIGHT, 2, 1);

  if (date_created_w != NULL)
    {
      GtkWidget *date_created_data;

      date_created_data = gtk_label_new (date_created_str);
      gtk_widget_set_halign (date_created_data, GTK_ALIGN_START);
      gtk_grid_attach_next_to (GTK_GRID (self->grid), date_created_data, date_created_w, GTK_POS_RIGHT, 2, 1);
    }

  type_description = photos_base_item_get_type_description (item);
  item_type_data = gtk_label_new (type_description);
  gtk_widget_set_halign (item_type_data, GTK_ALIGN_START);
  gtk_grid_attach_next_to (GTK_GRID (self->grid), item_type_data, item_type, GTK_POS_RIGHT, 2, 1);

  if (dimensions_w != NULL)
    {
      GtkWidget *dims_data;
      g_autofree gchar *dims_str = NULL;

      dims_str = g_strdup_printf ("%" G_GINT64_FORMAT " × %" G_GINT64_FORMAT " pixels", width, height);
      dims_data = gtk_label_new (dims_str);
      gtk_widget_set_halign (dims_data, GTK_ALIGN_START);
      gtk_grid_attach_next_to (GTK_GRID (self->grid), dims_data, dimensions_w, GTK_POS_RIGHT, 2, 1);
    }

  if (self->camera_w != NULL)
    {
      photos_camera_cache_get_camera_async (self->camera_cache,
                                            equipment,
                                            self->cancellable,
                                            photos_properties_dialog_get_camera,
                                            self);
    }

  if (exposure_time_w != NULL)
    {
      GtkWidget *exposure_time_data;
      g_autofree gchar *exposure_time_str = NULL;

      exposure_time_str = g_strdup_printf ("%.3lf sec", exposure_time);
      exposure_time_data = gtk_label_new (exposure_time_str);
      gtk_widget_set_halign (exposure_time_data, GTK_ALIGN_START);
      gtk_grid_attach_next_to (GTK_GRID (self->grid), exposure_time_data, exposure_time_w, GTK_POS_RIGHT, 2, 1);
    }

  if (fnumber_w != NULL)
    {
      GtkWidget *fnumber_data;
      g_autofree gchar *fnumber_str = NULL;

      fnumber_str = g_strdup_printf ("f/%.1lf", fnumber);
      fnumber_data = gtk_label_new (fnumber_str);
      gtk_widget_set_halign (fnumber_data, GTK_ALIGN_START);
      gtk_grid_attach_next_to (GTK_GRID (self->grid), fnumber_data, fnumber_w, GTK_POS_RIGHT, 2, 1);
    }

  if (focal_length_w != NULL)
    {
      GtkWidget *focal_length_data;
      g_autofree gchar *focal_length_str = NULL;

      focal_length_str = g_strdup_printf ("%.0lf mm", focal_length);
      focal_length_data = gtk_label_new (focal_length_str);
      gtk_widget_set_halign (focal_length_data, GTK_ALIGN_START);
      gtk_grid_attach_next_to (GTK_GRID (self->grid), focal_length_data, focal_length_w, GTK_POS_RIGHT, 2, 1);
    }

  if (iso_speed_w != NULL)
    {
      GtkWidget *iso_speed_data;
      g_autofree gchar *iso_speed_str = NULL;

      iso_speed_str = g_strdup_printf ("%.0lf", iso_speed);
      iso_speed_data = gtk_label_new (iso_speed_str);
      gtk_widget_set_halign (iso_speed_data, GTK_ALIGN_START);
      gtk_grid_attach_next_to (GTK_GRID (self->grid), iso_speed_data, iso_speed_w, GTK_POS_RIGHT, 2, 1);
    }

  if (flash_w != NULL)
    {
      GtkWidget *flash_data;
      g_autofree gchar *flash_str = NULL;

      if (flash == PHOTOS_FLASH_OFF)
        flash_str = g_strdup (_("Off, did not fire"));
      else if (flash == PHOTOS_FLASH_ON)
        flash_str = g_strdup (_("On, fired"));
      else
        {
          const gchar *str;

          str = g_quark_to_string (flash);
          g_warning ("Unknown value for nmm:flash: %s", str);
        }

      flash_data = gtk_label_new (flash_str);
      gtk_widget_set_halign (flash_data, GTK_ALIGN_START);
      gtk_grid_attach_next_to (GTK_GRID (self->grid), flash_data, flash_w, GTK_POS_RIGHT, 2, 1);
    }

  if (modified_w != NULL)
    {
      GtkWidget *modified_grid;

      photos_base_item_pipeline_is_edited_async (item,
                                                 self->cancellable,
                                                 photos_properties_dialog_pipeline_is_edited,
                                                 self);

      modified_grid = gtk_grid_new ();
      gtk_widget_set_hexpand (modified_grid, TRUE);
      gtk_orientable_set_orientation (GTK_ORIENTABLE (modified_grid), GTK_ORIENTATION_HORIZONTAL);

      self->modified_data = gtk_label_new (NULL);
      gtk_widget_set_halign (self->modified_data, GTK_ALIGN_START);
      gtk_widget_set_hexpand (self->modified_data, TRUE);
      gtk_widget_set_no_show_all (self->modified_data, TRUE);
      gtk_widget_set_valign (self->modified_data, GTK_ALIGN_BASELINE);
      gtk_widget_set_vexpand (self->modified_data, TRUE);
      context = gtk_widget_get_style_context (self->modified_data);
      gtk_style_context_add_class (context, "photos-fade-out");
      gtk_container_add (GTK_CONTAINER (modified_grid), self->modified_data);

      self->revert_button = gtk_button_new_with_label (_("Discard all Edits"));
      gtk_widget_set_halign (self->revert_button, GTK_ALIGN_END);
      gtk_widget_set_hexpand (self->revert_button, TRUE);
      gtk_widget_set_no_show_all (self->revert_button, TRUE);
      context = gtk_widget_get_style_context (self->revert_button);
      gtk_style_context_add_class (context, "destructive-action");
      gtk_style_context_add_class (context, "photos-fade-out");
      gtk_container_add (GTK_CONTAINER (modified_grid), self->revert_button);

      g_signal_connect_swapped (self->revert_button,
                                "clicked",
                                G_CALLBACK (photos_properties_dialog_revert_clicked),
                                self);

      gtk_grid_attach_next_to (GTK_GRID (self->grid), modified_grid, modified_w, GTK_POS_RIGHT, 2, 1);
    }

}


static void
photos_properties_dialog_dispose (GObject *object)
{
  PhotosPropertiesDialog *self = PHOTOS_PROPERTIES_DIALOG (object);

  /* Make sure we save the new entered name before destroying the dialog */
  if (self->title_entry_timeout != 0)
    photos_properties_dialog_name_update (self);

  photos_properties_dialog_remove_timeout (self);

  if (self->cancellable != NULL)
    {
      g_cancellable_cancel (self->cancellable);
      g_clear_object (&self->cancellable);
    }

  g_clear_object (&self->item_mngr);
  g_clear_object (&self->camera_cache);
  g_clear_object (&self->queue);

  G_OBJECT_CLASS (photos_properties_dialog_parent_class)->dispose (object);
}


static void
photos_properties_dialog_finalize (GObject *object)
{
  PhotosPropertiesDialog *self = PHOTOS_PROPERTIES_DIALOG (object);

  g_free (self->urn);

  G_OBJECT_CLASS (photos_properties_dialog_parent_class)->finalize (object);
}


static void
photos_properties_dialog_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
  PhotosPropertiesDialog *self = PHOTOS_PROPERTIES_DIALOG (object);

  switch (prop_id)
    {
    case PROP_URN:
      self->urn = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
photos_properties_dialog_init (PhotosPropertiesDialog *self)
{
  self->cancellable = g_cancellable_new ();
  self->camera_cache = photos_camera_cache_dup_singleton ();
}


static void
photos_properties_dialog_class_init (PhotosPropertiesDialogClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->constructed = photos_properties_dialog_constructed;
  object_class->dispose = photos_properties_dialog_dispose;
  object_class->finalize = photos_properties_dialog_finalize;
  object_class->set_property = photos_properties_dialog_set_property;

  g_object_class_install_property (object_class,
                                   PROP_URN,
                                   g_param_spec_string ("urn",
                                                        "Uniform Resource Name",
                                                        "An unique ID associated with this item",
                                                        NULL,
                                                        G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE));

  signals[DISCARD_ALL_EDITS] = g_signal_new ("discard-all-edits",
                                             G_TYPE_FROM_CLASS (class),
                                             G_SIGNAL_RUN_LAST,
                                             0,
                                             NULL, /* accumulator */
                                             NULL, /* accu_data */
                                             g_cclosure_marshal_VOID__VOID,
                                             G_TYPE_NONE,
                                             0);
}


GtkWidget *
photos_properties_dialog_new (GtkWindow *parent, const gchar *urn)
{
  g_return_val_if_fail (GTK_IS_WINDOW (parent), NULL);

  return g_object_new (PHOTOS_TYPE_PROPERTIES_DIALOG,
                       "default-width", 400,
                       "destroy-with-parent", TRUE,
                       "hexpand", TRUE,
                       "modal", TRUE,
                       "resizable", FALSE,
                       "title", _("Properties"),
                       "transient-for", parent,
                       "urn", urn,
                       "use-header-bar", TRUE,
                       NULL);
}
