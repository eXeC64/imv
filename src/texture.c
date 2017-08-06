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

#include "texture.h"

struct imv_texture *imv_texture_create(SDL_Renderer *r)
{
  struct imv_texture *tex = malloc(sizeof(struct imv_texture));
  memset(tex, 0, sizeof(struct imv_texture));
  tex->renderer = r;

  SDL_RendererInfo ri;
  SDL_GetRendererInfo(r, &ri);
  tex->chunk_width = ri.max_texture_width != 0 ? ri.max_texture_width : 4096;
  tex->chunk_height = ri.max_texture_height != 0 ? ri.max_texture_height : 4096;
  return tex;
}

void imv_texture_free(struct imv_texture *tex)
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
  free(tex);
}

int imv_texture_set_image(struct imv_texture *tex, FIBITMAP *image)
{
  tex->width = FreeImage_GetWidth(image);
  tex->height = FreeImage_GetHeight(image);

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
  tex->chunks = malloc(sizeof(SDL_Texture*) * tex->num_chunks);

  int failed_at = -1;
  for(int y = 0; y < tex->num_chunks_tall; ++y) {
    for(int x = 0; x < tex->num_chunks_wide; ++x) {
      const int is_last_h_chunk = (x == tex->num_chunks_wide - 1);
      const int is_last_v_chunk = (y == tex->num_chunks_tall - 1);
      tex->chunks[x + y * tex->num_chunks_wide] =
        SDL_CreateTexture(tex->renderer,
          SDL_PIXELFORMAT_ARGB8888,
          SDL_TEXTUREACCESS_STATIC,
          is_last_h_chunk ? tex->last_chunk_width : tex->chunk_width,
          is_last_v_chunk ? tex->last_chunk_height : tex->chunk_height);
      SDL_SetTextureBlendMode(tex->chunks[x + y * tex->num_chunks_wide],
          SDL_BLENDMODE_BLEND);
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

  BYTE* pixels = malloc(4 * tex->width * tex->height);
  FreeImage_ConvertToRawBits(pixels, image, 4 * tex->width, 32,
      FI_RGBA_RED_MASK, FI_RGBA_GREEN_MASK, FI_RGBA_BLUE_MASK, TRUE);

  for(int y = 0; y < tex->num_chunks_tall; ++y) {
    for(int x = 0; x < tex->num_chunks_wide; ++x) {
      ptrdiff_t offset = 4 * x * tex->chunk_width +
        y * 4 * tex->width * tex->chunk_height;
      BYTE* addr = pixels + offset;
      SDL_UpdateTexture(tex->chunks[x + y * tex->num_chunks_wide],
          NULL, addr, 4 * tex->width);
    }
  }

  free(pixels);
  return 0;
}

void imv_texture_draw(struct imv_texture *tex, int bx, int by, double scale)
{
  int offset_x = 0;
  int offset_y = 0;
  for(int y = 0; y < tex->num_chunks_tall; ++y) {
    for(int x = 0; x < tex->num_chunks_wide; ++x) {
      int img_w, img_h;
      SDL_QueryTexture(tex->chunks[x + y * tex->num_chunks_wide], NULL, NULL,
        &img_w, &img_h);
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


/* vim:set ts=2 sts=2 sw=2 et: */
