/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2012, 2013, 2014 Red Hat, Inc.
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

#include <gio/gio.h>
#include <glib/gi18n.h>

#include "photos-empty-results-box.h"
#include "photos-enums.h"
#include "photos-icons.h"
#include "photos-search-context.h"
#include "photos-source-manager.h"


struct _PhotosEmptyResultsBoxPrivate
{
  GtkWidget *labels_grid;
  PhotosBaseManager *src_mngr;
  PhotosWindowMode mode;
};

enum
{
  PROP_0,
  PROP_MODE
};


G_DEFINE_TYPE_WITH_PRIVATE (PhotosEmptyResultsBox, photos_empty_results_box, GTK_TYPE_GRID);


static gboolean
photos_empty_results_box_activate_link (PhotosEmptyResultsBox *self, const gchar *uri)
{
  GAppInfo *app = NULL;
  GError *error;
  GdkAppLaunchContext *ctx = NULL;
  GdkDisplay *display;
  GdkScreen *screen;
  gboolean ret_val = FALSE;

  if (g_strcmp0 (uri, "system-settings") != 0)
    goto out;

  error = NULL;
  app = g_app_info_create_from_commandline ("gnome-control-center online-accounts",
                                            NULL,
                                            G_APP_INFO_CREATE_NONE,
                                            &error);
  if (error != NULL)
    {
      g_warning ("Unable to launch gnome-control-center: %s", error->message);
      g_error_free (error);
      goto out;
    }

  screen = gtk_widget_get_screen (GTK_WIDGET (self));
  if (screen != NULL)
    display = gdk_screen_get_display (screen);
  else
    display = gdk_display_get_default ();

  ctx = gdk_display_get_app_launch_context (display);
  if (screen != NULL)
    gdk_app_launch_context_set_screen (ctx, screen);

  error = NULL;
  g_app_info_launch (app, NULL, G_APP_LAUNCH_CONTEXT (ctx), &error);
  if (error != NULL)
    {
      g_warning ("Unable to launch gnome-control-center: %s", error->message);
      g_error_free (error);
      goto out;
    }

  ret_val = TRUE;

 out:
  g_clear_object (&ctx);
  g_clear_object (&app);
  return ret_val;
}


static void
photos_empty_results_box_add_collections_label (PhotosEmptyResultsBox *self)
{
  GtkWidget *details;

  details = gtk_label_new (_("Name your first album"));
  gtk_widget_set_halign (details, GTK_ALIGN_START);
  gtk_misc_set_alignment (GTK_MISC (details), 0.0, 0.5);
  gtk_label_set_line_wrap (GTK_LABEL (details), TRUE);
  gtk_label_set_max_width_chars (GTK_LABEL (details), 24);
  gtk_label_set_use_markup (GTK_LABEL (details), TRUE);
  gtk_container_add (GTK_CONTAINER (self->priv->labels_grid), details);
}


static void
photos_empty_results_box_add_system_settings_label (PhotosEmptyResultsBox *self)
{
  GtkWidget *details;
  gchar *details_str;
  gchar *system_settings_href;

  /* Translators: this should be translated in the context of the "You
   * can add your online accounts in Settings" sentence below
   */
  system_settings_href = g_strconcat ("<a href=\"system-settings\">", _("Settings"), "</a>", NULL);

  /* Translators: %s here is "Settings", which is in a separate string
   * due to markup, and should be translated only in the context of
   * this sentence.
   */
  details_str = g_strdup_printf (_("You can add your online accounts in %s"), system_settings_href);

  details = gtk_label_new (details_str);
  gtk_widget_set_halign (details, GTK_ALIGN_START);
  gtk_misc_set_alignment (GTK_MISC (details), 0.0, 0.5);
  gtk_label_set_line_wrap (GTK_LABEL (details), TRUE);
  gtk_label_set_max_width_chars (GTK_LABEL (details), 24);
  gtk_label_set_use_markup (GTK_LABEL (details), TRUE);
  gtk_container_add (GTK_CONTAINER (self->priv->labels_grid), details);

  g_signal_connect_swapped (details, "activate-link", G_CALLBACK (photos_empty_results_box_activate_link), self);

  g_free (details_str);
  g_free (system_settings_href);
}


static void
photos_empty_results_box_constructed (GObject *object)
{
  PhotosEmptyResultsBox *self = PHOTOS_EMPTY_RESULTS_BOX (object);
  PhotosEmptyResultsBoxPrivate *priv = self->priv;
  GtkStyleContext *context;
  GtkWidget *image;
  GtkWidget *title_label;
  gchar *label;

  G_OBJECT_CLASS (photos_empty_results_box_parent_class)->constructed (object);

  gtk_widget_set_halign (GTK_WIDGET (self), GTK_ALIGN_CENTER);
  gtk_widget_set_hexpand (GTK_WIDGET (self), TRUE);
  gtk_widget_set_valign (GTK_WIDGET (self), GTK_ALIGN_CENTER);
  gtk_widget_set_vexpand (GTK_WIDGET (self), TRUE);
  gtk_orientable_set_orientation (GTK_ORIENTABLE (self), GTK_ORIENTATION_HORIZONTAL);
  gtk_grid_set_column_spacing (GTK_GRID (self), 12);
  context = gtk_widget_get_style_context (GTK_WIDGET (self));
  gtk_style_context_add_class (context, "dim-label");

  switch (priv->mode)
    {
    case PHOTOS_WINDOW_MODE_COLLECTIONS:
      image = gtk_image_new_from_icon_name (PHOTOS_ICON_PHOTOS_SYMBOLIC, GTK_ICON_SIZE_INVALID);
      label = g_strconcat ("<b><span size=\"large\">", _("No Albums Found"), "</span></b>", NULL);
      break;

    case PHOTOS_WINDOW_MODE_FAVORITES:
      image = gtk_image_new_from_icon_name (PHOTOS_ICON_FAVORITE_SYMBOLIC, GTK_ICON_SIZE_INVALID);
      label = g_strconcat ("<b><span size=\"large\">", _("Starred photos will appear here"), "</span></b>", NULL);
      break;

    case PHOTOS_WINDOW_MODE_OVERVIEW:
    case PHOTOS_WINDOW_MODE_SEARCH:
      image = gtk_image_new_from_icon_name (PHOTOS_ICON_PHOTOS_SYMBOLIC, GTK_ICON_SIZE_INVALID);
      label = g_strconcat ("<b><span size=\"large\">", _("No Photos Found"), "</span></b>", NULL);
      break;

    default:
      g_assert_not_reached ();
      break;
    }

  gtk_image_set_pixel_size (GTK_IMAGE (image), 64);
  gtk_container_add (GTK_CONTAINER (self), image);

  priv->labels_grid = gtk_grid_new ();
  gtk_orientable_set_orientation (GTK_ORIENTABLE (priv->labels_grid), GTK_ORIENTATION_VERTICAL);
  gtk_grid_set_row_spacing (GTK_GRID (priv->labels_grid), 12);
  gtk_container_add (GTK_CONTAINER (self), priv->labels_grid);

  title_label = gtk_label_new (label);
  gtk_widget_set_halign (title_label, GTK_ALIGN_START);
  gtk_widget_set_vexpand (title_label, TRUE);
  gtk_label_set_use_markup (GTK_LABEL (title_label), TRUE);
  gtk_container_add (GTK_CONTAINER (priv->labels_grid), title_label);
  g_free (label);

  switch (priv->mode)
    {
    case PHOTOS_WINDOW_MODE_COLLECTIONS:
      gtk_widget_set_valign (title_label, GTK_ALIGN_START);
      photos_empty_results_box_add_collections_label (self);
      break;

    case PHOTOS_WINDOW_MODE_FAVORITES:
    case PHOTOS_WINDOW_MODE_SEARCH:
      gtk_widget_set_valign (title_label, GTK_ALIGN_CENTER);
      break;

    case PHOTOS_WINDOW_MODE_OVERVIEW:
      if (photos_source_manager_has_online_sources (PHOTOS_SOURCE_MANAGER (self->priv->src_mngr)))
        gtk_widget_set_valign (title_label, GTK_ALIGN_CENTER);
      else
        {
          gtk_widget_set_valign (title_label, GTK_ALIGN_START);
          photos_empty_results_box_add_system_settings_label (self);
        }
      break;

    default:
      g_assert_not_reached ();
      break;
    }

  gtk_widget_show_all (GTK_WIDGET (self));
}


static void
photos_empty_results_box_dispose (GObject *object)
{
  PhotosEmptyResultsBox *self = PHOTOS_EMPTY_RESULTS_BOX (object);

  g_clear_object (&self->priv->src_mngr);

  G_OBJECT_CLASS (photos_empty_results_box_parent_class)->dispose (object);
}


static void
photos_empty_results_box_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
  PhotosEmptyResultsBox *self = PHOTOS_EMPTY_RESULTS_BOX (object);

  switch (prop_id)
    {
    case PROP_MODE:
      self->priv->mode = (PhotosWindowMode) g_value_get_enum (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
photos_empty_results_box_init (PhotosEmptyResultsBox *self)
{
  PhotosEmptyResultsBoxPrivate *priv;
  GApplication *app;
  PhotosSearchContextState *state;

  self->priv = photos_empty_results_box_get_instance_private (self);
  priv = self->priv;

  app = g_application_get_default ();
  state = photos_search_context_get_state (PHOTOS_SEARCH_CONTEXT (app));

  priv->src_mngr = g_object_ref (state->src_mngr);
}


static void
photos_empty_results_box_class_init (PhotosEmptyResultsBoxClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->constructed = photos_empty_results_box_constructed;
  object_class->dispose = photos_empty_results_box_dispose;
  object_class->set_property = photos_empty_results_box_set_property;

  g_object_class_install_property (object_class,
                                   PROP_MODE,
                                   g_param_spec_enum ("mode",
                                                      "PhotosWindowMode enum",
                                                      "The mode for which no results were found",
                                                      PHOTOS_TYPE_WINDOW_MODE,
                                                      PHOTOS_WINDOW_MODE_NONE,
                                                      G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE));
}


GtkWidget *
photos_empty_results_box_new (PhotosWindowMode mode)
{
  return g_object_new (PHOTOS_TYPE_EMPTY_RESULTS_BOX, "mode", mode, NULL);
}
