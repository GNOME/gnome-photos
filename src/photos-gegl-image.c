#include "photos-gegl-image.h"



G_DEFINE_TYPE(PhotosGeglImage, photos_gegl_image, GTK_TYPE_ABSTRACT_IMAGE)

static int
photos_gegl_image_get_width (GtkAbstractImage *_image)
{
  return PHOTOS_GEGL_IMAGE (_image)->width;
}

static int
photos_gegl_image_get_height (GtkAbstractImage *_image)
{
  return PHOTOS_GEGL_IMAGE (_image)->height;
}

static int
photos_gegl_image_get_scale_factor (GtkAbstractImage *_image)
{
  return PHOTOS_GEGL_IMAGE (_image)->scale_factor;
}

static void
photos_gegl_image_update_bbox (PhotosGeglImage *image, gboolean scale_changed)
{
  GeglRectangle box    = gegl_node_get_bounding_box (image->node);
  gboolean roi_changed = !gegl_rectangle_equal (&image->roi, &box);

  if (roi_changed || scale_changed)
    {
      int stride;
      image->width  = box.width;
      image->height = box.height;

      image->roi = box;

      stride = cairo_format_stride_for_width (CAIRO_FORMAT_ARGB32,
                                              box.width * image->view_scale);

      g_clear_pointer (&image->buf, g_free);
      g_clear_pointer (&image->surface, cairo_surface_destroy);

      image->buf = g_malloc (stride * box.height); /* stride is already scaled */
      image->surface = cairo_image_surface_create_for_data (image->buf,
                                                            CAIRO_FORMAT_ARGB32,
                                                            image->width * image->view_scale,
                                                            image->height * image->view_scale,
                                                            stride);

      image->need_blit = TRUE;

      g_signal_emit_by_name (G_OBJECT (image), "changed", 0);
    }
}

static void
photos_gegl_image_draw (GtkAbstractImage *_image, cairo_t *ct)
{
  PhotosGeglImage *image = PHOTOS_GEGL_IMAGE (_image);

  if (G_LIKELY (image->surface))
    {
      if (image->need_blit)
        {
          GeglRectangle roi = image->roi;
          roi.x      *= image->view_scale;
          roi.y      *= image->view_scale;
          roi.width  *= image->view_scale;
          roi.height *= image->view_scale;


          gegl_node_blit (image->node,
                          image->view_scale,
                          &roi,
                          image->format,
                          image->buf,
                          GEGL_AUTO_ROWSTRIDE,
                          GEGL_BLIT_CACHE | GEGL_BLIT_DIRTY);

          image->need_blit = FALSE;
        }

      cairo_scale (ct, 1.0 / image->view_scale, 1.0 / image->view_scale);
      cairo_set_source_surface (ct, image->surface, 0, 0);
    }
}

static void
photos_gegl_image_computed (PhotosGeglImage *image)
{
  image->need_blit = TRUE;
  photos_gegl_image_update_bbox (image, FALSE);
}


PhotosGeglImage *
photos_gegl_image_new (GeglNode *node, int scale_factor)
{
  PhotosGeglImage *image = (PhotosGeglImage *) g_object_new (PHOTOS_TYPE_GEGL_IMAGE, NULL);
  g_signal_connect_object (node, "computed", G_CALLBACK (photos_gegl_image_computed), image, G_CONNECT_SWAPPED);

  image->node = node;
  image->scale_factor = scale_factor;
  image->format = babl_format ("cairo-ARGB32");
  image->roi = gegl_node_get_bounding_box (image->node);
  image->width = image->roi.width;
  image->height = image->roi.height;

  return image;
}

static void
photos_gegl_image_init (PhotosGeglImage *image)
{
  image->surface = NULL;
  image->view_scale = 0.0;
}

static void
photos_gegl_image_finalize (GObject *_image)
{
  PhotosGeglImage *image = PHOTOS_GEGL_IMAGE (_image);

  if (image->surface)
    cairo_surface_destroy (image->surface);

  if (image->buf)
    g_free (image->buf);

  G_OBJECT_CLASS (photos_gegl_image_parent_class)->finalize (_image);
}

static void
photos_gegl_image_class_init (PhotosGeglImageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkAbstractImageClass *image_class = GTK_ABSTRACT_IMAGE_CLASS (klass);

  object_class->finalize = photos_gegl_image_finalize;

  image_class->draw = photos_gegl_image_draw;
  image_class->get_width = photos_gegl_image_get_width;
  image_class->get_height = photos_gegl_image_get_height;
  image_class->get_scale_factor = photos_gegl_image_get_scale_factor;
}

void
photos_gegl_image_set_view_scale (PhotosGeglImage *image, double view_scale)
{
  g_message ("New scale: %f, old scale: %f", view_scale, image->view_scale);
  if (view_scale == image->view_scale)
    return;

  image->view_scale = view_scale;

  photos_gegl_image_update_bbox (image, TRUE);
}
