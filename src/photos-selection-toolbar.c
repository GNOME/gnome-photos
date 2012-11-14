/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2012 Red Hat, Inc.
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

#include <clutter-gtk/clutter-gtk.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "photos-base-item.h"
#include "photos-item-manager.h"
#include "photos-organize-collection-dialog.h"
#include "photos-properties-dialog.h"
#include "photos-selection-controller.h"
#include "photos-selection-toolbar.h"
#include "photos-utils.h"


struct _PhotosSelectionToolbarPrivate
{
  ClutterActor *actor;
  ClutterActor *parent_actor;
  ClutterConstraint *width_constraint;
  GHashTable *item_listeners;
  GtkToolItem *left_group;
  GtkToolItem *right_group;
  GtkToolItem *separator;
  GtkWidget *left_box;
  GtkWidget *right_box;
  GtkWidget *toolbar_collection;
  GtkWidget *toolbar_favorite;
  GtkWidget *toolbar_open;
  GtkWidget *toolbar_print;
  GtkWidget *toolbar_properties;
  GtkWidget *toolbar_trash;
  GtkWidget *widget;
  PhotosBaseManager *item_mngr;
  PhotosSelectionController *sel_cntrlr;
  gboolean inside_refresh;
};

enum
{
  PROP_0,
  PROP_PARENT_ACTOR
};


G_DEFINE_TYPE (PhotosSelectionToolbar, photos_selection_toolbar, G_TYPE_OBJECT);


static void
photos_selection_toolbar_fade_in (PhotosSelectionToolbar *self)
{
  PhotosSelectionToolbarPrivate *priv = self->priv;
  guint8 opacity;

  opacity = clutter_actor_get_opacity (priv->actor);
  if (opacity != 0)
    return;

  clutter_actor_show (priv->actor);
  clutter_actor_animate (priv->actor, CLUTTER_EASE_OUT_QUAD, 300, "opacity", 255, NULL);
}


static void
photos_selection_toolbar_fade_out (PhotosSelectionToolbar *self)
{
  ClutterAnimation *animation;
  PhotosSelectionToolbarPrivate *priv = self->priv;

  animation = clutter_actor_animate (priv->actor, CLUTTER_EASE_OUT_QUAD, 300, "opacity", 0, NULL);
  g_signal_connect_swapped (animation, "completed", G_CALLBACK (clutter_actor_hide), priv->actor);
}


static void
photos_selection_toolbar_dialog_response (GtkDialog *dialog, gint response_id, gpointer user_data)
{
  PhotosSelectionToolbar *self = PHOTOS_SELECTION_TOOLBAR (user_data);

  if (response_id != GTK_RESPONSE_OK)
    return;

  gtk_widget_destroy (GTK_WIDGET (dialog));
  photos_selection_toolbar_fade_in (self);
}


static void
photos_selection_toolbar_collection_clicked (GtkButton *button, gpointer user_data)
{
  PhotosSelectionToolbar *self = PHOTOS_SELECTION_TOOLBAR (user_data);
  PhotosSelectionToolbarPrivate *priv = self->priv;
  GtkWidget *dialog;
  GtkWidget *toplevel;

  toplevel = gtk_widget_get_toplevel (priv->widget);
  if (!gtk_widget_is_toplevel (toplevel))
    return;

  dialog = photos_organize_collection_dialog_new (GTK_WINDOW (toplevel));
  photos_selection_toolbar_fade_out (self);
  g_signal_connect (dialog, "response", G_CALLBACK (photos_selection_toolbar_dialog_response), self);
}


static gboolean
photos_selection_toolbar_disconnect_listeners_foreach (gpointer key, gpointer value, gpointer user_data)
{
  g_signal_handler_disconnect (value, GPOINTER_TO_UINT (key));
  return TRUE;
}


static void
photos_selection_toolbar_favorite_clicked (GtkButton *button, gpointer user_data)
{
  PhotosSelectionToolbar *self = PHOTOS_SELECTION_TOOLBAR (user_data);
  PhotosSelectionToolbarPrivate *priv = self->priv;
  GList *selection;
  GList *l;

  if (priv->inside_refresh)
    return;

  selection = photos_selection_controller_get_selection (priv->sel_cntrlr);
  for (l = selection; l != NULL; l = l->next)
    {
      const gchar *urn = (gchar *) l->data;
      PhotosBaseItem *item;
      gboolean favorite;

      item = PHOTOS_BASE_ITEM (photos_base_manager_get_object_by_id (priv->item_mngr, urn));
      favorite = photos_base_item_is_favorite (item);
      photos_base_item_set_favorite (item, !favorite);
    }
}


static void
photos_selection_toolbar_notify_width (GObject *object, GParamSpec *pspec, gpointer user_data)
{
  PhotosSelectionToolbar *self = PHOTOS_SELECTION_TOOLBAR (user_data);
  PhotosSelectionToolbarPrivate *priv = self->priv;
  gfloat offset = 300.0;
  gfloat width;

  width = clutter_actor_get_width (priv->parent_actor);
  if (width > 1000)
    offset += (width - 1000);
  else if (width < 600)
    offset -= (600 - width);

  clutter_bind_constraint_set_offset (CLUTTER_BIND_CONSTRAINT (priv->width_constraint), -1 * offset);
}


static void
photos_selection_toolbar_open_clicked (GtkButton *button, gpointer user_data)
{
  PhotosSelectionToolbar *self = PHOTOS_SELECTION_TOOLBAR (user_data);
  PhotosSelectionToolbarPrivate *priv = self->priv;
  GList *selection;
  GList *l;

  selection = photos_selection_controller_get_selection (priv->sel_cntrlr);
  for (l = selection; l != NULL; l = l->next)
    {
      const gchar *urn = (gchar *) l->data;
      GdkScreen *screen;
      PhotosBaseItem *item;
      guint32 time;

      item = PHOTOS_BASE_ITEM (photos_base_manager_get_object_by_id (priv->item_mngr, urn));
      screen = gtk_widget_get_screen (GTK_WIDGET (button));
      time = gtk_get_current_event_time ();
      photos_base_item_open (item, screen, time);
    }
}


static void
photos_selection_toolbar_print_clicked (GtkButton *button, gpointer user_data)
{
  PhotosSelectionToolbar *self = PHOTOS_SELECTION_TOOLBAR (user_data);
  PhotosSelectionToolbarPrivate *priv = self->priv;
  GList *selection;
  GtkWidget *toplevel;
  PhotosBaseItem *item;
  const gchar *urn;

  selection = photos_selection_controller_get_selection (priv->sel_cntrlr);
  if (g_list_length (selection) != 1)
    return;

  urn = (gchar *) selection->data;
  item = PHOTOS_BASE_ITEM (photos_base_manager_get_object_by_id (priv->item_mngr, urn));
  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (button));
  photos_base_item_print (item, toplevel);
}


static void
photos_selection_toolbar_properties_response (GtkDialog *dialog, gint response_id, gpointer user_data)
{
  PhotosSelectionToolbar *self = PHOTOS_SELECTION_TOOLBAR (user_data);

  gtk_widget_destroy (GTK_WIDGET (dialog));
  photos_selection_toolbar_fade_in (self);
}


static void
photos_selection_toolbar_properties_clicked (GtkButton *button, gpointer user_data)
{
  PhotosSelectionToolbar *self = PHOTOS_SELECTION_TOOLBAR (user_data);
  PhotosSelectionToolbarPrivate *priv = self->priv;
  GList *selection;
  GtkWidget *dialog;
  GtkWidget *toplevel;
  const gchar *urn;

  selection = photos_selection_controller_get_selection (priv->sel_cntrlr);
  urn = (gchar *) selection->data;
  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (button));
  dialog = photos_properties_dialog_new (GTK_WINDOW (toplevel), urn);
  photos_selection_toolbar_fade_out (self);

  g_signal_connect (dialog, "response", G_CALLBACK (photos_selection_toolbar_properties_response), self);
}


static void
photos_selection_toolbar_set_item_visibility (PhotosSelectionToolbar *self)
{
  PhotosSelectionToolbarPrivate *priv = self->priv;
  GList *apps = NULL;
  GList *l;
  GList *selection;
  gboolean show_favorite = TRUE;
  gboolean show_open = TRUE;
  gboolean show_print = TRUE;
  gboolean show_properties = TRUE;
  gboolean show_trash = TRUE;
  gchar *open_label;
  guint fav_count = 0;
  guint apps_length;
  guint sel_length;

  priv->inside_refresh = TRUE;

  selection = photos_selection_controller_get_selection (priv->sel_cntrlr);
  for (l = selection; l != NULL; l = g_list_next (l))
    {
      PhotosBaseItem *item;
      const gchar *default_app_name;
      const gchar *urn = (gchar *) l->data;

      item = PHOTOS_BASE_ITEM (photos_base_manager_get_object_by_id (priv->item_mngr, urn));

      if (photos_base_item_is_favorite (item))
        fav_count++;

      default_app_name = photos_base_item_get_default_app_name (item);
      if (default_app_name != NULL && g_list_find (apps, default_app_name) == NULL)
        apps = g_list_prepend (apps, (gpointer) g_strdup (default_app_name));

      show_trash = show_trash && photos_base_item_can_trash (item);
      show_print = show_print && !photos_base_item_is_collection (item);
    }

  sel_length = g_list_length (selection);
  show_favorite = show_favorite && ((fav_count == 0) || (fav_count == sel_length));

  apps_length = g_list_length (apps);
  show_open = (apps_length > 0);

  if (sel_length > 1)
    {
      show_print = FALSE;
      show_properties = FALSE;
    }

  if (apps_length == 1)
    /* Translators: this is the Open action in a context menu */
    open_label = g_strdup_printf (_("Open with %s"), (gchar *) apps->data);
  else
    /* Translators: this is the Open action in a context menu */
    open_label = g_strdup (_("Open"));

  gtk_widget_set_tooltip_text (priv->toolbar_open, open_label);
  g_free (open_label);
  g_list_free_full (apps, g_free);

  if (show_favorite)
    {
      GtkStyleContext *context;
      gchar *favorite_label;

      context = gtk_widget_get_style_context (priv->toolbar_favorite);

      if (fav_count == sel_length)
        {
          favorite_label = g_strdup (_("Remove from favorites"));
          gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->toolbar_favorite), TRUE);
          gtk_style_context_add_class (context, "documents-favorite");
        }
      else
        {
          favorite_label = g_strdup (_("Add to favorites"));
          gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->toolbar_favorite), FALSE);
          gtk_style_context_remove_class (context, "documents-favorite");
        }

      gtk_widget_reset_style (priv->toolbar_favorite);
      gtk_widget_set_tooltip_text (priv->toolbar_favorite, favorite_label);
      g_free (favorite_label);
    }

  gtk_widget_set_visible (priv->toolbar_print, show_print);
  gtk_widget_set_visible (priv->toolbar_properties, show_properties);
  gtk_widget_set_visible (priv->toolbar_trash, show_trash);
  gtk_widget_set_visible (priv->toolbar_open, show_open);
  gtk_widget_set_visible (priv->toolbar_favorite, show_favorite);

  priv->inside_refresh = FALSE;
}


static void
photos_selection_toolbar_set_item_listeners (PhotosSelectionToolbar *self, GList *selection)
{
  PhotosSelectionToolbarPrivate *priv = self->priv;
  GList *l;

  g_hash_table_foreach_remove (priv->item_listeners, photos_selection_toolbar_disconnect_listeners_foreach, NULL);

  for (l = selection; l != NULL; l = g_list_next (l))
    {
      GObject *object;
      gchar *urn = (gchar *) l->data;
      gulong id;

      object = photos_base_manager_get_object_by_id (priv->item_mngr, urn);
      id = g_signal_connect_swapped (object,
                                     "info-updated",
                                     G_CALLBACK (photos_selection_toolbar_set_item_visibility),
                                     self);
      g_hash_table_insert (priv->item_listeners, GUINT_TO_POINTER (id), g_object_ref (object));
    }
}


static void
photos_selection_toolbar_selection_changed (PhotosSelectionController *sel_cntrlr, gpointer user_data)
{
  PhotosSelectionToolbar *self = PHOTOS_SELECTION_TOOLBAR (user_data);
  PhotosSelectionToolbarPrivate *priv = self->priv;
  GList *selection;

  if (!photos_selection_controller_get_selection_mode (priv->sel_cntrlr))
    return;

  selection = photos_selection_controller_get_selection (priv->sel_cntrlr);
  photos_selection_toolbar_set_item_listeners (self, selection);

  if (g_list_length (selection) > 0)
    {
      photos_selection_toolbar_set_item_visibility (self);
      photos_selection_toolbar_fade_in (self);
    }
  else
    photos_selection_toolbar_fade_out (self);
}


static void
photos_selection_toolbar_selection_mode_changed (PhotosSelectionController *sel_cntrlr,
                                                 gboolean mode,
                                                 gpointer user_data)
{
  PhotosSelectionToolbar *self = PHOTOS_SELECTION_TOOLBAR (user_data);

  if (mode)
    photos_selection_toolbar_selection_changed (sel_cntrlr, self);
  else
    photos_selection_toolbar_fade_out (self);
}


static void
photos_selection_toolbar_trash_clicked (GtkButton *button, gpointer user_data)
{
  PhotosSelectionToolbar *self = PHOTOS_SELECTION_TOOLBAR (user_data);
  PhotosSelectionToolbarPrivate *priv = self->priv;
  GList *selection;
  GList *l;

  selection = photos_selection_controller_get_selection (priv->sel_cntrlr);
  for (l = selection; l != NULL; l = l->next)
    {
      /* TODO: trash the doc */
    }
}


static void
photos_selection_toolbar_constructed (GObject *object)
{
  PhotosSelectionToolbar *self = PHOTOS_SELECTION_TOOLBAR (object);
  PhotosSelectionToolbarPrivate *priv = self->priv;
  ClutterConstraint *constraint;

  G_OBJECT_CLASS (photos_selection_toolbar_parent_class)->constructed (object);

  priv->width_constraint = clutter_bind_constraint_new (priv->parent_actor, CLUTTER_BIND_WIDTH, -300.0);
  clutter_actor_add_constraint (priv->actor, priv->width_constraint);
  g_signal_connect (priv->actor, "notify::width", G_CALLBACK (photos_selection_toolbar_notify_width), self);

  constraint = clutter_align_constraint_new (priv->parent_actor, CLUTTER_ALIGN_X_AXIS, 0.50);
  clutter_actor_add_constraint (priv->actor, constraint);

  constraint = clutter_align_constraint_new (priv->parent_actor, CLUTTER_ALIGN_Y_AXIS, 0.95);
  clutter_actor_add_constraint (priv->actor, constraint);
}


static void
photos_selection_toolbar_dispose (GObject *object)
{
  PhotosSelectionToolbar *self = PHOTOS_SELECTION_TOOLBAR (object);
  PhotosSelectionToolbarPrivate *priv = self->priv;

  g_clear_object (&priv->parent_actor);

  if (priv->item_listeners != NULL)
    {
      g_hash_table_unref (priv->item_listeners);
      priv->item_listeners = NULL;
    }

  g_clear_object (&priv->item_mngr);

  if (priv->sel_cntrlr != NULL)
    {
      g_object_unref (priv->sel_cntrlr);
      priv->sel_cntrlr = NULL;
    }

  G_OBJECT_CLASS (photos_selection_toolbar_parent_class)->dispose (object);
}


static void
photos_selection_toolbar_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
  PhotosSelectionToolbar *self = PHOTOS_SELECTION_TOOLBAR (object);

  switch (prop_id)
    {
    case PROP_PARENT_ACTOR:
      self->priv->parent_actor = CLUTTER_ACTOR (g_value_dup_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
photos_selection_toolbar_init (PhotosSelectionToolbar *self)
{
  PhotosSelectionToolbarPrivate *priv;
  GtkWidget *bin;
  GtkWidget *image;
  GtkStyleContext *context;

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, PHOTOS_TYPE_SELECTION_TOOLBAR, PhotosSelectionToolbarPrivate);
  priv = self->priv;

  priv->item_listeners = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, g_object_unref);

  priv->widget = gtk_toolbar_new ();
  gtk_toolbar_set_show_arrow (GTK_TOOLBAR (priv->widget), FALSE);
  gtk_toolbar_set_icon_size (GTK_TOOLBAR (priv->widget), GTK_ICON_SIZE_LARGE_TOOLBAR);
  context = gtk_widget_get_style_context (priv->widget);
  gtk_style_context_add_class (context, "osd");

  priv->actor = gtk_clutter_actor_new_with_contents (priv->widget);
  clutter_actor_set_opacity (priv->actor, 0);
  g_object_set (priv->actor, "show-on-set-parent", FALSE, NULL);

  bin = gtk_clutter_actor_get_widget (GTK_CLUTTER_ACTOR (priv->actor));
  photos_utils_alpha_gtk_widget (bin);

  priv->left_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  priv->left_group = gtk_tool_item_new ();
  gtk_container_add (GTK_CONTAINER (priv->left_group), priv->left_box);
  gtk_toolbar_insert (GTK_TOOLBAR (priv->widget), priv->left_group, -1);

  priv->toolbar_favorite = gtk_toggle_button_new ();
  image = gtk_image_new_from_icon_name ("emblem-favorite-symbolic", GTK_ICON_SIZE_INVALID);
  gtk_image_set_pixel_size (GTK_IMAGE (image), 32);
  gtk_container_add (GTK_CONTAINER (priv->toolbar_favorite), image);
  gtk_container_add (GTK_CONTAINER (priv->left_box), priv->toolbar_favorite);
  g_signal_connect (priv->toolbar_favorite,
                    "clicked",
                    G_CALLBACK (photos_selection_toolbar_favorite_clicked),
                    self);

  priv->toolbar_properties = gtk_button_new ();
  image = gtk_image_new_from_icon_name ("document-properties-symbolic", GTK_ICON_SIZE_INVALID);
  gtk_image_set_pixel_size (GTK_IMAGE (image), 32);
  gtk_container_add (GTK_CONTAINER (priv->toolbar_properties), image);
  gtk_widget_set_tooltip_text (GTK_WIDGET (priv->toolbar_properties), _("Properties"));
  gtk_container_add (GTK_CONTAINER (priv->left_box), priv->toolbar_properties);
  g_signal_connect (priv->toolbar_properties,
                    "clicked",
                    G_CALLBACK (photos_selection_toolbar_properties_clicked),
                    self);

  priv->toolbar_print = gtk_button_new ();
  image = gtk_image_new_from_icon_name ("printer-symbolic", GTK_ICON_SIZE_INVALID);
  gtk_image_set_pixel_size (GTK_IMAGE (image), 32);
  gtk_container_add (GTK_CONTAINER (priv->toolbar_print), image);
  gtk_widget_set_tooltip_text (GTK_WIDGET (priv->toolbar_print), _("Print"));
  gtk_container_add (GTK_CONTAINER (priv->left_box), priv->toolbar_print);
  g_signal_connect (priv->toolbar_print,
                    "clicked",
                    G_CALLBACK (photos_selection_toolbar_print_clicked),
                    self);

  priv->separator = gtk_separator_tool_item_new ();
  gtk_separator_tool_item_set_draw (GTK_SEPARATOR_TOOL_ITEM (priv->separator), FALSE);
  gtk_widget_set_visible (GTK_WIDGET (priv->separator), TRUE);
  gtk_tool_item_set_expand (priv->separator, TRUE);
  gtk_toolbar_insert (GTK_TOOLBAR (priv->widget), priv->separator, -1);

  priv->right_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  priv->right_group = gtk_tool_item_new ();
  gtk_container_add (GTK_CONTAINER (priv->right_group), priv->right_box);
  gtk_toolbar_insert (GTK_TOOLBAR (priv->widget), priv->right_group, -1);

  priv->toolbar_collection = gtk_button_new ();
  image = gtk_image_new_from_icon_name ("list-add-symbolic", GTK_ICON_SIZE_INVALID);
  gtk_image_set_pixel_size (GTK_IMAGE (image), 32);
  gtk_container_add (GTK_CONTAINER (priv->toolbar_collection), image);
  gtk_widget_set_tooltip_text (GTK_WIDGET (priv->toolbar_collection), _("Organize"));
  gtk_container_add (GTK_CONTAINER (priv->right_box), priv->toolbar_collection);
  g_signal_connect (priv->toolbar_collection,
                    "clicked",
                    G_CALLBACK (photos_selection_toolbar_collection_clicked),
                    self);

  priv->toolbar_trash = gtk_button_new ();
  image = gtk_image_new_from_icon_name ("user-trash-symbolic", GTK_ICON_SIZE_INVALID);
  gtk_image_set_pixel_size (GTK_IMAGE (image), 32);
  gtk_container_add (GTK_CONTAINER (priv->toolbar_trash), image);
  gtk_widget_set_tooltip_text (GTK_WIDGET (priv->toolbar_trash), _("Delete"));
  gtk_container_add (GTK_CONTAINER (priv->right_box), priv->toolbar_trash);
  g_signal_connect (priv->toolbar_trash,
                    "clicked",
                    G_CALLBACK (photos_selection_toolbar_trash_clicked),
                    self);

  priv->toolbar_open = gtk_button_new ();
  image = gtk_image_new_from_icon_name ("document-open-symbolic", GTK_ICON_SIZE_INVALID);
  gtk_image_set_pixel_size (GTK_IMAGE (image), 32);
  gtk_container_add (GTK_CONTAINER (priv->toolbar_open), image);
  gtk_container_add (GTK_CONTAINER (priv->right_box), priv->toolbar_open);
  g_signal_connect (priv->toolbar_open,
                    "clicked",
                    G_CALLBACK (photos_selection_toolbar_open_clicked),
                    self);

  gtk_widget_show_all (priv->widget);

  priv->item_mngr = photos_item_manager_new ();

  priv->sel_cntrlr = photos_selection_controller_new ();
  g_signal_connect (priv->sel_cntrlr,
                    "selection-changed",
                    G_CALLBACK (photos_selection_toolbar_selection_changed),
                    self);
  g_signal_connect (priv->sel_cntrlr,
                    "selection-mode-changed",
                    G_CALLBACK (photos_selection_toolbar_selection_mode_changed),
                    self);
}


static void
photos_selection_toolbar_class_init (PhotosSelectionToolbarClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->constructed= photos_selection_toolbar_constructed;
  object_class->dispose = photos_selection_toolbar_dispose;
  object_class->set_property = photos_selection_toolbar_set_property;

  g_object_class_install_property (object_class,
                                   PROP_PARENT_ACTOR,
                                   g_param_spec_object ("parent-actor",
                                                        "Parent actor",
                                                        "A ClutterActor used for calculating the the alignment and "
                                                        "width of the toolbar",
                                                        CLUTTER_TYPE_ACTOR,
                                                        G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE));

  g_type_class_add_private (class, sizeof (PhotosSelectionToolbarPrivate));
}


PhotosSelectionToolbar *
photos_selection_toolbar_new (ClutterActor *parent_actor)
{
  return g_object_new (PHOTOS_TYPE_SELECTION_TOOLBAR, "parent-actor", parent_actor, NULL);
}


ClutterActor *
photos_selection_toolbar_get_actor (PhotosSelectionToolbar *self)
{
  return self->priv->actor;
}
