/* Copyright (c) 2015 Harry Jeffery

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

#ifndef UTIL_H
#define UTIL_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

/* Read a binary image from stdin and return a buffer with the data */
char *load_img_stdin();

/* Creates a new SDL_Texture* containing a chequeboard texture */
SDL_Texture *create_chequered(SDL_Renderer *renderer);

/* Parses a triplet of hexadecimal bytes. Writes values to r, g, and b. */
int parse_hex_color(const char* str,
  unsigned char *r, unsigned char *g, unsigned char *b);

/* Loads a font using SDL2_ttf given a spec in the format "name:size" */
TTF_Font *load_font(const char *font_spec);

#endif
