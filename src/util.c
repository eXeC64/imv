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

#include "util.h"

#include <fontconfig/fontconfig.h>

char* load_img_stdin(size_t *lenout){
  size_t cap = 512;
    size_t len = 0;

    char *buf = malloc(cap);
    size_t n = 0;
    do {
      n = fread(&buf[len],1, cap-len, stdin);
      len += n;
      /* increase the size of our buffer if we run out of space */
      if(len == cap) {
        cap *= 2;
        buf = realloc(buf, cap);
      }
    } while(n > 0);
    
    *lenout = len;

  return buf;
}


TTF_Font *load_font(const char *font_spec)
{
  int font_size;
  char *font_name;

  /* figure out font size from name, or default to 24 */
  char *sep = strchr(font_spec, ':');
  if(sep) {
    font_name = strndup(font_spec, sep - font_spec);
    font_size = strtol(sep+1, NULL, 10);
  } else {
    font_name = strdup(font_spec);
    font_size = 24;
  }


  FcConfig *cfg = FcInitLoadConfigAndFonts();
  FcPattern *pattern = FcNameParse((const FcChar8*)font_name);
  FcConfigSubstitute(cfg, pattern, FcMatchPattern);
  FcDefaultSubstitute(pattern);

  TTF_Font *ret = NULL;

  FcResult result = FcResultNoMatch;
  FcPattern* font = FcFontMatch(cfg, pattern, &result);
  if (font) {
    FcChar8 *path = NULL;
    if (FcPatternGetString(font, FC_FILE, 0, &path) == FcResultMatch) {
      ret = TTF_OpenFont((char*)path, font_size);
    }
    FcPatternDestroy(font);
  }
  FcPatternDestroy(pattern);

  free(font_name);
  return ret;
}

SDL_Texture *create_chequered(SDL_Renderer *renderer)
{
  SDL_RendererInfo ri;
  SDL_GetRendererInfo(renderer, &ri);
  int width = 512;
  int height = 512;
  if(ri.max_texture_width != 0 && ri.max_texture_width < width) {
    width = ri.max_texture_width;
  }
  if(ri.max_texture_height != 0 && ri.max_texture_height < height) {
    height = ri.max_texture_height;
  }
  const int box_size = 16;
  /* Create a chequered texture */
  const unsigned char l = 196;
  const unsigned char d = 96;

  size_t pixels_len = 3 * width * height;
  unsigned char *pixels = malloc(pixels_len);
  for(int y = 0; y < height; y++) {
    for(int x = 0; x < width; x += box_size) {
      unsigned char color = l;
      if(((x/box_size) % 2 == 0) ^ ((y/box_size) % 2 == 0)) {
        color = d;
      }
      memset(pixels + 3 * x + 3 * width * y, color, 3 * box_size);
    }
  }
  SDL_Texture *ret = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB24,
    SDL_TEXTUREACCESS_STATIC,
    width, height);
  SDL_UpdateTexture(ret, NULL, pixels, 3 * width);
  free(pixels);
  return ret;
}

static int parse_hex_digit(char c) {
  if(c >= '0' && c <= '9') {
    return c - '0';
  } else if (c >= 'a' && c <= 'f') {
    return c - 'a' + 10;
  } else if (c >= 'A' && c <= 'F') {
    return c - 'A' + 10;
  }
  return -1;
}

int parse_hex_color(const char* str,
  unsigned char *r, unsigned char *g, unsigned char *b)
{
  if(str[0] == '#') {
    ++str;
  }

  if(strlen(str) != 6) {
    return 1;
  }

  for(int i = 0; i < 6; ++i) {
    if(!isxdigit(str[i])) {
      return 1;
    }
  }

  *r = (parse_hex_digit(str[0]) << 4) + parse_hex_digit(str[1]);
  *g = (parse_hex_digit(str[2]) << 4) + parse_hex_digit(str[3]);
  *b = (parse_hex_digit(str[4]) << 4) + parse_hex_digit(str[5]);
  return 0;
}
