#ifndef IMV_IMAGE_H
#define IMV_IMAGE_H

#include "bitmap.h"
#include <SDL2/SDL.h>

struct imv_image;

/* Creates an instance of imv_image */
struct imv_image *imv_image_create(SDL_Renderer *r);

/* Cleans up an imv_image instance */
void imv_image_free(struct imv_image *image);

/* Updates the image to contain the data in the bitmap parameter */
int imv_image_set_bitmap(struct imv_image *image, struct imv_bitmap *bmp);

/* Draw the image at the given position with the given scale */
void imv_image_draw(struct imv_image *image, int x, int y, double scale);

/* Get the image width */
int imv_image_width(const struct imv_image *image);

/* Get the image height */
int imv_image_height(const struct imv_image *image);

#endif


/* vim:set ts=2 sts=2 sw=2 et: */
