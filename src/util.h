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
