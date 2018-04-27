/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2017 Red Hat, Inc.
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

#include <math.h>

#include "photos-enums.h"
#include "photos-gesture-zoom.h"
#include "photos-marshalers.h"
#include "photos-utils.h"


struct _PhotosGestureZoom
{
  GObject parent_instance;
  GtkGesture *gesture;
  PhotosGestureZoomDirection previous_direction;
  gdouble initial_distance;
  gdouble previous_distance;
};

enum
{
  PROP_0,
  PROP_GESTURE
};

enum
{
  DIRECTION_CHANGED,
  SCALE_CHANGED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };


G_DEFINE_TYPE (PhotosGestureZoom, photos_gesture_zoom, G_TYPE_OBJECT);


static gdouble
photos_gesture_zoom_calculate_distance (PhotosGestureZoom *self, GdkTouchpadGesturePhase touchpad_phase)
{
  g_autoptr (GList) sequences = NULL;
  const GdkEvent *last_event;
  gdouble ret_val = -1.0;

  g_return_val_if_fail (gtk_gesture_is_recognized (self->gesture), -1.0);

  sequences = gtk_gesture_get_sequences (self->gesture);
  g_return_val_if_fail (sequences != NULL, -1.0);

  last_event = gtk_gesture_get_last_event (self->gesture, (GdkEventSequence *) sequences->data);
  if (last_event->type == GDK_TOUCHPAD_PINCH
      && (GdkTouchpadGesturePhase) last_event->touchpad_pinch.phase == touchpad_phase)
    {
      ret_val = last_event->touchpad_pinch.scale;
    }
  else
    {
      gdouble distance2;
      gdouble x1;
      gdouble x2;
      gdouble y1;
      gdouble y2;

      g_assert_nonnull (sequences->next);

      gtk_gesture_get_point (self->gesture, (GdkEventSequence *) sequences->data, &x1, &y1);
      gtk_gesture_get_point (self->gesture, (GdkEventSequence *) sequences->next->data, &x2, &y2);

      distance2 = (x1 - x2) * (x1 - x2) + (y1 - y2) * (y1 - y2);
      ret_val = sqrt (distance2);
    }

  return ret_val;
}


static void
photos_gesture_zoom_begin (PhotosGestureZoom *self)
{
  g_return_if_fail (PHOTOS_IS_GESTURE_ZOOM (self));
  g_return_if_fail (gtk_gesture_is_recognized (self->gesture));

  self->initial_distance = photos_gesture_zoom_calculate_distance (self, GDK_TOUCHPAD_GESTURE_PHASE_BEGIN);
  g_return_if_fail (self->initial_distance >= 0.0);

  self->previous_direction = PHOTOS_GESTURE_ZOOM_DIRECTION_NONE;
  self->previous_distance = self->initial_distance;
}


static void
photos_gesture_zoom_update (PhotosGestureZoom *self)
{
  PhotosGestureZoomDirection direction;
  gdouble distance;
  gdouble scale;

  g_return_if_fail (PHOTOS_IS_GESTURE_ZOOM (self));
  g_return_if_fail ((self->previous_direction == PHOTOS_GESTURE_ZOOM_DIRECTION_NONE
                     && photos_utils_equal_double (self->initial_distance, self->previous_distance))
                    || (self->previous_direction != PHOTOS_GESTURE_ZOOM_DIRECTION_NONE
                        && !photos_utils_equal_double (self->initial_distance, self->previous_distance)));

  distance = photos_gesture_zoom_calculate_distance (self, GDK_TOUCHPAD_GESTURE_PHASE_UPDATE);
  g_return_if_fail (distance >= 0.0);

  if (photos_utils_equal_double (distance, self->previous_distance))
    goto out;

  if (self->previous_distance > distance)
    direction = PHOTOS_GESTURE_ZOOM_DIRECTION_DECREASING;
  else
    direction = PHOTOS_GESTURE_ZOOM_DIRECTION_INCREASING;

  if (self->previous_direction != PHOTOS_GESTURE_ZOOM_DIRECTION_NONE && self->previous_direction != direction)
    {
      self->initial_distance = self->previous_distance;
      g_signal_emit (self, signals[DIRECTION_CHANGED], 0);
    }

  scale = distance / self->initial_distance;
  g_signal_emit (self, signals[SCALE_CHANGED], 0, scale, direction);

  self->previous_direction = direction;

 out:
  self->previous_distance = distance;
}


static void
photos_gesture_zoom_constructed (GObject *object)
{
  PhotosGestureZoom *self = PHOTOS_GESTURE_ZOOM (object);

  G_OBJECT_CLASS (photos_gesture_zoom_parent_class)->constructed (object);

  g_signal_connect_object (self->gesture,
                           "begin",
                           G_CALLBACK (photos_gesture_zoom_begin),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (self->gesture,
                           "update",
                           G_CALLBACK (photos_gesture_zoom_update),
                           self,
                           G_CONNECT_SWAPPED);
}


static void
photos_gesture_zoom_dispose (GObject *object)
{
  PhotosGestureZoom *self = PHOTOS_GESTURE_ZOOM (object);

  g_clear_object (&self->gesture);

  G_OBJECT_CLASS (photos_gesture_zoom_parent_class)->dispose (object);
}


static void
photos_gesture_zoom_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
  PhotosGestureZoom *self = PHOTOS_GESTURE_ZOOM (object);

  switch (prop_id)
    {
    case PROP_GESTURE:
      g_value_set_object (value, self->gesture);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
photos_gesture_zoom_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
  PhotosGestureZoom *self = PHOTOS_GESTURE_ZOOM (object);

  switch (prop_id)
    {
    case PROP_GESTURE:
      self->gesture = GTK_GESTURE (g_value_dup_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
photos_gesture_zoom_init (PhotosGestureZoom *self)
{
}


static void
photos_gesture_zoom_class_init (PhotosGestureZoomClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->constructed = photos_gesture_zoom_constructed;
  object_class->dispose = photos_gesture_zoom_dispose;
  object_class->get_property = photos_gesture_zoom_get_property;
  object_class->set_property = photos_gesture_zoom_set_property;

  g_object_class_install_property (object_class,
                                   PROP_GESTURE,
                                   g_param_spec_object ("gesture",
                                                        "Gesture",
                                                        "The GtkGesture whose behaviour will be extended",
                                                        GTK_TYPE_GESTURE,
                                                        G_PARAM_CONSTRUCT_ONLY
                                                        | G_PARAM_READWRITE));

  signals[DIRECTION_CHANGED] = g_signal_new ("direction-changed",
                                             G_TYPE_FROM_CLASS (class),
                                             G_SIGNAL_RUN_LAST,
                                             0,
                                             NULL, /* accumulator */
                                             NULL, /* accu_data */
                                             g_cclosure_marshal_VOID__VOID,
                                             G_TYPE_NONE,
                                             0);

  signals[SCALE_CHANGED] = g_signal_new ("scale-changed",
                                         G_TYPE_FROM_CLASS (class),
                                         G_SIGNAL_RUN_LAST,
                                         0,
                                         NULL, /* accumulator */
                                         NULL, /* accu_data */
                                         _photos_marshal_VOID__DOUBLE_ENUM,
                                         G_TYPE_NONE,
                                         2,
                                         G_TYPE_DOUBLE,
                                         PHOTOS_TYPE_GESTURE_ZOOM_DIRECTION);
}


PhotosGestureZoom *
photos_gesture_zoom_new (GtkGesture *gesture)
{
  g_return_val_if_fail (GTK_IS_GESTURE (gesture), NULL);
  return g_object_new (PHOTOS_TYPE_GESTURE_ZOOM, "gesture", gesture, NULL);
}


GtkGesture *
photos_gesture_zoom_get_gesture (PhotosGestureZoom *self)
{
  g_return_val_if_fail (PHOTOS_IS_GESTURE_ZOOM (self), NULL);
  return self->gesture;
}
