/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2012 – 2021 Red Hat, Inc.
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

/* Based on code from:
 *   + Documents
 */


#include "config.h"

#include <gio/gio.h>
#include <glib/gi18n.h>
#include <handy.h>

#include "photos-empty-results-box.h"
#include "photos-enums.h"
#include "photos-utils.h"


struct _PhotosEmptyResultsBox
{
  GtkBin parent_instance;
  GtkWidget *status_page;
  PhotosWindowMode mode;
};

enum
{
  PROP_0,
  PROP_MODE
};


G_DEFINE_TYPE (PhotosEmptyResultsBox, photos_empty_results_box, GTK_TYPE_BIN);


static gboolean
photos_empty_results_box_activate_link (PhotosEmptyResultsBox *self, const gchar *uri)
{
  gboolean ret_val = FALSE;

  if (g_strcmp0 (uri, "system-settings") != 0)
    goto out;

  photos_utils_launch_online_accounts (NULL);
  ret_val = TRUE;

 out:
  return ret_val;
}


static void
photos_empty_results_box_add_image (PhotosEmptyResultsBox *self)
{
  const gchar *icon_name = NULL;

  switch (self->mode)
    {
    case PHOTOS_WINDOW_MODE_COLLECTIONS:
      icon_name = "emblem-photos-symbolic";
      break;

    case PHOTOS_WINDOW_MODE_FAVORITES:
      icon_name = "starred-symbolic";
      break;

    /* TODO: Don't show a collection if there are no screenshots in
     * the relevant locations.
     */
    case PHOTOS_WINDOW_MODE_COLLECTION_VIEW:
    case PHOTOS_WINDOW_MODE_IMPORT:
    case PHOTOS_WINDOW_MODE_OVERVIEW:
      icon_name = "camera-photo-symbolic";
      break;

    case PHOTOS_WINDOW_MODE_SEARCH:
      icon_name = "system-search-symbolic";
      break;

    case PHOTOS_WINDOW_MODE_NONE:
    case PHOTOS_WINDOW_MODE_EDIT:
    case PHOTOS_WINDOW_MODE_PREVIEW:
    default:
      g_assert_not_reached ();
      break;
    }

  hdy_status_page_set_icon_name (HDY_STATUS_PAGE (self->status_page), icon_name);
}


static void
photos_empty_results_box_add_primary_label (PhotosEmptyResultsBox *self)
{
  const gchar *text = NULL;

  switch (self->mode)
    {
    case PHOTOS_WINDOW_MODE_COLLECTIONS:
      text = _("No Albums Found");
      break;

    case PHOTOS_WINDOW_MODE_FAVORITES:
      text = _("Starred Photos Will Appear Here");
      break;

    /* TODO: Don't show a collection if there are no screenshots in
     * the relevant locations.
     */
    case PHOTOS_WINDOW_MODE_COLLECTION_VIEW:
    case PHOTOS_WINDOW_MODE_IMPORT:
    case PHOTOS_WINDOW_MODE_OVERVIEW:
    case PHOTOS_WINDOW_MODE_SEARCH:
      text = _("No Photos Found");
      break;

    case PHOTOS_WINDOW_MODE_NONE:
    case PHOTOS_WINDOW_MODE_EDIT:
    case PHOTOS_WINDOW_MODE_PREVIEW:
    default:
      g_assert_not_reached ();
      break;
    }

  hdy_status_page_set_title (HDY_STATUS_PAGE (self->status_page), text);
}


static void
photos_empty_results_box_add_secondary_label (PhotosEmptyResultsBox *self)
{
  gboolean handle_activate_link = FALSE;
  gboolean use_markup = FALSE;
  g_autofree gchar *label = NULL;

  switch (self->mode)
    {
    case PHOTOS_WINDOW_MODE_COLLECTIONS:
      label = g_strdup (_("You can create albums from the Photos view"));
      break;

    /* TODO: Don't show a collection if there are no screenshots in
     * the relevant locations.
     */
    case PHOTOS_WINDOW_MODE_COLLECTION_VIEW:
    case PHOTOS_WINDOW_MODE_FAVORITES:
    case PHOTOS_WINDOW_MODE_IMPORT:
      break;

    case PHOTOS_WINDOW_MODE_OVERVIEW:
      {
        const gchar *pictures_path;
        g_autofree gchar *pictures_path_href = NULL;
        g_autofree gchar *system_settings_href = NULL;

        /* Translators: this should be translated in the context of
         * the "Photos from your Online Accounts and Pictures folder
         * will appear here." sentence below.
         */
        system_settings_href = g_strdup_printf ("<a href=\"system-settings\">%s</a>", _("Online Accounts"));

        pictures_path = g_get_user_special_dir (G_USER_DIRECTORY_PICTURES);
        /* Translators: this should be translated in the context of
         * the "Photos from your Online Accounts and Pictures folder
         * will appear here." sentence below.
         */
        pictures_path_href = g_strdup_printf ("<a href=\"file://%s\">%s</a>", pictures_path, _("Pictures folder"));

        /* Translators: the first %s here is "Online Accounts" and the
         * second %s is "Pictures folder", which are in separate
         * strings due to markup, and should be translated only in the
         * context of this sentence.
         */
        label = g_strdup_printf (_("Photos from your %s and %s will appear here."),
                                 system_settings_href,
                                 pictures_path_href);

        handle_activate_link = TRUE;
        use_markup = TRUE;
        break;
      }

    case PHOTOS_WINDOW_MODE_SEARCH:
      label = g_strdup (_("Try a different search."));
      break;

    case PHOTOS_WINDOW_MODE_NONE:
    case PHOTOS_WINDOW_MODE_EDIT:
    case PHOTOS_WINDOW_MODE_PREVIEW:
    default:
      g_assert_not_reached ();
      break;
    }

  if (label != NULL)
    {
      GtkWidget *secondary_label;
      GtkStyleContext *context;

      secondary_label = gtk_label_new (label);
      gtk_label_set_use_markup (GTK_LABEL (secondary_label), use_markup);
      gtk_label_set_justify (GTK_LABEL (secondary_label), GTK_JUSTIFY_CENTER);
      gtk_label_set_line_wrap (GTK_LABEL (secondary_label), TRUE);
      gtk_label_set_line_wrap_mode (GTK_LABEL (secondary_label), PANGO_WRAP_WORD_CHAR);
      context = gtk_widget_get_style_context (secondary_label);
      gtk_style_context_add_class (context, "body");
      gtk_style_context_add_class (context, "description");
      gtk_container_add (GTK_CONTAINER (self->status_page), secondary_label);
      if (handle_activate_link)
        {
          g_signal_connect_swapped (secondary_label,
                                    "activate-link",
                                    G_CALLBACK (photos_empty_results_box_activate_link),
                                    self);
        }
    }
}


static void
photos_empty_results_box_constructed (GObject *object)
{
  PhotosEmptyResultsBox *self = PHOTOS_EMPTY_RESULTS_BOX (object);

  G_OBJECT_CLASS (photos_empty_results_box_parent_class)->constructed (object);

  gtk_widget_set_hexpand (GTK_WIDGET (self), TRUE);
  gtk_widget_set_vexpand (GTK_WIDGET (self), TRUE);

  self->status_page = hdy_status_page_new ();
  gtk_container_add (GTK_CONTAINER (self), self->status_page);

  photos_empty_results_box_add_image (self);
  photos_empty_results_box_add_primary_label (self);
  photos_empty_results_box_add_secondary_label (self);

  gtk_widget_show_all (GTK_WIDGET (self));
}


static void
photos_empty_results_box_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
  PhotosEmptyResultsBox *self = PHOTOS_EMPTY_RESULTS_BOX (object);

  switch (prop_id)
    {
    case PROP_MODE:
      self->mode = (PhotosWindowMode) g_value_get_enum (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
photos_empty_results_box_init (PhotosEmptyResultsBox *self)
{
}


static void
photos_empty_results_box_class_init (PhotosEmptyResultsBoxClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->constructed = photos_empty_results_box_constructed;
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
