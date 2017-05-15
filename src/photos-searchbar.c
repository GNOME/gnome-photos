/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2014 Pranav Kant
 * Copyright © 2014 – 2017 Red Hat, Inc.
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

#include "photos-searchbar.h"


struct _PhotosSearchbarPrivate
{
  GAction *search;
  GtkWidget *search_container;
  GtkWidget *search_entry;
  gboolean search_change_blocked;
  gulong search_state_id;
};

enum
{
  ACTIVATE_RESULT,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };


G_DEFINE_TYPE_WITH_PRIVATE (PhotosSearchbar, photos_searchbar, GTK_TYPE_SEARCH_BAR);


static void
photos_searchbar_change_state (PhotosSearchbar *self, GVariant *value)
{
  PhotosSearchbarPrivate *priv;

  priv = photos_searchbar_get_instance_private (self);

  g_simple_action_set_state (G_SIMPLE_ACTION (priv->search), value);

  if (g_variant_get_boolean (value))
    photos_searchbar_show (self);
  else
    photos_searchbar_hide (self);
}


static void
photos_searchbar_default_hide (PhotosSearchbar *self)
{
  PhotosSearchbarPrivate *priv;

  priv = photos_searchbar_get_instance_private (self);

  gtk_search_bar_set_search_mode (GTK_SEARCH_BAR (self), FALSE);

  /* Clear all the search properties when hiding the entry */
  gtk_entry_set_text (GTK_ENTRY (priv->search_entry), "");
}


static void
photos_searchbar_default_show (PhotosSearchbar *self)
{
  gtk_search_bar_set_search_mode (GTK_SEARCH_BAR (self), TRUE);
}


static void
photos_searchbar_enable_search (PhotosSearchbar *self, gboolean enable)
{
  PhotosSearchbarPrivate *priv;
  GVariant *state;

  priv = photos_searchbar_get_instance_private (self);

  state = g_variant_new ("b", enable);
  g_action_change_state (priv->search, state);
}


static void
photos_searchbar_notify_search_mode_enabled (PhotosSearchbar *self)
{
  gboolean search_enabled;

  search_enabled = gtk_search_bar_get_search_mode (GTK_SEARCH_BAR (self));
  photos_searchbar_enable_search (self, search_enabled);
}


static void
photos_searchbar_search_changed (PhotosSearchbar *self)
{
  PhotosSearchbarPrivate *priv;

  priv = photos_searchbar_get_instance_private (self);

  if (priv->search_change_blocked)
    return;

  PHOTOS_SEARCHBAR_GET_CLASS (self)->entry_changed (self);
}


static void
photos_searchbar_constructed (GObject *object)
{
  PhotosSearchbar *self = PHOTOS_SEARCHBAR (object);
  PhotosSearchbarPrivate *priv;
  GApplication *app;
  GVariant *state;

  priv = photos_searchbar_get_instance_private (self);

  G_OBJECT_CLASS (photos_searchbar_parent_class)->constructed (object);

  PHOTOS_SEARCHBAR_GET_CLASS (self)->create_search_widgets (self);

  gtk_container_add (GTK_CONTAINER (self), priv->search_container);
  gtk_search_bar_connect_entry (GTK_SEARCH_BAR (self), GTK_ENTRY (priv->search_entry));

  g_signal_connect_swapped (priv->search_entry,
                            "search-changed",
                            G_CALLBACK (photos_searchbar_search_changed),
                            self);

  g_signal_connect (self,
                    "notify::search-mode-enabled",
                    G_CALLBACK (photos_searchbar_notify_search_mode_enabled),
                    NULL);

  app = g_application_get_default ();
  priv->search = g_action_map_lookup_action (G_ACTION_MAP (app), "search");

  /* g_signal_connect_object will not be able to disconnect the
   * handler in time because we change the state of the action during
   * dispose.
   */
  priv->search_state_id = g_signal_connect_swapped (priv->search,
                                                    "change-state",
                                                    G_CALLBACK (photos_searchbar_change_state),
                                                    self);
  state = g_action_get_state (priv->search);
  photos_searchbar_change_state (self, state);
  g_variant_unref (state);

  gtk_widget_show_all (GTK_WIDGET (self));
}


static void
photos_searchbar_dispose (GObject *object)
{
  PhotosSearchbar *self = PHOTOS_SEARCHBAR (object);
  PhotosSearchbarPrivate *priv;

  priv = photos_searchbar_get_instance_private (self);

  if (priv->search_state_id != 0)
    {
      g_signal_handler_disconnect (priv->search, priv->search_state_id);
      priv->search_state_id = 0;
    }

  photos_searchbar_enable_search (self, FALSE);

  G_OBJECT_CLASS (photos_searchbar_parent_class)->dispose (object);
}


static void
photos_searchbar_init (PhotosSearchbar *self)
{
}


static void
photos_searchbar_class_init (PhotosSearchbarClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->constructed = photos_searchbar_constructed;
  object_class->dispose = photos_searchbar_dispose;
  class->hide = photos_searchbar_default_hide;
  class->show = photos_searchbar_default_show;

  signals[ACTIVATE_RESULT] = g_signal_new ("activate-result",
                                           G_TYPE_FROM_CLASS (class),
                                           G_SIGNAL_RUN_LAST,
                                           G_STRUCT_OFFSET (PhotosSearchbarClass, activate_result),
                                           NULL, /* accumulator */
                                           NULL, /* accu_data */
                                           g_cclosure_marshal_VOID__VOID,
                                           G_TYPE_NONE,
                                           0);
}


GtkWidget *
photos_searchbar_new (void)
{
  return g_object_new (PHOTOS_TYPE_SEARCHBAR, NULL);
}


gboolean
photos_searchbar_handle_event (PhotosSearchbar *self, GdkEventKey *event)
{
  PhotosSearchbarPrivate *priv;
  gboolean ret_val = GDK_EVENT_PROPAGATE;
  gboolean search_mode_enabled;

  priv = photos_searchbar_get_instance_private (self);

  if (gtk_widget_get_parent (GTK_WIDGET (self)) == NULL)
    goto out;

  search_mode_enabled = gtk_search_bar_get_search_mode (GTK_SEARCH_BAR (self));

  /* Skip if the search bar is shown and the focus is elsewhere */
  if (search_mode_enabled && !gtk_widget_is_focus (priv->search_entry))
    goto out;

  if (search_mode_enabled && event->keyval == GDK_KEY_Return)
    {
      g_signal_emit (self, signals[ACTIVATE_RESULT], 0);
      ret_val = GDK_EVENT_STOP;
      goto out;
    }

  ret_val = gtk_search_bar_handle_event (GTK_SEARCH_BAR (self), (GdkEvent *) event);
  if (ret_val == GDK_EVENT_STOP)
    gtk_entry_grab_focus_without_selecting (GTK_ENTRY (priv->search_entry));

 out:
  return ret_val;
}


void
photos_searchbar_hide (PhotosSearchbar *self)
{
  PHOTOS_SEARCHBAR_GET_CLASS (self)->hide (self);
}


gboolean
photos_searchbar_is_focus (PhotosSearchbar *self)
{
  PhotosSearchbarPrivate *priv;

  priv = photos_searchbar_get_instance_private (self);
  return gtk_widget_is_focus (priv->search_entry);
}


void
photos_searchbar_set_search_change_blocked (PhotosSearchbar *self, gboolean search_change_blocked)
{
  PhotosSearchbarPrivate *priv;

  priv = photos_searchbar_get_instance_private (self);
  priv->search_change_blocked = search_change_blocked;
}


void
photos_searchbar_set_search_container (PhotosSearchbar *self, GtkWidget *search_container)
{
  PhotosSearchbarPrivate *priv;

  priv = photos_searchbar_get_instance_private (self);
  priv->search_container = search_container;
}


void
photos_searchbar_set_search_entry (PhotosSearchbar *self, GtkWidget *search_entry)
{
  PhotosSearchbarPrivate *priv;

  priv = photos_searchbar_get_instance_private (self);
  priv->search_entry = search_entry;
}


void
photos_searchbar_show (PhotosSearchbar *self)
{
  PHOTOS_SEARCHBAR_GET_CLASS (self)->show (self);
}
