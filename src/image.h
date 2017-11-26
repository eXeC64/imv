#ifndef IMV_TEXTURE_H
#define IMV_TEXTURE_H

#include <SDL2/SDL.h>
#include <FreeImage.h>

struct imv_image {
  int width;              /* width of the image overall */
  int height;             /* height of the image overall */
  int num_chunks;         /* number of chunks allocated */
  SDL_Texture **chunks;   /* array of chunks */
  int num_chunks_wide;    /* number of chunks per row of the image */
  int num_chunks_tall;    /* number of chunks per column of the image */
  int chunk_width;        /* chunk width */
  int chunk_height;       /* chunk height */
  int last_chunk_width;   /* width of rightmost chunk */
  int last_chunk_height;  /* height of bottommost chunk */
  SDL_Renderer *renderer; /* SDL renderer to draw to */
};


/* Creates an instance of imv_image */
struct imv_image *imv_image_create(SDL_Renderer *r);

/* Cleans up an imv_image instance */
void imv_image_free(struct imv_image *image);

/* Updates the image to contain the data in the bitmap parameter */
int imv_image_set_bitmap(struct imv_image *image, FIBITMAP *bmp);

/* Draw the image at the given position with the given scale */
void imv_image_draw(struct imv_image *image, int x, int y, double scale);

#endif


/* vim:set ts=2 sts=2 sw=2 et: */
