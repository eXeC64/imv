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

#include "texture.h"

void imv_init_texture(struct imv_texture *tex, SDL_Renderer *r)
{
  tex->renderer = r;
  tex->num_chunks = 0;
  tex->chunks = NULL;

  SDL_RendererInfo ri;
  SDL_GetRendererInfo(r, &ri);
  tex->chunk_width = ri.max_texture_width;
  tex->chunk_height = ri.max_texture_height;
}

void imv_destroy_texture(struct imv_texture *tex)
{
  if(tex->num_chunks > 0) {
    for(int i = 0; i < tex->num_chunks; ++i) {
      SDL_DestroyTexture(tex->chunks[i]);
    }
    free(tex->chunks);
    tex->num_chunks = 0;
    tex->chunks = NULL;
    tex->renderer = NULL;
  }
}

int imv_texture_set_image(struct imv_texture *tex, FIBITMAP *image)
{
  FIBITMAP *frame = FreeImage_ConvertTo32Bits(image);
  tex->width = FreeImage_GetWidth(frame);
  tex->height = FreeImage_GetHeight(frame);

  char* pixels = (char*)FreeImage_GetBits(frame);

  /* figure out how many chunks are needed, and create them */
  if(tex->num_chunks > 0) {
    for(int i = 0; i < tex->num_chunks; ++i) {
      SDL_DestroyTexture(tex->chunks[i]);
    }
    free(tex->chunks);
  }

  tex->num_chunks_wide = 1 + tex->width / tex->chunk_width;
  tex->num_chunks_tall = 1 + tex->height / tex->chunk_height;

  tex->last_chunk_width = tex->width % tex->chunk_width;
  tex->last_chunk_height = tex->height % tex->chunk_height;

  if(tex->last_chunk_width == 0) {
    tex->last_chunk_width = tex->chunk_width;
  }
  if(tex->last_chunk_height == 0) {
    tex->last_chunk_height = tex->chunk_height;
  }

  tex->num_chunks = tex->num_chunks_wide * tex->num_chunks_tall;
  tex->chunks = (SDL_Texture**)malloc(sizeof(SDL_Texture*) * tex->num_chunks);

  int failed_at = -1;
  for(int y = 0; y < tex->num_chunks_tall; ++y) {
    for(int x = 0; x < tex->num_chunks_wide; ++x) {
      const int is_last_h_chunk = (x == tex->num_chunks_wide - 1);
      const int is_last_v_chunk = (y == tex->num_chunks_tall - 1);
      tex->chunks[x + y * tex->num_chunks_wide] =
        SDL_CreateTexture(tex->renderer,
          SDL_PIXELFORMAT_RGB888,
          SDL_TEXTUREACCESS_STATIC,
          is_last_h_chunk ? tex->last_chunk_width : tex->chunk_width,
          is_last_v_chunk ? tex->last_chunk_height : tex->chunk_height);
      if(tex->chunks[x + y * tex->num_chunks_wide] == NULL) {
        failed_at = x + y * tex->num_chunks_wide;
        break;
      }
    }
  }

  if(failed_at != -1) {
    for(int i = 0; i <= failed_at; ++i) {
      SDL_DestroyTexture(tex->chunks[i]);
    }
    free(tex->chunks);
    tex->num_chunks = 0;
    tex->chunks = NULL;
    return 1;
  }

  for(int y = 0; y < tex->num_chunks_tall; ++y) {
    for(int x = 0; x < tex->num_chunks_wide; ++x) {
      ptrdiff_t offset = 4 * x * tex->chunk_width +
        y * 4 * tex->width * tex->chunk_height;
      char* addr = pixels + offset;
      SDL_UpdateTexture(tex->chunks[x + y * tex->num_chunks_wide],
          NULL, addr, 4 * tex->width);
    }
  }

  return 0;
}

void imv_texture_draw(struct imv_texture *tex, int bx, int by, double scale)
{
  int offset_x = 0;
  int offset_y = 0;

  for(int y = 0; y < tex->num_chunks_tall; ++y) {
    for(int x = 0; x < tex->num_chunks_wide; ++x) {
      int img_w, img_h, img_access;
      unsigned int img_format;
      SDL_QueryTexture(tex->chunks[x + y * tex->num_chunks_wide],
          &img_format, &img_access, &img_w, &img_h);
      SDL_Rect view_area = {
        bx + offset_x,
        by + offset_y,
        img_w * scale,
        img_h * scale
      };
      SDL_RenderCopy(tex->renderer,
          tex->chunks[x + y * tex->num_chunks_wide], NULL, &view_area);
      offset_x += tex->chunk_width * scale;
    }
    offset_x = 0;
    offset_y += tex->chunk_height * scale;
  }
}
