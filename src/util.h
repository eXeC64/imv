/* Copyright (c) imv authors

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

/* Read binary data from stdin into buffer */
size_t read_from_stdin(void **buffer);

/* Creates a new SDL_Texture* containing a chequeboard texture */
SDL_Texture *create_chequered(SDL_Renderer *renderer);

/* Loads a font using SDL2_ttf given a spec in the format "name:size" */
TTF_Font *load_font(const char *font_spec);

void imv_printf(SDL_Renderer *renderer, TTF_Font *font, int x, int y,
                SDL_Color *fg, SDL_Color *bg, const char *fmt, ...);

#endif


/* vim:set ts=2 sts=2 sw=2 et: */
