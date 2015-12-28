#ifndef IMV_TEXTURE_H
#define IMV_TEXTURE_H

/* Copyright (c) 2015 imv authors

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

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


/* Initialises an instance of imv_texture */
void imv_init_texture(struct imv_texture *tex, SDL_Renderer *r);

/* Cleans up all resources owned by a imv_texture instance */
void imv_destroy_texture(struct imv_texture *tex);

/* Updates the texture to contain the data in the image parameter */
int imv_texture_set_image(struct imv_texture *tex, FIBITMAP *image);

/* Draw the texture at the given position with the given scale */
void imv_texture_draw(struct imv_texture *tex, int x, int y, double scale);

#endif
