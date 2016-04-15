

#include <gtk/gtk.h>
#include <gegl.h>

typedef struct _PhotosGeglImage      PhotosGeglImage;
typedef struct _PhotosGeglImageClass PhotosGeglImageClass;

#define PHOTOS_TYPE_GEGL_IMAGE           (photos_gegl_image_get_type ())
#define PHOTOS_GEGL_IMAGE(obj)           (G_TYPE_CHECK_INSTANCE_CAST(obj, PHOTOS_TYPE_GEGL_IMAGE, PhotosGeglImage))
#define PHOTOS_GEGL_IMAGE_CLASS(cls)     (G_TYPE_CHECK_CLASS_CAST(cls, PHOTOS_TYPE_GEGL_IMAGE, PhotosGeglImageClass))
#define PHOTOS_IS_GEGL_IMAGE(obj)        (G_TYPE_CHECK_INSTANCE_TYPE(obj, PHOTOS_TYPE_GEGL_IMAGE))
#define PHOTOS_IS_GEGL_IMAGE_CLASS(cls)  (G_TYPE_CHECK_CLASS_TYPE(cls, PHOTOS_TYPE_GEGL_IMAGE))
#define PHOTOS_GEGL_IMAGE_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS(obj, PHOTOS_TYPE_GEGL_IMAGE, PhotosGeglImageClass))

struct _PhotosGeglImage
{
  GtkAbstractImage parent_instance;
  GeglNode *node;
  int width;
  int height;
  double view_scale;
  cairo_surface_t *surface;
  guchar *buf;
  const Babl *format;
  GeglRectangle roi;
  int scale_factor;
  gboolean need_blit;
};

struct _PhotosGeglImageClass
{
  GtkAbstractImageClass parent_class;
};

GType photos_gegl_image_get_type (void) G_GNUC_CONST;

PhotosGeglImage *photos_gegl_image_new (GeglNode *node, int scale_factor);

void photos_gegl_image_set_view_scale (PhotosGeglImage *image, double view_scale);
