#include "image.h"

#include "bitmap.h"

#include <stdlib.h>

struct imv_image {
  int width;
  int height;
  struct imv_bitmap *bitmap;
  #ifdef IMV_BACKEND_LIBRSVG
  RsvgHandle *svg;
  #endif
};

struct imv_image *imv_image_create_from_bitmap(struct imv_bitmap *bmp)
{
  struct imv_image *image = calloc(1, sizeof *image);
  image->width = bmp->width;
  image->height = bmp->height;
  image->bitmap = bmp;
  return image;
}

#ifdef IMV_BACKEND_LIBRSVG
struct imv_image *imv_image_create_from_svg(RsvgHandle *handle)
{
  struct imv_image *image = calloc(1, sizeof *image);
  image->svg = handle;

  RsvgDimensionData dim;
  rsvg_handle_get_dimensions(handle, &dim);
  image->width = dim.width;
  image->height = dim.height;
  return image;
}
#endif

void imv_image_free(struct imv_image *image)
{
  if (!image) {
    return;
  }

  if (image->bitmap) {
    imv_bitmap_free(image->bitmap);
  }

#ifdef IMV_BACKEND_LIBRSVG
  if (image->svg) {
    g_object_unref(image->svg);
  }
#endif

  free(image);
}

int imv_image_width(const struct imv_image *image)
{
  return image ? image->width : 0;
}

int imv_image_height(const struct imv_image *image)
{
  return image ? image->height : 0;
}

/* Non-public functions, only used by imv_canvas */
struct imv_bitmap *imv_image_get_bitmap(const struct imv_image *image)
{
  return image->bitmap;
}

#ifdef IMV_BACKEND_LIBRSVG
RsvgHandle *imv_image_get_svg(const struct imv_image *image)
{
  return image->svg;
}
#endif

/* vim:set ts=2 sts=2 sw=2 et: */
