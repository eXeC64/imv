#include "bitmap.h"

#include <stdlib.h>
#include <string.h>

struct imv_bitmap *imv_bitmap_clone(struct imv_bitmap *bmp)
{
  struct imv_bitmap *copy = malloc(sizeof(struct imv_bitmap));
  const size_t num_bytes = 4 * bmp->width * bmp->height;
  copy->width = bmp->width;
  copy->height = bmp->height;
  copy->format = bmp->format;
  copy->data = malloc(num_bytes);
  memcpy(copy->data, bmp->data, num_bytes);
  return copy;
}

void imv_bitmap_free(struct imv_bitmap *bmp)
{
  free(bmp->data);
  free(bmp);
}
