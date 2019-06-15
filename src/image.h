#ifndef IMV_IMAGE_H
#define IMV_IMAGE_H

#include "bitmap.h"

#ifdef IMV_BACKEND_LIBRSVG
#include <librsvg/rsvg.h>
#endif

struct imv_image;

struct imv_image *imv_image_create_from_bitmap(struct imv_bitmap *bmp);

#ifdef IMV_BACKEND_LIBRSVG
struct imv_image *imv_image_create_from_svg(RsvgHandle *handle);
#endif

/* Cleans up an imv_image instance */
void imv_image_free(struct imv_image *image);

/* Get the image width */
int imv_image_width(const struct imv_image *image);

/* Get the image height */
int imv_image_height(const struct imv_image *image);

#endif


/* vim:set ts=2 sts=2 sw=2 et: */
