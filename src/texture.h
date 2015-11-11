#ifndef IMV_TEXTURE_H
#define IMV_TEXTURE_H

/* Copyright (c) 2015 Harry Jeffery

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

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

void imv_init_texture(struct imv_texture *tex, SDL_Renderer *r);
void imv_destroy_texture(struct imv_texture *tex);

int imv_texture_set_image(struct imv_texture *tex, FIBITMAP *image);
void imv_texture_draw(struct imv_texture *tex, int x, int y, double scale);

#endif
