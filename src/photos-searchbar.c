/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2014 Pranav Kant
 * Copyright © 2014, 2015 Red Hat, Inc.
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
#include <libgd/gd.h>

#include "photos-searchbar.h"


struct _PhotosSearchbarPrivate
{
  GAction *search;
  GtkWidget *search_container;
  GtkWidget *search_entry;
  GtkWidget *toolbar;
  gboolean in;
  gboolean preedit_changed;
  gboolean search_change_blocked;
  gulong search_state_id;
};

enum
{
  ACTIVATE_RESULT,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };


G_DEFINE_TYPE_WITH_PRIVATE (PhotosSearchbar, photos_searchbar, GTK_TYPE_REVEALER);


static void
photos_searchbar_change_state (PhotosSearchbar *self, GVariant *value)
{
  g_simple_action_set_state (G_SIMPLE_ACTION (self->priv->search), value);

  if (g_variant_get_boolean (value))
    photos_searchbar_show (self);
  else
    photos_searchbar_hide (self);
}


static void
photos_searchbar_default_hide (PhotosSearchbar *self)
{
  PhotosSearchbarPrivate *priv = self->priv;

  priv->in = FALSE;
  gtk_revealer_set_reveal_child (GTK_REVEALER (self), FALSE);

  /* Clear all the search properties when hiding the entry */
  gtk_entry_set_text (GTK_ENTRY (priv->search_entry), "");
}


static void
photos_searchbar_default_show (PhotosSearchbar *self)
{
  PhotosSearchbarPrivate *priv = self->priv;
  GdkDevice *event_device;

  event_device = gtk_get_current_event_device ();
  gtk_revealer_set_reveal_child (GTK_REVEALER (self), TRUE);
  priv->in = TRUE;

  if (event_device != NULL)
    gd_entry_focus_hack (priv->search_entry, event_device);
}


static void
photos_searchbar_enable_search (PhotosSearchbar *self, gboolean enable)
{
  GVariant *state;

  state = g_variant_new ("b", enable);
  g_action_change_state (self->priv->search, state);
}


static gboolean
photos_is_escape_event (PhotosSearchbar *self, GdkEventKey *event)
{
  return event->keyval == GDK_KEY_Escape;
}


static gboolean
photos_is_keynav_event (PhotosSearchbar *self, GdkEventKey *event)
{
  return event->keyval == GDK_KEY_Up
    || event->keyval == GDK_KEY_KP_Up
    || event->keyval == GDK_KEY_Down
    || event->keyval == GDK_KEY_KP_Down
    || event->keyval == GDK_KEY_Left
    || event->keyval == GDK_KEY_KP_Left
    || event->keyval == GDK_KEY_Right
    || event->keyval == GDK_KEY_KP_Right
    || event->keyval == GDK_KEY_Home
    || event->keyval == GDK_KEY_KP_Home
    || event->keyval == GDK_KEY_End
    || event->keyval == GDK_KEY_KP_End
    || event->keyval == GDK_KEY_Page_Up
    || event->keyval == GDK_KEY_KP_Page_Up
    || event->keyval == GDK_KEY_Page_Down
    || event->keyval == GDK_KEY_KP_Page_Down;
}


static gboolean
photos_is_space_event (PhotosSearchbar *self, GdkEventKey *event)
{
  return event->keyval == GDK_KEY_space;
}


static gboolean
photos_is_tab_event (PhotosSearchbar *self, GdkEventKey *event)
{
  return event->keyval == GDK_KEY_Tab || event->keyval == GDK_KEY_KP_Tab;
}


static void
photos_searchbar_preedit_changed (PhotosSearchbar *self, const gchar *preedit)
{
  self->priv->preedit_changed = TRUE;
}


static void
photos_searchbar_search_changed (PhotosSearchbar *self)
{
  PhotosSearchbarPrivate *priv = self->priv;

  if (priv->search_change_blocked)
    return;

  PHOTOS_SEARCHBAR_GET_CLASS (self)->entry_changed (self);
}


static void
photos_searchbar_constructed (GObject *object)
{
  PhotosSearchbar *self = PHOTOS_SEARCHBAR (object);
  PhotosSearchbarPrivate *priv = self->priv;
  GApplication *app;
  GtkToolItem *item;
  GVariant *state;

  G_OBJECT_CLASS (photos_searchbar_parent_class)->constructed (object);

  PHOTOS_SEARCHBAR_GET_CLASS (self)->create_search_widgets (self);

  item = gtk_tool_item_new ();
  gtk_tool_item_set_expand (item, TRUE);
  gtk_container_add (GTK_CONTAINER (item), priv->search_container);
  gtk_toolbar_insert (GTK_TOOLBAR (priv->toolbar), item, 0);

  g_signal_connect_swapped (priv->search_entry,
                            "search-changed",
                            G_CALLBACK (photos_searchbar_search_changed),
                            self);


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
  PhotosSearchbarPrivate *priv = self->priv;

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
  PhotosSearchbarPrivate *priv;
  GtkStyleContext *context;

  self->priv = photos_searchbar_get_instance_private (self);
  priv = self->priv;

  priv->toolbar = gtk_toolbar_new ();
  context = gtk_widget_get_style_context (priv->toolbar);
  gtk_style_context_add_class (context, "search-bar");
  gtk_container_add (GTK_CONTAINER (self), priv->toolbar);
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
  PhotosSearchbarPrivate *priv = self->priv;
  gboolean is_escape;
  gboolean is_keynav;
  gboolean is_space;
  gboolean is_tab;
  gboolean res;
  gboolean ret_val = GDK_EVENT_PROPAGATE;
  gchar *new_text = NULL;
  gchar *old_text = NULL;

  if (gtk_widget_get_parent (GTK_WIDGET (self)) == NULL)
    goto out;

  /* Skip if the search bar is shown and the focus is elsewhere */
  if (priv->in && !gtk_widget_is_focus (priv->search_entry))
    goto out;

  is_escape = photos_is_escape_event (self, event);
  is_keynav = photos_is_keynav_event (self, event);
  is_space = photos_is_space_event (self, event);
  is_tab = photos_is_tab_event (self, event);

  /* Skip these if the search bar is hidden. */
  if (!priv->in && (is_escape || is_keynav || is_space || is_tab))
    goto out;

  /* At this point, either the search bar is hidden and the event is
   * neither escape nor keynav nor space; or the search bar is shown
   * and has the focus. */

  if (is_escape)
    {
      photos_searchbar_enable_search (self, FALSE);
      ret_val = GDK_EVENT_STOP;
      goto out;
    }
  else if (event->keyval == GDK_KEY_Return)
    {
      g_signal_emit (self, signals[ACTIVATE_RESULT], 0);
      ret_val = GDK_EVENT_STOP;
      goto out;
    }

  if (!gtk_widget_get_realized (priv->search_entry))
    gtk_widget_realize (priv->search_entry);

  /* Since we can have keynav or space only when the search bar is
   * show, we want to handle it. Otherwise it will hinder text
   * input. However, we don't want to handle tabs so that focus can
   * be shifted to other widgets.
   */
  ret_val = is_keynav || is_space;

  priv->preedit_changed = FALSE;
  g_signal_connect_swapped (priv->search_entry,
                            "preedit-changed",
                            G_CALLBACK (photos_searchbar_preedit_changed),
                            self);

  old_text = g_strdup (gtk_entry_get_text (GTK_ENTRY (priv->search_entry)));
  res = gtk_widget_event (priv->search_entry, (GdkEvent *) event);
  new_text = g_strdup (gtk_entry_get_text (GTK_ENTRY (priv->search_entry)));

  g_signal_handlers_disconnect_by_func (priv->search_entry, photos_searchbar_preedit_changed, self);

  if ((res && (g_strcmp0 (new_text, old_text) != 0)) || priv->preedit_changed)
    {
      ret_val = GDK_EVENT_STOP;

      if (!priv->in)
        photos_searchbar_enable_search (self, TRUE);
    }

 out:
  g_free (new_text);
  g_free (old_text);
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
  return gtk_widget_is_focus (self->priv->search_entry);
}


void
photos_searchbar_set_search_change_blocked (PhotosSearchbar *self, gboolean search_change_blocked)
{
  self->priv->search_change_blocked = search_change_blocked;
}


void
photos_searchbar_set_search_container (PhotosSearchbar *self, GtkWidget *search_container)
{
  self->priv->search_container = search_container;
}


void
photos_searchbar_set_search_entry (PhotosSearchbar *self, GtkWidget *search_entry)
{
  self->priv->search_entry = search_entry;
}


void
photos_searchbar_show (PhotosSearchbar *self)
{
  PHOTOS_SEARCHBAR_GET_CLASS (self)->show (self);
}
