#ifndef IMV_TEXTURE_H
#define IMV_TEXTURE_H

#include <SDL2/SDL.h>
#include <FreeImage.h>

struct imv_texture {
  int width;              /* width of the texture overall */
  int height;             /* height of the texture overall */
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


/* Creates an instance of imv_texture */
struct imv_texture *imv_texture_create(SDL_Renderer *r);

/* Cleans up an imv_texture instance */
void imv_texture_free(struct imv_texture *tex);

/* Updates the texture to contain the data in the image parameter */
int imv_texture_set_image(struct imv_texture *tex, FIBITMAP *image);

/* Draw the texture at the given position with the given scale */
void imv_texture_draw(struct imv_texture *tex, int x, int y, double scale);

#endif


/* vim:set ts=2 sts=2 sw=2 et: */
